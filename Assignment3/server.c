/* server.c - Shared Physical Channel Emulator (Senders -> Server -> Receiver) */
#include "csma_common.h"

typedef struct {
    sock_t        sock;
    int           active;
    int           waiting_for_proto;
    char          name[STATION_NAME_LEN];
    int           transmitting;
    CSMAProtocol  proto;
    unsigned char raw_buf[FRAME_SIZE];
    EthernetFrame frame;
} ClientSlot;

static const char* state_str(ChannelState st) {
    switch (st) {
        case CH_IDLE:      return "IDLE";
        case CH_BUSY:      return "BUSY";
        case CH_COLLISION: return "COLLISION";
        default:           return "UNKNOWN";
    }
}

int main(int argc, char *argv[]) {
    int expected_stations = 0;

    printf("=================================================================\n");
    printf("   IEEE 802.3 SHARED CHANNEL SERVER (SENDERS -> SERVER -> RX)\n");
    printf("=================================================================\n");

    if (argc > 1) {
        expected_stations = atoi(argv[1]);
    } else {
        printf("Enter number of expecting sender stations (1 - %d): ", MAX_STATIONS);
        fflush(stdout);
        if (scanf("%d", &expected_stations) != 1 || expected_stations < 1) {
            expected_stations = 2;
        }
        if (expected_stations > MAX_STATIONS) expected_stations = MAX_STATIONS;
    }

#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    sock_t listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = inet_addr(SERVER_IP); addr.sin_port = htons(SERVER_PORT);
    
    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(listen_sock, MAX_STATIONS + 2) == SOCKET_ERROR) {
        perror("Server bind/listen failed");
        return 1;
    }

    printf("\n-----------------------------------------------------------------\n");
    printf(" Shared Channel Server Active on %s:%d | State: [IDLE]\n", SERVER_IP, SERVER_PORT);
    printf(" Step 1: Start ./receiver first.\n");
    printf(" Step 2: Start %d ./sender instance(s).\n", expected_stations);
    printf("-----------------------------------------------------------------\n\n");

    ClientSlot clients[MAX_STATIONS];
    memset(clients, 0, sizeof(clients));
    for (int i = 0; i < MAX_STATIONS; i++) { 
        clients[i].sock = INVALID_SOCKET; 
        clients[i].proto = PROTO_UNSET; 
    }

    sock_t rx_sock = INVALID_SOCKET;
    ChannelState channel_state   = CH_IDLE;
    CSMAProtocol locked_protocol = PROTO_UNSET;
    int is_synced                = -1; /* -1: Unset, 1: Synced, 0: Unsynced */
    int proto_chooser_slot       = -1;

    double state_end_time = 0.0, sim_start_time = 0.0;
    int total_fwd = 0, total_col = 0, stations_conn = 0, stations_fin = 0, total_drops = 0;

    while (1) {
        fd_set readfds; FD_ZERO(&readfds); FD_SET(listen_sock, &readfds);
        sock_t max_sd = listen_sock;
        if (rx_sock != INVALID_SOCKET) { FD_SET(rx_sock, &readfds); if (rx_sock > max_sd) max_sd = rx_sock; }
        for (int i = 0; i < MAX_STATIONS; i++) {
            if (clients[i].active) { FD_SET(clients[i].sock, &readfds); if (clients[i].sock > max_sd) max_sd = clients[i].sock; }
        }

        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 1000;
        select((int)(max_sd + 1), &readfds, NULL, NULL, &tv);
        double now = get_time_ms();

        /* Channel State Timer */
        if (channel_state != CH_IDLE && now >= state_end_time) {
            if (channel_state == CH_BUSY) {
                for (int i = 0; i < MAX_STATIONS; i++) {
                    if (clients[i].active && clients[i].transmitting) {
                        if (rx_sock != INVALID_SOCKET) {
                            unsigned char fwd[1 + STATION_NAME_LEN + FRAME_SIZE];
                            fwd[0] = (clients[i].proto == PROTO_CSMA_CD) ? MSG_FWD_FRAME_ACK : MSG_FWD_FRAME_NOACK;
                            memset(&fwd[1], 0, STATION_NAME_LEN);
                            strncpy((char *)&fwd[1], clients[i].name, STATION_NAME_LEN - 1);
                            memcpy(&fwd[1 + STATION_NAME_LEN], clients[i].raw_buf, FRAME_SIZE);
                            send(rx_sock, (const char *)fwd, sizeof(fwd), 0);

                            if (clients[i].proto == PROTO_CSMA_CD) {
                                uint8_t ack[2] = {0, 0};
                                if (recv_exact(rx_sock, ack, 2) == 0) send(clients[i].sock, (const char *)ack, 2, 0);
                            }
                        }
                        total_fwd++;
                        
                        if (clients[i].proto == PROTO_CSMA_CD) {
                            printf("[%.1f ms] [CH: BUSY -> IDLE] Station \"%s\" | Seq %u forwarded to Receiver -> ACK Relayed\n",
                                   now - sim_start_time, clients[i].name, clients[i].frame.sequenceNumber);
                        } else {
                            printf("[%.1f ms] [CH: BUSY -> IDLE] Station \"%s\" | Seq %u forwarded to Receiver (No ACK in pure CSMA)\n",
                                   now - sim_start_time, clients[i].name, clients[i].frame.sequenceNumber);
                        }
                        clients[i].transmitting = 0;
                    }
                }
            } else if (channel_state == CH_COLLISION) {
                for (int i = 0; i < MAX_STATIONS; i++) clients[i].transmitting = 0;
                printf("[%.1f ms] [CH: COLLISION -> IDLE] Medium is IDLE.\n", now - sim_start_time);
            }
            channel_state = CH_IDLE;
        }

        /* Accept Connections */
        if (FD_ISSET(listen_sock, &readfds)) {
            sock_t cs = accept(listen_sock, NULL, NULL);
            unsigned char first_op = 0;
            if (recv_exact(cs, &first_op, 1) == 0) {
                if (first_op == MSG_REGISTER_RX) {
                    rx_sock = cs;
                    printf("[SERVER] Destination Receiver connected and ready!\n");
                } else if (first_op == MSG_REGISTER) {
                    unsigned char name_buf[STATION_NAME_LEN];
                    recv_exact(cs, name_buf, STATION_NAME_LEN);
                    name_buf[STATION_NAME_LEN - 1] = '\0';
                    for (int i = 0; i < MAX_STATIONS; i++) {
                        if (!clients[i].active) {
                            clients[i].sock = cs; clients[i].active = 1;
                            strncpy(clients[i].name, (const char *)name_buf, STATION_NAME_LEN);
                            stations_conn++;
                            printf("[SERVER] Sender Station \"%s\" connected (%d/%d expected)\n",
                                   clients[i].name, stations_conn, expected_stations);
                            
                            if (is_synced == -1 && proto_chooser_slot == -1) {
                                proto_chooser_slot = i;
                                uint8_t sync[3] = { MSG_PROTO_SYNC, PROTO_UNSET, 0xFF }; 
                                send(cs, (const char *)sync, 3, 0);
                                printf("[SERVER] Waiting for first station (\"%s\") to select Sync Mode...\n", clients[i].name);
                            } else if (is_synced == 1) {
                                if (locked_protocol != PROTO_UNSET) {
                                    uint8_t sync[3] = { MSG_PROTO_SYNC, (uint8_t)locked_protocol, 1 };
                                    send(cs, (const char *)sync, 3, 0);
                                    clients[i].proto = locked_protocol;
                                    printf("[SERVER] Mode [LOCKED]: Auto-assigned protocol [%s] to Station \"%s\"\n",
                                           getProtocolName(locked_protocol), clients[i].name);
                                } else {
                                    clients[i].waiting_for_proto = 1;
                                }
                            } else if (is_synced == 0) {
                                uint8_t sync[3] = { MSG_PROTO_SYNC, PROTO_UNSET, 0 }; 
                                send(cs, (const char *)sync, 3, 0);
                                printf("[SERVER] Mode [UNLOCKED]: Prompting Station \"%s\" for its protocol...\n", clients[i].name);
                            }
                            break;
                        }
                    }
                }
            }
        }

        /* Process Commands */
        for (int i = 0; i < MAX_STATIONS; i++) {
            if (!clients[i].active || !FD_ISSET(clients[i].sock, &readfds)) continue;
            unsigned char op = 0;
            if (recv(clients[i].sock, (char *)&op, 1, 0) <= 0) { CLOSESOCK(clients[i].sock); clients[i].active = 0; continue; }

            if (op == MSG_SET_MODE) {
                unsigned char mode = 0; recv_exact(clients[i].sock, &mode, 1);
                is_synced = mode;
                if (is_synced == 0) {
                    for (int k = 0; k < MAX_STATIONS; k++) {
                        if (clients[k].active && clients[k].waiting_for_proto) {
                            uint8_t sync[3] = { MSG_PROTO_SYNC, PROTO_UNSET, 0 };
                            send(clients[k].sock, (const char *)sync, 3, 0);
                            clients[k].waiting_for_proto = 0;
                            printf("[SERVER] Mode [UNLOCKED]: Prompting Station \"%s\" for its protocol...\n", clients[k].name);
                        }
                    }
                }
            }
            else if (op == MSG_SET_PROTO) {
                unsigned char proto_val = 0; recv_exact(clients[i].sock, &proto_val, 1);
                clients[i].proto = (CSMAProtocol)proto_val;
                
                if (is_synced == 1 && locked_protocol == PROTO_UNSET) {
                    locked_protocol = clients[i].proto;
                    printf("[SERVER] SYNC MODE: LOCKED. Protocol [%s] applied to all stations!\n", getProtocolName(locked_protocol));
                    for (int k = 0; k < MAX_STATIONS; k++) {
                        if (clients[k].active && clients[k].waiting_for_proto) {
                            uint8_t sync[3] = { MSG_PROTO_SYNC, (uint8_t)locked_protocol, 1 };
                            send(clients[k].sock, (const char *)sync, 3, 0);
                            clients[k].proto = locked_protocol;
                            clients[k].waiting_for_proto = 0;
                            printf("[SERVER] Mode [LOCKED]: Auto-assigned protocol [%s] to Station \"%s\"\n",
                                   getProtocolName(locked_protocol), clients[k].name);
                        }
                    }
                } else if (is_synced == 0) {
                    printf("[SERVER] Station \"%s\" independently chose protocol [%s]\n", clients[i].name, getProtocolName(clients[i].proto));
                }
            }
            else if (op == MSG_SENSE_REQ) {
                if (sim_start_time == 0.0) sim_start_time = get_time_ms();
                uint8_t state = (channel_state == CH_IDLE) ? CH_IDLE : CH_BUSY;
                uint8_t resp[2] = { MSG_SENSE_RESP, state };
                send(clients[i].sock, (const char *)resp, 2, 0);
            }
            else if (op == MSG_TX_START) {
                if (sim_start_time == 0.0) sim_start_time = get_time_ms();
                recv_exact(clients[i].sock, clients[i].raw_buf, FRAME_SIZE);
                unpackFrame(clients[i].raw_buf, &clients[i].frame);
                clients[i].transmitting = 1;

                if (channel_state == CH_IDLE) {
                    channel_state = CH_BUSY;
                    state_end_time = now + TX_TIME_MS;
                    printf("[%.1f ms] [CH: IDLE -> BUSY] Station \"%s\" transmitting Seq %u (%dB)\n",
                           now - sim_start_time, clients[i].name, clients[i].frame.sequenceNumber, FRAME_SIZE);
                } else {
                    ChannelState prev_state = channel_state;
                    if (channel_state == CH_BUSY) { total_col++; total_drops += 2; } else { total_drops += 1; }
                    channel_state = CH_COLLISION;
                    double max_end = now;
                    for (int k = 0; k < MAX_STATIONS; k++) {
                        if (clients[k].active && clients[k].transmitting) {
                            if (clients[k].proto == PROTO_CSMA_CD) {
                                uint8_t jam[2] = { MSG_COLLISION, CH_COLLISION };
                                send(clients[k].sock, (const char *)jam, 2, 0);
                                clients[k].transmitting = 0;
                                if (now + JAM_TIME_MS > max_end) max_end = now + JAM_TIME_MS;
                            } else {
                                if (now + TX_TIME_MS > max_end) max_end = now + TX_TIME_MS;
                            }
                        }
                    }
                    state_end_time = max_end;
                    printf("[%.1f ms] [CH: %s -> COLLISION!] Station \"%s\" collided! (Total Collisions: %d)\n",
                           now - sim_start_time, state_str(prev_state), clients[i].name, total_col);
                }
            }
            else if (op == MSG_STATION_DONE) {
                stations_fin++;
                CLOSESOCK(clients[i].sock); clients[i].active = 0;
            }
        }
        if (stations_conn >= expected_stations && stations_fin >= expected_stations) break;
    }

    if (rx_sock != INVALID_SOCKET) {
        uint8_t done_op = MSG_STATION_DONE;
        send(rx_sock, (const char *)&done_op, 1, 0);
        CLOSESOCK(rx_sock);
    }

    double total_time_s = (get_time_ms() - sim_start_time) / 1000.0;
    if (total_time_s <= 0.0) total_time_s = 0.001;
    double throughput_kbps = (total_fwd * FRAME_SIZE * 8.0) / (total_time_s * 1000.0);

    printf("\n================ SERVER CHANNEL SUMMARY ================\n");
    if (is_synced == 1) printf("Mode                   : LOCKED (%s)\n", getProtocolName(locked_protocol));
    else printf("Mode                   : UNLOCKED (Mixed Protocols)\n");
    printf("Contending Stations    : %d\n", expected_stations);
    printf("Frames Forwarded (65B) : %d\n", total_fwd);
    printf("Total Collisions       : %d\n", total_col);
    printf("Dropped Frames         : %d\n", total_drops);
    printf("Total Elapsed Time     : %.3f s\n", total_time_s);
    printf("Effective Throughput   : %.2f kbps (%.2f frames/sec)\n", throughput_kbps, total_fwd / total_time_s);
    printf("========================================================\n");

    CLOSESOCK(listen_sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
