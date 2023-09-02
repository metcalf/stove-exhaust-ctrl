import os
import sys
import gzip

import matplotlib.pyplot as plt

from utils import parse, create_axes, plot_data

directory_path = sys.argv[1]

(fig,vocAx, pmAx) = create_axes()
data = []

# Loop over files in the directory
for filename in os.listdir(directory_path):
    file_path = os.path.join(directory_path, filename)
    if os.path.isfile(file_path):
        if os.path.splitext(filename)[1] == ".gz":
            f = gzip.open(file_path, "rt", encoding='utf-8')
        else:
            f = open(file_path, 'rt', encoding='utf-8')

        for line in f:
            parsed = parse(line)
            if parsed:
                data.append(parsed)

        f.close()

data.sort(key=lambda x: x.time)

plot_data(vocAx, pmAx, data)
plt.show()
