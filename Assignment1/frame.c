#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "frame.h"


/*---------------------------------------------------------
  Initialize all frame fields
---------------------------------------------------------*/
void initializeFrame(EthernetFrame *frame)
{
    memset(frame->destinationMAC, 0, MAC_SIZE);
    memset(frame->sourceMAC, 0, MAC_SIZE);

    frame->length = 0;
    frame->type = 0;

    memset(frame->payload, 0, PAYLOAD_SIZE);

    frame->fcs = 0;
}


/*---------------------------------------------------------
  Set Source MAC Address
---------------------------------------------------------*/
void setSourceMAC(EthernetFrame *frame,
                  const uint8_t mac[MAC_SIZE])
{
    memcpy(frame->sourceMAC,
           mac,
           MAC_SIZE);
}


/*---------------------------------------------------------
  Set Destination MAC Address
---------------------------------------------------------*/
void setDestinationMAC(EthernetFrame *frame,
                       const uint8_t mac[MAC_SIZE])
{
    memcpy(frame->destinationMAC,
           mac,
           MAC_SIZE);
}


/*---------------------------------------------------------
  Copy Payload into Frame
---------------------------------------------------------*/
void copyPayload(EthernetFrame *frame,
                 const uint8_t *data,
                 uint16_t length)
{
    if(length > PAYLOAD_SIZE)
        length = PAYLOAD_SIZE;

    memcpy(frame->payload,
           data,
           length);

    if(length < PAYLOAD_SIZE)
    {
        memset(frame->payload + length,
               0,
               PAYLOAD_SIZE - length);
    }

    frame->length = length;
}


/*---------------------------------------------------------
  Print MAC Address
---------------------------------------------------------*/
static void printMAC(const uint8_t mac[MAC_SIZE])
{
    int i;

    for(i = 0; i < MAC_SIZE; i++)
    {
        printf("%02X", mac[i]);

        if(i != MAC_SIZE - 1)
            printf(":");
    }
}


/*---------------------------------------------------------
  Print Entire Ethernet Frame
---------------------------------------------------------*/
void printFrame(const EthernetFrame *frame)
{
    int i;
    int paddingBytes;

    printf("\n============================================\n");
    printf("            Ethernet Frame\n");
    printf("============================================\n");

    printf("Destination MAC : ");
    printMAC(frame->destinationMAC);
    printf("\n");

    printf("Source MAC      : ");
    printMAC(frame->sourceMAC);
    printf("\n");

    printf("Length          : %u Bytes\n",
           frame->length);

    printf("Type            : 0x%04X\n",
           frame->type);

    /*
       Calculate padding.
       Actual data = frame->length
       Physical payload = PAYLOAD_SIZE (44 bytes)
    */
    paddingBytes = PAYLOAD_SIZE - frame->length;

    if(paddingBytes < 0)
        paddingBytes = 0;

    /*
       Print ALL 44 payload bytes.
       This includes padding bytes.
    */
    printf("\nPayload (Hex) (44 Bytes)\n");
    printf("--------------------------------------------\n");

    for(i = 0; i < PAYLOAD_SIZE; i++)
    {
        printf("%02X ", frame->payload[i]);

        if((i + 1) % 16 == 0)
            printf("\n");
    }

    if(PAYLOAD_SIZE % 16 != 0)
        printf("\n");

    /*
       Print actual text only.
    */
    printf("Payload (Text)\n");
    printf("--------------------------------------------\n");

    for(i = 0; i < frame->length; i++)
    {
        unsigned char c = frame->payload[i];

        if(c == '\n')
            printf("\\n");
        else if(c == '\r')
            printf("\\r");
        else if(c >= 32 && c <= 126)
            printf("%c", c);
        else
            printf(".");
    }

    printf("\n");

    /*
       Print padding information.
    */
    printf("Padding         : %d Bytes\n",
           paddingBytes);

    if(paddingBytes > 0)
    {
        printf("Padding (Hex)   : ");

        for(i = frame->length;
            i < PAYLOAD_SIZE;
            i++)
        {
            printf("%02X ", frame->payload[i]);
        }

        printf("\n");
    }
    else
    {
        printf("Padding (Hex)   : None\n");
    }

    printf("\nFCS (CRC-32)    : 0x%08X\n",
           frame->fcs);

    printf("============================================\n");
}


/*---------------------------------------------------------
  Serialize Frame Without FCS
---------------------------------------------------------*/
int serializeFrame(const EthernetFrame *frame,
                   unsigned char buffer[])
{
    int index = 0;


    /* Destination MAC */

    memcpy(buffer + index,
           frame->destinationMAC,
           MAC_SIZE);

    index += MAC_SIZE;


    /* Source MAC */

    memcpy(buffer + index,
           frame->sourceMAC,
           MAC_SIZE);

    index += MAC_SIZE;


    /* Length - Big Endian */

    buffer[index++] =
        (frame->length >> 8) & 0xFF;

    buffer[index++] =
        frame->length & 0xFF;


    /* Type - Big Endian */

    buffer[index++] =
        (frame->type >> 8) & 0xFF;

    buffer[index++] =
        frame->type & 0xFF;


    /* Payload */

    memcpy(buffer + index,
           frame->payload,
           PAYLOAD_SIZE);

    index += PAYLOAD_SIZE;


    return index;
}


/*---------------------------------------------------------
  Deserialize Frame Without FCS
---------------------------------------------------------*/
int deserializeFrame(const unsigned char buffer[],
                     EthernetFrame *frame)
{
    int index = 0;


    /* Destination MAC */

    memcpy(frame->destinationMAC,
           buffer + index,
           MAC_SIZE);

    index += MAC_SIZE;


    /* Source MAC */

    memcpy(frame->sourceMAC,
           buffer + index,
           MAC_SIZE);

    index += MAC_SIZE;


    /* Length */

    frame->length =
        ((uint16_t)buffer[index] << 8) |
        buffer[index + 1];

    index += 2;


    /* Type */

    frame->type =
        ((uint16_t)buffer[index] << 8) |
        buffer[index + 1];

    index += 2;


    /* Payload */

    memcpy(frame->payload,
           buffer + index,
           PAYLOAD_SIZE);

    index += PAYLOAD_SIZE;


    return index;
}


/*---------------------------------------------------------
  Pack Complete 64-Byte Frame
---------------------------------------------------------*/
int packFrame(const EthernetFrame *frame,
              unsigned char buffer[])
{
    int index;


    index = serializeFrame(frame,
                           buffer);


    /* FCS */

    buffer[index++] =
        (frame->fcs >> 24) & 0xFF;

    buffer[index++] =
        (frame->fcs >> 16) & 0xFF;

    buffer[index++] =
        (frame->fcs >> 8) & 0xFF;

    buffer[index++] =
        frame->fcs & 0xFF;


    return index;
}


/*---------------------------------------------------------
  Unpack Complete 64-Byte Frame
---------------------------------------------------------*/
int unpackFrame(const unsigned char buffer[],
                EthernetFrame *frame)
{
    int index;


    index = deserializeFrame(buffer,
                             frame);


    /* FCS */

    frame->fcs =
        ((uint32_t)buffer[index] << 24) |
        ((uint32_t)buffer[index + 1] << 16) |
        ((uint32_t)buffer[index + 2] << 8) |
        ((uint32_t)buffer[index + 3]);


    index += 4;


    return index;
}