/* benchmark_csma.c - Corrected Discrete-Slot MAC Benchmark */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_STATIONS    64
#define TARGET_FRAMES   2000
#define TX_SLOTS        10    /* Frame transmission duration = 10 slots */
#define JAM_SLOTS       2     /* CSMA/CD abort window = 2 slots */
#define SLOT_MS         0.5   /* Slot duration = 0.5 ms */
#define NUM_RUNS        5     /* Average across 5 independent seeds */

typedef enum {
    PROTO_NON_PERSISTENT = 1,
    PROTO_1_PERSISTENT   = 2,
    PROTO_P_PERSISTENT   = 3,
    PROTO_CSMA_CD        = 4
} CSMAProtocol;

typedef struct {
    int  has_frame;
    int  wait_slots;       /* Backoff countdown timer (only decrements in IDLE slots) */
    int  attempts;         /* Transmission collision counter */
    long arrival_slot;
} SimStation;

static void run_single_iteration(CSMAProtocol proto, int N, double p_prob, unsigned int seed,
                                 int *out_col, double *out_dly, double *out_thr) {
    SimStation st[MAX_STATIONS];
    memset(st, 0, sizeof(st));
    srand(seed);

    int delivered = 0;
    int collisions = 0;
    long cur_slot = 0;
    long good_tx_slots = 0;
    double total_delay_slots = 0.0;
    double arrival_prob = 0.09 / (double)N;

    /* Initialize half the stations with traffic queued at t=0 */
    for (int i = 0; i < N; i++) {
        if (i % 2 == 0) {
            st[i].has_frame    = 1;
            st[i].wait_slots   = 0;
            st[i].attempts     = 0;
            st[i].arrival_slot = 0;
        }
    }

    while (delivered < TARGET_FRAMES) {
        /* 1. Traffic arrival at idle stations */
        int any_ready = 0;
        for (int i = 0; i < N; i++) {
            if (!st[i].has_frame) {
                if (((double)rand() / RAND_MAX) < arrival_prob) {
                    st[i].has_frame    = 1;
                    st[i].wait_slots   = 0;
                    st[i].attempts     = 0;
                    st[i].arrival_slot = cur_slot;
                }
            }
            if (st[i].has_frame) any_ready = 1;
        }
        if (!any_ready) {
            int p = rand() % N;
            st[p].has_frame    = 1;
            st[p].wait_slots   = 0;
            st[p].attempts     = 0;
            st[p].arrival_slot = cur_slot;
        }

        /* 2. Channel is IDLE at this boundary. Stations decrement backoff or contend */
        int tx_list[MAX_STATIONS];
        int tx_count = 0;

        for (int i = 0; i < N; i++) {
            if (!st[i].has_frame) continue;

            /* Decrement backoff counter ONLY in IDLE slots */
            if (st[i].wait_slots > 0) {
                st[i].wait_slots--;
                continue;
            }

            /* Station has timer == 0 and senses IDLE channel */
            if (proto == PROTO_NON_PERSISTENT || proto == PROTO_1_PERSISTENT || proto == PROTO_CSMA_CD) {
                tx_list[tx_count++] = i;
            }
            else if (proto == PROTO_P_PERSISTENT) {
                if (((double)rand() / RAND_MAX) <= p_prob) {
                    tx_list[tx_count++] = i;
                } else {
                    /* Defer 1 slot without extra backoff penalty */
                    st[i].wait_slots = 1;
                }
            }
        }

        /* 3. Evaluate outcome */
        if (tx_count == 0) {
            /* Idle slot */
            cur_slot++;
        }
        else if (tx_count == 1) {
            /* Successful clean transmission */
            int winner = tx_list[0];
            int duration = TX_SLOTS;

            /* BACKOFF FREEZE: During BUSY slots, wait_slots DO NOT decrement! */
            for (int s = 1; s <= duration; s++) {
                for (int i = 0; i < N; i++) {
                    if (i == winner) continue;
                    /* New arrivals during transmission */
                    if (!st[i].has_frame && ((double)rand() / RAND_MAX) < (arrival_prob * 1.2)) {
                        st[i].has_frame    = 1;
                        st[i].attempts     = 0;
                        st[i].arrival_slot = cur_slot + s;

                        if (proto == PROTO_NON_PERSISTENT) {
                            st[i].wait_slots = 2 + (rand() % (6 + N / 2));
                        } else {
                            st[i].wait_slots = 0; /* Wait for channel to clear */
                        }
                    } else if (st[i].has_frame && proto == PROTO_NON_PERSISTENT && st[i].wait_slots == 0) {
                        /* Non-persistent senses channel busy: back off once */
                        st[i].wait_slots = 2 + (rand() % (6 + N / 2));
                    }
                }
            }

            cur_slot += duration;
            good_tx_slots += TX_SLOTS;
            delivered++;
            total_delay_slots += (double)(cur_slot - st[winner].arrival_slot);

            /* Queue next frame for the winner */
            st[winner].has_frame    = 1;
            st[winner].attempts     = 0;
            st[winner].arrival_slot = cur_slot;
            st[winner].wait_slots   = 1 + (rand() % 3); /* Small interframe gap */
        }
        else {
            /* Collision */
            collisions++;
            int wasted = (proto == PROTO_CSMA_CD) ? JAM_SLOTS : TX_SLOTS;

            /* During collision period, backoffs remain frozen */
            for (int s = 1; s <= wasted; s++) {
                for (int i = 0; i < N; i++) {
                    if (st[i].has_frame && proto == PROTO_NON_PERSISTENT && st[i].wait_slots == 0) {
                        st[i].wait_slots = 2 + (rand() % (6 + N / 2));
                    }
                }
            }
            cur_slot += wasted;

            /* Colliding stations enter Binary Exponential Backoff */
            for (int k = 0; k < tx_count; k++) {
                int idx = tx_list[k];
                st[idx].attempts++;
                if (proto == PROTO_NON_PERSISTENT) {
                    st[idx].wait_slots = 2 + (rand() % (4 + N));
                } else {
                    int exp = (st[idx].attempts < 6) ? st[idx].attempts : 6;
                    st[idx].wait_slots = rand() % (1 << exp);
                }
            }
        }
    }

    *out_col = collisions;
    *out_dly = (total_delay_slots / (double)TARGET_FRAMES) * SLOT_MS;
    *out_thr = (double)good_tx_slots / (double)cur_slot;
}

static void evaluate_averaged(CSMAProtocol proto, int N, double p_prob,
                              int *mean_col, double *mean_dly, double *mean_thr) {
    double total_col = 0.0, total_dly = 0.0, total_thr = 0.0;
    for (int r = 0; r < NUM_RUNS; r++) {
        int c; double d, t;
        unsigned int seed = 5000 + (r * 7919) + (N * 101) + (int)(p_prob * 1000);
        run_single_iteration(proto, N, p_prob, seed, &c, &d, &t);
        total_col += c;
        total_dly += d;
        total_thr += t;
    }
    *mean_col = (int)(total_col / NUM_RUNS);
    *mean_dly = total_dly / NUM_RUNS;
    *mean_thr = total_thr / NUM_RUNS;
}

int main(void) {
    /* Test Case 1: p-Persistent vs p (Fixed N = 10) */
    FILE *f1 = fopen("case1_p_persistent.csv", "w");
    fprintf(f1, "p,Collisions,AvgDelay_ms,Throughput\n");
    printf("=== Test Case (i): p-Persistent CSMA vs p (N = 10, Averaged across %d Seeds) ===\n", NUM_RUNS);
    printf("   p   | Collisions | Avg Delay (ms) | Channel Utilization (Throughput)\n");
    printf("-------+------------+----------------+---------------------------------\n");

    double p_vals[] = {0.05, 0.10, 0.15, 0.20, 0.30, 0.40, 0.50, 0.60, 0.70, 0.80, 0.90, 1.00};
    int num_p = sizeof(p_vals) / sizeof(p_vals[0]);
    for (int i = 0; i < num_p; i++) {
        int col; double dly, thr;
        evaluate_averaged(PROTO_P_PERSISTENT, 10, p_vals[i], &col, &dly, &thr);
        fprintf(f1, "%.2f,%d,%.2f,%.4f\n", p_vals[i], col, dly, thr);
        printf("  %.2f |   %6d   |     %6.2f     |              %6.2f%%\n",
               p_vals[i], col, dly, thr * 100.0);
    }
    fclose(f1);

    /* Test Case 2: All 4 Schemes vs N */
    FILE *f2 = fopen("case2_all_schemes.csv", "w");
    fprintf(f2, "N,NonPers_Col,1Pers_Col,pPers_Col,CSMACD_Col,"
                "NonPers_Delay,1Pers_Delay,pPers_Delay,CSMACD_Delay,"
                "NonPers_Thr,1Pers_Thr,pPers_Thr,CSMACD_Thr\n");

    int N_vals[] = {2, 4, 8, 10, 12, 16, 20, 25, 30};
    int num_N = sizeof(N_vals) / sizeof(N_vals[0]);
    printf("\n=== Test Case (ii): All 4 CSMA Schemes vs N (Averaged across %d Seeds) ===\n", NUM_RUNS);

    for (int i = 0; i < num_N; i++) {
        int N = N_vals[i];
        int c_np, c_1p, c_pp, c_cd;
        double d_np, d_1p, d_pp, d_cd;
        double t_np, t_1p, t_pp, t_cd;

        evaluate_averaged(PROTO_NON_PERSISTENT, N, 0.0,  &c_np, &d_np, &t_np);
        evaluate_averaged(PROTO_1_PERSISTENT,   N, 1.0,  &c_1p, &d_1p, &t_1p);
        evaluate_averaged(PROTO_P_PERSISTENT,   N, 0.20, &c_pp, &d_pp, &t_pp);
        evaluate_averaged(PROTO_CSMA_CD,        N, 1.0,  &c_cd, &d_cd, &t_cd);

        fprintf(f2, "%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%.4f,%.4f,%.4f,%.4f\n",
                N, c_np, c_1p, c_pp, c_cd, d_np, d_1p, d_pp, d_cd, t_np, t_1p, t_pp, t_cd);

        printf("N=%2d | NonPers: %4d col, %.1f%% | 1-Pers: %4d col, %.1f%% | p-Pers: %4d col, %.1f%% | CSMA/CD: %4d col, %.1f%%\n",
               N, c_np, t_np * 100.0, c_1p, t_1p * 100.0, c_pp, t_pp * 100.0, c_cd, t_cd * 100.0);
    }
    fclose(f2);
    return 0;
}
