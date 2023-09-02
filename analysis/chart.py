
import matplotlib.animation as animation
import matplotlib.pyplot as plt

import sys
from threading import Thread
from queue import Queue, Empty

from utils import parse, create_axes, plot_data

q = Queue()
data = []
lines = int(sys.argv[1])
(fig,vocAx, pmAx) = create_axes()

def reader(f, queue):
   while True:
     line=f.readline()
     if line:
        queue.put(line)
     else:
        break

# This function is called periodically from FuncAnimation
def animate(i, data):
    while True:
        try:
            line = q.get(False)
        except Empty:
           break

        parsed = parse(line)
        if(parsed):
            data.append(parsed)
            [x, y1, y2] = parsed

    if len(data) == 0:
       return

    # Limit x and y lists
    data = data[-lines:]

    plot_data(vocAx, pmAx, data)

t = Thread(target=reader, args=(sys.stdin, q), daemon=True)
t.start()

# Set up plot to call animate() function periodically
ani = animation.FuncAnimation(fig, animate, fargs=(data,), interval=1000)
plt.show()
sys.stdin.close()
