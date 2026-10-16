<<<<<<< HEAD
#ifndef FILEIO_H
#define FILEIO_H

#include <stdio.h>
#include <stdint.h>


/*----------------------------------------------------------
 File Handling Functions
----------------------------------------------------------*/

/*
 * Opens a file for reading.
 *
 * Returns:
 *      File pointer if successful
 *      NULL if failed
 */
FILE *openInputFile(const char *filename);


/*
 * Reads next 44-byte payload from file.
 *
 * Parameters:
 *      fp      -> opened file
 *      payload -> payload buffer
 *      length  -> actual bytes read
 *
 * Returns:
 *      1 -> payload successfully read
 *      0 -> end of file
 */
int getNextPayload(FILE *fp,
                   unsigned char payload[],
                   uint16_t *length);


/*
 * Closes opened file.
 */
void closeInputFile(FILE *fp);


=======
#ifndef FILEIO_H
#define FILEIO_H

#include <stdio.h>
#include <stdint.h>


/*----------------------------------------------------------
 File Handling Functions
----------------------------------------------------------*/

/*
 * Opens a file for reading.
 *
 * Returns:
 *      File pointer if successful
 *      NULL if failed
 */
FILE *openInputFile(const char *filename);


/*
 * Reads next 44-byte payload from file.
 *
 * Parameters:
 *      fp      -> opened file
 *      payload -> payload buffer
 *      length  -> actual bytes read
 *
 * Returns:
 *      1 -> payload successfully read
 *      0 -> end of file
 */
int getNextPayload(FILE *fp,
                   unsigned char payload[],
                   uint16_t *length);


/*
 * Closes opened file.
 */
void closeInputFile(FILE *fp);


>>>>>>> 586962998533774a857e7ec7799719cff35dce58
#endif