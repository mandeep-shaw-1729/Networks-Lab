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

#endif