
#include <stdio.h>
#include <stdint.h>

#include "checksum.h"
#include "frame.h"


/* ============================================================
   Calculate Generic 16-bit Checksum
   ============================================================ */

uint16_t calculateChecksum(const unsigned char *data,
                           int length)
{
    uint32_t sum = 0;

    int i;


    /*
     * Process adjacent 8-bit bytes into 16-bit words (RFC 1071).
     */
    for(i = 0; i < length; i += 2)
    {
        uint16_t word = ((uint16_t)data[i] << 8);

        if(i + 1 < length)
        {
            word |= data[i + 1];
        }

        sum += word;


        /*
         * Fold carry back into the lower 16 bits.
         */
        if(sum > 0xFFFF)
        {
            sum = (sum & 0xFFFF) +
                  (sum >> 16);
        }
    }


    /*
     * Perform final carry folding.
     */
    while(sum > 0xFFFF)
    {
        sum = (sum & 0xFFFF) +
              (sum >> 16);
    }


    /*
     * Return one's complement.
     */
    return (uint16_t)(~sum);
}


/* ============================================================
   Calculate Checksum for Ethernet Frame
   ============================================================ */

uint16_t calculateFrameChecksum(const EthernetFrame *frame)
{
    unsigned char buffer[SERIALIZED_SIZE];

    int length;


    /*
     * Serialize only the 60-byte portion of the frame.
     *
     * The 4-byte FCS is not included.
     */
    length = serializeFrame(frame,
                            buffer);


    return calculateChecksum(buffer,
                             length);
}


/* ============================================================
   Verify Frame Checksum
   ============================================================ */

int verifyFrameChecksum(const EthernetFrame *frame,
                        uint16_t receivedChecksum)
{
    uint16_t calculatedChecksum;


    calculatedChecksum =
        calculateFrameChecksum(frame);


    if(calculatedChecksum == receivedChecksum)
    {
        return 1;
    }


    return 0;
}


/* ============================================================
   Print Checksum
   ============================================================ */

void printChecksum(uint16_t checksum)
{
    printf("Checksum : 0x%04X\n",
           checksum);
}
