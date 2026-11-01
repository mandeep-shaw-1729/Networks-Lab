/* csma_common.h - 3-Tier CDMA with Walsh Code Architecture (Senders -> Server -> Receiver) */
#ifndef CSMA_COMMON_H
#define CSMA_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    static inline void SLEEP_US(unsigned int us) {
        LARGE_INTEGER freq, start, now;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        double target = (double)us * (double)freq.QuadPart / 1e6;
        do {
            QueryPerformanceCounter(&now);
        } while ((double)(now.QuadPart - start.QuadPart) < target);
    }
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
    #define SLEEP_US(us) usleep(us)
#endif

#define SERVER_IP          "127.0.0.1"
#define SERVER_PORT        5000

static inline int get_server_port(void) {
    const char *p = getenv("CDMA_PORT");
    if (p && atoi(p) > 0) return atoi(p);
    return SERVER_PORT;
}

#define MAX_STATIONS       64
#define MAX_WALSH_SIZE     64
#define STATION_NAME_LEN   32
#define ACK_MARKER         0xA5
#define CDMA_SLOT_DELAY_US 0  /* 0.5 ms (500 microseconds) delay between CDMA slots */

#define BITS_PER_FRAME     (FRAME_SIZE * 8)          /* 65 bytes * 8 = 520 bits */
#define MAX_CHIPS_FRAME    (BITS_PER_FRAME * MAX_WALSH_SIZE)

typedef enum {
    MSG_REGISTER_RX   = 0x01, /* Receiver -> Server: Register receiver node */
    MSG_RX_INIT       = 0x02, /* Server -> Receiver: [op, num_stations, walsh_N, names...] */
    MSG_REGISTER      = 0x05, /* Sender -> Server: [op, station_name[32]] */
    MSG_ASSIGN_WALSH  = 0x06, /* Server -> Sender: [op, station_id, num_stations, walsh_N] */
    MSG_CDMA_FRAME    = 0x20, /* Sender -> Server: [op, seq_no, encoded_chips...] */
    MSG_COMPOSITE_SIG = 0x30, /* Server -> Receiver: [op, slot_no, summed_chips...] */
    MSG_TX_ACK        = 0xA5, /* Receiver -> Server -> Sender: Slot ACK */
    MSG_STATION_DONE  = 0xFF  /* Station/Server completion marker */
} MsgType;

static inline int recv_exact(sock_t s, unsigned char *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        int r = recv(s, (char *)(buf + got), (int)(len - got), 0);
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}

static inline int send_exact(sock_t s, const unsigned char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int r = send(s, (const char *)(buf + sent), (int)(len - sent), 0);
        if (r <= 0) return -1;
        sent += (size_t)r;
    }
    return 0;
}

/* Compute smallest power of 2 >= n (minimum 2 for Walsh matrix) */
static inline int next_power_of_2(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return (p < 2) ? 2 : p;
}

/* Generate N x N Walsh Matrix using Sylvester's recursive construction:
 * W(1) = [+1]
 * W(2k) = [ W(k)   W(k) ]
 *         [ W(k)  -W(k) ]
 */
static inline void generateWalshTable(int N, int table[MAX_WALSH_SIZE][MAX_WALSH_SIZE]) {
    table[0][0] = 1;
    for (int len = 1; len < N; len <<= 1) {
        for (int i = 0; i < len; i++) {
            for (int j = 0; j < len; j++) {
                table[i][j + len]       =  table[i][j];
                table[i + len][j]       =  table[i][j];
                table[i + len][j + len] = -table[i][j];
            }
        }
    }
}

static inline void printWalshRow(const int row[], int N) {
    printf("[");
    for (int i = 0; i < N; i++) {
        printf("%+d%s", row[i], (i == N - 1) ? "" : ", ");
    }
    printf("]");
}

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
