#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>

#define MAC_SIZE        6
#define PAYLOAD_SIZE    44
#define HEADER_SIZE     16
#define FCS_SIZE        4

#define SERIALIZED_SIZE     (HEADER_SIZE + PAYLOAD_SIZE)
#define FRAME_SIZE          (SERIALIZED_SIZE + FCS_SIZE)

typedef struct
{
    uint8_t destinationMAC[MAC_SIZE];

    uint8_t sourceMAC[MAC_SIZE];

    uint16_t length;

    uint16_t type;

    uint8_t payload[PAYLOAD_SIZE];

    uint32_t fcs;

} EthernetFrame;


/* Frame utility functions */

void initializeFrame(EthernetFrame *frame);

void printFrame(const EthernetFrame *frame);

void setSourceMAC(EthernetFrame *frame,
                  const uint8_t mac[MAC_SIZE]);

void setDestinationMAC(EthernetFrame *frame,
                       const uint8_t mac[MAC_SIZE]);

void copyPayload(EthernetFrame *frame,
                 const uint8_t *data,
                 uint16_t length);
int serializeFrame(const EthernetFrame *frame,
                   unsigned char buffer[]);                 
int deserializeFrame(const unsigned char buffer[],
                     EthernetFrame *frame);  
int packFrame(const EthernetFrame *frame,
              unsigned char buffer[]);       
int unpackFrame(const unsigned char buffer[],
                EthernetFrame *frame);                                             
#endif