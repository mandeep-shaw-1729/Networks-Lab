
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "frame.h"


/*---------------------------------------------------------
  Initialize all frame fields
---------------------------------------------------------*/
void initializeFrame(EthernetFrame *frame)
{
    memset(frame->sourceMAC, 0, MAC_SIZE);
    memset(frame->destinationMAC, 0, MAC_SIZE);

    frame->length = 0;
    frame->sequenceNumber = 0;

    memset(frame->payload, 0, PAYLOAD_SIZE);

    frame->fcs = 0;
}


/*---------------------------------------------------------
  Set Source MAC Address
---------------------------------------------------------*/
void setSourceMAC(EthernetFrame *frame,
                  const uint8_t mac[MAC_SIZE])
{
    memcpy(frame->sourceMAC, mac, MAC_SIZE);
}


/*---------------------------------------------------------
  Set Destination MAC Address
---------------------------------------------------------*/
void setDestinationMAC(EthernetFrame *frame,
                       const uint8_t mac[MAC_SIZE])
{
    memcpy(frame->destinationMAC, mac, MAC_SIZE);
}


/*---------------------------------------------------------
  Copy Payload into Frame
---------------------------------------------------------*/
void copyPayload(EthernetFrame *frame,
                 const uint8_t *data,
                 uint16_t length)
{
    if(data == NULL)
        length = 0;

    if(length > PAYLOAD_SIZE)
        length = PAYLOAD_SIZE;

    if(length > 0)
    {
        memcpy(frame->payload, data, length);
    }

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
  Print Entire Data Frame
---------------------------------------------------------*/
void printFrame(const EthernetFrame *frame)
{
    int i;
    int paddingBytes;

    /* Validate the frame pointer */
    if(frame == NULL)
        return;

    /* Validate payload length */
    if(frame->length > PAYLOAD_SIZE)
    {
        printf("\nError: Invalid payload length %u\n",
               (unsigned int)frame->length);
        return;
    }

    printf("\n============================================\n");
    printf("              Data Link Frame\n");
    printf("============================================\n");

    printf("Source MAC      : ");
    printMAC(frame->sourceMAC);
    printf("\n");

    printf("Destination MAC : ");
    printMAC(frame->destinationMAC);
    printf("\n");

    printf("Length          : %u Bytes\n",
           (unsigned int)frame->length);

    printf("Sequence Number : %u\n",
           (unsigned int)frame->sequenceNumber);

    /* Print all payload bytes in hexadecimal */
    printf("\nPayload (Hex) (%d Bytes)\n", PAYLOAD_SIZE);
    printf("--------------------------------------------\n");

    for(i = 0; i < PAYLOAD_SIZE; i++)
    {
        printf("%02X ", frame->payload[i]);

        if((i + 1) % 16 == 0)
            printf("\n");
    }

    if(PAYLOAD_SIZE % 16 != 0)
        printf("\n");

    /* Print actual payload text */
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

    /* Print padding information */
    paddingBytes = PAYLOAD_SIZE - frame->length;

    printf("Padding         : %d Bytes\n", paddingBytes);

    if(paddingBytes > 0)
    {
        printf("Padding (Hex)   : ");

        for(i = frame->length; i < PAYLOAD_SIZE; i++)
            printf("%02X ", frame->payload[i]);

        printf("\n");
    }
    else
    {
        printf("Padding (Hex)   : None\n");
    }

    /* Print FCS */
    printf("FCS             : 0x%08X\n",
           (unsigned int)frame->fcs);

    printf("============================================\n");
}



/*---------------------------------------------------------
  Serialize Frame Without FCS
---------------------------------------------------------*/
int serializeFrame(const EthernetFrame *frame,
                   unsigned char buffer[])
{
    int index = 0;


    /* Source MAC: 6 bytes */

    memcpy(buffer + index,
           frame->sourceMAC,
           MAC_SIZE);

    index += MAC_SIZE;


    /* Destination MAC: 6 bytes */

    memcpy(buffer + index,
           frame->destinationMAC,
           MAC_SIZE);

    index += MAC_SIZE;


    /* Length: 2 bytes, Big Endian */

    buffer[index++] =
        (frame->length >> 8) & 0xFF;

    buffer[index++] =
        frame->length & 0xFF;


    /* Frame Sequence Number: 1 byte */

    buffer[index++] = frame->sequenceNumber;


    /* Payload: 46 bytes */

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


    /* Source MAC */

    memcpy(frame->sourceMAC,
           buffer + index,
           MAC_SIZE);

    index += MAC_SIZE;


    /* Destination MAC */

    memcpy(frame->destinationMAC,
           buffer + index,
           MAC_SIZE);

    index += MAC_SIZE;


    /* Length: Big Endian */

    frame->length =
        ((uint16_t)buffer[index] << 8) |
        buffer[index + 1];

    index += 2;


    /* Sequence Number */

    frame->sequenceNumber = buffer[index++];


    /* Payload */

    memcpy(frame->payload,
           buffer + index,
           PAYLOAD_SIZE);

    index += PAYLOAD_SIZE;

    return index;
}


/*---------------------------------------------------------
  Pack Complete Frame Including FCS
---------------------------------------------------------*/
int packFrame(const EthernetFrame *frame,
              unsigned char buffer[])
{
    int index;

    index = serializeFrame(frame, buffer);


    /* FCS: 4 bytes, Big Endian */

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
  Unpack Complete Frame Including FCS
---------------------------------------------------------*/
int unpackFrame(const unsigned char buffer[],
                EthernetFrame *frame)
{
    int index;

    index = deserializeFrame(buffer, frame);


    /* FCS: Big Endian */

    frame->fcs =
        ((uint32_t)buffer[index] << 24) |
        ((uint32_t)buffer[index + 1] << 16) |
        ((uint32_t)buffer[index + 2] << 8) |
        ((uint32_t)buffer[index + 3]);

    index += FCS_SIZE;

    return index;
}
