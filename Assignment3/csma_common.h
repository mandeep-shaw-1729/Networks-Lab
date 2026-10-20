/* csma_common.h - Full Integration of Assignment 1 & 2 Modules with Assignment 3 CSMA/CD */
#ifndef CSMA_COMMON_H
#define CSMA_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* All 5 of your existing Assignment 1 & 2 module headers */
#include "frame.h"
#include "crc.h"
#include "checksum.h"
#include "error.h"
#include "fileio.h"

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET sock_t;
    #define CLOSESOCK(s) closesocket(s)
    #define SLEEP_MS(ms) Sleep(ms)
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/time.h>
    #include <errno.h>
    typedef int sock_t;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR   (-1)
    #define CLOSESOCK(s) close(s)
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

#define SERVER_IP        "127.0.0.1"
#define SERVER_PORT      5000
#define MAX_STATIONS     64
#define STATION_NAME_LEN 32
#define ACK_MARKER       0xA5

/* Shared Channel States required by Assignment 3 */
typedef enum {
    CH_IDLE      = 0,
    CH_BUSY      = 1,
    CH_COLLISION = 2
} ChannelState;

/* CSMA Protocol Modes */
typedef enum {
    PROTO_UNSET          = 0,
    PROTO_NON_PERSISTENT = 1,
    PROTO_1_PERSISTENT   = 2,
    PROTO_P_PERSISTENT   = 3,
    PROTO_CSMA_CD        = 4
} CSMAProtocol;

/* Control Packet Opcodes between Station and Shared Channel */
typedef enum {
    MSG_REGISTER     = 0x05, /* Station -> Channel: Register station name string */
    MSG_PROTO_SYNC   = 0x06, /* Channel -> Station: [0x06, locked_proto] (0 if first sender) */
    MSG_SET_PROTO    = 0x07, /* First Station -> Channel: [0x07, chosen_proto] */
    MSG_SENSE_REQ    = 0x10, /* Station -> Channel: Sense carrier state */
    MSG_SENSE_RESP   = 0x11, /* Channel -> Station: Replies CH_IDLE / CH_BUSY / CH_COLLISION */
    MSG_TX_START     = 0x20, /* Station -> Channel: Transmit 65-byte packed EthernetFrame */
    MSG_TX_ACK       = 0xA5, /* Channel -> Station: 2-byte ACK [0xA5, seqNo] */
    MSG_COLLISION    = 0xFE, /* Channel -> Station: Jamming / Collision detected */
    MSG_STATION_DONE = 0xFF  /* Station -> Channel: All frames from this station sent */
} MsgType;

/* Timing Parameters (ms) for Socket Emulation */
#define SLOT_TIME_MS     25   /* Contention slot time (2 * T_prop) */
#define TX_TIME_MS       80   /* Time to transmit 65-byte frame on medium */
#define JAM_TIME_MS      20   /* Jamming recovery window after collision */
#define MAX_BACKOFF_EXP  10   /* IEEE 802.3 Binary Exponential Backoff cap */
#define MAX_COLLISIONS   16   /* IEEE 802.3 max attempt limit */

/* Helper to get human-readable CSMA protocol name */
static inline const char* getProtocolName(CSMAProtocol proto) {
    switch (proto) {
        case PROTO_NON_PERSISTENT: return "Non-Persistent CSMA";
        case PROTO_1_PERSISTENT:   return "1-Persistent CSMA";
        case PROTO_P_PERSISTENT:   return "p-Persistent CSMA";
        case PROTO_CSMA_CD:        return "CSMA/CD (Collision Detection)";
        default:                   return "Unset";
    }
}

/* Monotonic millisecond timer */
static inline double get_time_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
#endif
}

#endif /* CSMA_COMMON_H */
