#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


/*----------------------------------------------------------
  Platform-specific socket headers
----------------------------------------------------------*/

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#endif


/*----------------------------------------------------------
  Project Headers
----------------------------------------------------------*/

#include "frame.h"
#include "crc.h"
#include "checksum.h"
#include "fileio.h"


/*----------------------------------------------------------
  Platform-specific socket close
----------------------------------------------------------*/

#ifdef _WIN32

#define CLOSE_SOCKET closesocket

#else

#define CLOSE_SOCKET close

#endif


/*----------------------------------------------------------
  Server Configuration
----------------------------------------------------------*/

#define SERVER_PORT 5000


/*----------------------------------------------------------
  Scheme Definitions
----------------------------------------------------------*/

#define SCHEME_CHECKSUM  1
#define SCHEME_CRC8      2
#define SCHEME_CRC10     3
#define SCHEME_CRC16     4
#define SCHEME_CRC32     5


/*----------------------------------------------------------
  Bypass Flag
 *
 * Bit 7:
 *
 *     0 = Normal Detection
 *     1 = Bypass Detection
 *
 * Lower 7 bits:
 *
 *     Detection scheme
 *----------------------------------------------------------*/

#define BYPASS_FLAG 0x80


/*----------------------------------------------------------
  Receive Exactly Required Number of Bytes
----------------------------------------------------------*/

int receiveAll(
#ifdef _WIN32
              SOCKET sock,
#else
              int sock,
#endif
              unsigned char buffer[],
              int length)
{
    int totalReceived = 0;


    while(totalReceived < length)
    {
        int bytesReceived;


        bytesReceived =
            recv(sock,
                 (char *)(buffer +
                          totalReceived),
                 length -
                 totalReceived,
                 0);


        if(bytesReceived <= 0)
        {
            return 0;
        }


        totalReceived += bytesReceived;
    }


    return 1;
}


/*----------------------------------------------------------
  Main Receiver Program
----------------------------------------------------------*/

int main(void)
{
#ifdef _WIN32

    WSADATA wsaData;

#endif


    /*------------------------------------------------------
      Initialize Windows Socket Library
      ------------------------------------------------------*/

#ifdef _WIN32

    if(WSAStartup(MAKEWORD(2, 2),
                  &wsaData) != 0)
    {
        printf("WSAStartup failed.\n");

        return 1;
    }

#endif


    /*------------------------------------------------------
      Socket Variables
      ------------------------------------------------------*/

#ifdef _WIN32

    SOCKET serverSocket;
    SOCKET clientSocket;

#else

    int serverSocket;
    int clientSocket;

#endif


    struct sockaddr_in serverAddress;

    struct sockaddr_in clientAddress;


#ifdef _WIN32

    int clientAddressLength =
        sizeof(clientAddress);

#else

    socklen_t clientAddressLength =
        sizeof(clientAddress);

#endif


    /*------------------------------------------------------
      Frame Variables
      ------------------------------------------------------*/

    unsigned char buffer[FRAME_SIZE];

    unsigned char checksumBuffer[2];


    /*
     * Frame information:
     *
     * frameInfo[0] = detection scheme + bypass flag
     *
     *     Lower 7 bits:
     *
     *     1 = Checksum
     *     2 = CRC-8
     *     3 = CRC-10
     *     4 = CRC-16
     *     5 = CRC-32
     *
     *     Bit 7:
     *
     *     0 = Normal Detection
     *     1 = Bypass Detection
     *
     *
     * frameInfo[1] = error injected
     *
     *     0 = No
     *     1 = Yes
     *
     *
     * frameInfo[2] = error type
     *
     *     0 = None
     *     1 = Single Bit
     *     2 = Double Bit
     *     3 = Odd Bit
     *     4 = Burst
     *     5 = Random
     */

    unsigned char frameInfo[3];


    unsigned char schemeByte;

    unsigned char scheme;

    unsigned char injectError;

    unsigned char errorChoice;


    EthernetFrame frame;

    uint16_t receivedChecksum;

    CRCType crcType;


    /*------------------------------------------------------
      Create Server Socket
      ------------------------------------------------------*/

    serverSocket =
        socket(AF_INET,
               SOCK_STREAM,
               0);


#ifdef _WIN32

    if(serverSocket == INVALID_SOCKET)

#else

    if(serverSocket < 0)

#endif
    {
        printf("Socket creation failed.\n");

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }


    /*------------------------------------------------------
      Clear Server Address
      ------------------------------------------------------*/

    memset(&serverAddress,
           0,
           sizeof(serverAddress));


    /*------------------------------------------------------
      Configure Server Address
      ------------------------------------------------------*/

    serverAddress.sin_family =
        AF_INET;

    serverAddress.sin_addr.s_addr =
        htonl(INADDR_ANY);

    serverAddress.sin_port =
        htons(SERVER_PORT);


    /*------------------------------------------------------
      Bind Socket
      ------------------------------------------------------*/

    if(bind(serverSocket,
            (struct sockaddr *)&serverAddress,
            sizeof(serverAddress)) < 0)
    {
        printf("Bind failed.\n");

        CLOSE_SOCKET(serverSocket);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }


    /*------------------------------------------------------
      Listen for Sender
      ------------------------------------------------------*/

    if(listen(serverSocket,
              1) < 0)
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

    printf("============================================\n");


    printf("\nReceiver is listening on port %d...\n",
           SERVER_PORT);


    /*------------------------------------------------------
      Accept Sender Connection
      ------------------------------------------------------*/

    clientSocket =
        accept(serverSocket,
               (struct sockaddr *)&clientAddress,
               &clientAddressLength);


#ifdef _WIN32

    if(clientSocket == INVALID_SOCKET)

#else

    if(clientSocket < 0)

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


    /*======================================================
      RECEIVE FRAMES
      ======================================================*/

    while(1)
    {
        /*==================================================
          RECEIVE FRAME INFORMATION
          ==================================================*/

        if(!receiveAll(clientSocket,
                       frameInfo,
                       3))
        {
            printf("\nSender disconnected.\n");

            break;
        }


        /*--------------------------------------------------
          Extract Scheme Byte
          --------------------------------------------------*/

        schemeByte =
            frameInfo[0];


        /*
         * Remove bypass flag.
         *
         * The lower 7 bits contain the actual
         * error detection scheme.
         */

        scheme =
            schemeByte & 0x7F;


        /*--------------------------------------------------
          Extract Remaining Frame Information
          --------------------------------------------------*/

        injectError =
            frameInfo[1];


        errorChoice =
            frameInfo[2];


        /*==================================================
          Validate Detection Scheme
          ==================================================*/

        if(scheme < 1 ||
           scheme > 5)
        {
            printf("\nInvalid error detection scheme received.\n");

            break;
        }


        /*==================================================
          Display Frame Information
          ==================================================*/

        printf("\n\n============================================\n");

        printf("              FRAME INFORMATION\n");

        printf("============================================\n");


        printf("Error Detection : ");


        switch(scheme)
        {
            case SCHEME_CHECKSUM:

                printf("CHECKSUM");

                break;


            case SCHEME_CRC8:

                printf("CRC-8");

                break;


            case SCHEME_CRC10:

                printf("CRC-10");

                break;


            case SCHEME_CRC16:

                printf("CRC-16");

                break;


            case SCHEME_CRC32:

                printf("CRC-32");

                break;
        }


        printf("\n");


        /*--------------------------------------------------
          Display Error Information
          --------------------------------------------------*/

        if(injectError == 1)
        {
            printf("Error Injection : YES\n");

            printf("Error Type      : ");


            switch(errorChoice)
            {
                case 1:

                    printf("Single Bit Error");

                    break;


                case 2:

                    printf("Double Bit Error");

                    break;


                case 3:

                    printf("Odd Bit Error");

                    break;


                case 4:

                    printf("Burst Error");

                    break;


                case 5:

                    printf("Random Error");

                    break;


                default:

                    printf("INVALID");

                    break;
            }


            printf("\n");
        }
        else
        {
            printf("Error Injection : NO\n");

            printf("Error Type      : NONE\n");
        }


        /*==================================================
          RECEIVE COMPLETE 64-BYTE FRAME
          ==================================================*/

        if(!receiveAll(clientSocket,
                       buffer,
                       FRAME_SIZE))
        {
            printf("\nSender disconnected while receiving frame.\n");

            break;
        }


        /*--------------------------------------------------
          Display Frame Reception
          --------------------------------------------------*/

        printf("\n============================================\n");

        printf("          64-BYTE FRAME RECEIVED\n");

        printf("============================================\n");


        /*--------------------------------------------------
          Unpack Frame
          --------------------------------------------------*/

        unpackFrame(buffer,
                    &frame);


        /*--------------------------------------------------
          Display Received Frame
          --------------------------------------------------*/

        printf("\nRECEIVED FRAME\n");

        printFrame(&frame);


        /*==================================================
          CHECKSUM
          ==================================================*/

        if(scheme == SCHEME_CHECKSUM)
        {
            /*------------------------------------------------
              Receive 2-byte checksum
              ------------------------------------------------*/

            if(!receiveAll(clientSocket,
                           checksumBuffer,
                           2))
            {
                printf("\nChecksum reception failed.\n");

                break;
            }


            /*------------------------------------------------
              Reconstruct checksum
              ------------------------------------------------*/

            receivedChecksum =
                ((uint16_t)checksumBuffer[1] << 8) |
                checksumBuffer[0];


            /*------------------------------------------------
              Display received checksum
              ------------------------------------------------*/

            printf("\nReceived checksum:\n");

            printChecksum(receivedChecksum);


            /*------------------------------------------------
              Verify checksum normally
              ------------------------------------------------*/

            if(verifyFrameChecksum(&frame,
                                   receivedChecksum))
            {
                printf("\nCHECKSUM RESULT: CORRECT\n");

                printf("Frame accepted.\n");
            }
            else
            {
                printf("\nCHECKSUM RESULT: INCORRECT\n");

                printf("Error detected in frame.\n");
            }
        }


        /*==================================================
          CRC
          ==================================================*/

        else
        {
            /*------------------------------------------------
              Select CRC type for this frame
              ------------------------------------------------*/

            switch(scheme)
            {
                case SCHEME_CRC8:

                    crcType = CRC_8;

                    break;


                case SCHEME_CRC10:

                    crcType = CRC_10;

                    break;


                case SCHEME_CRC16:

                    crcType = CRC_16;

                    break;


                case SCHEME_CRC32:

                    crcType = CRC_32;

                    break;


                default:

                    crcType = CRC_32;

                    break;
            }


            /*------------------------------------------------
              FCS is already inside the 64-byte frame.
              No additional FCS is received.
              ------------------------------------------------*/

            printf("\nReceived FCS:\n");

            printCRC(frame.fcs,
                     crcType);


            /*------------------------------------------------
              Verify CRC normally
              ------------------------------------------------*/

            if(verifyFrameCRC(&frame,
                              crcType))
            {
                printf("\nCRC RESULT: CORRECT\n");

                printf("Frame accepted.\n");
            }
            else
            {
                printf("\nCRC RESULT: INCORRECT\n");

                printf("Error detected in frame.\n");
            }
        }


        printf("\n--------------------------------------------\n");
    }


    /*------------------------------------------------------
      Close Client Socket
      ------------------------------------------------------*/

    CLOSE_SOCKET(clientSocket);


    /*------------------------------------------------------
      Close Server Socket
      ------------------------------------------------------*/

    CLOSE_SOCKET(serverSocket);


#ifdef _WIN32

    WSACleanup();

#endif


    /*------------------------------------------------------
      Receiver Terminated
      ------------------------------------------------------*/

    printf("\n============================================\n");

    printf("          Receiver terminated.\n");

    printf("============================================\n");


    return 0;
}
