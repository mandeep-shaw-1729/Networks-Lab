# plot_csma.py - Generates Test Case (i) and Test Case (ii) Performance Graphs
import csv
import matplotlib.pyplot as plt

def load_csv(filename):
    with open(filename, 'r') as f:
        return list(csv.DictReader(f))

# 1. Plot Test Case (i): p-Persistent vs p (Fixed N = 10)
c1 = load_csv("case1_p_persistent.csv")
p_vals = [float(r["p"]) for r in c1]
c1_col = [int(r["Collisions"]) for r in c1]
c1_dly = [float(r["AvgDelay_ms"]) for r in c1]
c1_thr = [float(r["Throughput"]) * 100.0 for r in c1]

fig, axes = plt.subplots(1, 3, figsize=(15, 4.5))
axes[0].plot(p_vals, c1_col, 'o-', color='#d62728', lw=2)
axes[0].set_title("Collisions vs. Probability p (N=10)")
axes[0].set_xlabel("Persistence Probability (p)")
axes[0].set_ylabel("Total Collisions (per 2000 frames)")
axes[0].grid(True, linestyle='--', alpha=0.6)

axes[1].plot(p_vals, c1_dly, 's-', color='#ff7f0e', lw=2)
axes[1].set_title("Avg Transmission Delay vs. p (N=10)")
axes[1].set_xlabel("Persistence Probability (p)")
axes[1].set_ylabel("Average Delay (ms)")
axes[1].grid(True, linestyle='--', alpha=0.6)

axes[2].plot(p_vals, c1_thr, '^-', color='#2ca02c', lw=2)
axes[2].set_title("Channel Throughput vs. p (N=10)")
axes[2].set_xlabel("Persistence Probability (p)")
axes[2].set_ylabel("Normalized Throughput (%)")
axes[2].grid(True, linestyle='--', alpha=0.6)

plt.tight_layout()
plt.savefig("TestCase1_p_Persistent.png", dpi=300)

# 2. Plot Test Case (ii): All 4 Schemes vs Contending Stations N
c2 = load_csv("case2_all_schemes.csv")
N_vals = [int(r["N"]) for r in c2]

fig2, ax2 = plt.subplots(1, 3, figsize=(16, 4.8))
schemes = [
    ("NonPers", "Non-Persistent", "o--", "#1f77b4"),
    ("1Pers",   "1-Persistent",   "s-.", "#d62728"),
    ("pPers",   "p-Persistent (p=0.2)", "^:", "#9467bd"),
    ("CSMACD",  "CSMA/CD",        "D-",  "#2ca02c")
]

for prefix, label, style, color in schemes:
    ax2[0].plot(N_vals, [int(r[f"{prefix}_Col"]) for r in c2], style, label=label, color=color, lw=2)
    ax2[1].plot(N_vals, [float(r[f"{prefix}_Delay"]) for r in c2], style, label=label, color=color, lw=2)
    ax2[2].plot(N_vals, [float(r[f"{prefix}_Thr"]) * 100.0 for r in c2], style, label=label, color=color, lw=2)

ax2[0].set_title("Total Collisions vs. Stations (N)")
ax2[0].set_xlabel("Number of Contending Stations (N)")
ax2[0].set_ylabel("Collisions (per 2000 frames)")

ax2[1].set_title("Avg Transmission Delay vs. Stations (N)")
ax2[1].set_xlabel("Number of Contending Stations (N)")
ax2[1].set_ylabel("Average Frame Delay (ms)")

ax2[2].set_title("Channel Throughput vs. Stations (N)")
ax2[2].set_xlabel("Number of Contending Stations (N)")
ax2[2].set_ylabel("Normalized Channel Throughput (%)")

for a in ax2:
    a.grid(True, linestyle='--', alpha=0.6)
    a.legend()

plt.tight_layout()
plt.savefig("TestCase2_All_Schemes_vs_N.png", dpi=300)
print("Saved plots: TestCase1_p_Persistent.png and TestCase2_All_Schemes_vs_N.png")