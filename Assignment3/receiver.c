/* receiver.c - Locks CSMA Protocol from First Sender for All Subsequent Senders */
#include "csma_common.h"

typedef struct {
    sock_t        sock;
    int           active;
    int           registered;
    int           waiting_for_proto;
    char          name[STATION_NAME_LEN];
    int           transmitting;
    unsigned char raw_buf[FRAME_SIZE];
    EthernetFrame frame;
} ClientSlot;

static int recv_exact(sock_t s, unsigned char *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        int r = recv(s, (char *)(buf + got), (int)(len - got), 0);
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}

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
    double noise_prob     = 0.0;
    int verbose_frames    = 0;

    printf("=================================================================\n");
    printf("   IEEE 802.3 CSMA/CD SHARED CHANNEL & RECEIVER EMULATOR\n");
    printf("=================================================================\n");

    if (argc > 1) {
        expected_stations = atoi(argv[1]);
        if (argc > 2) noise_prob = atof(argv[2]);
        if (argc > 3) verbose_frames = atoi(argv[3]);
    } else {
        printf("Enter number of expecting stations (1 - %d): ", MAX_STATIONS);
        fflush(stdout);
        if (scanf("%d", &expected_stations) != 1 || expected_stations < 1) {
            expected_stations = 2;
        }
        if (expected_stations > MAX_STATIONS) expected_stations = MAX_STATIONS;

        printf("Display full 65-byte frame details on receive? (0 = No, 1 = Yes): ");
        fflush(stdout);
        if (scanf("%d", &verbose_frames) != 1) {
            verbose_frames = 0;
        }
    }

    srand((unsigned int)time(NULL));

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    sock_t listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    addr.sin_port = htons(SERVER_PORT);

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(listen_sock, MAX_STATIONS) == SOCKET_ERROR) {
        perror("Channel bind/listen failed");
        return 1;
    }

    printf("\n-----------------------------------------------------------------\n");
    printf(" Shared Channel Active on %s:%d | State: [IDLE]\n", SERVER_IP, SERVER_PORT);
    printf(" Waiting for %d station(s) to connect and start transmission...\n", expected_stations);
    printf("-----------------------------------------------------------------\n\n");

    ClientSlot clients[MAX_STATIONS];
    memset(clients, 0, sizeof(clients));
    for (int i = 0; i < MAX_STATIONS; i++) clients[i].sock = INVALID_SOCKET;

    ChannelState channel_state   = CH_IDLE;
    CSMAProtocol locked_protocol = PROTO_UNSET;
    int proto_chooser_slot       = -1;

    double state_end_time = 0.0;
    double sim_start_time = 0.0;

    int total_frames_delivered = 0;
    int total_collisions       = 0;
    int total_crc_errors       = 0;
    int stations_connected     = 0;
    int stations_finished      = 0;

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_sock, &readfds);
        sock_t max_sd = listen_sock;

        for (int i = 0; i < MAX_STATIONS; i++) {
            if (clients[i].active) {
                FD_SET(clients[i].sock, &readfds);
                if (clients[i].sock > max_sd) max_sd = clients[i].sock;
            }
        }

        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 2000;

        select((int)(max_sd + 1), &readfds, NULL, NULL, &tv);
        double now = get_time_ms();

        /* 1. Check if BUSY or COLLISION interval has completed */
        if (channel_state != CH_IDLE && now >= state_end_time) {
            if (channel_state == CH_BUSY) {
                for (int i = 0; i < MAX_STATIONS; i++) {
                    if (clients[i].active && clients[i].transmitting) {
                        EthernetFrame *rxFrame = &clients[i].frame;
                        unpackFrame(clients[i].raw_buf, rxFrame);

                        if (noise_prob > 0.0 && ((double)rand() / RAND_MAX) < noise_prob) {
                            injectRandomError(rxFrame);
                        }

                        if (verifyFrameCRC(rxFrame, CRC_32)) {
                            uint16_t csum = calculateFrameChecksum(rxFrame);
                            uint8_t ack[2] = { ACK_MARKER, rxFrame->sequenceNumber };
                            send(clients[i].sock, (const char *)ack, 2, 0);
                            total_frames_delivered++;

                            printf("[%.1f ms] [CH: BUSY -> IDLE] Station \"%s\" | Seq %u | CRC-32 OK (0x%08X) | Checksum: 0x%04X -> ACK Sent\n",
                                   now - sim_start_time, clients[i].name,
                                   rxFrame->sequenceNumber, rxFrame->fcs, csum);

                            if (verbose_frames) {
                                printFrame(rxFrame);
                            }
                        } else {
                            total_crc_errors++;
                            printf("[%.1f ms] [CH: CRC ERROR] Station \"%s\" | Seq %u failed CRC-32 check, discarded!\n",
                                   now - sim_start_time, clients[i].name, rxFrame->sequenceNumber);
                            uint8_t nak[2] = { 0x00, rxFrame->sequenceNumber };
                            send(clients[i].sock, (const char *)nak, 2, 0);
                        }
                        clients[i].transmitting = 0;
                    }
                }
            } else if (channel_state == CH_COLLISION) {
                for (int i = 0; i < MAX_STATIONS; i++) {
                    clients[i].transmitting = 0;
                }
                printf("[%.1f ms] [CH: COLLISION -> IDLE] Jam interval ended; medium is IDLE.\n",
                       now - sim_start_time);
            }
            channel_state = CH_IDLE;
        }

        /* 2. Accept new station connections */
        if (FD_ISSET(listen_sock, &readfds)) {
            sock_t cs = accept(listen_sock, NULL, NULL);
            if (cs != INVALID_SOCKET) {
                for (int i = 0; i < MAX_STATIONS; i++) {
                    if (!clients[i].active) {
                        clients[i].sock = cs;
                        clients[i].active = 1;
                        clients[i].registered = 0;
                        clients[i].waiting_for_proto = 0;
                        snprintf(clients[i].name, STATION_NAME_LEN, "Slot-%d", i + 1);
                        clients[i].transmitting = 0;
                        stations_connected++;
                        break;
                    }
                }
            }
        }

        /* 3. Process Station Registration, Protocol Lock, Carrier Sense & Transmissions */
        for (int i = 0; i < MAX_STATIONS; i++) {
            if (!clients[i].active || !FD_ISSET(clients[i].sock, &readfds)) continue;

            unsigned char opcode = 0;
            int r = recv(clients[i].sock, (char *)&opcode, 1, 0);
            if (r <= 0) {
                CLOSESOCK(clients[i].sock);
                clients[i].active = 0;
                continue;
            }

            if (opcode == MSG_REGISTER) {
                unsigned char name_buf[STATION_NAME_LEN];
                if (recv_exact(clients[i].sock, name_buf, STATION_NAME_LEN) == 0) {
                    name_buf[STATION_NAME_LEN - 1] = '\0';
                    strncpy(clients[i].name, (const char *)name_buf, STATION_NAME_LEN);
                    clients[i].registered = 1;
                    printf("[CHANNEL] Station \"%s\" connected (%d/%d expected)\n",
                           clients[i].name, stations_connected, expected_stations);

                    if (locked_protocol == PROTO_UNSET && proto_chooser_slot == -1) {
                        /* First station gets to choose the protocol for everyone */
                        proto_chooser_slot = i;
                        uint8_t sync[2] = { MSG_PROTO_SYNC, (uint8_t)PROTO_UNSET };
                        send(clients[i].sock, (const char *)sync, 2, 0);
                        printf("[CHANNEL] Waiting for first station (\"%s\") to select CSMA protocol...\n",
                               clients[i].name);
                    } else if (locked_protocol != PROTO_UNSET) {
                        /* Protocol is already locked; immediately inform this station */
                        uint8_t sync[2] = { MSG_PROTO_SYNC, (uint8_t)locked_protocol };
                        send(clients[i].sock, (const char *)sync, 2, 0);
                        printf("[CHANNEL] Assigned locked protocol [%s] to Station \"%s\"\n",
                               getProtocolName(locked_protocol), clients[i].name);
                    } else {
                        /* First station is still at the menu; queue this station until chosen */
                        clients[i].waiting_for_proto = 1;
                    }
                }
            }
            else if (opcode == MSG_SET_PROTO) {
                unsigned char chosen = 0;
                if (recv_exact(clients[i].sock, &chosen, 1) == 0) {
                    locked_protocol = (CSMAProtocol)chosen;
                    printf("[CHANNEL] Protocol LOCKED to [%s] by first station \"%s\"!\n",
                           getProtocolName(locked_protocol), clients[i].name);

                    /* Release any other stations that registered while first station was choosing */
                    for (int k = 0; k < MAX_STATIONS; k++) {
                        if (clients[k].active && clients[k].waiting_for_proto) {
                            uint8_t sync[2] = { MSG_PROTO_SYNC, (uint8_t)locked_protocol };
                            send(clients[k].sock, (const char *)sync, 2, 0);
                            clients[k].waiting_for_proto = 0;
                            printf("[CHANNEL] Assigned locked protocol [%s] to Station \"%s\"\n",
                                   getProtocolName(locked_protocol), clients[k].name);
                        }
                    }
                }
            }
            else if (opcode == MSG_SENSE_REQ) {
                if (sim_start_time == 0.0) sim_start_time = get_time_ms();
                uint8_t resp[2] = { MSG_SENSE_RESP, (uint8_t)channel_state };
                send(clients[i].sock, (const char *)resp, 2, 0);
            }
            else if (opcode == MSG_TX_START) {
                if (sim_start_time == 0.0) sim_start_time = get_time_ms();
                recv_exact(clients[i].sock, clients[i].raw_buf, FRAME_SIZE);
                unpackFrame(clients[i].raw_buf, &clients[i].frame);
                clients[i].transmitting = 1;

                if (channel_state == CH_IDLE) {
                    channel_state = CH_BUSY;
                    state_end_time = now + TX_TIME_MS;
                    printf("[%.1f ms] [CH: IDLE -> BUSY] Station \"%s\" transmitting Seq %u (%dB)\n",
                           now - sim_start_time, clients[i].name,
                           clients[i].frame.sequenceNumber, FRAME_SIZE);
                } else {
                    ChannelState prev_state = channel_state;
                    if (channel_state == CH_BUSY) {
                        total_collisions++;
                    }
                    channel_state = CH_COLLISION;
                    state_end_time = now + JAM_TIME_MS;

                    injectBurstError(&clients[i].frame, 16);

                    printf("[%.1f ms] [CH: %s -> COLLISION!] Station \"%s\" collided on medium! (Total Collisions: %d)\n",
                           now - sim_start_time, state_str(prev_state), clients[i].name, total_collisions);

                    uint8_t jam[2] = { MSG_COLLISION, (uint8_t)CH_COLLISION };
                    for (int k = 0; k < MAX_STATIONS; k++) {
                        if (clients[k].active && clients[k].transmitting) {
                            send(clients[k].sock, (const char *)jam, 2, 0);
                            clients[k].transmitting = 0;
                        }
                    }
                }
            }
            else if (opcode == MSG_STATION_DONE) {
                stations_finished++;
                printf("[CHANNEL] Station \"%s\" completed all transmissions (%d/%d done)\n",
                       clients[i].name, stations_finished, expected_stations);
                CLOSESOCK(clients[i].sock);
                clients[i].active = 0;
            }
        }

        if (stations_connected >= expected_stations && stations_finished >= expected_stations) {
            break;
        }
    }

    double total_time_s = (get_time_ms() - sim_start_time) / 1000.0;
    if (total_time_s <= 0.0) total_time_s = 0.001;
    double throughput_kbps = (total_frames_delivered * FRAME_SIZE * 8.0) / (total_time_s * 1000.0);

    printf("\n================ CHANNEL SUMMARY ================\n");
    printf("Locked Protocol        : %s\n", getProtocolName(locked_protocol));
    printf("Contending Stations    : %d\n", expected_stations);
    printf("Frames Delivered (65B) : %d\n", total_frames_delivered);
    printf("Total Collisions       : %d\n", total_collisions);
    printf("CRC-32 Noise Drops     : %d\n", total_crc_errors);
    printf("Total Elapsed Time     : %.3f s\n", total_time_s);
    printf("Effective Throughput   : %.2f kbps (%.2f frames/sec)\n",
           throughput_kbps, total_frames_delivered / total_time_s);
    printf("=================================================\n");

    CLOSESOCK(listen_sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
