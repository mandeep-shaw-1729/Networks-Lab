#include <stdio.h>
#include <string.h>

#include "fileio.h"
#include "frame.h"


/*----------------------------------------------------------
 Open Input File
----------------------------------------------------------*/

FILE *openInputFile(const char *filename)
{
    FILE *fp;

    fp = fopen(filename, "rb");

    if(fp == NULL)
    {
        printf("Unable to open file: %s\n",
               filename);
    }

    return fp;
}


/*----------------------------------------------------------
 Read 44-byte Payload
----------------------------------------------------------*/

int getNextPayload(FILE *fp,
                   unsigned char payload[],
                   uint16_t *length)
{
    int bytesRead;


    /*
       Clear payload before reading.
       This pads the last frame with zeros.
    */
    memset(payload,
           0,
           PAYLOAD_SIZE);


    bytesRead = fread(payload,
                      1,
                      PAYLOAD_SIZE,
                      fp);


    if(bytesRead == 0)
    {
        return 0;
    }


    *length = bytesRead;


    return 1;
}


/*----------------------------------------------------------
 Close File
----------------------------------------------------------*/

void closeInputFile(FILE *fp)
{
    if(fp != NULL)
    {
        fclose(fp);
    }
}