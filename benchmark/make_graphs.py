import matplotlib.pyplot as plt

clients = [1, 2, 4, 8]

agg_256  = [112.4, 221.1, 400.8, 570.4]
agg_1024 = [112.2, 217.8, 379.0, 615.8]

avg_256  = [112.4, 110.6, 100.2, 71.3]
avg_1024 = [112.2, 108.9,  94.8, 77.0]

ideal = [112.4 * c for c in clients]

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5))

ax1.plot(clients, agg_256,  'o-', color='steelblue',  label='256 KB chunks',  linewidth=2, markersize=7)
ax1.plot(clients, agg_1024, 's-', color='darkorange', label='1024 KB chunks', linewidth=2, markersize=7)
ax1.plot(clients, ideal,    '--', color='gray',        label='Ideal linear',   linewidth=1.5, alpha=0.5)
ax1.set_xlabel('Simultaneous Clients')
ax1.set_ylabel('Aggregate Throughput (MB/s)')
ax1.set_title('Aggregate Throughput vs. Client Count')
ax1.set_xticks(clients)
ax1.legend()
ax1.grid(True, alpha=0.3)
for x, y in zip(clients, agg_256):
    ax1.annotate(f'{y}', (x, y), textcoords='offset points', xytext=(5,  5), fontsize=8)
for x, y in zip(clients, agg_1024):
    ax1.annotate(f'{y}', (x, y), textcoords='offset points', xytext=(5, -14), fontsize=8)

ax2.plot(clients, avg_256,  'o-', color='steelblue',  label='256 KB chunks',  linewidth=2, markersize=7)
ax2.plot(clients, avg_1024, 's-', color='darkorange', label='1024 KB chunks', linewidth=2, markersize=7)
ax2.set_xlabel('Simultaneous Clients')
ax2.set_ylabel('Per-Client Throughput (MB/s)')
ax2.set_title('Per-Client Throughput vs. Client Count')
ax2.set_xticks(clients)
ax2.legend()
ax2.grid(True, alpha=0.3)
for x, y in zip(clients, avg_256):
    ax2.annotate(f'{y}', (x, y), textcoords='offset points', xytext=(5,  5), fontsize=8)
for x, y in zip(clients, avg_1024):
    ax2.annotate(f'{y}', (x, y), textcoords='offset points', xytext=(5, -14), fontsize=8)

plt.tight_layout()
import os
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'graphs.png')
plt.savefig(out, dpi=150, bbox_inches='tight')
plt.close()
print("Saved graphs.png")
