#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

 //Platform-specific socket headers

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#endif

  //Project Headers

#include "frame.h"
#include "crc.h"
#include "checksum.h"
#include "error.h"
#include "fileio.h"

  //Platform-specific definitions

#ifdef _WIN32

#define CLOSE_SOCKET closesocket

#else

#define CLOSE_SOCKET close

#endif

  //Server Configuration

#define SERVER_PORT 5000

#define SERVER_IP "127.0.0.1"

  //Scheme Definitions

#define SCHEME_CHECKSUM  1
#define SCHEME_CRC8      2
#define SCHEME_CRC10     3
#define SCHEME_CRC16     4
#define SCHEME_CRC32     5

#define BYPASS_FLAG 0x80

  //Main Sender Program

int main(void)
{
#ifdef _WIN32

    WSADATA wsaData;

#endif


#ifdef _WIN32

     //Windows Socket Library.

    if(WSAStartup(MAKEWORD(2, 2),
                  &wsaData) != 0)
    {
        printf("WSAStartup failed.\n");

        return 1;
    }

#endif

      //Socket variable

#ifdef _WIN32

    SOCKET sock;

#else

    int sock;

#endif


    struct sockaddr_in serverAddress;

    FILE *fp;

    char filename[256];

    unsigned char payload[PAYLOAD_SIZE];

    unsigned char buffer[FRAME_SIZE];


    /*
     * Frame information:
     *
     * frameInfo[0] = error detection scheme
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
     * //frameInfo[1] = error injection
     *
     *     0 = No error
     *     1 = Error injected
     *
     *
     * //frameInfo[2] = error type
     *
     *     0 = No error
     *     1 = Single Bit
     *     2 = Double Bit
     *     3 = Odd Bit
     *     4 = Burst
     *     5 = Random
     */

    unsigned char frameInfo[3];


    uint16_t length;

    uint16_t checksum = 0;

    EthernetFrame frame;


    /*------------------------------------------------------
      Mode variables
    ------------------------------------------------------*/

    int modeChoice;

    int manualScheme;

    int manualBypass;

    int manualInjectError;

    int manualErrorChoice;


    /*------------------------------------------------------
      Per-frame variables
    ------------------------------------------------------*/

    int choice;

    int bypassDetection;

    int errorChoice;

    int injectError;


    /*------------------------------------------------------
      Initialize Random Number Generator
    ------------------------------------------------------*/

    srand((unsigned int)time(NULL));


    /*------------------------------------------------------
      Display Program Information
    ------------------------------------------------------*/

    printf("============================================\n");
    printf("          Ethernet Frame Sender\n");
    printf("============================================\n");


    /*======================================================
      SELECT FRAME GENERATION MODE
      ======================================================*/

    printf("\nSelect Frame Generation Mode:\n");

    printf("1. Randomize\n");
    printf("2. Manual Choose\n");

    printf("Enter choice: ");

    scanf("%d",
          &modeChoice);


    while(modeChoice != 1 &&
          modeChoice != 2)
    {
        printf("Invalid choice.\n");

        printf("Enter choice: ");

        scanf("%d",
              &modeChoice);
    }


    /*======================================================
      MANUAL MODE SETTINGS
      ======================================================*/

    manualScheme = 0;

    manualBypass = 0;

    manualInjectError = 0;

    manualErrorChoice = 0;


    if(modeChoice == 2)
    {
        /*--------------------------------------------------
          Manual Error Detection Scheme
          --------------------------------------------------*/

        printf("\n============================================\n");
        printf("       MANUAL ERROR DETECTION SCHEME\n");
        printf("============================================\n");

        printf("\nSelect Error Detection Scheme:\n\n");

        printf("1. Checksum\n");
        printf("2. Checksum (Bypass Detection)\n");
        printf("3. CRC-8\n");
        printf("4. CRC-8 (Bypass Detection)\n");
        printf("5. CRC-10\n");
        printf("6. CRC-10 (Bypass Detection)\n");
        printf("7. CRC-16\n");
        printf("8. CRC-16 (Bypass Detection)\n");
        printf("9. CRC-32\n");
        printf("10. CRC-32 (Bypass Detection)\n");

        printf("\nEnter choice: ");

        scanf("%d",
              &manualScheme);


        while(manualScheme < 1 ||
              manualScheme > 10)
        {
            printf("Invalid choice.\n");

            printf("Enter choice: ");

            scanf("%d",
                  &manualScheme);
        }


        /*
         * Convert the 10 menu choices into:
         *
         * manualScheme = 1 to 5
         * manualBypass = 0 or 1
         */

        if(manualScheme % 2 == 0)
        {
            manualBypass = 1;
        }
        else
        {
            manualBypass = 0;
        }


        if(manualScheme == 1 ||
           manualScheme == 2)
        {
            manualScheme = SCHEME_CHECKSUM;
        }
        else if(manualScheme == 3 ||
                manualScheme == 4)
        {
            manualScheme = SCHEME_CRC8;
        }
        else if(manualScheme == 5 ||
                manualScheme == 6)
        {
            manualScheme = SCHEME_CRC10;
        }
        else if(manualScheme == 7 ||
                manualScheme == 8)
        {
            manualScheme = SCHEME_CRC16;
        }
        else
        {
            manualScheme = SCHEME_CRC32;
        }


        /*--------------------------------------------------
          Manual Error Injection
          --------------------------------------------------*/

        printf("\nInject error into frames?\n");

        printf("1. Yes\n");
        printf("2. No\n");

        printf("Enter choice: ");

        scanf("%d",
              &manualInjectError);


        while(manualInjectError != 1 &&
              manualInjectError != 2)
        {
            printf("Invalid choice.\n");

            printf("Enter choice: ");

            scanf("%d",
                  &manualInjectError);
        }


        if(manualInjectError == 1)
        {
            /*----------------------------------------------
              Manual Error Type
              ----------------------------------------------*/

            printf("\nSelect Error Type:\n");

            printf("1. Single Bit Error\n");
            printf("2. Double Bit Error\n");
            printf("3. Odd Bit Error\n");
            printf("4. Burst Error\n");
            printf("5. Random Error\n");

            printf("Enter choice: ");

            scanf("%d",
                  &manualErrorChoice);


            while(manualErrorChoice < 1 ||
                  manualErrorChoice > 5)
            {
                printf("Invalid choice.\n");

                printf("Enter choice: ");

                scanf("%d",
                      &manualErrorChoice);
            }
        }
    }


    /*------------------------------------------------------
      Display Selected Mode
    ------------------------------------------------------*/

    printf("\n============================================\n");

    if(modeChoice == 1)
    {
        printf("Mode : RANDOMIZE\n");
    }
    else
    {
        printf("Mode : MANUAL CHOOSE\n");
    }

    printf("============================================\n");


    /*------------------------------------------------------
      Get Input File Name
    ------------------------------------------------------*/

    printf("\nEnter input file name: ");

    scanf("%255s",
          filename);


    /*------------------------------------------------------
      Open Input File
    ------------------------------------------------------*/

    fp = openInputFile(filename);

    if(fp == NULL)
    {
#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }


    /*------------------------------------------------------
      Create TCP Socket
    ------------------------------------------------------*/

    sock = socket(AF_INET,
                  SOCK_STREAM,
                  0);


#ifdef _WIN32

    if(sock == INVALID_SOCKET)

#else

    if(sock < 0)

#endif
    {
        printf("Socket creation failed.\n");

        closeInputFile(fp);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }


    /*------------------------------------------------------
      Clear Server Address Structure
    ------------------------------------------------------*/

    memset(&serverAddress,
           0,
           sizeof(serverAddress));


    /*------------------------------------------------------
      Configure Server Address
    ------------------------------------------------------*/

    serverAddress.sin_family =
        AF_INET;

    serverAddress.sin_port =
        htons(SERVER_PORT);


    /*------------------------------------------------------
      Convert IP Address
    ------------------------------------------------------*/

    serverAddress.sin_addr.s_addr =
        inet_addr(SERVER_IP);


    if(serverAddress.sin_addr.s_addr ==
       INADDR_NONE)
    {
        printf("Invalid server IP address.\n");

        CLOSE_SOCKET(sock);

        closeInputFile(fp);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }


    /*------------------------------------------------------
      Connect to Server
    ------------------------------------------------------*/

    printf("\nConnecting to server %s:%d...\n",
           SERVER_IP,
           SERVER_PORT);


    if(connect(sock,
               (struct sockaddr *)&serverAddress,
               sizeof(serverAddress)) < 0)
    {
        printf("Connection failed.\n");

        CLOSE_SOCKET(sock);

        closeInputFile(fp);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }


    printf("Connected to server.\n");


    /*======================================================
      READ FILE AND CREATE FRAMES
      ======================================================*/

    while(getNextPayload(fp,
                         payload,
                         &length))
    {
        /*--------------------------------------------------
          Initialize Frame
          --------------------------------------------------*/

        initializeFrame(&frame);


        /*==================================================
          SELECT FRAME SETTINGS
          ==================================================*/

        if(modeChoice == 1)
        {
            /*
             * RANDOMIZE MODE
             *
             * Every frame independently gets:
             *
             * - Detection scheme
             * - Bypass or normal detection
             * - Error injection
             * - Error type
             */

            choice =
                (rand() % 5) + 1;


            bypassDetection =
                rand() % 2;


            injectError =
                rand() % 2;


            if(injectError == 1)
            {
                errorChoice =
                    (rand() % 5) + 1;
            }
            else
            {
                errorChoice = 0;
            }
        }
        else
        {
            /*
             * MANUAL MODE
             *
             * Use the settings selected
             * before the frame loop.
             */

            choice =
                manualScheme;

            bypassDetection =
                manualBypass;


            if(manualInjectError == 1)
            {
                injectError = 1;

                errorChoice =
                    manualErrorChoice;
            }
            else
            {
                injectError = 0;

                errorChoice = 0;
            }
        }


        /*--------------------------------------------------
          Store Frame Information
          --------------------------------------------------*/

        frameInfo[0] =
            (unsigned char)choice;


        /*
         * Set bit 7 if bypass detection
         * is selected.
         */

        if(bypassDetection == 1)
        {
            frameInfo[0] |= BYPASS_FLAG;
        }


        frameInfo[1] =
            (unsigned char)injectError;


        frameInfo[2] =
            (unsigned char)errorChoice;


        /*--------------------------------------------------
          Source MAC Address
          --------------------------------------------------*/

        unsigned char sourceMAC[MAC_SIZE] =
        {
            0x00,
            0x11,
            0x22,
            0x33,
            0x44,
            0x55
        };


        /*--------------------------------------------------
          Destination MAC Address
          --------------------------------------------------*/

        unsigned char destinationMAC[MAC_SIZE] =
        {
            0x66,
            0x77,
            0x88,
            0x99,
            0xAA,
            0xBB
        };


        /*--------------------------------------------------
          Set Source MAC
          --------------------------------------------------*/

        setSourceMAC(&frame,
                     sourceMAC);


        /*--------------------------------------------------
          Set Destination MAC
          --------------------------------------------------*/

        setDestinationMAC(&frame,
                          destinationMAC);


        /*--------------------------------------------------
          Set Frame Type
          --------------------------------------------------*/

        frame.type = 0x0800;


        /*--------------------------------------------------
          Copy Payload
          --------------------------------------------------*/

        copyPayload(&frame,
                    payload,
                    length);


        /*==================================================
          DISPLAY FRAME INFORMATION
          ==================================================*/

        printf("\n\n============================================\n");

        printf("              FRAME INFORMATION\n");

        printf("============================================\n");


        /*--------------------------------------------------
          Display Detection Scheme
          --------------------------------------------------*/

        printf("Error Detection : ");


        switch(choice)
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


        if(bypassDetection == 1)
        {
            printf(" (BYPASS)");
        }


        printf("\n");


        /*--------------------------------------------------
          Display Error Injection
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
            }


            printf("\n");
        }
        else
        {
            printf("Error Injection : NO\n");

            printf("Error Type      : NONE\n");
        }


        /*==================================================
          NORMAL DETECTION
          ==================================================

          Normal operation:

          1. Generate checksum/CRC.
          2. Inject error.
          3. Receiver compares against original value.

          Therefore an injected error can be detected.
        */

        if(bypassDetection == 0)
        {
            if(choice == SCHEME_CHECKSUM)
            {
                /*------------------------------------------
                  CHECKSUM
                  ------------------------------------------*/

                checksum =
                    calculateFrameChecksum(&frame);


                printf("\nChecksum generated successfully.\n");

                printChecksum(checksum);
            }
            else
            {
                /*------------------------------------------
                  CRC
                  ------------------------------------------*/

                CRCType crcType;


                switch(choice)
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


                generateFrameCRC(&frame,
                                 crcType);


                printf("\nCRC generated successfully.\n");

                printCRC(frame.fcs,
                         crcType);
            }
        }


        /*==================================================
          DISPLAY FRAME BEFORE ERROR
          ==================================================*/

        printf("\n\nFRAME BEFORE ERROR INJECTION\n");

        printFrame(&frame);


        /*==================================================
          ERROR INJECTION
          ==================================================*/

        if(injectError == 1)
        {
            switch(errorChoice)
            {
                case 1:

                    injectSingleBitError(&frame);

                    break;


                case 2:

                    injectDoubleBitError(&frame);

                    break;


                case 3:

                    injectOddBitError(&frame,
                                      5);

                    break;


                case 4:

                    injectBurstError(&frame,
                                     8);

                    break;


                case 5:

                    injectRandomError(&frame);

                    break;
            }


            /*----------------------------------------------
              Display Frame After Error Injection
              ----------------------------------------------*/

            printf("\nFRAME AFTER ERROR INJECTION\n");

            printFrame(&frame);
        }


        /*==================================================
          BYPASS DETECTION
          ==================================================

          In bypass mode, the error is injected FIRST.

          Then the checksum/CRC is generated from the
          already modified frame.

          Therefore the receiver performs its normal
          verification and can report:

              CORRECT

          even though an error was injected.
        */

        if(bypassDetection == 1)
        {
            if(choice == SCHEME_CHECKSUM)
            {
                /*------------------------------------------
                  CHECKSUM AFTER ERROR
                  ------------------------------------------*/

                checksum =
                    calculateFrameChecksum(&frame);


                printf("\nChecksum generated successfully.\n");

                printChecksum(checksum);
            }
            else
            {
                /*------------------------------------------
                  CRC AFTER ERROR
                  ------------------------------------------*/

                CRCType crcType;


                switch(choice)
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


                generateFrameCRC(&frame,
                                 crcType);


                printf("\nCRC generated successfully.\n");

                printCRC(frame.fcs,
                         crcType);
            }
        }


        /*==================================================
          PACK COMPLETE 64-BYTE FRAME
          ==================================================*/

        packFrame(&frame,
                  buffer);


        /*==================================================
          SEND FRAME INFORMATION
          ==================================================*/

        {
            int totalInfoSent = 0;


            while(totalInfoSent < 3)
            {
                int bytesSent;


                bytesSent =
                    send(sock,
                         (const char *)(frameInfo +
                                        totalInfoSent),
                         3 - totalInfoSent,
                         0);


                if(bytesSent <= 0)
                {
                    printf("Frame information transmission failed.\n");

                    CLOSE_SOCKET(sock);

                    closeInputFile(fp);

#ifdef _WIN32
                    WSACleanup();
#endif

                    return 1;
                }


                totalInfoSent += bytesSent;
            }
        }


        printf("\nFrame information sent successfully.\n");


        /*==================================================
          SEND COMPLETE 64-BYTE FRAME
          ==================================================*/

        {
            int totalSent = 0;


            while(totalSent < FRAME_SIZE)
            {
                int bytesSent;


                bytesSent =
                    send(sock,
                         (const char *)(buffer +
                                        totalSent),
                         FRAME_SIZE -
                         totalSent,
                         0);


                if(bytesSent <= 0)
                {
                    printf("Frame transmission failed.\n");

                    CLOSE_SOCKET(sock);

                    closeInputFile(fp);

#ifdef _WIN32
                    WSACleanup();
#endif

                    return 1;
                }


                totalSent += bytesSent;
            }
        }


        printf("\n64-byte frame sent successfully.\n");


        /*==================================================
          SEND CHECKSUM IF CHECKSUM SCHEME WAS SELECTED
          ==================================================*/

        if(choice == SCHEME_CHECKSUM)
        {
            int checksumSent = 0;

            int totalChecksumSent = 0;

            unsigned char checksumBuffer[2];


            checksumBuffer[0] =
                checksum & 0xFF;


            checksumBuffer[1] =
                (checksum >> 8) & 0xFF;


            while(totalChecksumSent < 2)
            {
                checksumSent =
                    send(sock,
                         (const char *)(checksumBuffer +
                                        totalChecksumSent),
                         2 - totalChecksumSent,
                         0);


                if(checksumSent <= 0)
                {
                    printf("Checksum transmission failed.\n");

                    CLOSE_SOCKET(sock);

                    closeInputFile(fp);

#ifdef _WIN32
                    WSACleanup();
#endif

                    return 1;
                }


                totalChecksumSent +=
                    checksumSent;
            }


            printf("2-byte checksum sent successfully.\n");
        }


        /*--------------------------------------------------
          Separator
          --------------------------------------------------*/

        printf("\n--------------------------------------------\n");
    }


    /*======================================================
      CLOSE INPUT FILE
      ======================================================*/

    closeInputFile(fp);


    /*======================================================
      CLOSE SOCKET
      ======================================================*/

    CLOSE_SOCKET(sock);


#ifdef _WIN32

    WSACleanup();

#endif


    /*------------------------------------------------------
      Transmission Completed
      ------------------------------------------------------*/

    printf("\n============================================\n");

    printf("        File transmission completed.\n");

    printf("============================================\n");


    return 0;
}
