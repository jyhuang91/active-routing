#!/bin/python

import sys
import numpy as np
import re
import matplotlib.pyplot as plt
from scipy import stats
import json
from easypyplot import barchart, color, pdf
from easypyplot import format as fmt


def add_line(ax, xpos, ypos):
    line = plt.Line2D(
        #[xpos, xpos], [ypos + linelen, ypos],
        [xpos, xpos],
        [0, ypos],
        transform=ax.transAxes,
        color='black',
        linewidth=1)
    line.set_clip_on(False)
    ax.add_line(line)


def main(filepath):

    schemes = ['PEI', 'ART-tid', 'ART-addr']
    benchmarks = ['reduce', 'mac']
    networks = ['16-cube-6.4M', '64-cube-6.4M', '64-cube-25.6M']
    network_labels = ['16 cubes, 6.4M', '64 cubes, 6.4M', '64 cubes, 25.6M']
    xlabels = benchmarks * len(networks)

    cycles = np.zeros(
        (int(len(schemes)), int(len(benchmarks)*len(networks))), dtype=np.float)
    norm_cycles = np.zeros(
        (int(len(schemes)), int(len(benchmarks)*len(networks))), dtype=np.float)


    sim = None
    with open(filepath, 'r') as jsonfile:
        sim = json.load(jsonfile)
        jsonfile.close()

    for s, scheme in enumerate(schemes):
        for n, network in enumerate(networks):
            for b, bench in enumerate(benchmarks):
                idx = n * len(benchmarks) + b

                cycles[s][idx] = sim[network][bench][scheme]
                norm_cycles[s][idx] = cycles[s][idx] / cycles[0][idx]


    speedup = 1 / norm_cycles

    colors = ['#f0f9e8', '#bae4bc',
              '#7bccc4']  #, '#43a2ca', '#0868ac', '#0796ac']
    plt.rc('legend', fontsize=18)
    plt.rc('font', size=18)

    # benchmarks over PEI
    data = [list(i) for i in zip(*speedup)]
    data = np.array(data, dtype=np.float64)
    figname = 'scalability.pdf'
    pdfpage, fig = pdf.plot_setup(figname, figsize=(8, 5.5), fontsize=24)
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names=xlabels,
        entry_names=schemes,
        colors=colors,
        breakdown=False,
        legendloc='upper center',
        legendncol=3,
        xticks=[0,1, 3,4, 6,7],
        xticklabelfontsize=20)
    #fig.autofmt_xdate()
    #ax.set_ylim(0, 20)
    ax.yaxis.grid(True, linestyle='--')
    ax.set_ylabel('Runtime Speedup over PEI')
    ax.legend(
        hdls,
        schemes,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.23),
        ncol=3,
        frameon=False,
        handletextpad=0.5,
        columnspacing=1)
    ly = len(networks)
    scale = 1. / ly
    ypos = -.2
    pos = 0
    for pos in xrange(ly + 1):
        lxpos = (pos + 0.5) * scale
        if pos < ly:
            ax.text(lxpos, ypos, network_labels[pos], fontsize=20,
                    ha='center', transform=ax.transAxes)
        add_line(ax, pos * scale, ypos)
    fmt.resize_ax_box(ax, hratio=0.7)
    pdf.plot_teardown(pdfpage, fig)

    plt.show()


if __name__ == '__main__':

    if len(sys.argv) != 2:
        print 'usage: ' + sys.argv[0] + ' filepath'
        exit()
    main(sys.argv[1])
