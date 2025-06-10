import matplotlib.pyplot as plt
import numpy as np


labels = ['1', '2', '4', '8', '12', '14', '16', '20', '24', '28', '32']
uncompressed = [
    11909.0, 21645.4, 40004.2, 56435.1, 57749.5, 58024.4, 49663.5, 55813.8, 55357.2, 56296.0, 46849.2]
blosclz_cl1 = [
    6016.4, 11062.0, 21284.2, 41452.4, 57542.6, 65535.6, 54084.9, 65367.7, 74655.7, 86628.0, 53853.5]
lz4_cl1 = [
    5194.0, 9465.5, 18262.0, 35698.7, 50247.9, 57009.0, 42268.3, 52207.0, 59755.1, 67353.5, 36658.2]
lz4hc_cl1 = [
    6109.7, 11114.1, 21368.7, 41932.4, 57268.5, 66780.0, 50380.2, 61220.5, 69262.0, 82380.5, 37305.5]
zstd_cl1 = [
    1593.8, 2955.5, 5669.4, 11244.7, 16363.2, 18377.6, 12865.2, 15830.7, 18736.9, 21474.3, 10466.3]

# Use GB/s
uncompressed = np.array(uncompressed) / 1024.
blosclz_cl1 = np.array(blosclz_cl1) / 1024.
lz4_cl1 = np.array(lz4_cl1) / 1024.
lz4hc_cl1 = np.array(lz4hc_cl1) / 1024.
zstd_cl1 = np.array(zstd_cl1) / 1024.

x = np.arange(len(labels))  # the label locations
width = 0.30 / 2  # the width of the bars

fig, ax = plt.subplots()
rects1 = ax.bar(x, uncompressed, width, label='Uncompressed')
rects2 = ax.bar(x + width, blosclz_cl1, width, label='blosclz cl-1 (3.7x cr)')
rects3 = ax.bar(x + 2 * width, lz4_cl1, width, label='lz4 cl-1 (4.5x cr)')
rects4 = ax.bar(x + 3 * width, lz4hc_cl1, width, label='lz4hc cl-1 (5.0x cr)')
rects5 = ax.bar(x + 4 * width, zstd_cl1, width, label='zstd cl-1 (5.9x cr)')

# Add some text for labels, title and custom x-axis tick labels, etc.
ax.set_ylabel('Speed (GB/s)')
ax.set_xlabel('Threads')
ax.set_title('Summing up precipitation data (381.5 MB, float32)')
ax.set_xticks(x)
ax.set_xticklabels(labels)
ax.legend()

#ax.bar_label(rects1, padding=3)
#ax.bar_label(rects2, padding=3)
#ax.bar_label(rects2, padding=3)

fig.tight_layout()

plt.show()