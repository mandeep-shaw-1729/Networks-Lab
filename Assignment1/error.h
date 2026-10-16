<<<<<<< HEAD
#ifndef ERROR_H
#define ERROR_H

#include <stdint.h>
#include "frame.h"

void injectSingleBitError(EthernetFrame *frame);

void injectDoubleBitError(EthernetFrame *frame);

void injectOddBitError(EthernetFrame *frame,
                       int numberOfBits);

void injectBurstError(EthernetFrame *frame,
                      int burstLength);

void injectRandomError(EthernetFrame *frame);

=======
#ifndef ERROR_H
#define ERROR_H

#include <stdint.h>
#include "frame.h"

void injectSingleBitError(EthernetFrame *frame);

void injectDoubleBitError(EthernetFrame *frame);

void injectOddBitError(EthernetFrame *frame,
                       int numberOfBits);

void injectBurstError(EthernetFrame *frame,
                      int burstLength);

void injectRandomError(EthernetFrame *frame);

>>>>>>> 586962998533774a857e7ec7799719cff35dce58
#endif