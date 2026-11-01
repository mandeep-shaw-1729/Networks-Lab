/* receiver.c - Destination Receiver Node (Sends ACKs ONLY for CSMA/CD) */
#include "csma_common.h"

int main(int argc, char *argv[]) {
    int verbose_frames = 0;

    printf("=================================================================\n");
    printf("   IEEE 802.3 DESTINATION RECEIVER NODE\n");
    printf("=================================================================\n");

    if (argc > 1) {
        verbose_frames = atoi(argv[1]);
    } else {
        printf("Display full 65-byte frame details on receive? (0 = No, 1 = Yes): ");
        fflush(stdout);
        if (scanf("%d", &verbose_frames) != 1) verbose_frames = 0;
    }

#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    sock_t sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server; memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET; server.sin_addr.s_addr = inet_addr(SERVER_IP); server.sin_port = htons(SERVER_PORT);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        perror("Receiver connect failed (Start ./server first!)");
        return 1;
    }

    uint8_t reg_op = MSG_REGISTER_RX;
    send(sock, (const char *)&reg_op, 1, 0);

    printf("\n-----------------------------------------------------------------\n");
    printf(" Receiver Connected to Shared Channel Server (%s:%d)\n", SERVER_IP, SERVER_PORT);
    printf(" Waiting for incoming frames from Sender Stations...\n");
    printf("-----------------------------------------------------------------\n\n");

    int total_received = 0;

    while (1) {
        unsigned char opcode = 0;
        if (recv_exact(sock, &opcode, 1) < 0) break;

        if (opcode == MSG_FWD_FRAME_ACK || opcode == MSG_FWD_FRAME_NOACK) {
            char station_name[STATION_NAME_LEN];
            unsigned char raw_buf[FRAME_SIZE];
            EthernetFrame rxFrame;

            if (recv_exact(sock, (unsigned char *)station_name, STATION_NAME_LEN) < 0) break;
            if (recv_exact(sock, raw_buf, FRAME_SIZE) < 0) break;
            station_name[STATION_NAME_LEN - 1] = '\0';

            unpackFrame(raw_buf, &rxFrame);
            total_received++;

            if (opcode == MSG_FWD_FRAME_ACK) {
                printf("[RECEIVER] Frame #%d Received from Station \"%s\" | Seq: %u | Length: %uB | Trailer(FCS): 0x%08X -> Sending ACK\n",
                       total_received, station_name, rxFrame.sequenceNumber, rxFrame.length, rxFrame.fcs);
                uint8_t ack[2] = { ACK_MARKER, rxFrame.sequenceNumber };
                send(sock, (const char *)ack, 2, 0);
            } else {
                printf("[RECEIVER] Frame #%d Received from Station \"%s\" | Seq: %u | Length: %uB | Trailer(FCS): 0x%08X (No ACK sent)\n",
                       total_received, station_name, rxFrame.sequenceNumber, rxFrame.length, rxFrame.fcs);
            }

            if (verbose_frames) printFrame(&rxFrame);
        }
        else if (opcode == MSG_STATION_DONE) {
            printf("\n[RECEIVER] Server signaled all stations completed transmission.\n");
            break;
        }
    }

    printf("\n================ RECEIVER SUMMARY ================\n");
    printf("Total Clean Frames Received : %d\n", total_received);
    printf("Total Bytes Delivered       : %d Bytes\n", total_received * FRAME_SIZE);
    printf("==================================================\n");

    CLOSESOCK(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
