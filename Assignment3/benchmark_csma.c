/* benchmark_csma.c - Generates CSV datasets for Assignment 3 Test Cases (i) and (ii) */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "csma_common.h"

#define TARGET_FRAMES 2000
#define TX_SLOTS      10    /* A successful 65-byte frame takes 10 propagation slots */
#define JAM_SLOTS     2     /* CSMA/CD aborts after 2 slots (1 RTT + jam) instead of 10 */

typedef struct {
    int    ready;
    int    backoff;
    int    attempts;
    double arrival_slot;
} SimStation;

static void run_simulation(CSMAProtocol proto, int N, double p_prob,
                           int *out_collisions, double *out_avg_delay_ms, double *out_throughput) {
    SimStation st[MAX_STATIONS];
    memset(st, 0, sizeof(st));

    int delivered = 0;
    int collisions = 0;
    long total_slots = 0;
    double total_delay_slots = 0.0;
    double slot_duration_ms = 0.5; /* 0.5 ms per slot time */

    srand(1000 + N * 37 + (int)(p_prob * 100));

    /* Saturated/heavy-load arrival model: stations always have frames queued */
    for (int i = 0; i < N; i++) {
        st[i].ready = 1;
        st[i].backoff = 0;
        st[i].attempts = 0;
        st[i].arrival_slot = 0.0;
    }

    while (delivered < TARGET_FRAMES) {
        int tx_count = 0;
        int last_tx = -1;

        /* Each station senses the IDLE channel at slot boundary */
        for (int i = 0; i < N; i++) {
            if (st[i].backoff > 0) {
                st[i].backoff--;
                continue;
            }
            if (proto == PROTO_P_PERSISTENT) {
                double r = (double)rand() / (double)RAND_MAX;
                if (r <= p_prob) {
                    tx_count++;
                    last_tx = i;
                }
            } else {
                tx_count++;
                last_tx = i;
            }
        }

        if (tx_count == 0) {
            /* Channel IDLE for 1 slot */
            total_slots += 1;
        }
        else if (tx_count == 1) {
            /* Successful transmission: channel BUSY for TX_SLOTS */
            total_slots += TX_SLOTS;
            delivered++;
            total_delay_slots += (total_slots - st[last_tx].arrival_slot);

            /* Station queues next frame */
            st[last_tx].attempts = 0;
            st[last_tx].arrival_slot = (double)total_slots;
            if (proto == PROTO_NON_PERSISTENT) {
                st[last_tx].backoff = rand() % 4;
            } else {
                st[last_tx].backoff = 0;
            }
        }
        else {
            /* Collision: 2 or more stations transmitted in the same vulnerability slot */
            collisions++;
            int wasted_slots = (proto == PROTO_CSMA_CD) ? JAM_SLOTS : TX_SLOTS;
            total_slots += wasted_slots;

            for (int i = 0; i < N; i++) {
                if (st[i].backoff == 0) {
                    st[i].attempts++;
                    if (proto == PROTO_NON_PERSISTENT) {
                        /* Non-persistent draws a random wait and re-senses */
                        st[i].backoff = 1 + (rand() % (4 * N));
                    } else if (proto == PROTO_1_PERSISTENT) {
                        /* 1-persistent suffers synchronized post-busy collisions unless backed off */
                        int exp = (st[i].attempts < 6) ? st[i].attempts : 6;
                        st[i].backoff = rand() % (1 << exp);
                    } else {
                        /* Binary Exponential Backoff */
                        int exp = (st[i].attempts < 10) ? st[i].attempts : 10;
                        st[i].backoff = rand() % (1 << exp);
                    }
                }
            }
        }
    }

    *out_collisions   = collisions;
    *out_avg_delay_ms = (total_delay_slots / TARGET_FRAMES) * slot_duration_ms;
    *out_throughput   = (double)(TARGET_FRAMES * TX_SLOTS) / (double)total_slots;
}

int main(void) {
    /* TEST CASE (i): p-Persistent CSMA as a function of p (Fixed N = 10) */
    FILE *f1 = fopen("case1_p_persistent.csv", "w");
    fprintf(f1, "p,Collisions,AvgDelay_ms,Throughput\n");
    printf("=== Test Case (i): p-Persistent CSMA (Fixed N = 10) ===\n");
    printf("   p   | Collisions | Avg Delay (ms) | Normalized Throughput\n");
    printf("-------+------------+----------------+----------------------\n");

    double p_vals[] = {0.05, 0.10, 0.15, 0.20, 0.30, 0.40, 0.50, 0.60, 0.70, 0.80, 0.90, 1.00};
    int num_p = sizeof(p_vals) / sizeof(p_vals[0]);
    for (int i = 0; i < num_p; i++) {
        int col = 0; double dly = 0, thr = 0;
        run_simulation(PROTO_P_PERSISTENT, 10, p_vals[i], &col, &dly, &thr);
        fprintf(f1, "%.2f,%d,%.3f,%.4f\n", p_vals[i], col, dly, thr);
        printf("  %.2f |   %6d   |     %7.2f    |        %.4f\n", p_vals[i], col, dly, thr);
    }
    fclose(f1);

    /* TEST CASE (ii): All 4 Schemes as a function of N */
    FILE *f2 = fopen("case2_all_schemes.csv", "w");
    fprintf(f2, "N,NonPers_Col,1Pers_Col,pPers_Col,CSMACD_Col,"
                "NonPers_Delay,1Pers_Delay,pPers_Delay,CSMACD_Delay,"
                "NonPers_Thr,1Pers_Thr,pPers_Thr,CSMACD_Thr\n");

    int N_vals[] = {2, 4, 8, 12, 16, 20, 25, 30};
    int num_N = sizeof(N_vals) / sizeof(N_vals[0]);
    printf("\n=== Test Case (ii): All 4 CSMA Schemes vs. Contending Stations (N) ===\n");

    for (int i = 0; i < num_N; i++) {
        int N = N_vals[i];
        int c_np, c_1p, c_pp, c_cd;
        double d_np, d_1p, d_pp, d_cd;
        double t_np, t_1p, t_pp, t_cd;

        run_simulation(PROTO_NON_PERSISTENT, N, 0.0,    &c_np, &d_np, &t_np);
        run_simulation(PROTO_1_PERSISTENT,   N, 1.0,    &c_1p, &d_1p, &t_1p);
        run_simulation(PROTO_P_PERSISTENT,   N, 0.20,   &c_pp, &d_pp, &t_pp);
        run_simulation(PROTO_CSMA_CD,        N, 1.0,    &c_cd, &d_cd, &t_cd);

        fprintf(f2, "%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.4f\n",
                N, c_np, c_1p, c_pp, c_cd, d_np, d_1p, d_pp, d_cd, t_np, t_1p, t_pp, t_cd);

        printf("N=%2d | Thr: NonPers=%.3f, 1-Pers=%.3f, p-Pers(0.2)=%.3f, CSMA/CD=%.3f\n",
               N, t_np, t_1p, t_pp, t_cd);
    }
    fclose(f2);
    printf("\nCSV files generated: case1_p_persistent.csv, case2_all_schemes.csv\n");
    return 0;
}