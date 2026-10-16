
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define CLOSE_SOCKET closesocket
typedef SOCKET SocketType;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#define CLOSE_SOCKET close
typedef int SocketType;
#endif

#include "frame.h"
#include "crc.h"
#include "checksum.h"
#include "error.h"
#include "fileio.h"

#define SERVER_PORT 5000
#define SERVER_IP "127.0.0.1"

#define SCHEME_CHECKSUM 1
#define SCHEME_CRC8     2
#define SCHEME_CRC10    3
#define SCHEME_CRC16    4
#define SCHEME_CRC32    5

#define ACK_SIZE 2
#define ACK_MARKER 0xA5
#define ACK_TIMEOUT_MS 1500
#define MAX_RETRIES 5

/* Send all requested bytes over TCP. */
static int sendAll(SocketType sock,
                   const unsigned char *buffer,
                   int length)
{
    int total = 0;

    while (total < length)
    {
        int n = send(sock,
                     (const char *)buffer + total,
                     length - total, 0);

        if (n <= 0)
            return 0;

        total += n;
    }

    return 1;
}

/*
 * Receive a complete ACK.
 * Preserve a partially received ACK if a timeout occurs.
 */
static int receiveACK(SocketType sock, unsigned char ack[ACK_SIZE])
{
    static unsigned char partial[ACK_SIZE];
    static int have = 0;

    while (have < ACK_SIZE)
    {
        int n = recv(sock,
                     (char *)partial + have,
                     ACK_SIZE - have, 0);

        if (n <= 0)
            return 0;

        have += n;
    }

    memcpy(ack, partial, ACK_SIZE);
    have = 0;

    return 1;
}

static CRCType getCRCType(int scheme)
{
    switch (scheme)
    {
        case SCHEME_CRC8:
            return CRC_8;

        case SCHEME_CRC10:
            return CRC_10;

        case SCHEME_CRC16:
            return CRC_16;

        default:
            return CRC_32;
    }
}

static const char *schemeName(int scheme)
{
    switch (scheme)
    {
        case SCHEME_CHECKSUM:
            return "CHECKSUM";

        case SCHEME_CRC8:
            return "CRC-8";

        case SCHEME_CRC10:
            return "CRC-10";

        case SCHEME_CRC16:
            return "CRC-16";

        case SCHEME_CRC32:
            return "CRC-32";

        default:
            return "UNKNOWN";
    }
}

static const char *errorName(int choice)
{
    switch (choice)
    {
        case 1:
            return "Single Bit";

        case 2:
            return "Double Bit";

        case 3:
            return "Odd Bit";

        case 4:
            return "Burst";

        case 5:
            return "Random";

        default:
            return "NONE";
    }
}

static void injectSelectedError(EthernetFrame *frame, int choice)
{
    switch (choice)
    {
        case 1:
            injectSingleBitError(frame);
            break;

        case 2:
            injectDoubleBitError(frame);
            break;

        case 3:
            injectOddBitError(frame, 5);
            break;

        case 4:
            injectBurstError(frame, 8);
            break;

        case 5:
            injectRandomError(frame);
            break;
    }
}

/*
 * Calculate the error-detection value and store it in FCS.
 *
 * CHECKSUM: 16-bit checksum stored in the lower 16 bits.
 * CRC schemes: generateFrameCRC() fills the FCS field.
 */
static void calculateFrameCheckValue(EthernetFrame *frame, int scheme)
{
    if (scheme == SCHEME_CHECKSUM)
    {
        uint16_t checksum = calculateFrameChecksum(frame);
        frame->fcs = (uint32_t)checksum;
    }
    else
    {
        generateFrameCRC(frame, getCRCType(scheme));
    }
}

int main(void)
{
#ifdef _WIN32
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup failed.\n");
        return 1;
    }
#endif

    SocketType sock;
    struct sockaddr_in serverAddress;
    FILE *fp = NULL;

    char filename[256];

    unsigned char payload[PAYLOAD_SIZE];
    unsigned char buffer[FRAME_SIZE];
    unsigned char frameInfo[3];
    unsigned char ack[ACK_SIZE];

    uint16_t length;
    uint8_t sequenceNumber = 0;

    EthernetFrame frame;

    int modeChoice;
    int manualScheme = 0;
    int manualInjectError = 0;
    int manualErrorChoice = 0;

    int choice;
    int injectError;
    int errorChoice;

    srand((unsigned int)time(NULL));

    printf("============================================\n");
    printf("          Ethernet Frame Sender\n");
    printf("           Stop-and-Wait ARQ\n");
    printf("============================================\n");

    printf("\nSelect Frame Generation Mode:\n");
    printf("1. Randomize\n");
    printf("2. Manual Choose\n");
    printf("Enter choice: ");

    if (scanf("%d", &modeChoice) != 1 ||
        (modeChoice != 1 && modeChoice != 2))
    {
        printf("Invalid mode.\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (modeChoice == 2)
    {
        printf("\nSelect Error Detection Scheme:\n");
        printf("1. Checksum\n");
        printf("2. CRC-8\n");
        printf("3. CRC-10\n");
        printf("4. CRC-16\n");
        printf("5. CRC-32\n");
        printf("Enter choice: ");

        if (scanf("%d", &manualScheme) != 1 ||
            manualScheme < 1 || manualScheme > 5)
        {
            printf("Invalid scheme.\n");
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }

        printf("\nEnable channel error simulation?\n");
        printf("1. Yes\n");
        printf("2. No\n");
        printf("Enter choice: ");

        if (scanf("%d", &manualInjectError) != 1 ||
            (manualInjectError != 1 && manualInjectError != 2))
        {
            printf("Invalid error-injection choice.\n");
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }

        if (manualInjectError == 1)
        {
            printf("\nSelect Error Type:\n");
            printf("1. Single Bit\n");
            printf("2. Double Bit\n");
            printf("3. Odd Bit\n");
            printf("4. Burst\n");
            printf("5. Random\n");
            printf("Enter choice: ");

            if (scanf("%d", &manualErrorChoice) != 1 ||
                manualErrorChoice < 1 || manualErrorChoice > 5)
            {
                printf("Invalid error type.\n");
#ifdef _WIN32
                WSACleanup();
#endif
                return 1;
            }
        }
    }

    printf("\nMode: %s\n",
           modeChoice == 1 ? "RANDOMIZE" : "MANUAL CHOOSE");

    printf("Enter input file name: ");

    if (scanf("%255s", filename) != 1)
    {
        printf("Invalid filename.\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    fp = openInputFile(filename);

    if (fp == NULL)
    {
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);

#ifdef _WIN32
    if (sock == INVALID_SOCKET)
#else
    if (sock < 0)
#endif
    {
        printf("Socket creation failed.\n");
        closeInputFile(fp);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    memset(&serverAddress, 0, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);
    serverAddress.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (serverAddress.sin_addr.s_addr == INADDR_NONE)
    {
        printf("Invalid server IP address.\n");
        CLOSE_SOCKET(sock);
        closeInputFile(fp);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    printf("\nConnecting to %s:%d...\n", SERVER_IP, SERVER_PORT);

    if (connect(sock,
                (struct sockaddr *)&serverAddress,
                sizeof(serverAddress)) < 0)
    {
        printf("Connection failed. Start receiver.c first.\n");
        CLOSE_SOCKET(sock);
        closeInputFile(fp);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    /* Configure ACK receive timeout. */
#ifdef _WIN32
    {
        DWORD timeout = ACK_TIMEOUT_MS;

        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   (const char *)&timeout, sizeof(timeout));
    }
#else
    {
        struct timeval timeout;

        timeout.tv_sec = ACK_TIMEOUT_MS / 1000;
        timeout.tv_usec = (ACK_TIMEOUT_MS % 1000) * 1000;

        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout));
    }
#endif

    printf("Connected. ACK timeout: %d ms\n", ACK_TIMEOUT_MS);

    while (getNextPayload(fp, payload, &length))
    {
        int acknowledged = 0;
        int retries = 0;

        unsigned char sourceMAC[MAC_SIZE] =
            {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

        unsigned char destinationMAC[MAC_SIZE] =
            {0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB};

        initializeFrame(&frame);
        frame.sequenceNumber = sequenceNumber;

        /*
         * Select one scheme and error policy per logical frame.
         */
        if (modeChoice == 1)
        {
            choice = (rand() % 5) + 1;

            injectError = rand() % 2;
            errorChoice = injectError ? (rand() % 5) + 1 : 0;
        }
        else
        {
            choice = manualScheme;

            injectError = (manualInjectError == 1);
            errorChoice = injectError ? manualErrorChoice : 0;
        }

        setSourceMAC(&frame, sourceMAC);
        setDestinationMAC(&frame, destinationMAC);
        copyPayload(&frame, payload, length);

        /*
         * Calculate FCS once for the original, uncorrupted frame.
         * Retransmissions reuse this original frame and FCS.
         */
        calculateFrameCheckValue(&frame, choice);

        printf("\n============================================\n");
        printf("FRAME SEQUENCE NUMBER: %u\n",
               (unsigned)sequenceNumber);
        printf("Error Detection: %s\n", schemeName(choice));
        printf("Channel Error Simulation: %s\n",
               injectError ? "ENABLED" : "DISABLED");

        if (injectError)
            printf("Configured Error Type: %s\n",
                   errorName(errorChoice));

        printf("\nORIGINAL FRAME (BEFORE CHANNEL)\n");
        printFrame(&frame);

        /*
         * Stop-and-Wait ARQ:
         * retransmit the original frame after timeout.
         * Channel corruption is independently simulated per attempt.
         */
        while (!acknowledged && retries <= MAX_RETRIES)
        {
            EthernetFrame txFrame = frame;
            int applyErrorThisAttempt = 0;

            if (retries > 0)
            {
                printf("\nTimeout/no valid ACK. Retransmission %d "
                       "of %d for sequence %u.\n",
                       retries,
                       MAX_RETRIES,
                       (unsigned)sequenceNumber);
            }

            /*
             * If channel error simulation is enabled, each attempt
             * has a 50% chance of being corrupted.
             */
            if (injectError && (rand() % 2 == 0))
            {
                applyErrorThisAttempt = 1;
                injectSelectedError(&txFrame, errorChoice);

                printf("Channel result: ERROR INJECTED.\n");
                printf("Transmitted frame after corruption:\n");
                printFrame(&txFrame);
            }
            else if (injectError)
            {
                printf("Channel result: NO ERROR on this attempt.\n");
            }
            else
            {
                printf("Channel result: Error simulation disabled.\n");
            }

            /*
             * Per-attempt metadata:
             * byte 0 = scheme (1..5), with no bypass flag
             * byte 1 = whether this attempt was actually corrupted
             * byte 2 = error type if corrupted, otherwise 0
             */
            frameInfo[0] = (unsigned char)choice;
            frameInfo[1] = (unsigned char)applyErrorThisAttempt;
            frameInfo[2] = (unsigned char)
                           (applyErrorThisAttempt ? errorChoice : 0);

            if (packFrame(&txFrame, buffer) != FRAME_SIZE)
            {
                printf("Frame packing failed.\n");
                CLOSE_SOCKET(sock);
                closeInputFile(fp);
#ifdef _WIN32
                WSACleanup();
#endif
                return 1;
            }

            if (!sendAll(sock, frameInfo, 3) ||
                !sendAll(sock, buffer, FRAME_SIZE))
            {
                printf("Transmission failed or connection closed.\n");
                CLOSE_SOCKET(sock);
                closeInputFile(fp);
#ifdef _WIN32
                WSACleanup();
#endif
                return 1;
            }

            printf("Frame sequence %u sent (%d bytes).\n",
                   (unsigned)sequenceNumber,
                   FRAME_SIZE);

            if (receiveACK(sock, ack))
            {
                if (ack[0] == ACK_MARKER &&
                    ack[1] == sequenceNumber)
                {
                    printf("ACK received for sequence %u.\n",
                           (unsigned)sequenceNumber);

                    acknowledged = 1;
                }
                else
                {
                    printf("Unexpected ACK received "
                           "(marker=0x%02X, sequence=%u).\n",
                           ack[0],
                           ack[1]);
                }
            }
            else
            {
                printf("ACK timeout for sequence %u.\n",
                       (unsigned)sequenceNumber);
            }

            if (!acknowledged)
                retries++;
        }

        if (!acknowledged)
        {
            printf("\nMaximum retries reached for sequence %u. "
                   "Transmission stopped.\n",
                   (unsigned)sequenceNumber);

            CLOSE_SOCKET(sock);
            closeInputFile(fp);
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }

        sequenceNumber = (uint8_t)(sequenceNumber + 1);

        printf("Frame successfully acknowledged.\n");
    }

    closeInputFile(fp);
    CLOSE_SOCKET(sock);

#ifdef _WIN32
    WSACleanup();
#endif

    printf("\n============================================\n");
    printf("File transmission completed successfully.\n");
    printf("============================================\n");

    return 0;
}
