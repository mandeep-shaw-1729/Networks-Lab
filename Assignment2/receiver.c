#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define CLOSE_SOCKET closesocket
typedef SOCKET SocketType;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#define CLOSE_SOCKET close
typedef int SocketType;
#endif

#include "frame.h"
#include "crc.h"
#include "checksum.h"

#define SERVER_PORT 5000

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
#define MAX_WINDOW_SIZE 64

typedef struct
{
    int occupied;
    EthernetFrame frame;
} SRSlot;

/* Receive exactly the requested number of bytes over TCP. */
static int receiveAll(SocketType sock,
                      unsigned char buffer[],
                      int length)
{
    int total = 0;

    while (total < length)
    {
        int n = recv(sock,
                     (char *)buffer + total,
                     length - total, 0);

        if (n <= 0)
            return 0;

        total += n;
    }

    return 1;
}

/* Send all requested bytes. */
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

/* Send ACK marker followed by sequence number. */
static int sendACK(SocketType sock, uint8_t sequenceNumber)
{
    unsigned char ack[ACK_SIZE];

    ack[0] = ACK_MARKER;
    ack[1] = sequenceNumber;

    return sendAll(sock, ack, ACK_SIZE);
}

/*
 * Display actual payload characters safely.
 * Control characters are escaped instead of affecting the terminal.
 *
 * This function does not change or remove any payload bytes.
 * Padding is still displayed by printFrame().
 */
static void printPayloadText(const EthernetFrame *frame)
{
    unsigned int i;

    printf("Payload: ");

    for (i = 0; i < frame->length; i++)
    {
        unsigned char ch = frame->payload[i];

        if (ch == '\r')
        {
            printf("\\r");
        }
        else if (ch == '\n')
        {
            printf("\\n");
        }
        else if (ch == '\t')
        {
            printf("\\t");
        }
        else if (ch >= 32 && ch <= 126)
        {
            putchar(ch);
        }
        else
        {
            printf("\\x%02X", (unsigned int)ch);
        }
    }

    putchar('\n');
}

/*
 * Receive and verify the initial protocol configuration frame from the sender.
 */
static int receiveProtocolConfigFrame(SocketType clientSocket,
                                      int *channelType,
                                      int *protocolType,
                                      int *windowSize)
{
    unsigned char frameInfo[3];
    unsigned char buffer[FRAME_SIZE];
    EthernetFrame configFrame;

    if (!receiveAll(clientSocket, frameInfo, 3))
        return 0;

    if (!receiveAll(clientSocket, buffer, FRAME_SIZE))
        return 0;

    unpackFrame(buffer, &configFrame);

    if (configFrame.length < 4 ||
        configFrame.payload[0] != CONFIG_FRAME_MARKER)
    {
        printf("Invalid protocol configuration frame marker.\n");
        return 0;
    }

    if (!verifyFrameCRC(&configFrame, CRC_32))
    {
        printf("Protocol configuration frame failed CRC-32 verification.\n");
        return 0;
    }

    *channelType = (int)configFrame.payload[1];
    *protocolType = (int)configFrame.payload[2];
    *windowSize = (int)configFrame.payload[3];

    if (*windowSize < 1 || *windowSize > MAX_WINDOW_SIZE)
        *windowSize = 1;

    printf("\n============================================\n");
    printf("   PROTOCOL CONFIGURATION FRAME RECEIVED\n");
    printf("============================================\n");
    printf("Channel Type  : %s\n", channelName(*channelType));
    printf("Flow Control  : %s\n", protocolName(*protocolType));
    if (*protocolType == PROTO_GO_BACK_N_ARQ ||
        *protocolType == PROTO_SELECTIVE_REPEAT)
    {
        printf("Window Size N : %d\n", *windowSize);
    }
    printf("============================================\n");

    return sendACK(clientSocket, 0);
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

    SocketType serverSocket;
    SocketType clientSocket;

    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;

#ifdef _WIN32
    int clientAddressLength = sizeof(clientAddress);
#else
    socklen_t clientAddressLength = sizeof(clientAddress);
#endif

    unsigned char frameInfo[3];
    unsigned char buffer[FRAME_SIZE];

    unsigned char scheme;
    unsigned char injectError;
    unsigned char errorChoice;

    uint16_t receivedChecksum;
    uint8_t expectedSequence = 0;
    int anyInOrderAccepted = 0;

    int channelType = CHANNEL_NOISY;
    int protocolType = PROTO_STOP_AND_WAIT_ARQ;
    int windowSize = 1;

    SRSlot srBuffer[256];

    EthernetFrame frame;
    CRCType crcType;

    memset(srBuffer, 0, sizeof(srBuffer));

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

#ifdef _WIN32
    if (serverSocket == INVALID_SOCKET)
#else
    if (serverSocket < 0)
#endif
    {
        printf("Socket creation failed.\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    memset(&serverAddress, 0, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(SERVER_PORT);

    if (bind(serverSocket,
             (struct sockaddr *)&serverAddress,
             sizeof(serverAddress)) < 0)
    {
        printf("Bind failed. Is port %d already in use?\n",
               SERVER_PORT);

        CLOSE_SOCKET(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (listen(serverSocket, 1) < 0)
    {
        printf("Listen failed.\n");

        CLOSE_SOCKET(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    printf("============================================\n");
    printf("          Ethernet Frame Receiver\n");
    printf("============================================\n");
    printf("\nListening on port %d...\n", SERVER_PORT);

    clientSocket = accept(serverSocket,
                          (struct sockaddr *)&clientAddress,
                          &clientAddressLength);

#ifdef _WIN32
    if (clientSocket == INVALID_SOCKET)
#else
    if (clientSocket < 0)
#endif
    {
        printf("Accept failed.\n");

        CLOSE_SOCKET(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    printf("Sender connected successfully.\n");

    if (!receiveProtocolConfigFrame(clientSocket,
                                    &channelType,
                                    &protocolType,
                                    &windowSize))
    {
        printf("Failed to receive protocol configuration frame.\n");
        CLOSE_SOCKET(clientSocket);
        CLOSE_SOCKET(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    while (1)
    {
        int frameValid = 0;

        /*
         * Protocol:
         *   3 bytes of frame information
         *   FRAME_SIZE bytes containing the packed frame
         *
         * The checksum is stored in frame.fcs.
         * No separate checksum bytes are received.
         */

        if (!receiveAll(clientSocket, frameInfo, 3))
        {
            printf("\nSender disconnected.\n");
            break;
        }

        scheme = frameInfo[0];
        injectError = frameInfo[1];
        errorChoice = frameInfo[2];

        if (scheme < SCHEME_CHECKSUM || scheme > SCHEME_CRC32)
        {
            printf("Invalid error-detection scheme received: %u\n",
                   (unsigned)scheme);
            break;
        }

        printf("\n============================================\n");
        printf("              FRAME INFORMATION\n");
        printf("============================================\n");

        printf("Channel          : %s\n", channelName(channelType));
        printf("Flow Control     : %s\n", protocolName(protocolType));
        printf("Detection Scheme : %s\n", schemeName(scheme));

        printf("Error Injected   : %s\n",
               injectError ? "YES" : "NO");

        if (injectError)
        {
            const char *errorNames[] = {
                "NONE",
                "Single Bit",
                "Double Bit",
                "Odd Bit",
                "Burst",
                "Random"
            };

            if (errorChoice >= 1 && errorChoice <= 5)
            {
                printf("Error Type       : %s\n",
                       errorNames[errorChoice]);
            }
            else
            {
                printf("Error Type       : INVALID\n");
            }
        }
        else
        {
            printf("Error Type       : NONE\n");
        }

        /* Receive the complete frame, including the 4-byte FCS. */
        if (!receiveAll(clientSocket, buffer, FRAME_SIZE))
        {
            printf("Connection closed while receiving frame.\n");
            break;
        }

        unpackFrame(buffer, &frame);

        printf("\nFRAME RECEIVED (%d bytes)\n", FRAME_SIZE);

        /*
         * Keep the complete frame display:
         * - all payload bytes in hexadecimal
         * - actual payload text
         * - padding length
         * - padding bytes in hexadecimal
         * - FCS
         */
        printFrame(&frame);

        /*
         * Reject invalid payload lengths before accessing payload.
         * The complete frame has already been consumed from TCP.
         */
        if (frame.length > PAYLOAD_SIZE)
        {
            printf("\nInvalid payload length: %u bytes.\n",
                   (unsigned)frame.length);
            printf("Frame rejected. No ACK sent.\n");
            printf("--------------------------------------------\n");
            continue;
        }

        /* Verify the error-detection value stored in the FCS. */
        if (scheme == SCHEME_CHECKSUM)
        {
            receivedChecksum = (uint16_t)(frame.fcs & 0xFFFF);

            printf("\nChecksum extracted from FCS:\n");
            printChecksum(receivedChecksum);

            frameValid = verifyFrameChecksum(&frame,
                                             receivedChecksum);

            if (frameValid)
                printf("\nCHECKSUM RESULT: CORRECT\n");
            else
                printf("\nCHECKSUM RESULT: INCORRECT\n");
        }
        else
        {
            crcType = getCRCType(scheme);

            printf("\nReceived FCS:\n");
            printCRC(frame.fcs, crcType);

            frameValid = verifyFrameCRC(&frame, crcType);

            if (frameValid)
                printf("\nCRC RESULT: CORRECT\n");
            else
                printf("\nCRC RESULT: INCORRECT\n");
        }

        /*
         * Corrupted frames are discarded without an ACK.
         */
        if (!frameValid)
        {
            printf("Frame rejected. No ACK sent.\n");
            printf("--------------------------------------------\n");
            continue;
        }

        printf("Error-detection check passed.\n");

        /*---------------------------------------------------------
          FLOW CONTROL 1 & 2: STOP-AND-WAIT & STOP-AND-WAIT ARQ
        ---------------------------------------------------------*/
        if (protocolType == PROTO_STOP_AND_WAIT ||
            protocolType == PROTO_STOP_AND_WAIT_ARQ)
        {
            if (frame.sequenceNumber == expectedSequence)
            {
                printf("Expected sequence %u received.\n",
                       (unsigned)frame.sequenceNumber);

                printf("Frame accepted. Payload length: %u bytes.\n",
                       (unsigned)frame.length);

                printPayloadText(&frame);

                expectedSequence = (uint8_t)(expectedSequence + 1);
                anyInOrderAccepted = 1;

                if (!sendACK(clientSocket, frame.sequenceNumber))
                {
                    printf("Failed to send ACK. Connection closed.\n");
                    break;
                }

                printf("ACK sent for sequence %u.\n",
                       (unsigned)frame.sequenceNumber);
            }
            else if (anyInOrderAccepted &&
                     frame.sequenceNumber == (uint8_t)(expectedSequence - 1))
            {
                printf("Duplicate frame %u received.\n",
                       (unsigned)frame.sequenceNumber);

                printf("Payload will NOT be accepted again.\n");

                if (!sendACK(clientSocket, frame.sequenceNumber))
                {
                    printf("Failed to resend ACK. Connection closed.\n");
                    break;
                }

                printf("ACK resent for sequence %u.\n",
                       (unsigned)frame.sequenceNumber);
            }
            else
            {
                printf("Unexpected sequence %u; expected %u.\n",
                       (unsigned)frame.sequenceNumber,
                       (unsigned)expectedSequence);

                printf("Frame discarded. No ACK sent.\n");
            }
        }
        /*---------------------------------------------------------
          FLOW CONTROL 3: GO-BACK-N ARQ
          Receiver window size = 1; accepts in-order frames only
          and sends cumulative ACKs.
        ---------------------------------------------------------*/
        else if (protocolType == PROTO_GO_BACK_N_ARQ)
        {
            if (frame.sequenceNumber == expectedSequence)
            {
                printf("In-order sequence %u received (Go-Back-N).\n",
                       (unsigned)frame.sequenceNumber);
                printf("Frame accepted. Payload length: %u bytes.\n",
                       (unsigned)frame.length);

                printPayloadText(&frame);

                expectedSequence = (uint8_t)(expectedSequence + 1);
                anyInOrderAccepted = 1;

                if (!sendACK(clientSocket, frame.sequenceNumber))
                {
                    printf("Failed to send cumulative ACK. Connection closed.\n");
                    break;
                }

                printf("Cumulative ACK sent for sequence %u.\n",
                       (unsigned)frame.sequenceNumber);
            }
            else
            {
                printf("Out-of-order/duplicate frame %u received; expected %u.\n",
                       (unsigned)frame.sequenceNumber,
                       (unsigned)expectedSequence);
                printf("Frame discarded (Go-Back-N receiver window = 1).\n");

                if (anyInOrderAccepted)
                {
                    uint8_t lastAck = (uint8_t)(expectedSequence - 1);
                    if (!sendACK(clientSocket, lastAck))
                    {
                        printf("Failed to resend cumulative ACK. Connection closed.\n");
                        break;
                    }
                    printf("Cumulative ACK resent for last in-order sequence %u.\n",
                           (unsigned)lastAck);
                }
            }
        }
        /*---------------------------------------------------------
          FLOW CONTROL 4: SELECTIVE REPEAT ARQ
          Receiver window size = N; buffers out-of-order frames,
          sends independent ACKs, and delivers sorted frames.
        ---------------------------------------------------------*/
        else if (protocolType == PROTO_SELECTIVE_REPEAT)
        {
            uint8_t dist = (uint8_t)(frame.sequenceNumber - expectedSequence);
            uint8_t backDist = (uint8_t)(expectedSequence - frame.sequenceNumber);

            if (dist < (uint8_t)windowSize)
            {
                if (!srBuffer[frame.sequenceNumber].occupied)
                {
                    srBuffer[frame.sequenceNumber].occupied = 1;
                    srBuffer[frame.sequenceNumber].frame = frame;

                    if (frame.sequenceNumber == expectedSequence)
                    {
                        printf("In-order sequence %u received (Selective Repeat).\n",
                               (unsigned)frame.sequenceNumber);
                    }
                    else
                    {
                        printf("Out-of-order sequence %u buffered in receiver window "
                               "(waiting for %u).\n",
                               (unsigned)frame.sequenceNumber,
                               (unsigned)expectedSequence);
                    }
                }
                else
                {
                    printf("Duplicate buffered frame %u received.\n",
                           (unsigned)frame.sequenceNumber);
                }

                /* Send independent ACK for this specific sequence number */
                if (!sendACK(clientSocket, frame.sequenceNumber))
                {
                    printf("Failed to send Selective ACK. Connection closed.\n");
                    break;
                }

                printf("Selective ACK sent for sequence %u.\n",
                       (unsigned)frame.sequenceNumber);

                /* Deliver all consecutive buffered frames in sorted order */
                while (srBuffer[expectedSequence].occupied)
                {
                    EthernetFrame *accepted = &srBuffer[expectedSequence].frame;

                    printf("Delivering sorted frame sequence %u "
                           "(Payload length: %u bytes).\n",
                           (unsigned)accepted->sequenceNumber,
                           (unsigned)accepted->length);
                    printPayloadText(accepted);

                    srBuffer[expectedSequence].occupied = 0;
                    expectedSequence = (uint8_t)(expectedSequence + 1);
                    anyInOrderAccepted = 1;
                }
            }
            else if (anyInOrderAccepted && backDist <= (uint8_t)windowSize)
            {
                printf("Duplicate previously delivered frame %u received.\n",
                       (unsigned)frame.sequenceNumber);

                if (!sendACK(clientSocket, frame.sequenceNumber))
                {
                    printf("Failed to resend Selective ACK. Connection closed.\n");
                    break;
                }

                printf("Selective ACK resent for sequence %u.\n",
                       (unsigned)frame.sequenceNumber);
            }
            else
            {
                printf("Frame sequence %u outside Selective Repeat window "
                       "[%u..%u]. Frame discarded.\n",
                       (unsigned)frame.sequenceNumber,
                       (unsigned)expectedSequence,
                       (unsigned)(uint8_t)(expectedSequence + windowSize - 1));
            }
        }

        printf("--------------------------------------------\n");
    }

    CLOSE_SOCKET(clientSocket);
    CLOSE_SOCKET(serverSocket);

#ifdef _WIN32
    WSACleanup();
#endif

    printf("\n============================================\n");
    printf("            Receiver terminated.\n");
    printf("============================================\n");

    return 0;
}
