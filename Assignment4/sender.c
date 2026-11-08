/* sender.c - CDMA Station Transmitter Using Orthogonal Walsh Codes */
#include <ctype.h>
#include "csma_common.h"

#ifdef _WIN32
    #include <io.h>
#else
    #include <unistd.h>
#endif
static uint8_t hashStationName(const char *name) {
    uint32_t h = 5381;
    while (*name) h = ((h << 5) + h) + (unsigned char)(*name++);
    return (uint8_t)((h & 0xFF) ? (h & 0xFF) : 1);
}

static void prepareStationFrame(EthernetFrame *frame,
                                unsigned char packedBuf[FRAME_SIZE],
                                uint8_t macId,
                                uint8_t seqNo,
                                const unsigned char *payloadData,
                                uint16_t payloadLen) {
    uint8_t srcMAC[MAC_SIZE] = { 0x00, 0x1A, 0x2B, 0x3C, 0x4D, macId };
    uint8_t dstMAC[MAC_SIZE] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    initializeFrame(frame);
    setSourceMAC(frame, srcMAC);
    setDestinationMAC(frame, dstMAC);
    frame->sequenceNumber = seqNo;
    copyPayload(frame, payloadData, payloadLen);
    frame->fcs = 0x00000000;
    packFrame(frame, packedBuf);
}

/* Spread 65-byte EthernetFrame (520 bits) using station's N-chip Walsh codeword:
 * Bit 0 -> -1, Bit 1 -> +1
 * Each bit is multiplied by walshCode[0..N-1]
 */
static void encodeFrameCDMA(const unsigned char packedBuf[FRAME_SIZE],
                            const int walshCode[],
                            int walshN,
                            int16_t chipsOut[]) {
    int chipIdx = 0;
    for (int byteIdx = 0; byteIdx < FRAME_SIZE; byteIdx++) {
        unsigned char b = packedBuf[byteIdx];
        for (int bitIdx = 7; bitIdx >= 0; bitIdx--) {
            int bitVal = (b >> bitIdx) & 1;
            int bipolar = (bitVal == 1) ? 1 : -1;
            for (int c = 0; c < walshN; c++) {
                chipsOut[chipIdx++] = (int16_t)(bipolar * walshCode[c]);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    char stationName[STATION_NAME_LEN];
    char inputFileName[128] = "input.txt";
    memset(stationName, 0, sizeof(stationName));

    printf("=================================================================\n");
    printf("   CDMA STATION TRANSMITTER WITH WALSH CODE ENCODING\n");
    printf("=================================================================\n");

    if (argc >= 2) {
        if (isdigit((unsigned char)argv[1][0])) {
            snprintf(stationName, sizeof(stationName), "Station_%s", argv[1]);
        } else {
            strncpy(stationName, argv[1], STATION_NAME_LEN - 1);
        }
    } else {
        printf("Enter Station Name (e.g., Station_1): ");
        fflush(stdout);
        if (scanf("%31s", stationName) != 1) strcpy(stationName, "Station_1");
    }

    if (argc >= 3) {
        strncpy(inputFileName, argv[2], sizeof(inputFileName) - 1);
    } else {
        char autoInput[128];
        char lowerName[64];
        strncpy(lowerName, stationName, sizeof(lowerName) - 1);
        lowerName[sizeof(lowerName) - 1] = '\0';
        for (int k = 0; lowerName[k]; k++) lowerName[k] = (char)tolower((unsigned char)lowerName[k]);
        snprintf(autoInput, sizeof(autoInput), "input_%s.txt", lowerName);
        if (access(autoInput, F_OK) == 0) {
            strncpy(inputFileName, autoInput, sizeof(inputFileName) - 1);
        }
    }

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    FILE *fp = openInputFile(inputFileName);
    if (!fp) {
        fprintf(stderr, "[Station \"%s\"] Error: Could not open %s\n", stationName, inputFileName);
        return 1;
    }

    int port = get_server_port();
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_port = htons(port);

    sock_t sock = INVALID_SOCKET;
    int conn_ok = 0;
    for (int retry = 0; retry < 25; retry++) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) != SOCKET_ERROR) {
            conn_ok = 1;
            break;
        }
        CLOSESOCK(sock);
        sock = INVALID_SOCKET;
        SLEEP_MS(100);
    }
    if (!conn_ok) {
        perror("Station connect failed (Start ./server first!)");
        closeInputFile(fp);
        return 1;
    }

    /* Register Station with Server */
    unsigned char regPkt[1 + STATION_NAME_LEN];
    regPkt[0] = MSG_REGISTER;
    memset(&regPkt[1], 0, STATION_NAME_LEN);
    strncpy((char *)&regPkt[1], stationName, STATION_NAME_LEN - 1);
    send_exact(sock, regPkt, sizeof(regPkt));

    printf("[Station \"%s\"] Connected to Server. Waiting for all stations & Walsh code assignment...\n",
           stationName);

    /* Receive Walsh assignment: [MSG_ASSIGN_WALSH, station_id, total_stations, walsh_N] */
    uint8_t assignResp[4] = {0};
    if (recv_exact(sock, assignResp, 4) < 0 || assignResp[0] != MSG_ASSIGN_WALSH) {
        fprintf(stderr, "[Station \"%s\"] Failed to receive Walsh code assignment from server.\n", stationName);
        CLOSESOCK(sock);
        closeInputFile(fp);
        return 1;
    }

    int stationId   = assignResp[1];
    int numStations = assignResp[2];
    int walshN      = assignResp[3];

    int walshTable[MAX_WALSH_SIZE][MAX_WALSH_SIZE];
    generateWalshTable(walshN, walshTable);

    uint8_t macId = hashStationName(stationName);

    printf("\n-----------------------------------------------------------------\n");
    printf(" Station Name   : \"%s\" (MAC Suffix: 0x%02X)\n", stationName, macId);
    printf(" Assigned Index : Station #%d (of %d stations)\n", stationId, numStations);
    printf(" Walsh Length N : %d chips/bit\n", walshN);
    printf(" Walsh Code W_%d : ", stationId);
    printWalshRow(walshTable[stationId], walshN);
    printf("\n-----------------------------------------------------------------\n\n");

    unsigned char payloadBuf[PAYLOAD_SIZE];
    uint16_t payloadLen = 0;
    uint8_t seqNo = 0;
    double total_delay_ms = 0.0;

    int totalChips = BITS_PER_FRAME * walshN;
    size_t chipBytes = (size_t)totalChips * sizeof(int16_t);
    unsigned char *txPkt = (unsigned char *)malloc(2 + chipBytes);
    int16_t *encodedChips = (int16_t *)malloc(chipBytes);

    while (getNextPayload(fp, payloadBuf, &payloadLen)) {
        EthernetFrame frame;
        unsigned char packedBuf[FRAME_SIZE];
        prepareStationFrame(&frame, packedBuf, macId, seqNo, payloadBuf, payloadLen);

        double t0 = get_time_ms();

        /* Spread frame bits with assigned Walsh codeword */
        encodeFrameCDMA(packedBuf, walshTable[stationId], walshN, encodedChips);

        txPkt[0] = MSG_CDMA_FRAME;
        txPkt[1] = seqNo;
        /* Convert chips to network byte order (big-endian int16_t) */
        for (int c = 0; c < totalChips; c++) {
            uint16_t netChip = htons((uint16_t)encodedChips[c]);
            memcpy(&txPkt[2 + c * 2], &netChip, 2);
        }

        if (send_exact(sock, txPkt, 2 + chipBytes) < 0) {
            fprintf(stderr, "[Station \"%s\"] Connection lost during frame %u.\n", stationName, seqNo);
            break;
        }

        /* Wait for synchronous CDMA slot completion ACK from Server/Receiver */
        uint8_t ack[2] = {0, 0};
        if (recv_exact(sock, ack, 2) < 0 || ack[0] != MSG_TX_ACK) {
            fprintf(stderr, "[Station \"%s\"] Failed to receive slot ACK for Seq %u.\n", stationName, seqNo);
            break;
        }

        double delay = get_time_ms() - t0;
        total_delay_ms += delay;

        /* Show sample encoding of first data bit in payload for verification */
        int firstPayloadBit = (frame.payload[0] >> 7) & 1;
        int firstBipolar = firstPayloadBit ? 1 : -1;
        printf("[Station \"%s\"] Seq %u Sent & Reconstructed | %d bits -> %d chips | 1st Payload Bit (%d -> %+d) * W_%d | RTT: %.2f ms\n",
               stationName, seqNo, BITS_PER_FRAME, totalChips, firstPayloadBit, firstBipolar, stationId, delay);

        seqNo++;
    }

    free(txPkt);
    free(encodedChips);
    closeInputFile(fp);

    /* Notify server that this station has finished all frames */
    uint8_t done = MSG_STATION_DONE;
    send_exact(sock, &done, 1);

    printf("\n================ STATION \"%s\" CDMA SUMMARY ================\n", stationName);
    printf("Assigned Walsh Code W_%d : ", stationId);
    printWalshRow(walshTable[stationId], walshN);
    printf("\nTotal Frames Encoded    : %u (%d Bytes each)\n", seqNo, FRAME_SIZE);
    printf("Spreading Factor (N)    : %d chips/bit (%d chips/frame)\n", walshN, totalChips);
    printf("Collisions              : 0 (Orthogonal Walsh CDMA Access)\n");
    if (seqNo > 0) {
        printf("Average Slot Time       : %.2f ms\n", total_delay_ms / seqNo);
    }
    printf("============================================================\n");

    CLOSESOCK(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
