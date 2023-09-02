import matplotlib.pyplot as plt
import pytz

from collections import namedtuple
from datetime import datetime
import re

log_regex = re.compile("(\w{3}  \d+ [\d:]+) stove-exhaust-ctrl app: voc_ticks=\d+ voc_index=(\d+) pm25=(\d+)")
target_timezone = pytz.timezone('America/Los_Angeles')

DataPoint = namedtuple('DataPoint', ('time', 'voc', 'pm'))

def parse(line):
    match = log_regex.match(line)
    if match is None:
        print("Unable to parse: %s" % line)
        return None

    return DataPoint(
        datetime.strptime(match.group(1), "%b %d %H:%M:%S"),
        int(match.group(2)),
        int(match.group(3))
    )

def create_axes():
    # Create figure for plotting
    fig = plt.figure()
    fig.autofmt_xdate()

    vocAx = fig.add_subplot(1, 1, 1)
    pmAx = vocAx.twinx()

    return (fig, vocAx, pmAx)

def plot_data(vocAx, pmAx, data):
    xs = [d.time for d in data]
    vocs = [d.voc for d in data]
    pms = [d.pm for d in data]

    # Draw x and y lists
    vocAx.clear()
    p1, = vocAx.plot(xs, vocs, "b-")
    p2, = pmAx.plot(xs, pms, "g-")

    vocAx.set_ylim(bottom=0.0, top=500.0)
    vocAx.yaxis.label.set_color(p1.get_color())
    vocAx.set_ylabel("VOC index")

    pmAx.set_ylim(bottom=0.0, top=100.0)
    pmAx.set_ylabel("PM 2.5 ug/m3")
    pmAx.yaxis.label.set_color(p2.get_color())

    # Format plot
    plt.title('Air quality over time')
