#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "error.h"
#include "frame.h"

/*----------------------------------------------------------
 Flip a bit inside the PAYLOAD only
----------------------------------------------------------*/
static void flipPayloadBit(EthernetFrame *frame,
                           int bitPosition)
{
    int byte;
    int bit;

    byte = bitPosition / 8;
    bit = bitPosition % 8;

    frame->payload[byte] ^= (1 << bit);
}

/*----------------------------------------------------------
 Single Bit Error
----------------------------------------------------------*/
void injectSingleBitError(EthernetFrame *frame)
{
    int totalBits;
    int bit;

    /*
       Only the actual payload is modified.
       We use PAYLOAD_SIZE rather than SERIALIZED_SIZE.
    */
    totalBits = PAYLOAD_SIZE * 8;

    bit = rand() % totalBits;

    flipPayloadBit(frame, bit);

    printf("\nSingle Bit Error Injected.\n");
}

/*----------------------------------------------------------
 Double Bit Error
----------------------------------------------------------*/
void injectDoubleBitError(EthernetFrame *frame)
{
    int totalBits;
    int bit1;
    int bit2;

    totalBits = PAYLOAD_SIZE * 8;

    bit1 = rand() % totalBits;

    do
    {
        bit2 = rand() % totalBits;
    }
    while(bit1 == bit2);

    flipPayloadBit(frame, bit1);
    flipPayloadBit(frame, bit2);

    printf("\nDouble Bit Error Injected.\n");
}

/*----------------------------------------------------------
 Odd Bit Error
----------------------------------------------------------*/
void injectOddBitError(EthernetFrame *frame,
                       int numberOfBits)
{
    int totalBits;
    int i;
    int bit;

    /*
       Make sure the number of flipped bits is odd.
    */
    if(numberOfBits % 2 == 0)
        numberOfBits++;

    totalBits = PAYLOAD_SIZE * 8;

    for(i = 0; i < numberOfBits; i++)
    {
        bit = rand() % totalBits;

        flipPayloadBit(frame, bit);
    }

    printf("\nOdd Bit Error (%d bits) Injected.\n",
           numberOfBits);
}

/*----------------------------------------------------------
 Burst Error
----------------------------------------------------------*/
void injectBurstError(EthernetFrame *frame,
                      int burstLength)
{
    int totalBits;
    int start;
    int i;

    totalBits = PAYLOAD_SIZE * 8;

    if(burstLength > totalBits)
        burstLength = totalBits;

    start = rand() %
            (totalBits - burstLength + 1);

    for(i = 0; i < burstLength; i++)
    {
        flipPayloadBit(frame,
                       start + i);
    }

    printf("\nBurst Error (%d bits) Injected.\n",
           burstLength);
}

/*----------------------------------------------------------
 Random Error
----------------------------------------------------------*/
void injectRandomError(EthernetFrame *frame)
{
    int choice;

    choice = rand() % 4;

    switch(choice)
    {
        case 0:
            injectSingleBitError(frame);
            break;

        case 1:
            injectDoubleBitError(frame);
            break;

        case 2:
            injectOddBitError(frame, 5);
            break;

        case 3:
            injectBurstError(frame, 8);
            break;
    }
}