/* csma_common.h - 3-Tier CSMA/CD Architecture (Senders -> Server -> Receiver) */
#ifndef CSMA_COMMON_H
#define CSMA_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Frame and File I/O modules */
#include "frame.h"
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

/* Shared Channel States */
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

/* Control Packet Opcodes between Senders, Channel Server, and Receiver */
typedef enum {
    MSG_REGISTER_RX     = 0x01, /* Receiver -> Server: Register destination receiver */
    MSG_REGISTER        = 0x05, /* Sender -> Server: Register station name string */
    MSG_PROTO_SYNC      = 0x06, /* Server -> Sender: [0x06, locked_proto] (0 if first sender) */
    MSG_SET_PROTO       = 0x07, /* First Sender -> Server: [0x07, chosen_proto] */
    MSG_SENSE_REQ       = 0x10, /* Sender -> Server: Sense carrier state */
    MSG_SENSE_RESP      = 0x11, /* Server -> Sender: Replies CH_IDLE / CH_BUSY */
    MSG_TX_START        = 0x20, /* Sender -> Server: Transmit 65-byte packed EthernetFrame */
    MSG_FWD_FRAME_NOACK = 0x30, /* Server -> Receiver: Forward frame (No ACK, pure CSMA) */
    MSG_FWD_FRAME_ACK   = 0x31, /* Server -> Receiver: Forward frame & request ACK (CSMA/CD only) */
    MSG_TX_ACK          = 0xA5, /* Receiver -> Server -> Sender: 2-byte ACK [0xA5, seqNo] (CSMA/CD only) */
    MSG_COLLISION       = 0xFE, /* Server -> Sender: Jamming signal (CSMA/CD only) */
    MSG_STATION_DONE    = 0xFF  /* Sender -> Server / Server -> Receiver: All transmissions complete */
} MsgType;

/* Timing Parameters (ms) for Socket Emulation */
#define SLOT_TIME_MS     25   /* Contention slot time (2 * T_prop) */
#define TX_TIME_MS       80   /* Time to transmit 65-byte frame on medium */
#define JAM_TIME_MS      20   /* Jamming recovery window (CSMA/CD only) */
#define MAX_BACKOFF_EXP  10   /* IEEE 802.3 Binary Exponential Backoff cap */
#define MAX_COLLISIONS   16   /* IEEE 802.3 max attempt limit */

/* Helper to guarantee full TCP stream reads */
static inline int recv_exact(sock_t s, unsigned char *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        int r = recv(s, (char *)(buf + got), (int)(len - got), 0);
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}

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
