#ifndef CRC_H
#define CRC_H

#include <stdint.h>
#include "frame.h"

/* ============================================================
   CRC Generator Polynomials
   ============================================================ */

/* CRC-8  : x^8 + x^7 + x^6 + x^4 + x^2 + 1 */
#define CRC8_POLY   0x1D5ULL

/* CRC-10 : x^10 + x^9 + x^5 + x^4 + x + 1 */
#define CRC10_POLY  0x633ULL

/* CRC-16 : x^16 + x^15 + x^2 + 1 */
#define CRC16_POLY  0x18005ULL

/* CRC-32 : Ethernet IEEE 802.3 */
#define CRC32_POLY  0x104C11DB7ULL


/* ============================================================
   CRC Types
   ============================================================ */

typedef enum
{
    CRC_8 = 8,
    CRC_10 = 10,
    CRC_16 = 16,
    CRC_32 = 32

} CRCType;


/* ============================================================
   Generic CRC Functions
   ============================================================ */

/*
 * Calculates CRC of arbitrary data.
 *
 * Parameters:
 *      data        -> pointer to data
 *      length      -> number of bytes
 *      polynomial  -> generator polynomial
 *      degree      -> CRC size (8,10,16,32)
 *
 * Returns:
 *      Calculated CRC value
 */
uint32_t calculateCRC(unsigned char *data,
                      int length,
                      uint64_t polynomial,
                      int degree);


/*
 * Returns polynomial corresponding to CRC type.
 */
uint64_t getPolynomial(CRCType type);


/*
 * Returns CRC degree.
 */
int getDegree(CRCType type);


/* ============================================================
   Ethernet Frame CRC Functions
   ============================================================ */

/*
 * Generates CRC for a frame and stores it in frame->fcs.
 */
void generateFrameCRC(EthernetFrame *frame,
                      CRCType type);


/*
 * Verifies frame CRC.
 *
 * Returns:
 *      1 -> CRC correct
 *      0 -> CRC incorrect
 */
int verifyFrameCRC(EthernetFrame *frame,
                   CRCType type);


/* ============================================================
   Display Utilities
   ============================================================ */

/*
 * Prints CRC value in hexadecimal.
 */
void printCRC(uint32_t crc,
              CRCType type);


/*
 * Prints generator polynomial.
 */
void printPolynomial(CRCType type);

#endif