/* receiver.c - CDMA Multi-Station Receiver & Walsh Correlator Decoder */
#include <ctype.h>
#include "csma_common.h"

/* Reconstruct a station's 65-byte frame from the composite channel signal
 * using inner-product correlation with the station's Walsh code W_i:
 *   corr = (1 / N) * sum_{c=0}^{N-1} (composite[bit*N + c] * W_i[c])
 * Returns:
 *   1 -> Station was active; packedBuf[FRAME_SIZE] reconstructed
 *   0 -> Station was silent (correlation == 0)
 */
static int decodeStationCDMA(const int16_t compositeChips[],
                             const int walshCode[],
                             int walshN,
                             unsigned char packedBuf[FRAME_SIZE]) {
    memset(packedBuf, 0, FRAME_SIZE);
    int chipIdx = 0;

    for (int byteIdx = 0; byteIdx < FRAME_SIZE; byteIdx++) {
        unsigned char decodedByte = 0;
        for (int bitIdx = 7; bitIdx >= 0; bitIdx--) {
            int sum = 0;
            for (int c = 0; c < walshN; c++) {
                sum += (int)compositeChips[chipIdx++] * walshCode[c];
            }
            int bipolar = sum / walshN;

            /* If correlation is 0 on the very first bit, this station did not transmit in this slot */
            if (byteIdx == 0 && bitIdx == 7 && bipolar == 0) {
                return 0;
            }

            if (bipolar > 0) {
                decodedByte |= (1 << bitIdx);
            }
        }
        packedBuf[byteIdx] = decodedByte;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    int verbose_frames = 0;

    printf("=================================================================\n");
    printf("   CDMA MULTI-STATION RECEIVER (WALSH CORRELATOR DECODER)\n");
    printf("=================================================================\n");

    if (argc > 1) {
        verbose_frames = atoi(argv[1]);
    } else {
        printf("Display full 65-byte frame details on receive? (0 = No, 1 = Yes): ");
        fflush(stdout);
        if (scanf("%d", &verbose_frames) != 1) verbose_frames = 0;
    }

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

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
        perror("Receiver connect failed (Start ./server first!)");
        return 1;
    }

    uint8_t reg_op = MSG_REGISTER_RX;
    send_exact(sock, &reg_op, 1);

    printf("\n-----------------------------------------------------------------\n");
    printf(" Receiver Connected to Shared CDMA Server (%s:%d)\n", SERVER_IP, SERVER_PORT);
    printf(" Waiting for all Sender Stations to register...\n");
    printf("-----------------------------------------------------------------\n\n");

    /* Receive Walsh setup and station list from Server */
    uint8_t initHdr[3] = {0};
    if (recv_exact(sock, initHdr, 3) < 0 || initHdr[0] != MSG_RX_INIT) {
        fprintf(stderr, "[RECEIVER] Failed to receive CDMA initialization from server.\n");
        CLOSESOCK(sock);
        return 1;
    }

    int numStations = initHdr[1];
    int walshN      = initHdr[2];
    int totalChips  = BITS_PER_FRAME * walshN;
    size_t chipBytes = (size_t)totalChips * sizeof(int16_t);

    char stationNames[MAX_STATIONS][STATION_NAME_LEN];
    for (int i = 0; i < numStations; i++) {
        recv_exact(sock, (unsigned char *)stationNames[i], STATION_NAME_LEN);
        stationNames[i][STATION_NAME_LEN - 1] = '\0';
    }
    for (int i = numStations; i < walshN; i++) {
        snprintf(stationNames[i], STATION_NAME_LEN, "Unused_W_%d", i);
    }

    /* Open output payload files for active stations */
    FILE *outFiles[MAX_STATIONS] = {NULL};
    for (int i = 0; i < numStations; i++) {
        char fname[128];
        char lowerName[64];
        strncpy(lowerName, stationNames[i], sizeof(lowerName) - 1);
        lowerName[sizeof(lowerName) - 1] = '\0';
        for (int k = 0; lowerName[k]; k++) {
            lowerName[k] = (char)tolower((unsigned char)lowerName[k]);
        }
        snprintf(fname, sizeof(fname), "output_%s.txt", lowerName);
        outFiles[i] = fopen(fname, "wb");
    }

    int walshTable[MAX_WALSH_SIZE][MAX_WALSH_SIZE];
    generateWalshTable(walshN, walshTable);

    printf("[RECEIVER] Initialized %dx%d Walsh-Hadamard Decoder for %d Stations (N = %d):\n",
           walshN, walshN, numStations, walshN);
    for (int i = 0; i < numStations; i++) {
        printf("  -> Station #%d (\"%s\") Correlator W_%d = ", i, stationNames[i], i);
        printWalshRow(walshTable[i], walshN);
        printf("\n");
    }
    for (int i = numStations; i < walshN; i++) {
        printf("  -> Channel #%d (Unused Matrix Row) Correlator W_%d = ", i, i);
        printWalshRow(walshTable[i], walshN);
        printf(" [Zero-Padded / Silent Channel]\n");
    }
    printf("\n");

    unsigned char *rawChipBuf = (unsigned char *)malloc(chipBytes);
    int16_t *compositeChips   = (int16_t *)malloc(chipBytes);
    int stationFrameCount[MAX_STATIONS] = {0};
    int total_reconstructed = 0;

    while (1) {
        unsigned char opcode = 0;
        if (recv_exact(sock, &opcode, 1) < 0) break;

        if (opcode == MSG_COMPOSITE_SIG) {
            uint32_t netSlot = 0;
            if (recv_exact(sock, (unsigned char *)&netSlot, 4) < 0) break;
            if (recv_exact(sock, rawChipBuf, chipBytes) < 0) break;
            uint32_t slotNo = ntohl(netSlot);

            for (int c = 0; c < totalChips; c++) {
                uint16_t netVal;
                memcpy(&netVal, &rawChipBuf[c * 2], 2);
                compositeChips[c] = (int16_t)ntohs(netVal);
            }

            printf("--- [CDMA Slot #%u] Composite Signal Received (%d chips) ---\n", slotNo, totalChips);

            /* Decode each channel's signal using its orthogonal Walsh code row */
            for (int i = 0; i < walshN; i++) {
                unsigned char rawFrameBuf[FRAME_SIZE];
                if (decodeStationCDMA(compositeChips, walshTable[i], walshN, rawFrameBuf)) {
                    EthernetFrame rxFrame;
                    unpackFrame(rawFrameBuf, &rxFrame);
                    total_reconstructed++;
                    stationFrameCount[i]++;

                    /* Save payload to station output file */
                    if (i < numStations && outFiles[i] && rxFrame.length > 0) {
                        fwrite(rxFrame.payload, 1, rxFrame.length, outFiles[i]);
                    }

                    /* Extract text preview of payload */
                    char preview[25];
                    int pLen = (rxFrame.length < 24) ? rxFrame.length : 24;
                    for (int k = 0; k < pLen; k++) {
                        char ch = (char)rxFrame.payload[k];
                        preview[k] = (ch >= 32 && ch <= 126) ? ch : '.';
                    }
                    preview[pLen] = '\0';

                    printf("  [Station #%d \"%s\"] Reconstructed Frame #%d | Seq: %u | Len: %uB | Data: \"%s%s\"\n",
                           i, stationNames[i], stationFrameCount[i],
                           rxFrame.sequenceNumber, rxFrame.length,
                           preview, (rxFrame.length > 24) ? "..." : "");

                    if (verbose_frames) {
                        printFrame(&rxFrame);
                    }
                } else {
                    if (i < numStations) {
                        printf("  [Station #%d] Inner Product = 0 (Station Idle in Slot #%u)\n",
                               i, slotNo);
                    } else {
                        printf("  [Channel #%d W_%d (Unused)] Inner Product = 0 (Channel Idle in Slot #%u)\n",
                               i, i, slotNo);
                    }
                }
            }

            /* Send Slot ACK back to Server */
            uint8_t ack[2] = { MSG_TX_ACK, (uint8_t)(slotNo & 0xFF) };
            send_exact(sock, ack, 2);
        } else if (opcode == MSG_STATION_DONE) {
            printf("\n[RECEIVER] Server signaled all stations completed CDMA transmission.\n");
            break;
        }
    }

    /* Close all output payload files */
    for (int i = 0; i < numStations; i++) {
        if (outFiles[i]) {
            fclose(outFiles[i]);
            outFiles[i] = NULL;
            char lowerName[64];
            strncpy(lowerName, stationNames[i], sizeof(lowerName) - 1);
            lowerName[sizeof(lowerName) - 1] = '\0';
            for (int k = 0; lowerName[k]; k++) {
                lowerName[k] = (char)tolower((unsigned char)lowerName[k]);
            }
            printf("[RECEIVER] Saved reconstructed payload to output_%s.txt\n", lowerName);
        }
    }

    printf("\n================ CDMA RECEIVER SUMMARY ================\n");
    for (int i = 0; i < numStations; i++) {
        char label[64];
        snprintf(label, sizeof(label), "Station #%d (\"%s\")", i, stationNames[i]);
        printf("%-26s : %d Frames Perfectly Reconstructed\n", label, stationFrameCount[i]);
    }
    for (int i = numStations; i < walshN; i++) {
        char label[64];
        snprintf(label, sizeof(label), "Channel #%d (Unused W_%d)", i, i);
        printf("%-26s : 0 Frames (Zero-Padded / 100%% Silent)\n", label);
    }
    printf("-------------------------------------------------------\n");
    printf("%-26s : %d\n", "Total Frames Reconstructed", total_reconstructed);
    printf("%-26s : %d Bytes\n", "Total Bytes Delivered", total_reconstructed * FRAME_SIZE);
    printf("=======================================================\n");

    free(rawChipBuf);
    free(compositeChips);
    CLOSESOCK(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
