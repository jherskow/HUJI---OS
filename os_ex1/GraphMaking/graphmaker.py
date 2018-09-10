# Graphmaker, Grahpmaker, Make me a Graph

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import FormatStrFormatter

N = 3
in_vm_times = (4.3212, 29.5345, 387.902)
direct_times = (0.3281, 1.544, 351.368)



ind = np.arange(N)  # the x locations for the groups
width = 0.40       # the width of the bars

fig, ax = plt.subplots()

direct_bars = ax.bar(ind, direct_times, width, edgecolor='black', color='slategray')

vm_bars = ax.bar(ind + width, in_vm_times, width, edgecolor='black', color='maroon')

# add some text for labels, title and axes ticks
ax.set_title('Time Comparison')

ax.set_ylabel('Time in ns  (log scale)\n\n')
ax.set_xlabel('\n\nTest Type')

ax.set_yscale('log')

ax.set_xticks(ind + width / 2)
ax.set_xticklabels(('Operation', 'Function', 'Syscall'))

ax.legend((direct_bars[0], vm_bars[0]), ('Direct', 'VM'))


def autolabel(rects):
    """
    Attach a text label above each bar displaying its height
    """
    for rect in rects:
        height = rect.get_height()
        ax.text(rect.get_x() + rect.get_width()/2., 1.05*height,
                '%.3f' % float(height),
                ha='center', va='bottom')

autolabel(direct_bars)
autolabel(vm_bars)

plt.show()