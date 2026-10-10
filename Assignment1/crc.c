#include <stdio.h>
#include <stdint.h>
#include "crc.h"

/* ============================================================
   Return Polynomial
   ============================================================ */

uint64_t getPolynomial(CRCType type)
{
    switch(type)
    {
        case CRC_8:
            return CRC8_POLY;

        case CRC_10:
            return CRC10_POLY;

        case CRC_16:
            return CRC16_POLY;

        case CRC_32:
            return CRC32_POLY;

        default:
            return CRC32_POLY;
    }
}


/* ============================================================
   Return Degree
   ============================================================ */

int getDegree(CRCType type)
{
    return (int)type;
}


/* ============================================================
   Generic Bitwise CRC Engine
   ============================================================ */

uint32_t calculateCRC(unsigned char *data,
                      int length,
                      uint64_t polynomial,
                      int degree)
{
    uint64_t remainder = 0;
    uint64_t topBit = 1ULL << degree;

    int byte;
    int bit;

    /*
        Process every byte of the message bit-by-bit (MSB to LSB)
    */
    for(byte = 0; byte < length; byte++)
    {
        for(bit = 7; bit >= 0; bit--)
        {
            int b = (data[byte] >> bit) & 1;
            remainder = (remainder << 1) | b;

            if(remainder & topBit)
            {
                remainder ^= polynomial;
            }
        }
    }

    /*
        Simulate appending 'degree' zero bits
    */
    for(bit = 0; bit < degree; bit++)
    {
        remainder <<= 1;

        if(remainder & topBit)
        {
            remainder ^= polynomial;
        }
    }

    /*
        Return only the CRC bits
    */
    if(degree == 32)
    {
        return (uint32_t)(remainder & 0xFFFFFFFF);
    }
    else
    {
        return (uint32_t)(remainder & ((1ULL << degree) - 1));
    }
}
/* ============================================================
   Generate CRC for Ethernet Frame
   ============================================================ */

void generateFrameCRC(EthernetFrame *frame,
                      CRCType type)
{
    unsigned char buffer[SERIALIZED_SIZE];

    int length;

    length = serializeFrame(frame, buffer);

    frame->fcs =
        calculateCRC(buffer,
                     length,
                     getPolynomial(type),
                     getDegree(type));
}


/* ============================================================
   Verify Frame CRC
   ============================================================ */

int verifyFrameCRC(EthernetFrame *frame,
                   CRCType type)
{
    unsigned char buffer[SERIALIZED_SIZE];

    uint32_t calculatedCRC;

    int length;

    length = serializeFrame(frame, buffer);

    calculatedCRC =
        calculateCRC(buffer,
                     length,
                     getPolynomial(type),
                     getDegree(type));

    return (calculatedCRC == frame->fcs);
}


/* ============================================================
   Print CRC Value
   ============================================================ */

void printCRC(uint32_t crc,
              CRCType type)
{
    switch(type)
    {
        case CRC_8:

            printf("CRC-8  : 0x%02X\n",
                    crc & 0xFF);
            break;

        case CRC_10:

            printf("CRC-10 : 0x%03X\n",
                    crc & 0x3FF);
            break;

        case CRC_16:

            printf("CRC-16 : 0x%04X\n",
                    crc & 0xFFFF);
            break;

        case CRC_32:

            printf("CRC-32 : 0x%08X\n",
                    crc);
            break;

        default:

            printf("Unknown CRC\n");
    }
}


/* ============================================================
   Print Generator Polynomial
   ============================================================ */

void printPolynomial(CRCType type)
{
    switch(type)
    {
        case CRC_8:

            printf("Generator Polynomial : ");
            printf("x^8 + x^7 + x^6 + x^4 + x^2 + 1\n");
            break;

        case CRC_10:

            printf("Generator Polynomial : ");
            printf("x^10 + x^9 + x^5 + x^4 + x + 1\n");
            break;

        case CRC_16:

            printf("Generator Polynomial : ");
            printf("x^16 + x^15 + x^2 + 1\n");
            break;

        case CRC_32:

            printf("Generator Polynomial : ");
            printf("Ethernet IEEE 802.3\n");
            printf("x^32 + x^26 + x^23 + x^22 + ");
            printf("x^16 + x^12 + x^11 + x^10 + ");
            printf("x^8 + x^7 + x^5 + x^4 + ");
            printf("x^2 + x + 1\n");
            break;

        default:

            printf("Unknown Polynomial\n");
    }
}
