/* server.c - Shared Physical Channel Emulator (Senders -> Server -> Receiver) */
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
    for (int i = 0; i < MAX_STATIONS; i++) clients[i].sock = INVALID_SOCKET;

    sock_t rx_sock = INVALID_SOCKET;

    ChannelState channel_state   = CH_IDLE;
    CSMAProtocol locked_protocol = PROTO_UNSET;
    int proto_chooser_slot       = -1;

    double state_end_time = 0.0;
    double sim_start_time = 0.0;

    int total_frames_forwarded = 0;
    int total_collisions       = 0;
    int total_dropped_frames   = 0;
    int stations_connected     = 0;
    int stations_finished      = 0;

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_sock, &readfds);
        sock_t max_sd = listen_sock;

        if (rx_sock != INVALID_SOCKET) {
            FD_SET(rx_sock, &readfds);
            if (rx_sock > max_sd) max_sd = rx_sock;
        }

        for (int i = 0; i < MAX_STATIONS; i++) {
            if (clients[i].active) {
                FD_SET(clients[i].sock, &readfds);
                if (clients[i].sock > max_sd) max_sd = clients[i].sock;
            }
        }

        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 1000;

        select((int)(max_sd + 1), &readfds, NULL, NULL, &tv);
        double now = get_time_ms();

        /* 1. Check if BUSY or COLLISION interval on the shared channel has completed */
        if (channel_state != CH_IDLE && now >= state_end_time) {
            if (channel_state == CH_BUSY) {
                for (int i = 0; i < MAX_STATIONS; i++) {
                    if (clients[i].active && clients[i].transmitting) {
                        /* Clean transmission completed without collision -> Forward to Receiver */
                        if (rx_sock != INVALID_SOCKET) {
                            unsigned char fwd_pkt[1 + STATION_NAME_LEN + FRAME_SIZE];
                            fwd_pkt[0] = (locked_protocol == PROTO_CSMA_CD) ? MSG_FWD_FRAME_ACK : MSG_FWD_FRAME_NOACK;
                            memset(&fwd_pkt[1], 0, STATION_NAME_LEN);
                            strncpy((char *)&fwd_pkt[1], clients[i].name, STATION_NAME_LEN - 1);
                            memcpy(&fwd_pkt[1 + STATION_NAME_LEN], clients[i].raw_buf, FRAME_SIZE);

                            send(rx_sock, (const char *)fwd_pkt, sizeof(fwd_pkt), 0);

                            /* ONLY CSMA/CD receives and relays an ACK */
                            if (locked_protocol == PROTO_CSMA_CD) {
                                uint8_t ack[2] = {0, 0};
                                if (recv_exact(rx_sock, ack, 2) == 0) {
                                    send(clients[i].sock, (const char *)ack, 2, 0);
                                }
                            }
                        } else if (locked_protocol == PROTO_CSMA_CD) {
                            /* Fallback ACK for CSMA/CD if receiver wasn't started */
                            uint8_t ack[2] = { ACK_MARKER, clients[i].frame.sequenceNumber };
                            send(clients[i].sock, (const char *)ack, 2, 0);
                        }

                        total_frames_forwarded++;
                        if (locked_protocol == PROTO_CSMA_CD) {
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
                for (int i = 0; i < MAX_STATIONS; i++) {
                    clients[i].transmitting = 0;
                }
                if (locked_protocol == PROTO_CSMA_CD) {
                    printf("[%.1f ms] [CH: COLLISION -> IDLE] CSMA/CD Jam interval ended; medium is IDLE.\n",
                           now - sim_start_time);
                } else {
                    printf("[%.1f ms] [CH: COLLISION -> IDLE] Collided frames dropped naturally; medium is IDLE.\n",
                           now - sim_start_time);
                }
            }
            channel_state = CH_IDLE;
        }

        /* 2. Accept new connections (either Receiver or Sender stations) */
        if (FD_ISSET(listen_sock, &readfds)) {
            sock_t cs = accept(listen_sock, NULL, NULL);
            if (cs != INVALID_SOCKET) {
                unsigned char first_op = 0;
                if (recv_exact(cs, &first_op, 1) == 0) {
                    if (first_op == MSG_REGISTER_RX) {
                        rx_sock = cs;
                        printf("[SERVER] Destination Receiver connected and ready!\n");
                    } else if (first_op == MSG_REGISTER) {
                        unsigned char name_buf[STATION_NAME_LEN];
                        if (recv_exact(cs, name_buf, STATION_NAME_LEN) == 0) {
                            name_buf[STATION_NAME_LEN - 1] = '\0';
                            for (int i = 0; i < MAX_STATIONS; i++) {
                                if (!clients[i].active) {
                                    clients[i].sock = cs;
                                    clients[i].active = 1;
                                    clients[i].registered = 1;
                                    clients[i].waiting_for_proto = 0;
                                    strncpy(clients[i].name, (const char *)name_buf, STATION_NAME_LEN);
                                    clients[i].transmitting = 0;
                                    stations_connected++;

                                    printf("[SERVER] Sender Station \"%s\" connected (%d/%d expected)\n",
                                           clients[i].name, stations_connected, expected_stations);

                                    if (locked_protocol == PROTO_UNSET && proto_chooser_slot == -1) {
                                        proto_chooser_slot = i;
                                        uint8_t sync[2] = { MSG_PROTO_SYNC, (uint8_t)PROTO_UNSET };
                                        send(clients[i].sock, (const char *)sync, 2, 0);
                                        printf("[SERVER] Waiting for first station (\"%s\") to select CSMA protocol...\n",
                                               clients[i].name);
                                    } else if (locked_protocol != PROTO_UNSET) {
                                        uint8_t sync[2] = { MSG_PROTO_SYNC, (uint8_t)locked_protocol };
                                        send(clients[i].sock, (const char *)sync, 2, 0);
                                        printf("[SERVER] Assigned locked protocol [%s] to Station \"%s\"\n",
                                               getProtocolName(locked_protocol), clients[i].name);
                                    } else {
                                        clients[i].waiting_for_proto = 1;
                                    }
                                    break;
                                }
                            }
                        }
                    } else {
                        CLOSESOCK(cs);
                    }
                } else {
                    CLOSESOCK(cs);
                }
            }
        }

        /* 3. Process Sender Commands: Protocol Lock, Carrier Sense & Frame Transmissions */
        for (int i = 0; i < MAX_STATIONS; i++) {
            if (!clients[i].active || !FD_ISSET(clients[i].sock, &readfds)) continue;

            unsigned char opcode = 0;
            int r = recv(clients[i].sock, (char *)&opcode, 1, 0);
            if (r <= 0) {
                CLOSESOCK(clients[i].sock);
                clients[i].active = 0;
                continue;
            }

            if (opcode == MSG_SET_PROTO) {
                unsigned char chosen = 0;
                if (recv_exact(clients[i].sock, &chosen, 1) == 0) {
                    locked_protocol = (CSMAProtocol)chosen;
                    printf("[SERVER] Protocol LOCKED to [%s] by first station \"%s\"!\n",
                           getProtocolName(locked_protocol), clients[i].name);

                    for (int k = 0; k < MAX_STATIONS; k++) {
                        if (clients[k].active && clients[k].waiting_for_proto) {
                            uint8_t sync[2] = { MSG_PROTO_SYNC, (uint8_t)locked_protocol };
                            send(clients[k].sock, (const char *)sync, 2, 0);
                            clients[k].waiting_for_proto = 0;
                            printf("[SERVER] Assigned locked protocol [%s] to Station \"%s\"\n",
                                   getProtocolName(locked_protocol), clients[k].name);
                        }
                    }
                }
            }
            else if (opcode == MSG_SENSE_REQ) {
                if (sim_start_time == 0.0) sim_start_time = get_time_ms();
                uint8_t sensed_state = (channel_state == CH_IDLE) ? (uint8_t)CH_IDLE : (uint8_t)CH_BUSY;
                uint8_t resp[2] = { MSG_SENSE_RESP, sensed_state };
                send(clients[i].sock, (const char *)resp, 2, 0);
            }
            else if (opcode == MSG_TX_START) {
                if (sim_start_time == 0.0) sim_start_time = get_time_ms();
                if (recv_exact(clients[i].sock, clients[i].raw_buf, FRAME_SIZE) < 0) continue;
                unpackFrame(clients[i].raw_buf, &clients[i].frame);
                clients[i].transmitting = 1;

                if (channel_state == CH_IDLE) {
                    channel_state = CH_BUSY;
                    state_end_time = now + TX_TIME_MS;
                    printf("[%.1f ms] [CH: IDLE -> BUSY] Station \"%s\" transmitting Seq %u (%dB)\n",
                           now - sim_start_time, clients[i].name,
                           clients[i].frame.sequenceNumber, FRAME_SIZE);
                } else {
                    /* COLLISION OCCURRED: Concurrent transmission on busy medium */
                    ChannelState prev_state = channel_state;
                    if (channel_state == CH_BUSY) {
                        total_collisions++;
                        total_dropped_frames += 2; /* Both overlapping frames are dropped */
                    } else {
                        total_dropped_frames += 1;
                    }
                    channel_state = CH_COLLISION;

                    if (locked_protocol == PROTO_CSMA_CD) {
                        /* CSMA/CD ONLY: Broadcast Jam Signal (0xFE) and abort in JAM_TIME_MS */
                        state_end_time = now + JAM_TIME_MS;
                        printf("[%.1f ms] [CH: %s -> COLLISION!] Station \"%s\" collided! Sending CSMA/CD JAM Signal (Total Collisions: %d)\n",
                               now - sim_start_time, state_str(prev_state), clients[i].name, total_collisions);

                        uint8_t jam[2] = { MSG_COLLISION, (uint8_t)CH_COLLISION };
                        for (int k = 0; k < MAX_STATIONS; k++) {
                            if (clients[k].active && clients[k].transmitting) {
                                send(clients[k].sock, (const char *)jam, 2, 0);
                                clients[k].transmitting = 0;
                            }
                        }
                    } else {
                        /* Non-CD CSMA: NO Jam Signal and NO ACK! Channel stays occupied for full TX_TIME_MS
                         * and collided frames are naturally dropped (never forwarded to Receiver). */
                        if (now + TX_TIME_MS > state_end_time) {
                            state_end_time = now + TX_TIME_MS;
                        }
                        for (int k = 0; k < MAX_STATIONS; k++) {
                            if (clients[k].active && clients[k].transmitting) {
                                clients[k].transmitting = 0;
                            }
                        }
                        printf("[%.1f ms] [CH: %s -> COLLISION!] Station \"%s\" collided! Frames dropped naturally (No Jam/No ACK) (Total Collisions: %d)\n",
                               now - sim_start_time, state_str(prev_state), clients[i].name, total_collisions);
                    }
                }
            }
            else if (opcode == MSG_STATION_DONE) {
                stations_finished++;
                printf("[SERVER] Station \"%s\" completed all transmissions (%d/%d done)\n",
                       clients[i].name, stations_finished, expected_stations);
                CLOSESOCK(clients[i].sock);
                clients[i].active = 0;
            }
        }

        if (stations_connected >= expected_stations && stations_finished >= expected_stations) {
            break;
        }
    }

    /* Wait for any final frame in flight on the medium before closing */
    if (channel_state == CH_BUSY && rx_sock != INVALID_SOCKET) {
        double rem = state_end_time - get_time_ms();
        if (rem > 0) SLEEP_MS((int)rem + 5);
        for (int i = 0; i < MAX_STATIONS; i++) {
            if (clients[i].transmitting) {
                unsigned char fwd_pkt[1 + STATION_NAME_LEN + FRAME_SIZE];
                fwd_pkt[0] = (locked_protocol == PROTO_CSMA_CD) ? MSG_FWD_FRAME_ACK : MSG_FWD_FRAME_NOACK;
                memset(&fwd_pkt[1], 0, STATION_NAME_LEN);
                strncpy((char *)&fwd_pkt[1], clients[i].name, STATION_NAME_LEN - 1);
                memcpy(&fwd_pkt[1 + STATION_NAME_LEN], clients[i].raw_buf, FRAME_SIZE);
                send(rx_sock, (const char *)fwd_pkt, sizeof(fwd_pkt), 0);
                total_frames_forwarded++;
                clients[i].transmitting = 0;
            }
        }
    }

    /* Notify Receiver that all stations have finished */
    if (rx_sock != INVALID_SOCKET) {
        uint8_t done_op = MSG_STATION_DONE;
        send(rx_sock, (const char *)&done_op, 1, 0);
        CLOSESOCK(rx_sock);
    }

    double total_time_s = (get_time_ms() - sim_start_time) / 1000.0;
    if (total_time_s <= 0.0) total_time_s = 0.001;
    double throughput_kbps = (total_frames_forwarded * FRAME_SIZE * 8.0) / (total_time_s * 1000.0);

    printf("\n================ SERVER CHANNEL SUMMARY ================\n");
    printf("Locked Protocol        : %s\n", getProtocolName(locked_protocol));
    printf("Contending Stations    : %d\n", expected_stations);
    printf("Frames Forwarded (65B) : %d\n", total_frames_forwarded);
    printf("Total Collisions       : %d\n", total_collisions);
    printf("Dropped Frames         : %d\n", total_dropped_frames);
    printf("Total Elapsed Time     : %.3f s\n", total_time_s);
    printf("Effective Throughput   : %.2f kbps (%.2f frames/sec)\n",
           throughput_kbps, total_frames_forwarded / total_time_s);
    printf("========================================================\n");

    CLOSESOCK(listen_sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
