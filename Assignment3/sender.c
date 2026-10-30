/* sender.c - Station Transmitter Implementing Theoretical CSMA & CSMA/CD */
#include "csma_common.h"

/* Hash station name string/integer into a unique byte for Source MAC & RNG seed */
static uint8_t hashStationName(const char *name) {
    uint32_t h = 5381;
    while (*name) {
        h = ((h << 5) + h) + (unsigned char)(*name++);
    }
    return (uint8_t)((h & 0xFF) ? (h & 0xFF) : 1);
}

/* Construct and pack 65-byte EthernetFrame (Trailer kept at 0x00000000, no CRC/Error) */
static void prepareStationFrame(EthernetFrame *frame, unsigned char packedBuf[FRAME_SIZE],
                                uint8_t macId, uint8_t seqNo,
                                const unsigned char *payloadData, uint16_t payloadLen) {
    uint8_t srcMAC[MAC_SIZE] = { 0x00, 0x1A, 0x2B, 0x3C, 0x4D, macId };
    uint8_t dstMAC[MAC_SIZE] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    initializeFrame(frame);
    setSourceMAC(frame, srcMAC);
    setDestinationMAC(frame, dstMAC);
    frame->sequenceNumber = seqNo;
    copyPayload(frame, payloadData, payloadLen);

    /* Trailer field is retained in the 65-byte structure without error calculation */
    frame->fcs = 0x00000000;

    /* Pack into 65-byte buffer (15B Header + 46B Payload + 4B Trailer) */
    packFrame(frame, packedBuf);
}

/* 1. Carrier Sensing Method */
static ChannelState senseCarrier(sock_t sock) {
    uint8_t req = MSG_SENSE_REQ;
    if (send(sock, (const char *)&req, 1, 0) <= 0) return CH_BUSY;

    uint8_t resp[2] = {0, 0};
    if (recv_exact(sock, resp, 2) == 0 && resp[0] == MSG_SENSE_RESP) {
        return (ChannelState)resp[1];
    }
    return CH_BUSY;
}

/* 2. Backoff Method */
static void performBackoff(int collisionAttempt, CSMAProtocol proto) {
    int slots = 1;
    if (proto == PROTO_NON_PERSISTENT) {
        slots = 1 + (rand() % 8);
    } else {
        int exp = (collisionAttempt < MAX_BACKOFF_EXP) ? collisionAttempt : MAX_BACKOFF_EXP;
        int max_slots = (1 << exp);
        slots = rand() % max_slots;
        if (slots == 0) slots = 1;
    }
    SLEEP_MS(slots * SLOT_TIME_MS);
}

/* 3. Theoretical CSMA & CSMA/CD Transmission Method */
static int transmitFrameCSMA(sock_t sock, const char *stationName,
                             const EthernetFrame *frame, const unsigned char packedBuf[FRAME_SIZE],
                             CSMAProtocol proto, double p_prob, int *collision_count) {
    int attempts = 0;

    while (attempts < MAX_COLLISIONS) {
        /* Step A: Sense Carrier according to theoretical CSMA scheme */
        if (proto == PROTO_NON_PERSISTENT) {
            /* Non-Persistent: If busy, wait a random time and sense again */
            while (senseCarrier(sock) != CH_IDLE) {
                printf("[Station \"%s\"] Channel BUSY -> Non-Persistent random wait...\n", stationName);
                performBackoff(1, PROTO_NON_PERSISTENT);
            }
        }
        else if (proto == PROTO_1_PERSISTENT || proto == PROTO_CSMA_CD) {
            /* 1-Persistent & CSMA/CD: Continuously sense until idle, then transmit immediately */
            while (senseCarrier(sock) != CH_IDLE) {
                SLEEP_MS(2);
            }
        }
        else if (proto == PROTO_P_PERSISTENT) {
            /* p-Persistent: When idle, transmit with prob p, or defer 1 slot with prob (1 - p) */
            while (1) {
                if (senseCarrier(sock) == CH_IDLE) {
                    double r = (double)rand() / (double)RAND_MAX;
                    if (r <= p_prob) {
                        break; /* Transmit in current slot */
                    } else {
                        SLEEP_MS(SLOT_TIME_MS); /* Defer 1 slot and re-sense */
                    }
                } else {
                    /* Channel busy: sense continuously until it becomes idle */
                    SLEEP_MS(2);
                }
            }
        }

        /* Step B: Send 1-byte TX opcode + 65-byte packed EthernetFrame */
        unsigned char pkt[1 + FRAME_SIZE];
        pkt[0] = MSG_TX_START;
        memcpy(&pkt[1], packedBuf, FRAME_SIZE);
        if (send(sock, (const char *)pkt, sizeof(pkt), 0) <= 0) return 0;

        /* Step C: Protocol-Specific Post-Transmission Behavior */
        if (proto == PROTO_CSMA_CD) {
            /* CSMA/CD ONLY: Receives either 2-byte ACK (0xA5) or immediate 2-byte Jam Signal (0xFE) */
            uint8_t reply[2] = {0, 0};
            if (recv_exact(sock, reply, 2) < 0) return 0;

            if (reply[0] == ACK_MARKER && reply[1] == frame->sequenceNumber) {
                return 1; /* Frame delivered to Receiver and ACKed */
            }

            /* Jam Signal (0xFE) received: Immediate abort and Binary Exponential Backoff */
            attempts++;
            (*collision_count)++;
            printf("[Station \"%s\"] CSMA/CD Jam Signal Detected on Seq %u! Aborting immediately (Attempt %d)\n",
                   stationName, frame->sequenceNumber, attempts);
            performBackoff(attempts, proto);
        } else {
            /* Pure CSMA (Non-Persistent, 1-Persistent, p-Persistent):
             * NO Jam Signal and NO Acknowledgement! The station transmits for the full frame duration
             * (TX_TIME_MS) and assumes transmission is complete. If a collision occurred on the server,
             * the server drops the frame naturally. */
            SLEEP_MS(TX_TIME_MS);
            return 1;
        }
    }
    return 0; /* Max collisions reached (CSMA/CD) */
}

int main(int argc, char *argv[]) {
    char stationName[STATION_NAME_LEN];
    memset(stationName, 0, sizeof(stationName));
    int protoChoice = 4;
    double p_prob   = 0.25;

    printf("=================================================================\n");
    printf("   IEEE 802.3 CSMA STATION TRANSMITTER (CLIENT)\n");
    printf("=================================================================\n");

    /* Step 1: Prompt for Station Name */
    if (argc >= 2) {
        strncpy(stationName, argv[1], STATION_NAME_LEN - 1);
    } else {
        printf("Enter Station Name (integer or string, e.g., 1 or Station_A): ");
        fflush(stdout);
        if (scanf("%31s", stationName) != 1) {
            strcpy(stationName, "Station_1");
        }
    }

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    /* Open input.txt using fileio.c */
    FILE *fp = openInputFile("input.txt");
    if (!fp) {
        fprintf(stderr, "[Station \"%s\"] Error: Could not open input.txt\n", stationName);
        return 1;
    }

    /* Step 2: Connect to Shared Channel Server & Register Station Name */
    sock_t sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_port = htons(SERVER_PORT);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        perror("Station connect failed (Is ./server running?)");
        closeInputFile(fp);
        return 1;
    }

    unsigned char regPkt[1 + STATION_NAME_LEN];
    regPkt[0] = MSG_REGISTER;
    memset(&regPkt[1], 0, STATION_NAME_LEN);
    strncpy((char *)&regPkt[1], stationName, STATION_NAME_LEN - 1);
    send(sock, (const char *)regPkt, sizeof(regPkt), 0);

    /* Step 3: Receive Protocol Lock Status from Channel Server */
    uint8_t syncResp[2] = {0, 0};
    if (recv_exact(sock, syncResp, 2) < 0 || syncResp[0] != MSG_PROTO_SYNC) {
        fprintf(stderr, "[Station \"%s\"] Failed to synchronize protocol with server.\n", stationName);
        closeInputFile(fp);
        CLOSESOCK(sock);
        return 1;
    }

    CSMAProtocol lockedProto = (CSMAProtocol)syncResp[1];
    CSMAProtocol proto = PROTO_CSMA_CD;

    if (lockedProto == PROTO_UNSET) {
        if (argc >= 3) {
            protoChoice = atoi(argv[2]);
        } else {
            printf("\n[First Station] Select CSMA Protocol (locks for all stations):\n");
            printf("  1. Non-Persistent CSMA\n");
            printf("  2. 1-Persistent CSMA\n");
            printf("  3. p-Persistent CSMA\n");
            printf("  4. CSMA/CD (with Collision Detection)\n");
            printf("Enter choice (1-4): ");
            fflush(stdout);
            if (scanf("%d", &protoChoice) != 1 || protoChoice < 1 || protoChoice > 4) {
                protoChoice = 4;
            }
        }
        proto = (CSMAProtocol)protoChoice;

        uint8_t setPkt[2] = { MSG_SET_PROTO, (uint8_t)proto };
        send(sock, (const char *)setPkt, 2, 0);
    } else {
        proto = lockedProto;
        printf("\n[Channel Sync] Protocol Locked by First Station: %s\n", getProtocolName(proto));
    }

    /* Step 4: If protocol is p-Persistent, EVERY station sets its own p value */
    if (proto == PROTO_P_PERSISTENT) {
        if (argc >= 4) {
            p_prob = atof(argv[3]);
        } else {
            printf("Enter persistence probability p for Station \"%s\" (0.05 to 1.0, e.g., 0.25): ",
                   stationName);
            fflush(stdout);
            if (scanf("%lf", &p_prob) != 1 || p_prob <= 0.0 || p_prob > 1.0) {
                p_prob = 0.25;
            }
        }
    }

    uint8_t macId = hashStationName(stationName);
    srand((unsigned int)(time(NULL) ^ (macId * 7919) ^ (unsigned int)get_time_ms()));

    printf("\n-----------------------------------------------------------------\n");
    printf(" Station Name : \"%s\" (MAC Suffix: 0x%02X)\n", stationName, macId);
    printf(" Protocol     : %s\n", getProtocolName(proto));
    if (proto == PROTO_P_PERSISTENT) {
        printf(" p Value      : %.2f\n", p_prob);
    }
    printf("-----------------------------------------------------------------\n\n");

    unsigned char payloadBuf[PAYLOAD_SIZE];
    uint16_t payloadLen = 0;
    uint8_t seqNo = 0;
    int total_collisions = 0;
    double total_delay_ms = 0.0;

    while (getNextPayload(fp, payloadBuf, &payloadLen)) {
        EthernetFrame frame;
        unsigned char packedBuf[FRAME_SIZE];

        prepareStationFrame(&frame, packedBuf, macId, seqNo, payloadBuf, payloadLen);

        double t0 = get_time_ms();
        int status = transmitFrameCSMA(sock, stationName, &frame, packedBuf, proto, p_prob, &total_collisions);
        double delay = get_time_ms() - t0;

        if (status == 1) {
            total_delay_ms += delay;
            if (proto == PROTO_CSMA_CD) {
                printf("[Station \"%s\"] Seq %u Delivered & ACKed (65B) | Delay: %.1f ms | Collisions: %d\n",
                       stationName, seqNo, delay, total_collisions);
            } else {
                printf("[Station \"%s\"] Seq %u Transmitted (65B) | Delay: %.1f ms\n",
                       stationName, seqNo, delay);
            }
        } else {
            printf("[Station \"%s\"] Seq %u Aborted after %d collision attempts.\n",
                   stationName, seqNo, MAX_COLLISIONS);
        }

        seqNo++;

        /* Inter-frame yielding pause for all CSMA schemes EXCEPT 1-Persistent CSMA */
        if (proto != PROTO_1_PERSISTENT) {
            SLEEP_MS(SLOT_TIME_MS + (rand() % (2 * SLOT_TIME_MS)));
        }
    }

    closeInputFile(fp);

    uint8_t done = MSG_STATION_DONE;
    send(sock, (const char *)&done, 1, 0);

    if (seqNo > 0) {
        printf("\n================ STATION \"%s\" SUMMARY ================\n", stationName);
        printf("Protocol Used          : %s\n", getProtocolName(proto));
        if (proto == PROTO_P_PERSISTENT) {
            printf("Persistence Prob (p)   : %.2f\n", p_prob);
        }
        printf("Total Frames Sent      : %u\n", seqNo);
        if (proto == PROTO_CSMA_CD) {
            printf("Collisions Detected    : %d\n", total_collisions);
        }
        printf("Average Frame Delay    : %.2f ms\n", total_delay_ms / seqNo);
        printf("======================================================\n");
    }

    CLOSESOCK(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
