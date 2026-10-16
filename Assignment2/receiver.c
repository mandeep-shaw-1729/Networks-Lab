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

#define ACK_SIZE 2
#define ACK_MARKER 0xA5

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

    EthernetFrame frame;
    CRCType crcType;

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
    printf("         Ethernet Frame Receiver\n");
    printf("           Stop-and-Wait ARQ\n");
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
         * The sender retries after its timeout.
         */
        if (!frameValid)
        {
            printf("Frame rejected. No ACK sent.\n");
            printf("--------------------------------------------\n");
            continue;
        }

        printf("Error-detection check passed.\n");

        /*
         * Stop-and-Wait ARQ:
         * expectedSequence is the next new frame expected.
         */
        if (frame.sequenceNumber == expectedSequence)
        {
            printf("Expected sequence %u received.\n",
                   (unsigned)frame.sequenceNumber);

            printf("Frame accepted. Payload length: %u bytes.\n",
                   (unsigned)frame.length);

            /*
             * Print actual payload text with escaped control characters.
             * Padding remains visible in the full printFrame() output.
             */
            printPayloadText(&frame);

            /*
             * Advance before sending ACK so that a retransmission
             * of this accepted frame will be identified as a duplicate.
             */
            expectedSequence =
                (uint8_t)(expectedSequence + 1);

            if (!sendACK(clientSocket, frame.sequenceNumber))
            {
                printf("Failed to send ACK. Connection closed.\n");
                break;
            }

            printf("ACK sent for sequence %u.\n",
                   (unsigned)frame.sequenceNumber);
        }
        else if (frame.sequenceNumber ==
                 (uint8_t)(expectedSequence - 1))
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

        printf("--------------------------------------------\n");
    }

    CLOSE_SOCKET(clientSocket);
    CLOSE_SOCKET(serverSocket);

#ifdef _WIN32
    WSACleanup();
#endif

    printf("\n============================================\n");
    printf("          Receiver terminated.\n");
    printf("============================================\n");

    return 0;
}