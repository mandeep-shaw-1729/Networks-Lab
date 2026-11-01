/* server.c - Shared CDMA Multiplexing Channel Server (Senders -> Server -> Receiver) */
#include "csma_common.h"

typedef struct {
    sock_t  sock;
    int     active;
    int     finished;
    int     has_slot_data;
    uint8_t current_seq;
    char    name[STATION_NAME_LEN];
    int16_t chips[MAX_CHIPS_FRAME];
} ClientSlot;

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    int expected_stations = 0;

    printf("=================================================================\n");
    printf("   CDMA SHARED CHANNEL SERVER (WALSH CODE MULTIPLEXER)\n");
    printf("=================================================================\n");

    if (argc > 1) {
        expected_stations = atoi(argv[1]);
    } else {
        printf("Enter number of sender stations n (1 - %d): ", MAX_STATIONS);
        fflush(stdout);
        if (scanf("%d", &expected_stations) != 1 || expected_stations < 1) {
            expected_stations = 2;
        }
        if (expected_stations > MAX_STATIONS) expected_stations = MAX_STATIONS;
    }

    int walshN = next_power_of_2(expected_stations);
    int totalChips = BITS_PER_FRAME * walshN;
    size_t chipBytes = (size_t)totalChips * sizeof(int16_t);

    int walshTable[MAX_WALSH_SIZE][MAX_WALSH_SIZE];
    generateWalshTable(walshN, walshTable);

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    int port = get_server_port();
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    addr.sin_port = htons(port);

    sock_t listen_sock = INVALID_SOCKET;
    int bind_ok = 0;
    for (int retry = 0; retry < 15; retry++) {
        listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#ifdef SO_REUSEPORT
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEPORT, (const char *)&opt, sizeof(opt));
#endif
        if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) != SOCKET_ERROR &&
            listen(listen_sock, MAX_STATIONS + 2) != SOCKET_ERROR) {
            bind_ok = 1;
            break;
        }
        CLOSESOCK(listen_sock);
        listen_sock = INVALID_SOCKET;
        SLEEP_MS(100);
    }
    if (!bind_ok) {
        perror("Server bind/listen failed");
        return 1;
    }

    printf("\n-----------------------------------------------------------------\n");
    printf(" CDMA Channel Server Active on %s:%d\n", SERVER_IP, port);
    printf(" Expected Stations (n) : %d\n", expected_stations);
    printf(" Walsh Dimension (N)   : %dx%d Orthogonal Matrix\n", walshN, walshN);
    printf(" Slot Propagation Delay: %.1f ms\n", CDMA_SLOT_DELAY_US / 1000.0);
    printf(" Step 1: Start ./receiver first.\n");
    printf(" Step 2: Start %d ./sender instance(s).\n", expected_stations);
    printf("-----------------------------------------------------------------\n\n");

    ClientSlot *clients = (ClientSlot *)calloc(expected_stations, sizeof(ClientSlot));
    for (int i = 0; i < expected_stations; i++) {
        clients[i].sock = INVALID_SOCKET;
    }

    sock_t rx_sock = INVALID_SOCKET;
    int stations_conn = 0;

    /* Phase 1: Wait for Receiver and all n Senders to register */
    while (rx_sock == INVALID_SOCKET || stations_conn < expected_stations) {
        sock_t cs = accept(listen_sock, NULL, NULL);
        if (cs == INVALID_SOCKET) continue;

        unsigned char first_op = 0;
        if (recv_exact(cs, &first_op, 1) < 0) {
            CLOSESOCK(cs);
            continue;
        }

        if (first_op == MSG_REGISTER_RX) {
            rx_sock = cs;
            printf("[SERVER] Destination Receiver connected and waiting for Walsh table sync!\n");
        } else if (first_op == MSG_REGISTER && stations_conn < expected_stations) {
            unsigned char name_buf[STATION_NAME_LEN];
            if (recv_exact(cs, name_buf, STATION_NAME_LEN) == 0) {
                name_buf[STATION_NAME_LEN - 1] = '\0';
                int idx = stations_conn++;
                clients[idx].sock = cs;
                clients[idx].active = 1;
                strncpy(clients[idx].name, (const char *)name_buf, STATION_NAME_LEN);
                printf("[SERVER] Station #%d \"%s\" registered (%d/%d connected)\n",
                       idx, clients[idx].name, stations_conn, expected_stations);
            } else {
                CLOSESOCK(cs);
            }
        } else {
            CLOSESOCK(cs);
        }
    }

    printf("\n[SERVER] All %d stations and Receiver connected! Distributing %dx%d Walsh Codes:\n",
           expected_stations, walshN, walshN);

    /* Send initialization header & station directory to Receiver */
    size_t rxInitLen = 3 + (size_t)expected_stations * STATION_NAME_LEN;
    unsigned char *rxInitPkt = (unsigned char *)malloc(rxInitLen);
    rxInitPkt[0] = MSG_RX_INIT;
    rxInitPkt[1] = (uint8_t)expected_stations;
    rxInitPkt[2] = (uint8_t)walshN;
    for (int i = 0; i < expected_stations; i++) {
        memcpy(&rxInitPkt[3 + i * STATION_NAME_LEN], clients[i].name, STATION_NAME_LEN);
    }
    send_exact(rx_sock, rxInitPkt, rxInitLen);
    free(rxInitPkt);

    /* Send Walsh Code Assignment to each Sender */
    for (int i = 0; i < expected_stations; i++) {
        uint8_t assignPkt[4] = {
            MSG_ASSIGN_WALSH,
            (uint8_t)i,
            (uint8_t)expected_stations,
            (uint8_t)walshN
        };
        send_exact(clients[i].sock, assignPkt, 4);
        printf("  -> Station #%d (\"%s\") assigned W_%d = ", i, clients[i].name, i);
        printWalshRow(walshTable[i], walshN);
        printf("\n");
    }
    printf("\n");

    /* Phase 2: Synchronous CDMA Slot Multiplexing Loop */
    double sim_start_time = get_time_ms();
    uint32_t slot_no = 0;
    int total_frames_multiplexed = 0;

    unsigned char *rxBuf = (unsigned char *)malloc(chipBytes);
    int16_t *compositeSignal = (int16_t *)malloc(chipBytes);
    unsigned char *fwdPkt = (unsigned char *)malloc(5 + chipBytes);

    while (1) {
        int active_transmitters = 0;

        /* Collect 1 frame (or DONE signal) from every non-finished station for the current CDMA slot */
        for (int i = 0; i < expected_stations; i++) {
            clients[i].has_slot_data = 0;
            if (!clients[i].active || clients[i].finished) continue;

            unsigned char op = 0;
            if (recv_exact(clients[i].sock, &op, 1) < 0) {
                clients[i].finished = 1;
                clients[i].active = 0;
                CLOSESOCK(clients[i].sock);
                continue;
            }

            if (op == MSG_STATION_DONE) {
                clients[i].finished = 1;
                clients[i].active = 0;
                CLOSESOCK(clients[i].sock);
                printf("[SERVER] Station #%d (\"%s\") completed all transmissions.\n", i, clients[i].name);
            } else if (op == MSG_CDMA_FRAME) {
                uint8_t seq = 0;
                if (recv_exact(clients[i].sock, &seq, 1) < 0 ||
                    recv_exact(clients[i].sock, rxBuf, chipBytes) < 0) {
                    clients[i].finished = 1;
                    clients[i].active = 0;
                    CLOSESOCK(clients[i].sock);
                    continue;
                }
                clients[i].current_seq = seq;
                for (int c = 0; c < totalChips; c++) {
                    uint16_t netVal;
                    memcpy(&netVal, &rxBuf[c * 2], 2);
                    clients[i].chips[c] = (int16_t)ntohs(netVal);
                }
                clients[i].has_slot_data = 1;
                active_transmitters++;
            }
        }

        if (active_transmitters == 0) {
            break; /* All stations have finished transmitting */
        }

        /* 0.5 ms pacing delay between CDMA slots */
        SLEEP_US(CDMA_SLOT_DELAY_US);

        slot_no++;

        /* Superpose (sum) chip sequences across all simultaneously transmitting stations */
        memset(compositeSignal, 0, chipBytes);
        for (int i = 0; i < expected_stations; i++) {
            if (clients[i].has_slot_data) {
                for (int c = 0; c < totalChips; c++) {
                    compositeSignal[c] += clients[i].chips[c];
                }
                total_frames_multiplexed++;
            }
        }

        double elapsed = get_time_ms() - sim_start_time;
        printf("[%.1f ms] [CDMA SLOT #%u] Superposing %d simultaneous station(s): ",
               elapsed, slot_no, active_transmitters);
        for (int i = 0; i < expected_stations; i++) {
            if (clients[i].has_slot_data) {
                printf("\"%s\"(Seq %u) ", clients[i].name, clients[i].current_seq);
            }
        }
        printf("-> Sample Combined Chips[0..%d]: [", walshN - 1);
        for (int c = 0; c < walshN; c++) {
            printf("%+d%s", compositeSignal[c], (c == walshN - 1) ? "" : ", ");
        }
        printf("]\n");

        /* Forward single composite signal over the shared channel socket to Receiver */
        fwdPkt[0] = MSG_COMPOSITE_SIG;
        uint32_t netSlot = htonl(slot_no);
        memcpy(&fwdPkt[1], &netSlot, 4);
        for (int c = 0; c < totalChips; c++) {
            uint16_t netChip = htons((uint16_t)compositeSignal[c]);
            memcpy(&fwdPkt[5 + c * 2], &netChip, 2);
        }
        send_exact(rx_sock, fwdPkt, 5 + chipBytes);

        /* Wait for Receiver to decode and acknowledge the slot */
        uint8_t rxAck[2] = {0, 0};
        recv_exact(rx_sock, rxAck, 2);

        /* Relay ACK to all stations that transmitted in this slot */
        for (int i = 0; i < expected_stations; i++) {
            if (clients[i].has_slot_data) {
                uint8_t stAck[2] = { MSG_TX_ACK, clients[i].current_seq };
                send_exact(clients[i].sock, stAck, 2);
            }
        }
    }

    /* Signal Receiver that all stations are done */
    if (rx_sock != INVALID_SOCKET) {
        uint8_t done_op = MSG_STATION_DONE;
        send_exact(rx_sock, &done_op, 1);
        CLOSESOCK(rx_sock);
    }

    double total_time_s = (get_time_ms() - sim_start_time) / 1000.0;
    if (total_time_s <= 0.0) total_time_s = 0.001;
    double throughput_kbps = (total_frames_multiplexed * FRAME_SIZE * 8.0) / (total_time_s * 1000.0);

    printf("\n================ CDMA CHANNEL SERVER SUMMARY ================\n");
    printf("Multiplexing Scheme     : Synchronous CDMA with Walsh Codes\n");
    printf("Connected Stations (n)  : %d (Walsh Order N = %d)\n", expected_stations, walshN);
    printf("Total CDMA Slots        : %u\n", slot_no);
    printf("Total Frames Combined   : %d (%d Bytes each)\n", total_frames_multiplexed, FRAME_SIZE);
    printf("Channel Collisions      : 0 (Lossless Linear Superposition)\n");
    printf("Total Elapsed Time      : %.3f s\n", total_time_s);
    printf("Aggregate Throughput    : %.2f kbps\n", throughput_kbps);
    printf("=============================================================\n");

    free(rxBuf);
    free(compositeSignal);
    free(fwdPkt);
    free(clients);
    CLOSESOCK(listen_sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
