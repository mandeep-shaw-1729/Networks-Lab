#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "frame.h"
#include "crc.h"
#include "checksum.h"
#include "fileio.h"

#define NUM_TRIALS 200000
#define BENCHMARK_ITERATIONS 5000000

typedef struct {
    unsigned long both_detected;
    unsigned long checksum_only;
    unsigned long crc_only;
    unsigned long neither;
} TestStats;

static void flip_bit(EthernetFrame *frame, int bit_pos) {
    int byte = bit_pos / 8;
    int bit = bit_pos % 8;
    frame->payload[byte] ^= (1 << bit);
}

static void setup_base_frame(EthernetFrame *frame) {
    const uint8_t src[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    const uint8_t dst[6] = {0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB};
    const char *text = "The brown fox jumps over the lazy dog today.";
    
    initializeFrame(frame);
    setSourceMAC(frame, src);
    setDestinationMAC(frame, dst);
    frame->type = 0x0800;
    copyPayload(frame, (const uint8_t *)text, (uint16_t)strlen(text));
}

static void print_stats_table_row(const char *scheme_name, const TestStats *st, unsigned long total) {
    double p_both = (double)st->both_detected * 100.0 / total;
    double p_chk  = (double)st->checksum_only * 100.0 / total;
    double p_crc  = (double)st->crc_only * 100.0 / total;
    double p_none = (double)st->neither * 100.0 / total;
    printf("%-10s | %10lu (%8.4f%%) | %10lu (%8.4f%%) | %10lu (%8.4f%%) | %10lu (%8.4f%%)\n",
           scheme_name,
           st->both_detected, p_both,
           st->checksum_only, p_chk,
           st->crc_only, p_crc,
           st->neither, p_none);
}

void test_single_bit_errors(void) {
    printf("\n========================================================================================\n");
    printf("1. Single Bit Error Evaluation (%d trials per scheme)\n", NUM_TRIALS);
    printf("========================================================================================\n");
    printf("%-10s | %-22s | %-22s | %-22s | %-22s\n",
           "Scheme", "Both Detected", "Checksum Only", "CRC Only", "Neither");
    printf("----------------------------------------------------------------------------------------\n");

    CRCType types[4] = {CRC_8, CRC_10, CRC_16, CRC_32};
    const char *names[4] = {"CRC-8", "CRC-10", "CRC-16", "CRC-32"};
    int total_bits = PAYLOAD_SIZE * 8;

    for (int t = 0; t < 4; t++) {
        CRCType ctype = types[t];
        TestStats st = {0, 0, 0, 0};

        for (int i = 0; i < NUM_TRIALS; i++) {
            EthernetFrame frame;
            setup_base_frame(&frame);
            uint16_t orig_chk = calculateFrameChecksum(&frame);
            generateFrameCRC(&frame, ctype);

            int bit = rand() % total_bits;
            flip_bit(&frame, bit);

            int chk_detected = !verifyFrameChecksum(&frame, orig_chk);
            int crc_detected = !verifyFrameCRC(&frame, ctype);

            if (chk_detected && crc_detected) st.both_detected++;
            else if (chk_detected && !crc_detected) st.checksum_only++;
            else if (!chk_detected && crc_detected) st.crc_only++;
            else st.neither++;
        }
        print_stats_table_row(names[t], &st, NUM_TRIALS);
    }
}

void test_double_bit_errors(void) {
    printf("\n========================================================================================\n");
    printf("2. Isolated Double Bit Error Evaluation (%d trials per scheme)\n", NUM_TRIALS);
    printf("========================================================================================\n");
    printf("%-10s | %-22s | %-22s | %-22s | %-22s\n",
           "Scheme", "Both Detected", "Checksum Only", "CRC Only", "Neither");
    printf("----------------------------------------------------------------------------------------\n");

    CRCType types[4] = {CRC_8, CRC_10, CRC_16, CRC_32};
    const char *names[4] = {"CRC-8", "CRC-10", "CRC-16", "CRC-32"};
    int total_bits = PAYLOAD_SIZE * 8;

    for (int t = 0; t < 4; t++) {
        CRCType ctype = types[t];
        TestStats st = {0, 0, 0, 0};

        for (int i = 0; i < NUM_TRIALS; i++) {
            EthernetFrame frame;
            setup_base_frame(&frame);
            uint16_t orig_chk = calculateFrameChecksum(&frame);
            generateFrameCRC(&frame, ctype);

            int bit1 = rand() % total_bits;
            int bit2;
            do {
                bit2 = rand() % total_bits;
            } while (bit1 == bit2);

            flip_bit(&frame, bit1);
            flip_bit(&frame, bit2);

            int chk_detected = !verifyFrameChecksum(&frame, orig_chk);
            int crc_detected = !verifyFrameCRC(&frame, ctype);

            if (chk_detected && crc_detected) st.both_detected++;
            else if (chk_detected && !crc_detected) st.checksum_only++;
            else if (!chk_detected && crc_detected) st.crc_only++;
            else st.neither++;
        }
        print_stats_table_row(names[t], &st, NUM_TRIALS);
    }
}

void test_odd_bit_errors(void) {
    printf("\n========================================================================================\n");
    printf("3. Odd Number of Errors Evaluation (%d trials per scheme)\n", NUM_TRIALS);
    printf("========================================================================================\n");
    printf("%-10s | %-22s | %-22s | %-22s | %-22s\n",
           "Scheme", "Both Detected", "Checksum Only", "CRC Only", "Neither");
    printf("----------------------------------------------------------------------------------------\n");

    CRCType types[4] = {CRC_8, CRC_10, CRC_16, CRC_32};
    const char *names[4] = {"CRC-8", "CRC-10", "CRC-16", "CRC-32"};
    int total_bits = PAYLOAD_SIZE * 8;

    for (int t = 0; t < 4; t++) {
        CRCType ctype = types[t];
        TestStats st = {0, 0, 0, 0};

        for (int i = 0; i < NUM_TRIALS; i++) {
            EthernetFrame frame;
            setup_base_frame(&frame);
            uint16_t orig_chk = calculateFrameChecksum(&frame);
            generateFrameCRC(&frame, ctype);

            // Random odd count between 1 and 15
            int num_errors = 1 + 2 * (rand() % 8);
            int flipped[17];
            for (int k = 0; k < num_errors; k++) {
                int b;
                int unique;
                do {
                    unique = 1;
                    b = rand() % total_bits;
                    for (int prev = 0; prev < k; prev++) {
                        if (flipped[prev] == b) { unique = 0; break; }
                    }
                } while (!unique);
                flipped[k] = b;
                flip_bit(&frame, b);
            }

            int chk_detected = !verifyFrameChecksum(&frame, orig_chk);
            int crc_detected = !verifyFrameCRC(&frame, ctype);

            if (chk_detected && crc_detected) st.both_detected++;
            else if (chk_detected && !crc_detected) st.checksum_only++;
            else if (!chk_detected && crc_detected) st.crc_only++;
            else st.neither++;
        }
        print_stats_table_row(names[t], &st, NUM_TRIALS);
    }
}

void test_burst_errors(int burst_len) {
    printf("\n========================================================================================\n");
    printf("4. Uniform Burst Error Evaluation: Length L = %d bits (%d trials per scheme)\n", burst_len, NUM_TRIALS);
    printf("========================================================================================\n");
    printf("%-10s | %-22s | %-22s | %-22s | %-22s\n",
           "Scheme", "Both Detected", "Checksum Only", "CRC Only", "Neither");
    printf("----------------------------------------------------------------------------------------\n");

    CRCType types[4] = {CRC_8, CRC_10, CRC_16, CRC_32};
    const char *names[4] = {"CRC-8", "CRC-10", "CRC-16", "CRC-32"};
    int total_bits = PAYLOAD_SIZE * 8;

    for (int t = 0; t < 4; t++) {
        CRCType ctype = types[t];
        TestStats st = {0, 0, 0, 0};

        for (int i = 0; i < NUM_TRIALS; i++) {
            EthernetFrame frame;
            setup_base_frame(&frame);
            uint16_t orig_chk = calculateFrameChecksum(&frame);
            generateFrameCRC(&frame, ctype);

            // Burst model: start uniform in [0, total_bits - burst_len]
            // First and last bits flipped, intermediate bits flipped with p = 0.70
            int start = rand() % (total_bits - burst_len + 1);
            flip_bit(&frame, start);
            for (int b = 1; b < burst_len - 1; b++) {
                if ((rand() % 100) < 70) {
                    flip_bit(&frame, start + b);
                }
            }
            if (burst_len > 1) {
                flip_bit(&frame, start + burst_len - 1);
            }

            int chk_detected = !verifyFrameChecksum(&frame, orig_chk);
            int crc_detected = !verifyFrameCRC(&frame, ctype);

            if (chk_detected && crc_detected) st.both_detected++;
            else if (chk_detected && !crc_detected) st.checksum_only++;
            else if (!chk_detected && crc_detected) st.crc_only++;
            else st.neither++;
        }
        print_stats_table_row(names[t], &st, NUM_TRIALS);
    }
}

static void print_hex_diff_case(const EthernetFrame *f1, const EthernetFrame *f2) {
    printf("Original Frame (Payload 44B):\n  ");
    for (int i = 0; i < PAYLOAD_SIZE; i++) {
        printf("%02X ", f1->payload[i]);
        if ((i + 1) % 16 == 0) printf("\n  ");
    }
    printf("\nCorrupted Frame (Payload 44B):\n  ");
    for (int i = 0; i < PAYLOAD_SIZE; i++) {
        printf("%02X ", f2->payload[i]);
        if ((i + 1) % 16 == 0) printf("\n  ");
    }
    printf("\nDiffering Bytes:\n");
    for (int i = 0; i < PAYLOAD_SIZE; i++) {
        if (f1->payload[i] != f2->payload[i]) {
            printf("  Byte offset %2d: 0x%02X ('%c') -> 0x%02X ('%c') [XOR mask: 0x%02X]\n",
                   i, f1->payload[i], (f1->payload[i] >= 32 && f1->payload[i] <= 126) ? f1->payload[i] : '.',
                   f2->payload[i], (f2->payload[i] >= 32 && f2->payload[i] <= 126) ? f2->payload[i] : '.',
                   f1->payload[i] ^ f2->payload[i]);
        }
    }
}

void search_concrete_collisions(void) {
    printf("\n========================================================================================\n");
    printf("6. Mining Concrete Boundary Collisions (Real Hex Dumps)\n");
    printf("========================================================================================\n");

    int found1 = 0, found2 = 0, found3 = 0;
    int total_bits = PAYLOAD_SIZE * 8;

    for (long it = 0; it < 5000000 && !(found1 && found2 && found3); it++) {
        EthernetFrame f1, f2;
        setup_base_frame(&f1);
        f2 = f1;

        uint16_t chk1 = calculateFrameChecksum(&f1);
        uint32_t crc1 = calculateCRC((unsigned char *)&f1, SERIALIZED_SIZE, CRC8_POLY, 8);

        int mode = it % 3;
        if (mode == 0) {
            flip_bit(&f2, rand() % total_bits);
            if (rand() % 2) flip_bit(&f2, rand() % total_bits);
        } else if (mode == 1) {
            int burst_len = 12;
            int start = rand() % (total_bits - burst_len + 1);
            for (int b = 0; b < burst_len; b++) {
                if (b == 0 || b == burst_len - 1 || (rand() % 100 < 70))
                    flip_bit(&f2, start + b);
            }
        } else {
            int k = 2 + (rand() % 3);
            for (int b = 0; b < k; b++) flip_bit(&f2, rand() % total_bits);
        }

        uint16_t chk2 = calculateFrameChecksum(&f2);
        uint32_t crc2 = calculateCRC((unsigned char *)&f2, SERIALIZED_SIZE, CRC8_POLY, 8);

        int chk_det = (chk1 != chk2);
        int crc_det = (crc1 != crc2);

        if (!found1 && chk_det && crc_det) {
            found1 = 1;
            printf("============================================================\n");
            printf("CASE 1: Detected by BOTH Checksum and CRC-8\n");
            printf("============================================================\n");
            printf("Original Checksum : 0x%04X | Corrupted Checksum: 0x%04X (Detected: YES)\n", chk1, chk2);
            printf("Original CRC-8    : 0x%02X   | Corrupted CRC-8   : 0x%02X   (Detected: YES)\n", crc1, crc2);
            print_hex_diff_case(&f1, &f2);
            printf("\n");
        }

        if (!found2 && chk_det && !crc_det) {
            found2 = 1;
            printf("============================================================\n");
            printf("CASE 2: Detected by Checksum, MISSED by CRC-8 (CRC Collision)\n");
            printf("============================================================\n");
            printf("Original Checksum : 0x%04X | Corrupted Checksum: 0x%04X (Detected: YES)\n", chk1, chk2);
            printf("Original CRC-8    : 0x%02X   | Corrupted CRC-8   : 0x%02X   (COLLISION! Missed)\n", crc1, crc2);
            print_hex_diff_case(&f1, &f2);
            printf("\n");
        }

        if (!found3 && !chk_det && crc_det) {
            found3 = 1;
            printf("============================================================\n");
            printf("CASE 3: Detected by CRC-8, MISSED by Checksum (Compensating Sum)\n");
            printf("============================================================\n");
            printf("Original Checksum : 0x%04X | Corrupted Checksum: 0x%04X (COLLISION! Missed)\n", chk1, chk2);
            printf("Original CRC-8    : 0x%02X   | Corrupted CRC-8   : 0x%02X   (Detected: YES)\n", crc1, crc2);
            print_hex_diff_case(&f1, &f2);
            printf("\n");
        }
    }
}

void benchmark_performance(void) {
    printf("\n========================================================================================\n");
    printf("5. Computational Efficiency and Benchmarks (%d iterations)\n", BENCHMARK_ITERATIONS);
    printf("========================================================================================\n");
    printf("%-16s | %-15s | %-15s | %-15s | %-15s\n",
           "Scheme", "Total Time (s)", "Time / Frame", "Throughput", "Slowdown");
    printf("----------------------------------------------------------------------------------------\n");

    EthernetFrame frame;
    setup_base_frame(&frame);
    unsigned char buffer[SERIALIZED_SIZE];
    int len = serializeFrame(&frame, buffer);

    struct timespec start, end;
    double t_chk = 0.0, t_crc8 = 0.0, t_crc10 = 0.0, t_crc16 = 0.0, t_crc32 = 0.0;
    volatile uint64_t sink = 0;

    // Checksum benchmark
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        sink += calculateChecksum(buffer, len);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_chk = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

    // CRC-8 benchmark
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        sink += calculateCRC(buffer, len, CRC8_POLY, 8);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_crc8 = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

    // CRC-10 benchmark
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        sink += calculateCRC(buffer, len, CRC10_POLY, 10);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_crc10 = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

    // CRC-16 benchmark
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        sink += calculateCRC(buffer, len, CRC16_POLY, 16);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_crc16 = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

    // CRC-32 benchmark
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        sink += calculateCRC(buffer, len, CRC32_POLY, 32);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_crc32 = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;

    double total_bytes = (double)BENCHMARK_ITERATIONS * (double)len;
    double mb = total_bytes / (1024.0 * 1024.0);

    double times[5] = {t_chk, t_crc8, t_crc10, t_crc16, t_crc32};
    const char *names[5] = {"16-Bit Checksum", "CRC-8", "CRC-10", "CRC-16", "CRC-32"};

    for (int i = 0; i < 5; i++) {
        double time_per_frame_ns = (times[i] / BENCHMARK_ITERATIONS) * 1e9;
        double throughput_mb_s = mb / times[i];
        double slowdown = times[i] / t_chk;
        if (i == 0) {
            printf("%-16s | %13.4f s | %11.2f ns | %10.2f MB/s | %10.2fx (Base)\n",
                   names[i], times[i], time_per_frame_ns, throughput_mb_s, slowdown);
        } else {
            printf("%-16s | %13.4f s | %11.2f ns | %10.2f MB/s | %10.2fx slower\n",
                   names[i], times[i], time_per_frame_ns, throughput_mb_s, slowdown);
        }
    }
    (void)sink;
}

int main(void) {
    srand(42); // deterministic seed for reproducibility
    printf("========================================================================================\n");
    printf("        COMPREHENSIVE ERROR DETECTION EVALUATION BENCHMARK SUITE\n");
    printf("        (16-Bit Internet Checksum vs CRC-8, CRC-10, CRC-16, CRC-32)\n");
    printf("========================================================================================\n");

    test_single_bit_errors();
    test_double_bit_errors();
    test_odd_bit_errors();
    test_burst_errors(12);
    test_burst_errors(32);
    search_concrete_collisions();
    benchmark_performance();

    printf("\n========================================================================================\n");
    printf("Benchmark suite completed successfully.\n");
    printf("========================================================================================\n");
    return 0;
}
