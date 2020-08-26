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

    schemes = ['ART-tid', 'ART-addr']
    scheme_names = ['ART-tid-CLP', 'ART-addr-CLP', 'ART-tid-VLP', 'ART-addr-VLP', 'ART-tid-CLP-Page', 'ART-addr-CLP-Page', 'ART-tid-VLP-Page', 'ART-addr-VLP-Page']
    stats_keys = ['Instruction-Offloading-CLP', 'Instruction-Offloading-VLP', 'Page-Offloading-CLP', 'Page-Offloading-VLP']
    benchmarks = ['sgemm', 'mac', 'reduce']

    cycles = np.zeros(
        (int(len(scheme_names)), int(len(benchmarks))), dtype=np.float)
    norm_cycles = np.zeros(
        (int(len(scheme_names)), int(len(benchmarks))), dtype=np.float)
    speedup = np.zeros(
        (int(len(scheme_names)), int(len(benchmarks))), dtype=np.float)
    pei_cycles = np.zeros(int(len(benchmarks)), dtype=np.float)


    sim = None
    with open(filepath, 'r') as jsonfile:
        sim = json.load(jsonfile)
        jsonfile.close()

    for b, bench in enumerate(benchmarks):
        pei_cycles[b] = sim['Instruction-Offloading-CLP'][bench]['PEI']

    for k, statkey in enumerate(stats_keys):
        for s, scheme in enumerate(schemes):
            idx = k * len(schemes) + s
            for b, bench in enumerate(benchmarks):
                if bench in sim[statkey].keys():
                    cycles[idx][b] = sim[statkey][bench][scheme]
                    #norm_cycles[idx][b] = cycles[idx][b] / cycles[0][b]
                    norm_cycles[idx][b] = cycles[idx][b] / pei_cycles[b]
                    speedup[idx][b] = 1 / norm_cycles[idx][b]


    print(speedup)

    colors = ['#f0f9e8', '#bae4bc',
              '#7bccc4']  #, '#43a2ca', '#0868ac', '#0796ac']
    colors = ['#f7fcf0','#e0f3db','#ccebc5','#a8ddb5','#7bccc4','#4eb3d3','#2b8cbe','#08589e']
    plt.rc('legend', fontsize=18)
    plt.rc('font', size=18)

    data = [list(i) for i in zip(*speedup)]
    data = np.array(data, dtype=np.float64)
    figname = 'speedupPageVLP.pdf'
    pdfpage, fig = pdf.plot_setup(figname, figsize=(12, 5.5), fontsize=24)
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names=benchmarks,
        entry_names=scheme_names,
        colors=colors,
        breakdown=False,
        legendloc='upper left',
        legendncol=2,
        #xticks=[0,1, 3,4, 6,7],
        xticklabelfontsize=24,
        log=True)
    #fig.autofmt_xdate()
    #ax.set_ylim(0, 20)
    ax.yaxis.grid(True, linestyle='--')
    #ax.xaxis.grid(True, linestyle='--')
    ax.set_ylabel('Runtime Speedup over PEI (log)')
    ax.legend(
        hdls,
        scheme_names,
        loc='upper left',
        #bbox_to_anchor=(0.5, 1.23),
        #bbox_to_anchor=(0.5, 1),
        fontsize=22,
        ncol=2,
        frameon=True,
        handletextpad=0.5,
        columnspacing=1)
    for xpos in [0.25, 0.5]:
        base = xpos + .125 / 10
        for i in range(4):
            xxpos = base + i * 0.25 / 10
            ax.text(xxpos, 0, '$\\times$', fontsize=20,
                    ha='center', transform=ax.transAxes)
    #fmt.resize_ax_box(ax, hratio=0.8)
    pdf.plot_teardown(pdfpage, fig)

    plt.show()


if __name__ == '__main__':

    if len(sys.argv) != 2:
        print 'usage: ' + sys.argv[0] + ' filepath'
        exit()
    main(sys.argv[1])
