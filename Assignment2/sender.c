//This is my current sender.c :-
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

#define CHANNEL_NOISELESS 1
#define CHANNEL_NOISY     2

#define PROTO_STOP_AND_WAIT     1
#define PROTO_STOP_AND_WAIT_ARQ 2
#define PROTO_GO_BACK_N_ARQ     3
#define PROTO_SELECTIVE_REPEAT  4

#define CONFIG_FRAME_MARKER 0xC0

#define ACK_SIZE 2
#define ACK_MARKER 0xA5
#define ACK_TIMEOUT_MS 1500
#define MAX_RETRIES 5
#define MAX_WINDOW_SIZE 64
#define INITIAL_CAPACITY 64

typedef struct
{
    EthernetFrame frame;
    int choice;
    int injectError;
    int errorChoice;
} PreparedFrame;

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

static void setSocketTimeout(SocketType sock, int timeoutMs)
{
#ifdef _WIN32
    DWORD timeout = (timeoutMs > 0) ? (DWORD)timeoutMs : 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               (const char *)&timeout, sizeof(timeout));
#else
    struct timeval timeout;
    timeout.tv_sec = (timeoutMs > 0) ? (timeoutMs / 1000) : 0;
    timeout.tv_usec = (timeoutMs > 0) ? ((timeoutMs % 1000) * 1000) : 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               &timeout, sizeof(timeout));
#endif
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

static const char *channelName(int channelType)
{
    return (channelType == CHANNEL_NOISELESS)
               ? "Noiseless Channel"
               : "Noisy Channel";
}

static const char *protocolName(int protocolType)
{
    switch (protocolType)
    {
        case PROTO_STOP_AND_WAIT:
            return "Stop-and-Wait";

        case PROTO_STOP_AND_WAIT_ARQ:
            return "Stop-and-Wait ARQ";

        case PROTO_GO_BACK_N_ARQ:
            return "Go-Back-N ARQ";

        case PROTO_SELECTIVE_REPEAT:
            return "Selective Repeat ARQ";

        default:
            return "Unknown Protocol";
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

/*
 * Send a protocol configuration frame to the receiver so it knows
 * which channel type, flow-control protocol, and window size to use.
 */
static int sendProtocolConfigFrame(SocketType sock,
                                   int channelType,
                                   int protocolType,
                                   int windowSize)
{
    EthernetFrame configFrame;
    unsigned char frameInfo[3];
    unsigned char buffer[FRAME_SIZE];
    unsigned char ack[ACK_SIZE];
    unsigned char configPayload[4];

    unsigned char sourceMAC[MAC_SIZE] =
        {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    unsigned char destinationMAC[MAC_SIZE] =
        {0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB};

    initializeFrame(&configFrame);
    setSourceMAC(&configFrame, sourceMAC);
    setDestinationMAC(&configFrame, destinationMAC);

    configPayload[0] = CONFIG_FRAME_MARKER;
    configPayload[1] = (unsigned char)channelType;
    configPayload[2] = (unsigned char)protocolType;
    configPayload[3] = (unsigned char)windowSize;

    copyPayload(&configFrame, configPayload, 4);
    configFrame.sequenceNumber = 0;

    calculateFrameCheckValue(&configFrame, SCHEME_CRC32);

    frameInfo[0] = SCHEME_CRC32;
    frameInfo[1] = 0;
    frameInfo[2] = 0;

    if (packFrame(&configFrame, buffer) != FRAME_SIZE)
        return 0;

    printf("\n============================================\n");
    printf("SENDING PROTOCOL CONFIGURATION FRAME\n");
    printf("============================================\n");
    printf("Channel Type  : %s\n", channelName(channelType));
    printf("Flow Control  : %s\n", protocolName(protocolType));
    if (protocolType == PROTO_GO_BACK_N_ARQ ||
        protocolType == PROTO_SELECTIVE_REPEAT)
    {
        printf("Window Size N : %d\n", windowSize);
    }

    if (!sendAll(sock, frameInfo, 3) ||
        !sendAll(sock, buffer, FRAME_SIZE))
    {
        return 0;
    }

    if (!receiveACK(sock, ack) ||
        ack[0] != ACK_MARKER ||
        ack[1] != 0)
    {
        return 0;
    }

    printf("Protocol configuration acknowledged by receiver.\n");
    return 1;
}

/*
 * Transmit a single prepared frame through the simulated channel.
 */
static int transmitFrameOnce(SocketType sock,
                             const PreparedFrame *pf,
                             int channelType,
                             int attemptNumber,
                             int maxRetries)
{
    EthernetFrame txFrame = pf->frame;
    unsigned char frameInfo[3];
    unsigned char buffer[FRAME_SIZE];
    int applyErrorThisAttempt = 0;
    int actualErrorChoice = 0;

    if (attemptNumber > 0)
    {
        printf("\nTimeout/no valid ACK. Retransmission %d "
               "of %d for sequence %u.\n",
               attemptNumber,
               maxRetries,
               (unsigned)pf->frame.sequenceNumber);
    }

    if (channelType == CHANNEL_NOISELESS)
    {
        /*
         * Noiseless Channel Stop-and-Wait:
         * Probability of NO error injection = 5/6
         * Probability of 5 error injections combined = 1/6
         */
        if (pf->injectError)
        {
            applyErrorThisAttempt = 1;
            actualErrorChoice = pf->errorChoice;
            injectSelectedError(&txFrame, actualErrorChoice);

            printf("Channel result (1/6 probability): ERROR INJECTED (%s).\n",
                   errorName(actualErrorChoice));
            printf("Transmitted frame after corruption:\n");
            printFrame(&txFrame);
        }
        else
        {
            printf("Channel result (5/6 probability): NO ERROR on this frame.\n");
        }
    }
    else
    {
        /*
         * Noisy Channel ARQ protocols:
         * If error simulation is enabled for this frame, each transmission
         * attempt independently has a 50% chance of corruption.
         */
        if (pf->injectError && (rand() % 2 == 0))
        {
            applyErrorThisAttempt = 1;
            actualErrorChoice = pf->errorChoice;
            injectSelectedError(&txFrame, actualErrorChoice);

            printf("Channel result: ERROR INJECTED.\n");
            printf("Transmitted frame after corruption:\n");
            printFrame(&txFrame);
        }
        else if (pf->injectError)
        {
            printf("Channel result: NO ERROR on this attempt.\n");
        }
        else
        {
            printf("Channel result: Error simulation disabled.\n");
        }
    }

    frameInfo[0] = (unsigned char)pf->choice;
    frameInfo[1] = (unsigned char)applyErrorThisAttempt;
    frameInfo[2] = (unsigned char)(applyErrorThisAttempt ? actualErrorChoice : 0);

    if (packFrame(&txFrame, buffer) != FRAME_SIZE)
    {
        printf("Frame packing failed.\n");
        return 0;
    }

    if (!sendAll(sock, frameInfo, 3) ||
        !sendAll(sock, buffer, FRAME_SIZE))
    {
        printf("Transmission failed or connection closed.\n");
        return 0;
    }

    printf("Frame sequence %u sent (%d bytes).\n",
           (unsigned)pf->frame.sequenceNumber,
           FRAME_SIZE);

    return 1;
}

static void printPreparedFrameHeader(const PreparedFrame *pf, int channelType)
{
    printf("\n============================================\n");
    printf("FRAME SEQUENCE NUMBER: %u\n",
           (unsigned)pf->frame.sequenceNumber);
    printf("Error Detection: %s\n", schemeName(pf->choice));

    if (channelType == CHANNEL_NOISELESS)
    {
        printf("Channel Mode: Noiseless (5/6 No Error, 1/6 Error)\n");
        printf("Selected Outcome: %s\n",
               pf->injectError ? "ERROR INJECTION (1/6)" : "NO ERROR (5/6)");
        if (pf->injectError)
            printf("Configured Error Type: %s\n", errorName(pf->errorChoice));
    }
    else
    {
        printf("Channel Error Simulation: %s\n",
               pf->injectError ? "ENABLED" : "DISABLED");
        if (pf->injectError)
            printf("Configured Error Type: %s\n", errorName(pf->errorChoice));
    }

    printf("\nORIGINAL FRAME (BEFORE CHANNEL)\n");
    printFrame(&pf->frame);
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
    unsigned char ack[ACK_SIZE];

    uint16_t length;

    int modeChoice;
    int channelType;
    int protocolType;
    int windowSize = 1;

    int manualScheme = 0;
    int manualInjectError = 1;
    int manualErrorChoice = 0;

    PreparedFrame *frames = NULL;
    int frameCount = 0;
    int frameCapacity = 0;

    srand((unsigned int)time(NULL));

    printf("============================================\n");
    printf("          Ethernet Frame Sender\n");
    printf("============================================\n");

    /* 1. Select Frame Generation Mode */
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

    /* 2. Select Channel Type irrespective of the choice above */
    printf("\nSelect Channel Type:\n");
    printf("1. Noiseless Channels\n");
    printf("2. Noisy Channels\n");
    printf("Enter choice: ");

    if (scanf("%d", &channelType) != 1 ||
        (channelType != CHANNEL_NOISELESS && channelType != CHANNEL_NOISY))
    {
        printf("Invalid channel type.\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    /* 3. Select Protocol based on Channel Type */
    if (channelType == CHANNEL_NOISELESS)
    {
        protocolType = PROTO_STOP_AND_WAIT;
        windowSize = 1;
        printf("\nNoiseless Channel selected -> "
               "\"Stop-and-Wait\" selected automatically.\n");
        printf("(No-error probability = 5/6, "
               "Combined 5-error probability = 1/6)\n");
    }
    else
    {
        int noisyProtoChoice = 0;

        printf("\nSelect Noisy Channel Flow Control Protocol:\n");
        printf("1. Stop And Wait ARQ\n");
        printf("2. Go-Back-N ARQ\n");
        printf("3. Selective Repeat ARQ\n");
        printf("Enter choice: ");

        if (scanf("%d", &noisyProtoChoice) != 1 ||
            noisyProtoChoice < 1 || noisyProtoChoice > 3)
        {
            printf("Invalid protocol choice.\n");
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }

        if (noisyProtoChoice == 1)
        {
            protocolType = PROTO_STOP_AND_WAIT_ARQ;
            windowSize = 1;
        }
        else if (noisyProtoChoice == 2)
        {
            protocolType = PROTO_GO_BACK_N_ARQ;
        }
        else
        {
            protocolType = PROTO_SELECTIVE_REPEAT;
        }

        if (protocolType == PROTO_GO_BACK_N_ARQ ||
            protocolType == PROTO_SELECTIVE_REPEAT)
        {
            printf("\nEnter Window Size (N) [1-%d]: ", MAX_WINDOW_SIZE);
            if (scanf("%d", &windowSize) != 1 ||
                windowSize < 1 || windowSize > MAX_WINDOW_SIZE)
            {
                printf("Invalid window size.\n");
#ifdef _WIN32
                WSACleanup();
#endif
                return 1;
            }
        }
    }

    /* Manual Mode configuration */
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

        if (channelType == CHANNEL_NOISY)
        {
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
        else
        {
            /*
             * In Noiseless Channel + Manual Choose, the 5/6 vs 1/6 probability
             * still governs whether an error occurs, and if that 1/6 case
             * triggers, we use the user's selected error type.
             */
            printf("\nSelect Error Type (used when the 1/6 error case occurs):\n");
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

    printf("\n============================================\n");
    printf("Mode          : %s\n",
           modeChoice == 1 ? "RANDOMIZE" : "MANUAL CHOOSE");
    printf("Channel       : %s\n", channelName(channelType));
    printf("Flow Control  : %s\n", protocolName(protocolType));
    if (protocolType == PROTO_GO_BACK_N_ARQ ||
        protocolType == PROTO_SELECTIVE_REPEAT)
    {
        printf("Window Size N : %d\n", windowSize);
    }
    printf("============================================\n");

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

    /* Read all payloads from file into prepared frames list */
    frameCapacity = INITIAL_CAPACITY;
    frames = (PreparedFrame *)malloc(sizeof(PreparedFrame) * (size_t)frameCapacity);
    if (frames == NULL)
    {
        printf("Memory allocation failed.\n");
        closeInputFile(fp);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    while (getNextPayload(fp, payload, &length))
    {
        PreparedFrame *pf;
        unsigned char sourceMAC[MAC_SIZE] =
            {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
        unsigned char destinationMAC[MAC_SIZE] =
            {0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB};

        if (frameCount >= frameCapacity)
        {
            int newCap = frameCapacity * 2;
            PreparedFrame *resized = (PreparedFrame *)realloc(
                frames, sizeof(PreparedFrame) * (size_t)newCap);
            if (resized == NULL)
            {
                printf("Memory reallocation failed.\n");
                free(frames);
                closeInputFile(fp);
#ifdef _WIN32
                WSACleanup();
#endif
                return 1;
            }
            frames = resized;
            frameCapacity = newCap;
        }

        pf = &frames[frameCount];
        initializeFrame(&pf->frame);
        pf->frame.sequenceNumber = (uint8_t)frameCount;

        if (modeChoice == 1)
        {
            pf->choice = (rand() % 5) + 1;

            if (channelType == CHANNEL_NOISELESS)
            {
                /* 5/6 chance of no error, 1/6 chance of error (1..5) */
                if ((rand() % 6) == 0)
                {
                    pf->injectError = 1;
                    pf->errorChoice = (rand() % 5) + 1;
                }
                else
                {
                    pf->injectError = 0;
                    pf->errorChoice = 0;
                }
            }
            else
            {
                pf->injectError = rand() % 2;
                pf->errorChoice = pf->injectError ? ((rand() % 5) + 1) : 0;
            }
        }
        else
        {
            pf->choice = manualScheme;

            if (channelType == CHANNEL_NOISELESS)
            {
                /* 5/6 chance of no error, 1/6 chance of error */
                if ((rand() % 6) == 0)
                {
                    pf->injectError = 1;
                    pf->errorChoice = manualErrorChoice;
                }
                else
                {
                    pf->injectError = 0;
                    pf->errorChoice = 0;
                }
            }
            else
            {
                pf->injectError = (manualInjectError == 1);
                pf->errorChoice = pf->injectError ? manualErrorChoice : 0;
            }
        }

        setSourceMAC(&pf->frame, sourceMAC);
        setDestinationMAC(&pf->frame, destinationMAC);
        copyPayload(&pf->frame, payload, length);
        calculateFrameCheckValue(&pf->frame, pf->choice);

        frameCount++;
    }

    closeInputFile(fp);
    fp = NULL;

    sock = socket(AF_INET, SOCK_STREAM, 0);

#ifdef _WIN32
    if (sock == INVALID_SOCKET)
#else
    if (sock < 0)
#endif
    {
        printf("Socket creation failed.\n");
        free(frames);
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
        free(frames);
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
        free(frames);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    /* Set timeout for the initial configuration handshake */
    setSocketTimeout(sock, ACK_TIMEOUT_MS);

    if (!sendProtocolConfigFrame(sock, channelType, protocolType, windowSize))
    {
        printf("Failed to send/acknowledge protocol configuration frame.\n");
        CLOSE_SOCKET(sock);
        free(frames);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    /*---------------------------------------------------------
      PROTOCOL 1: NOISELESS CHANNEL -> STOP-AND-WAIT
      (Blocks indefinitely for ACK if error causes drop;
       user stops deadlock with Ctrl+C)
    ---------------------------------------------------------*/
    if (protocolType == PROTO_STOP_AND_WAIT)
    {
        int i;

        /* Disable receive timeout so missing ACK causes intentional deadlock */
        setSocketTimeout(sock, 0);
        printf("Connected. Running Noiseless Stop-and-Wait "
               "(Waiting indefinitely for ACK; press Ctrl+C if deadlocked).\n");

        for (i = 0; i < frameCount; i++)
        {
            PreparedFrame *pf = &frames[i];

            printPreparedFrameHeader(pf, channelType);

            if (!transmitFrameOnce(sock, pf, channelType, 0, 0))
            {
                CLOSE_SOCKET(sock);
                free(frames);
#ifdef _WIN32
                WSACleanup();
#endif
                return 1;
            }

            printf("Waiting for ACK for sequence %u...\n",
                   (unsigned)pf->frame.sequenceNumber);

            while (1)
            {
                if (!receiveACK(sock, ack))
                {
                    printf("Connection closed while waiting for ACK.\n");
                    CLOSE_SOCKET(sock);
                    free(frames);
#ifdef _WIN32
                    WSACleanup();
#endif
                    return 1;
                }

                if (ack[0] == ACK_MARKER &&
                    ack[1] == pf->frame.sequenceNumber)
                {
                    printf("ACK received for sequence %u.\n",
                           (unsigned)pf->frame.sequenceNumber);
                    printf("Frame successfully acknowledged.\n");
                    break;
                }
            }
        }
    }
    /*---------------------------------------------------------
      PROTOCOL 2: NOISY CHANNEL -> STOP-AND-WAIT ARQ
    ---------------------------------------------------------*/
    else if (protocolType == PROTO_STOP_AND_WAIT_ARQ)
    {
        int i;

        setSocketTimeout(sock, ACK_TIMEOUT_MS);
        printf("Connected. ACK timeout: %d ms\n", ACK_TIMEOUT_MS);

        for (i = 0; i < frameCount; i++)
        {
            PreparedFrame *pf = &frames[i];
            int acknowledged = 0;
            int retries = 0;

            printPreparedFrameHeader(pf, channelType);

            while (!acknowledged && retries <= MAX_RETRIES)
            {
                if (!transmitFrameOnce(sock, pf, channelType, retries, MAX_RETRIES))
                {
                    CLOSE_SOCKET(sock);
                    free(frames);
#ifdef _WIN32
                    WSACleanup();
#endif
                    return 1;
                }

                if (receiveACK(sock, ack))
                {
                    if (ack[0] == ACK_MARKER &&
                        ack[1] == pf->frame.sequenceNumber)
                    {
                        printf("ACK received for sequence %u.\n",
                               (unsigned)pf->frame.sequenceNumber);
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
                           (unsigned)pf->frame.sequenceNumber);
                }

                if (!acknowledged)
                    retries++;
            }

            if (!acknowledged)
            {
                printf("\nMaximum retries reached for sequence %u. "
                       "Transmission stopped.\n",
                       (unsigned)pf->frame.sequenceNumber);
                CLOSE_SOCKET(sock);
                free(frames);
#ifdef _WIN32
                WSACleanup();
#endif
                return 1;
            }

            printf("Frame successfully acknowledged.\n");
        }
    }
    /*---------------------------------------------------------
      PROTOCOL 3: NOISY CHANNEL -> GO-BACK-N ARQ
    ---------------------------------------------------------*/
    else if (protocolType == PROTO_GO_BACK_N_ARQ)
    {
        int base = 0;
        int nextSeq = 0;
        int retries = 0;

        setSocketTimeout(sock, ACK_TIMEOUT_MS);
        printf("Connected. Go-Back-N ARQ (Window Size N = %d, Timeout = %d ms)\n",
               windowSize, ACK_TIMEOUT_MS);

        while (base < frameCount)
        {
            /* Send new frames within the current window */
            while (nextSeq < frameCount && nextSeq < base + windowSize)
            {
                PreparedFrame *pf = &frames[nextSeq];

                printPreparedFrameHeader(pf, channelType);

                if (!transmitFrameOnce(sock, pf, channelType, 0, MAX_RETRIES))
                {
                    CLOSE_SOCKET(sock);
                    free(frames);
#ifdef _WIN32
                    WSACleanup();
#endif
                    return 1;
                }

                nextSeq++;
            }

            /* Wait for cumulative ACK */
            if (receiveACK(sock, ack))
            {
                if (ack[0] == ACK_MARKER)
                {
                    uint8_t ackSeq = ack[1];
                    int matchedIndex = -1;
                    int k;

                    for (k = base; k < nextSeq; k++)
                    {
                        if (frames[k].frame.sequenceNumber == ackSeq)
                        {
                            matchedIndex = k;
                            break;
                        }
                    }

                    if (matchedIndex >= base)
                    {
                        printf("Cumulative ACK received for sequence %u "
                               "(acknowledges frames %d through %d).\n",
                               (unsigned)ackSeq, base, matchedIndex);
                        base = matchedIndex + 1;
                        retries = 0;
                    }
                    else
                    {
                        printf("Duplicate/older ACK received for sequence %u "
                               "(current window base = %u).\n",
                               (unsigned)ackSeq,
                               (unsigned)frames[base].frame.sequenceNumber);
                    }
                }
            }
            else
            {
                int k;

                retries++;
                if (retries > MAX_RETRIES)
                {
                    printf("\nMaximum retries reached for Go-Back-N window "
                           "starting at sequence %u. Transmission stopped.\n",
                           (unsigned)frames[base].frame.sequenceNumber);
                    CLOSE_SOCKET(sock);
                    free(frames);
#ifdef _WIN32
                    WSACleanup();
#endif
                    return 1;
                }

                printf("\nTimeout waiting for ACK of sequence %u. "
                       "Go-Back-N Retransmission %d of %d for window [%d..%d].\n",
                       (unsigned)frames[base].frame.sequenceNumber,
                       retries, MAX_RETRIES, base, nextSeq - 1);

                for (k = base; k < nextSeq; k++)
                {
                    if (!transmitFrameOnce(sock, &frames[k],
                                           channelType, retries, MAX_RETRIES))
                    {
                        CLOSE_SOCKET(sock);
                        free(frames);
#ifdef _WIN32
                        WSACleanup();
#endif
                        return 1;
                    }
                }
            }
        }
    }
    /*---------------------------------------------------------
      PROTOCOL 4: NOISY CHANNEL -> SELECTIVE REPEAT ARQ
    ---------------------------------------------------------*/
    else if (protocolType == PROTO_SELECTIVE_REPEAT)
    {
        int *acked = NULL;
        int *retryCounts = NULL;
        int base = 0;
        int nextSeq = 0;

        acked = (int *)calloc((size_t)(frameCount > 0 ? frameCount : 1), sizeof(int));
        retryCounts = (int *)calloc((size_t)(frameCount > 0 ? frameCount : 1), sizeof(int));

        if (acked == NULL || retryCounts == NULL)
        {
            printf("Memory allocation failed for Selective Repeat state.\n");
            free(acked);
            free(retryCounts);
            CLOSE_SOCKET(sock);
            free(frames);
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }

        setSocketTimeout(sock, ACK_TIMEOUT_MS);
        printf("Connected. Selective Repeat ARQ (Window Size N = %d, Timeout = %d ms)\n",
               windowSize, ACK_TIMEOUT_MS);

        while (base < frameCount)
        {
            /* Send all new frames entering the window */
            while (nextSeq < frameCount && nextSeq < base + windowSize)
            {
                PreparedFrame *pf = &frames[nextSeq];

                printPreparedFrameHeader(pf, channelType);

                if (!transmitFrameOnce(sock, pf, channelType, 0, MAX_RETRIES))
                {
                    free(acked);
                    free(retryCounts);
                    CLOSE_SOCKET(sock);
                    free(frames);
#ifdef _WIN32
                    WSACleanup();
#endif
                    return 1;
                }

                nextSeq++;
            }

            /* Wait for independent ACK */
            if (receiveACK(sock, ack))
            {
                if (ack[0] == ACK_MARKER)
                {
                    uint8_t ackSeq = ack[1];
                    int k;
                    int matched = 0;

                    for (k = base; k < nextSeq; k++)
                    {
                        if (frames[k].frame.sequenceNumber == ackSeq)
                        {
                            acked[k] = 1;
                            matched = 1;
                            printf("Selective ACK received for sequence %u.\n",
                                   (unsigned)ackSeq);
                            break;
                        }
                    }

                    if (!matched)
                    {
                        printf("ACK received for sequence %u (outside active unacked set).\n",
                               (unsigned)ackSeq);
                    }

                    while (base < frameCount && acked[base])
                    {
                        printf("Window slides past acknowledged sequence %u.\n",
                               (unsigned)frames[base].frame.sequenceNumber);
                        base++;
                    }
                }
            }
            else
            {
                int k;

                printf("\nTimeout in Selective Repeat window [%d..%d]. "
                       "Selectively retransmitting unacknowledged frames.\n",
                       base, nextSeq - 1);

                for (k = base; k < nextSeq; k++)
                {
                    if (!acked[k])
                    {
                        retryCounts[k]++;
                        if (retryCounts[k] > MAX_RETRIES)
                        {
                            printf("\nMaximum retries reached for sequence %u. "
                                   "Transmission stopped.\n",
                                   (unsigned)frames[k].frame.sequenceNumber);
                            free(acked);
                            free(retryCounts);
                            CLOSE_SOCKET(sock);
                            free(frames);
#ifdef _WIN32
                            WSACleanup();
#endif
                            return 1;
                        }

                        if (!transmitFrameOnce(sock, &frames[k],
                                               channelType, retryCounts[k], MAX_RETRIES))
                        {
                            free(acked);
                            free(retryCounts);
                            CLOSE_SOCKET(sock);
                            free(frames);
#ifdef _WIN32
                            WSACleanup();
#endif
                            return 1;
                        }
                    }
                }
            }
        }

        free(acked);
        free(retryCounts);
    }

    free(frames);
    CLOSE_SOCKET(sock);

#ifdef _WIN32
    WSACleanup();
#endif

    printf("\n============================================\n");
    printf("File transmission completed successfully.\n");
    printf("============================================\n");

    return 0;
}
