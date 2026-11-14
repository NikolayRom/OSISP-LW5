import matplotlib.pyplot as plt

configs = [
    'Config A\n(64B, 100 cyc)',
    'Config B\n(64B, 500 cyc)',
    'Config C\n(4KB, 100 cyc)',
    'Config D\n(4KB, 500 cyc)'
]

avg_times = [11.20, 54.08, 310.40, 1508.48]
avg_rtts  = [0.1120, 0.1082, 3.1040, 3.0170]
speeds    = [1.10, 1.13, 2.58, 2.65]

plt.figure(figsize=(8, 5))
bars1 = plt.bar(configs, avg_rtts, color=['#377eb8', '#4daf4a', '#e41a1c', '#ff7f00'], width=0.45)
plt.title('Graph 1. Average Round-Trip Time (RTT) per Cycle (RFC 862 Echo)', fontsize=11, fontweight='bold')
plt.xlabel('Test Configuration', fontsize=10)
plt.ylabel('Average RTT (ms)', fontsize=10)
plt.ylim(0, 3.8)
plt.grid(axis='y', linestyle='--', alpha=0.6)

for bar in bars1:
    yval = bar.get_height()
    plt.text(bar.get_x() + bar.get_width() / 2.0, yval + 0.08, f'{yval:.4f} ms', ha='center', va='bottom', fontsize=9, fontweight='bold')

plt.tight_layout()
plt.savefig('graph1_rtt.png', dpi=300)
print('Graph 1 saved: graph1_rtt.png')

plt.figure(figsize=(8, 5))
bars2 = plt.bar(configs, speeds, color=['#4c72b0', '#55a868', '#c44e52', '#8172b2'], width=0.45)
plt.title('Graph 2. Effective Bidirectional Throughput (MB/s)', fontsize=11, fontweight='bold')
plt.xlabel('Test Configuration', fontsize=10)
plt.ylabel('Throughput (MB/s)', fontsize=10)
plt.ylim(0, 3.5)
plt.grid(axis='y', linestyle='--', alpha=0.6)

for bar in bars2:
    yval = bar.get_height()
    plt.text(bar.get_x() + bar.get_width() / 2.0, yval + 0.08, f'{yval:.2f} MB/s', ha='center', va='bottom', fontsize=9, fontweight='bold')

plt.tight_layout()
plt.savefig('graph2_throughput.png', dpi=300)
print('Graph 2 saved: graph2_throughput.png')

plt.show()