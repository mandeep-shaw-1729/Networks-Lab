#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdint.h>
#include "frame.h"


/* ============================================================
   Checksum Functions
   ============================================================ */

/*
 * Calculates 16-bit checksum of the given data.
 *
 * Parameters:
 *      data   -> pointer to data
 *      length -> number of bytes
 *
 * Returns:
 *      Calculated 16-bit checksum
 */
uint16_t calculateChecksum(const unsigned char *data,
                           int length);


/*
 * Calculates checksum of an Ethernet frame.
 *
 * The FCS field is not included.
 */
uint16_t calculateFrameChecksum(const EthernetFrame *frame);


/*
 * Verifies checksum of an Ethernet frame.
 *
 * Parameters:
 *      frame          -> Ethernet frame
 *      receivedChecksum -> checksum received/stored separately
 *
 * Returns:
 *      1 -> checksum is correct
 *      0 -> checksum is incorrect
 */
int verifyFrameChecksum(const EthernetFrame *frame,
                        uint16_t receivedChecksum);


/*
 * Prints checksum in hexadecimal.
 */
void printChecksum(uint16_t checksum);

#endif