/* sender.c - Station Transmitter Implementing Theoretical CSMA & CSMA/CD */
#include "csma_common.h"

static uint8_t hashStationName(const char *name) {
    uint32_t h = 5381; while (*name) h = ((h << 5) + h) + (unsigned char)(*name++); return (uint8_t)((h & 0xFF) ? (h & 0xFF) : 1);
}

static void prepareStationFrame(EthernetFrame *frame, unsigned char packedBuf[FRAME_SIZE], uint8_t macId, uint8_t seqNo, const unsigned char *payloadData, uint16_t payloadLen) {
    uint8_t srcMAC[MAC_SIZE] = { 0x00, 0x1A, 0x2B, 0x3C, 0x4D, macId };
    uint8_t dstMAC[MAC_SIZE] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    initializeFrame(frame); setSourceMAC(frame, srcMAC); setDestinationMAC(frame, dstMAC);
    frame->sequenceNumber = seqNo; copyPayload(frame, payloadData, payloadLen);
    frame->fcs = 0x00000000; packFrame(frame, packedBuf);
}

static ChannelState senseCarrier(sock_t sock) {
    uint8_t req = MSG_SENSE_REQ; send(sock, (const char *)&req, 1, 0);
    uint8_t resp[2] = {0, 0};
    if (recv_exact(sock, resp, 2) == 0 && resp[0] == MSG_SENSE_RESP) return (ChannelState)resp[1];
    return CH_BUSY;
}

static void performBackoff(int collisionAttempt, CSMAProtocol proto) {
    int slots = 1;
    if (proto == PROTO_NON_PERSISTENT) slots = 1 + (rand() % 8);
    else { int exp = (collisionAttempt < MAX_BACKOFF_EXP) ? collisionAttempt : MAX_BACKOFF_EXP; slots = rand() % (1 << exp); if (slots == 0) slots = 1; }
    SLEEP_MS(slots * SLOT_TIME_MS);
}

static int transmitFrameCSMA(sock_t sock, const char *stationName, const EthernetFrame *frame, const unsigned char packedBuf[FRAME_SIZE], CSMAProtocol proto, double p_prob, int *col_count) {
    int attempts = 0;
    while (attempts < MAX_COLLISIONS) {
        if (proto == PROTO_NON_PERSISTENT) { while (senseCarrier(sock) != CH_IDLE) { printf("[Station \"%s\"] Channel BUSY -> Non-Persistent random wait...\n", stationName); performBackoff(1, proto); } }
        else if (proto == PROTO_1_PERSISTENT || proto == PROTO_CSMA_CD) { while (senseCarrier(sock) != CH_IDLE) SLEEP_MS(2); }
        else if (proto == PROTO_P_PERSISTENT) {
            while (1) {
                if (senseCarrier(sock) == CH_IDLE) { if (((double)rand() / RAND_MAX) <= p_prob) break; else SLEEP_MS(SLOT_TIME_MS); }
                else SLEEP_MS(2);
            }
        }

        unsigned char pkt[1 + FRAME_SIZE]; pkt[0] = MSG_TX_START; memcpy(&pkt[1], packedBuf, FRAME_SIZE);
        send(sock, (const char *)pkt, sizeof(pkt), 0);

        if (proto == PROTO_CSMA_CD) {
            uint8_t reply[2] = {0, 0};
            if (recv_exact(sock, reply, 2) < 0) return 0;
            if (reply[0] == ACK_MARKER && reply[1] == frame->sequenceNumber) return 1;
            attempts++; (*col_count)++;
            printf("[Station \"%s\"] CSMA/CD Jam Signal Detected on Seq %u! Aborting immediately (Attempt %d)\n", stationName, frame->sequenceNumber, attempts);
            performBackoff(attempts, proto);
        } else {
            SLEEP_MS(TX_TIME_MS); /* Pure CSMA waits 80 ms, assumes success, no ACK */
            return 1; 
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    char stationName[STATION_NAME_LEN]; memset(stationName, 0, sizeof(stationName));
    int protoChoice = 4; double p_prob = 0.25;

    printf("=================================================================\n");
    printf("   IEEE 802.3 CSMA STATION TRANSMITTER (CLIENT)\n");
    printf("=================================================================\n");

    if (argc >= 2) strncpy(stationName, argv[1], STATION_NAME_LEN - 1);
    else { printf("Enter Station Name (e.g., 1 or Station_A): "); fflush(stdout); if (scanf("%31s", stationName) != 1) strcpy(stationName, "Station_1"); }

#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    FILE *fp = openInputFile("input.txt");
    if (!fp) { fprintf(stderr, "[Station \"%s\"] Error: Could not open input.txt\n", stationName); return 1; }

    sock_t sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server; memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET; server.sin_addr.s_addr = inet_addr(SERVER_IP); server.sin_port = htons(SERVER_PORT);
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) { perror("Station connect failed"); return 1; }

    unsigned char regPkt[1 + STATION_NAME_LEN]; regPkt[0] = MSG_REGISTER;
    memset(&regPkt[1], 0, STATION_NAME_LEN); strncpy((char *)&regPkt[1], stationName, STATION_NAME_LEN - 1);
    send(sock, (const char *)regPkt, sizeof(regPkt), 0);

    uint8_t syncResp[3] = {0, 0, 0};
    if (recv_exact(sock, syncResp, 3) < 0 || syncResp[0] != MSG_PROTO_SYNC) return 1;
    
    CSMAProtocol lockedProto = (CSMAProtocol)syncResp[1];
    uint8_t syncFlag         = syncResp[2];
    CSMAProtocol proto       = PROTO_CSMA_CD;

    if (syncFlag == 0xFF) {
        int is_synced = 1;
        printf("\n[First Station] Do you want to Lock the protocol for all stations?\n  1. Yes (Locked for all)\n  0. No (Each station chooses individually)\nEnter choice (1 or 0): ");
        fflush(stdout); if (scanf("%d", &is_synced) != 1) is_synced = 1;
        
        uint8_t modePkt[2] = { MSG_SET_MODE, (uint8_t)is_synced };
        send(sock, (const char *)modePkt, 2, 0);

        printf("\n[First Station] Select CSMA Protocol:\n  1. Non-Persistent\n  2. 1-Persistent\n  3. p-Persistent\n  4. CSMA/CD\nEnter choice (1-4): ");
        fflush(stdout); if (scanf("%d", &protoChoice) != 1) protoChoice = 4;
        proto = (CSMAProtocol)protoChoice;
        
        uint8_t protoPkt[2] = { MSG_SET_PROTO, (uint8_t)proto };
        send(sock, (const char *)protoPkt, 2, 0);
    } else if (syncFlag == 1) {
        proto = lockedProto;
        printf("\n[Channel Sync] Mode [LOCKED] by First Station: %s\n", getProtocolName(proto));
    } else if (syncFlag == 0) {
        printf("\n[Channel Sync] Mode [UNLOCKED] - Choose your protocol.\n  1. Non-Persistent\n  2. 1-Persistent\n  3. p-Persistent\n  4. CSMA/CD\nEnter choice (1-4): ");
        fflush(stdout); if (scanf("%d", &protoChoice) != 1) protoChoice = 4;
        proto = (CSMAProtocol)protoChoice;
        
        uint8_t protoPkt[2] = { MSG_SET_PROTO, (uint8_t)proto };
        send(sock, (const char *)protoPkt, 2, 0);
    }

    if (proto == PROTO_P_PERSISTENT) {
        if (argc >= 4) p_prob = atof(argv[3]);
        else { printf("Enter persistence probability p (0.05 to 1.0): "); fflush(stdout); if (scanf("%lf", &p_prob) != 1) p_prob = 0.25; }
    }

    uint8_t macId = hashStationName(stationName);
    srand((unsigned int)(time(NULL) ^ (macId * 7919) ^ (unsigned int)get_time_ms()));

    printf("\n-----------------------------------------------------------------\n");
    printf(" Station Name : \"%s\" (MAC Suffix: 0x%02X)\n Protocol     : %s\n", stationName, macId, getProtocolName(proto));
    printf("-----------------------------------------------------------------\n\n");

    unsigned char payloadBuf[PAYLOAD_SIZE]; uint16_t payloadLen = 0; uint8_t seqNo = 0;
    int total_collisions = 0; double total_delay_ms = 0.0;

    while (getNextPayload(fp, payloadBuf, &payloadLen)) {
        EthernetFrame frame; unsigned char packedBuf[FRAME_SIZE];
        prepareStationFrame(&frame, packedBuf, macId, seqNo, payloadBuf, payloadLen);

        double t0 = get_time_ms();
        int status = transmitFrameCSMA(sock, stationName, &frame, packedBuf, proto, p_prob, &total_collisions);
        double delay = get_time_ms() - t0;

        if (status == 1) {
            total_delay_ms += delay;
            if (proto == PROTO_CSMA_CD) {
                printf("[Station \"%s\"] Seq %u Delivered & ACKed (65B) | Delay: %.1f ms | Collisions: %d\n", stationName, seqNo, delay, total_collisions);
            } else {
                printf("[Station \"%s\"] Seq %u Transmitted (65B, No ACK) | Delay: %.1f ms\n", stationName, seqNo, delay);
            }
        } else {
            printf("[Station \"%s\"] Seq %u Aborted after %d collision attempts.\n", stationName, seqNo, MAX_COLLISIONS);
        }

        seqNo++;
        if (proto != PROTO_1_PERSISTENT) SLEEP_MS(SLOT_TIME_MS + (rand() % (2 * SLOT_TIME_MS)));
    }

    closeInputFile(fp);
    uint8_t done = MSG_STATION_DONE; send(sock, (const char *)&done, 1, 0);

    if (seqNo > 0) {
        printf("\n================ STATION \"%s\" SUMMARY ================\n", stationName);
        printf("Protocol Used          : %s\n", getProtocolName(proto));
        printf("Total Frames Sent      : %u\n", seqNo);
        if (proto == PROTO_CSMA_CD) printf("Collisions Detected    : %d\n", total_collisions);
        printf("Average Frame Delay    : %.2f ms\n", total_delay_ms / seqNo);
        printf("======================================================\n");
    }

    CLOSESOCK(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
