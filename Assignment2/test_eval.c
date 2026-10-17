#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#include "frame.h"
#include "crc.h"
#include "checksum.h"
#include "error.h"
#include "fileio.h"

#define ACK_SIZE 2
#define NUM_EVAL_FRAMES 10000

typedef struct {
    unsigned long total_sent;
    unsigned long original_delivered;
    unsigned long retransmissions;
    unsigned long corrupted_discarded;
    unsigned long timeouts;
    double efficiency;
    double avg_delay_ms;
} EvalStats;

/* Helper pseudo-random float [0.0, 1.0) */
static double rand_double(void) {
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

/* -------------------------------------------------------------
 * Simulation of Stop-and-Wait ARQ
 * ------------------------------------------------------------- */
static EvalStats simulate_stop_and_wait(int total_frames, double error_prob, double prop_delay_ms) {
    EvalStats st = {0};
    int current_frame = 0;
    double t_trans_data = 0.5; // ms
    double t_trans_ack = 0.1;  // ms
    double t_proc = 0.05;      // ms
    double rtt = 2 * prop_delay_ms + t_trans_data + t_trans_ack + t_proc;
    double timeout = 2.5 * rtt;
    double total_sim_time = 0.0;

    while (current_frame < total_frames) {
        int acked = 0;
        int attempts = 0;

        while (!acked && attempts < 10) {
            st.total_sent++;
            attempts++;

            /* Channel error: packet corrupted or dropped */
            int data_corrupted = (rand_double() < error_prob);
            /* Channel delay/loss for ACK */
            int ack_lost = (rand_double() < (error_prob * 0.5)); // ACK loss probability

            if (data_corrupted) {
                st.corrupted_discarded++;
                st.timeouts++;
                total_sim_time += timeout;
            } else if (ack_lost) {
                st.timeouts++;
                total_sim_time += timeout;
            } else {
                acked = 1;
                total_sim_time += rtt;
            }

            if (!acked) {
                st.retransmissions++;
            }
        }
        current_frame++;
        st.original_delivered++;
    }

    st.efficiency = (double)st.original_delivered / (double)st.total_sent * 100.0;
    st.avg_delay_ms = total_sim_time / st.original_delivered;
    return st;
}

/* -------------------------------------------------------------
 * Simulation of Go-Back-N ARQ
 * ------------------------------------------------------------- */
static EvalStats simulate_go_back_n(int total_frames, int window_size, double error_prob, double prop_delay_ms) {
    EvalStats st = {0};
    int base = 0;
    int next_seq = 0;
    double t_trans_data = 0.5;
    double t_trans_ack = 0.1;
    double t_proc = 0.05;
    double rtt = 2 * prop_delay_ms + t_trans_data + t_trans_ack + t_proc;
    double timeout = 2.0 * rtt;
    double total_sim_time = 0.0;

    while (base < total_frames) {
        /* Send frames up to window */
        int sent_in_burst = 0;
        while (next_seq < total_frames && next_seq < base + window_size) {
            st.total_sent++;
            next_seq++;
            sent_in_burst++;
        }

        total_sim_time += sent_in_burst * t_trans_data;

        /* Evaluate status of base frame */
        int data_corrupted = (rand_double() < error_prob);
        int ack_lost = (rand_double() < (error_prob * 0.5));

        if (!data_corrupted && !ack_lost) {
            /* Successful base delivery and cumulative ACK */
            base++;
            st.original_delivered++;
            total_sim_time += prop_delay_ms + t_trans_ack + t_proc;
        } else {
            /* Timeout on base frame: all frames from base to next_seq-1 must be retransmitted */
            st.timeouts++;
            if (data_corrupted) st.corrupted_discarded++;
            int dropped_window = (next_seq - base);
            st.retransmissions += dropped_window;
            total_sim_time += timeout;
            /* Go-Back-N: reset next_seq to base */
            next_seq = base;
        }
    }

    st.efficiency = (double)total_frames / (double)st.total_sent * 100.0;
    st.avg_delay_ms = total_sim_time / total_frames;
    return st;
}

/* -------------------------------------------------------------
 * Simulation of Selective Repeat ARQ
 * ------------------------------------------------------------- */
static EvalStats simulate_selective_repeat(int total_frames, int window_size, double error_prob, double prop_delay_ms) {
    EvalStats st = {0};
    int base = 0;
    int next_seq = 0;
    char *acked = (char *)calloc(total_frames, sizeof(char));
    double t_trans_data = 0.5;
    double t_trans_ack = 0.1;
    double t_proc = 0.05;
    double rtt = 2 * prop_delay_ms + t_trans_data + t_trans_ack + t_proc;
    double timeout = 2.0 * rtt;
    double total_sim_time = 0.0;

    while (base < total_frames) {
        /* Send new frames in window */
        int sent_count = 0;
        while (next_seq < total_frames && next_seq < base + window_size) {
            st.total_sent++;
            int data_corrupted = (rand_double() < error_prob);
            int ack_lost = (rand_double() < (error_prob * 0.5));

            if (!data_corrupted && !ack_lost) {
                acked[next_seq] = 1;
            } else {
                acked[next_seq] = 0;
                if (data_corrupted) st.corrupted_discarded++;
            }
            next_seq++;
            sent_count++;
        }

        total_sim_time += sent_count * t_trans_data;

        /* Slide window past acknowledged frames */
        while (base < total_frames && acked[base]) {
            base++;
            st.original_delivered++;
        }

        /* If unacknowledged frames remain in the window, retransmit ONLY them */
        if (base < next_seq) {
            for (int k = base; k < next_seq; k++) {
                if (!acked[k]) {
                    st.total_sent++;
                    st.retransmissions++;
                    st.timeouts++;
                    total_sim_time += timeout;

                    int retry_corrupted = (rand_double() < error_prob);
                    int retry_ack_lost = (rand_double() < (error_prob * 0.5));
                    if (!retry_corrupted && !retry_ack_lost) {
                        acked[k] = 1;
                    } else if (retry_corrupted) {
                        st.corrupted_discarded++;
                    }
                }
            }

            while (base < total_frames && acked[base]) {
                base++;
                st.original_delivered++;
            }
        }
    }

    free(acked);
    st.efficiency = (double)total_frames / (double)st.total_sent * 100.0;
    st.avg_delay_ms = total_sim_time / total_frames;
    return st;
}

int main(void) {
    srand(123456); // Deterministic seed for reproducible evaluation

    printf("========================================================================================\n");
    printf("ASSIGNMENT 2 FLOW CONTROL BENCHMARK EVALUATION SUITE\n");
    printf("========================================================================================\n\n");

    /* TEST CASE 1: Propagation Time vs ACK Reception Time (RTT & Delay Breakdown) */
    printf("----------------------------------------------------------------------------------------\n");
    printf("TEST CASE 1: Packet Propagation Time vs. ACK Reception Time (RTT Dynamics)\n");
    printf("----------------------------------------------------------------------------------------\n");
    printf("%-12s | %-12s | %-12s | %-12s | %-12s | %-12s\n",
           "Payload (B)", "T_prop (ms)", "T_tx_frame", "T_tx_ack", "RTT (ms)", "Timeout (ms)");
    printf("----------------------------------------------------------------------------------------\n");

    int payloads[] = {46, 128, 512, 1024, 1500};
    double prop_delays[] = {5.0, 10.0, 20.0, 50.0};
    double bandwidth_mbps = 10.0; // 10 Mbps channel

    for (int p = 0; p < 5; p++) {
        int payload_len = payloads[p];
        int frame_bytes = HEADER_SIZE + payload_len + FCS_SIZE;
        double t_tx_frame = (frame_bytes * 8.0) / (bandwidth_mbps * 1000.0); // ms
        double t_tx_ack = (ACK_SIZE * 8.0) / (bandwidth_mbps * 1000.0);     // ms
        double t_proc = 0.05; // 50 us processing time

        for (int d = 0; d < 4; d++) {
            double tp = prop_delays[d];
            double rtt = 2 * tp + t_tx_frame + t_tx_ack + t_proc;
            double to = 2.0 * rtt + 5.0; // Dynamic timeout calculation
            printf("%-12d | %-12.1f | %-12.4f | %-12.4f | %-12.3f | %-12.3f\n",
                   payload_len, tp, t_tx_frame, t_tx_ack, rtt, to);
        }
        printf("----------------------------------------------------------------------------------------\n");
    }

    /* TEST CASE 2: Efficiency in Error-Free (Noiseless) Channel */
    printf("\n----------------------------------------------------------------------------------------\n");
    printf("TEST CASE 2: Protocol Efficiency without Error or Lost Frame (p = 0.0, 10,000 frames)\n");
    printf("----------------------------------------------------------------------------------------\n");
    printf("%-20s | %-10s | %-12s | %-12s | %-14s | %-12s\n",
           "Protocol Scheme", "Window (N)", "Delivered", "Transmitted", "Efficiency (%)", "Avg Delay (ms)");
    printf("----------------------------------------------------------------------------------------\n");

    EvalStats snw_ideal = simulate_stop_and_wait(NUM_EVAL_FRAMES, 0.0, 10.0);
    printf("%-20s | %-10d | %-12lu | %-12lu | %-14.2f | %-12.3f\n",
           "Stop-and-Wait", 1, snw_ideal.original_delivered, snw_ideal.total_sent, snw_ideal.efficiency, snw_ideal.avg_delay_ms);

    int windows[] = {2, 4, 8, 16};
    for (int w = 0; w < 4; w++) {
        int N = windows[w];
        EvalStats gbn_ideal = simulate_go_back_n(NUM_EVAL_FRAMES, N, 0.0, 10.0);
        printf("%-20s | %-10d | %-12lu | %-12lu | %-14.2f | %-12.3f\n",
               "Go-Back-N ARQ", N, gbn_ideal.original_delivered, gbn_ideal.total_sent, gbn_ideal.efficiency, gbn_ideal.avg_delay_ms);
    }
    printf("----------------------------------------------------------------------------------------\n");
    for (int w = 0; w < 4; w++) {
        int N = windows[w];
        EvalStats sr_ideal = simulate_selective_repeat(NUM_EVAL_FRAMES, N, 0.0, 10.0);
        printf("%-20s | %-10d | %-12lu | %-12lu | %-14.2f | %-12.3f\n",
               "Selective Repeat ARQ", N, sr_ideal.original_delivered, sr_ideal.total_sent, sr_ideal.efficiency, sr_ideal.avg_delay_ms);
    }

    /* TEST CASE 3: Efficiency under Error Probabilities (p = 0.1 to 0.5) */
    printf("\n----------------------------------------------------------------------------------------\n");
    printf("TEST CASE 3: Efficiency vs. Error Probability (p = 0.1 to 0.5, 10,000 frames per run)\n");
    printf("----------------------------------------------------------------------------------------\n");
    printf("%-6s | %-18s | %-18s | %-18s | %-18s | %-18s\n",
           "Prob", "Stop-and-Wait (%)", "Go-Back-N (N=4)", "Go-Back-N (N=8)", "Sel-Repeat (N=4)", "Sel-Repeat (N=8)");
    printf("----------------------------------------------------------------------------------------\n");

    double probs[] = {0.10, 0.20, 0.30, 0.40, 0.50};
    for (int i = 0; i < 5; i++) {
        double p = probs[i];
        EvalStats snw = simulate_stop_and_wait(NUM_EVAL_FRAMES, p, 10.0);
        EvalStats gbn4 = simulate_go_back_n(NUM_EVAL_FRAMES, 4, p, 10.0);
        EvalStats gbn8 = simulate_go_back_n(NUM_EVAL_FRAMES, 8, p, 10.0);
        EvalStats sr4 = simulate_selective_repeat(NUM_EVAL_FRAMES, 4, p, 10.0);
        EvalStats sr8 = simulate_selective_repeat(NUM_EVAL_FRAMES, 8, p, 10.0);

        printf("%-6.2f | %12.2f%%     | %12.2f%%     | %12.2f%%     | %12.2f%%     | %12.2f%%\n",
               p, snw.efficiency, gbn4.efficiency, gbn8.efficiency, sr4.efficiency, sr8.efficiency);
    }

    /* TEST CASE 3B: Retransmission Overhead & Discards */
    printf("\n----------------------------------------------------------------------------------------\n");
    printf("DETAILED BREAKDOWN: Retransmissions & Timeouts across Error Probabilities (10,000 frames)\n");
    printf("----------------------------------------------------------------------------------------\n");
    printf("%-6s | %-16s | %-10s | %-12s | %-12s | %-12s | %-10s\n",
           "Prob", "Scheme", "Window", "Total Tx", "Retransmits", "Timeouts", "Efficiency");
    printf("----------------------------------------------------------------------------------------\n");

    for (int i = 0; i < 5; i++) {
        double p = probs[i];
        EvalStats snw = simulate_stop_and_wait(NUM_EVAL_FRAMES, p, 10.0);
        EvalStats gbn4 = simulate_go_back_n(NUM_EVAL_FRAMES, 4, p, 10.0);
        EvalStats sr4 = simulate_selective_repeat(NUM_EVAL_FRAMES, 4, p, 10.0);

        printf("%-6.2f | %-16s | %-10d | %-12lu | %-12lu | %-12lu | %8.2f%%\n",
               p, "Stop-and-Wait", 1, snw.total_sent, snw.retransmissions, snw.timeouts, snw.efficiency);
        printf("%-6.2f | %-16s | %-10d | %-12lu | %-12lu | %-12lu | %8.2f%%\n",
               p, "Go-Back-N", 4, gbn4.total_sent, gbn4.retransmissions, gbn4.timeouts, gbn4.efficiency);
        printf("%-6.2f | %-16s | %-10d | %-12lu | %-12lu | %-12lu | %8.2f%%\n",
               p, "Selective Repeat", 4, sr4.total_sent, sr4.retransmissions, sr4.timeouts, sr4.efficiency);
        printf("----------------------------------------------------------------------------------------\n");
    }

    return 0;
}
