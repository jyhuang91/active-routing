#!/usr/bin/python

import sys
import numpy as np
import re
import matplotlib.pyplot as plt
from scipy import stats
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


def main(folder_path):

    folders = ['McSimARFtid', 'McSimARFaddr']
    names = ['arftid', 'arfaddr']
    schemes = ['ART-tid', 'ART-addr']
    benchmarks = ['backprop', 'lud', 'pagerank', 'sgemm', 'spmv']
    xlabels = ['backprop', 'lud', 'pagerank', 'sgemm', 'spmv']
    microbenches = ['reduce', 'rand_reduce', 'mac', 'rand_mac']
    microxlabels = ['reduce', 'rand_reduce', 'mac', 'rand_mac']
    #entry_names = ['req_lat', 'stall_lat', 'resp_lat']
    entry_names = ['request latency', 'stall latency', 'response latency']
    entry_names = ['request', 'stall', 'response']
    group_names = []
    micro_group_names = []

    update_req_lat = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    update_stall_lat = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    update_rr_lat = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    lat_breakdown = np.zeros(
        (3, int(len(benchmarks) * len(schemes))), dtype=np.float)

    micro_update_req_lat = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_update_stall_lat = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_update_rr_lat = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_lat_breakdown = np.zeros(
        (3, int(len(microbenches) * len(schemes))), dtype=np.float)

    for b, bench in enumerate(benchmarks):
        for s, scheme in enumerate(schemes):
            group_names.append(scheme)
            if bench == 'lud':
                #bench = bench + '_s256'
                bench = bench + '_4096_0.75_1'
            elif bench == 'sgemm':
                bench = bench + '_4096_1'
            elif bench == 'spmv':
                bench = bench + '_4096_0.3'
            elif bench == 'backprop':
                bench = bench + '_2097152'
            elif bench == 'pagerank':
                bench = bench + '_web-Google'
            filename = folder_path + '/' + folders[s] + '/dfly_log/' + names[s] + '_' + bench + '.log'

            infile = open(filename)
            for l, line in enumerate(infile):
                if '-- average update request latency' in line:
                    line = line.split()
                    if bench == 'lud_4096_0.75_1':
                        update_req_lat[s][b] = line[-1]
                        lat_breakdown[0][b * len(schemes) + s] = line[-1]
                    else:
                        update_req_lat[s][b] = line[-1]
                        lat_breakdown[0][b * len(schemes) + s] = line[-1]
                elif '-- average update stalls' in line:
                    line = line.split()
                    if bench == 'lud_4096_0.75_1':
                        update_stall_lat[s][b] = line[-1]
                        lat_breakdown[1][b * len(schemes) + s] = line[-1]
                    else:
                        update_stall_lat[s][b] = line[-1]
                        lat_breakdown[1][b * len(schemes) + s] = line[-1]
                elif '-- average update roundtrip' in line:
                    line = line.split()
                    if bench == 'lud_4096_0.75_1':
                        update_rr_lat[s][b] = line[-1]
                        lat_breakdown[2][b * len(schemes) + s] = np.float64(
                            line[-1]) - lat_breakdown[0][b * len(
                                schemes
                            ) + s] - lat_breakdown[1][b * len(schemes) + s]
                    else:
                        update_rr_lat[s][b] = line[-1]
                        lat_breakdown[2][b * len(schemes) + s] = np.float64(
                            line[-1]) - lat_breakdown[0][b * len(
                                schemes
                            ) + s] - lat_breakdown[1][b * len(schemes) + s]
                else:
                    continue

    for b, bench in enumerate(microbenches):
        for s, scheme in enumerate(schemes):
            micro_group_names.append(scheme)
            filename = folder_path + '/' + folders[s] + '/dfly_log/' + names[s] + '_' + bench + '.log'

            infile = open(filename)
            for l, line in enumerate(infile):
                if '-- average update request latency' in line:
                    line = line.split()
                    micro_update_req_lat[s][b] = line[-1]
                    micro_lat_breakdown[0][b * len(schemes) + s] = line[-1]
                elif '-- average update stalls' in line:
                    line = line.split()
                    micro_update_stall_lat[s][b] = line[-1]
                    micro_lat_breakdown[1][b * len(schemes) + s] = line[-1]
                elif '-- average update roundtrip' in line:
                    line = line.split()
                    micro_update_rr_lat[s][b] = line[-1]
                    micro_lat_breakdown[2][b * len(schemes) + s] = np.float64(
                        line[-1]) - micro_lat_breakdown[0][b * len(
                            schemes
                        ) + s] - micro_lat_breakdown[1][b * len(schemes) + s]
                else:
                    continue

    result_file = open('latency_breakdown.csv', mode='w')
    result_file.write('latency breakdown:\n')
    result_file.write(',,request latency,stall latency,response latency\n')
    for b, benchmark in enumerate(benchmarks):
        row = '\n' + benchmark + ',ART-tid,' + str(
            lat_breakdown[0][b * len(schemes)]) + ',' + str(
                lat_breakdown[1][b * len(schemes)]) + ',' + str(
                    lat_breakdown[2][b * len(schemes)]) + '\n'
        result_file.write(row)
        row = ',ART-addr,' + str(
            lat_breakdown[0][b * len(schemes) + 1]) + ',' + str(
                lat_breakdown[1][b * len(schemes) + 1]) + ',' + str(
                    lat_breakdown[2][b * len(schemes) + 1]) + '\n\n'
        result_file.write(row)
    result_file.write('\n')

    result_file.write(',,request latency,stall latency,response latency\n')
    for b, benchmark in enumerate(microbenches):
        row = '\n' + benchmark + ',ART-tid,' + str(
            micro_lat_breakdown[0][b * len(schemes)]) + ',' + str(
                micro_lat_breakdown[1][b * len(schemes)]) + ',' + str(
                    micro_lat_breakdown[2][b * len(schemes)]) + '\n'
        result_file.write(row)
        row = ',ART-addr,' + str(
            micro_lat_breakdown[0][b * len(schemes) + 1]) + ',' + str(
                micro_lat_breakdown[1][b * len(schemes) + 1]) + ',' + str(
                    micro_lat_breakdown[2][b * len(schemes) + 1]) + '\n\n'
        result_file.write(row)
    result_file.close()

    colors = ['#e0f3db', '#a8ddb5', '#43a2ca']
    plt.rc('legend', fontsize=24)
    #plt.subplots_adjust(wspace=0.26, left=0.16, right=0.97, top=0.95)
    plt.rc('font', size=24)

    xticks = []
    for i in range(0, len(benchmarks)):
        for j in range(0, len(schemes)):
            xticks.append(i * (len(schemes) + 1) + j)
    data = [list(i) for i in zip(*lat_breakdown)]
    data = np.array(data, dtype=np.float64)

    #fig = plt.figure(figsize=(6.4, 4))
    figname = folder_path + '/results/benchLatency.pdf'
    pdfpage, fig = pdf.plot_setup(figname, figsize=(8.7, 5.5), fontsize=24)
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names=group_names,
        entry_names=entry_names,
        breakdown=True,
        xticks=xticks,
        width=0.8,
        colors=colors,
        legendloc='upper center',
        legendncol=3,
        xticklabelfontsize=22,
        xticklabelrotation=90)
    #fig.autofmt_xdate()
    #ax.set_ylim([0, 800])
    #ax.text(11, 800, r'5910', fontsize=16)
    #ax.text(10, 750, r'2903', fontsize=14)
    ax.set_ylabel('Latency (cycles)')
    ax.yaxis.grid(True, linestyle='--')
    ax.legend(
        hdls,
        entry_names,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.2),
        ncol=3,
        #fontsize=20,
        frameon=False,
        handletextpad=0.3,
        columnspacing=0.8)
    fmt.resize_ax_box(ax, hratio=0.8)

    ly = len(benchmarks)
    scale = 1. / ly
    ypos = -.45
    pos = 0
    for pos in xrange(ly + 1):
        lxpos = (pos + 0.5) * scale
        if pos < ly:
            ax.text(
                lxpos,
                ypos,
                benchmarks[pos],
                ha='center',
                transform=ax.transAxes)
        add_line(ax, pos * scale, ypos)

    #pdf.plot_teardown(pdfpage, fig)
    fig.savefig(folder_path + '/results/benchLatency.pdf', bbox_inches='tight')

    # microbenchmarks
    xticks = []
    for i in range(0, len(microbenches)):
        for j in range(0, len(schemes)):
            xticks.append(i * (len(schemes) + 1) + j)
    data = [list(i) for i in zip(*micro_lat_breakdown)]
    data = np.array(data, dtype=np.float64)

    #fig = plt.figure(figsize=(6.4, 4))
    figname = folder_path + '/results/microLatency.pdf'
    pdfpage, fig = pdf.plot_setup(figname, figsize=(8.7, 5.5), fontsize=24)
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names=micro_group_names,
        entry_names=entry_names,
        breakdown=True,
        xticks=xticks,
        width=0.8,
        colors=colors,
        legendloc='upper center',
        legendncol=3,
        xticklabelfontsize=22,
        xticklabelrotation=90)
    ax.set_ylabel('Latency (cycles)')
    ax.yaxis.grid(True, linestyle='--')
    ax.legend(
        hdls,
        entry_names,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.2),
        ncol=3,
        #fontsize=20,
        frameon=False,
        handletextpad=0.3,
        columnspacing=0.8)
    fmt.resize_ax_box(ax, hratio=0.8)

    ly = len(microbenches)
    scale = 1. / ly
    ypos = -.45
    pos = 0
    for pos in xrange(ly + 1):
        lxpos = (pos + 0.5) * scale
        if pos < ly:
            ax.text(
                lxpos,
                ypos,
                microxlabels[pos],
                ha='center',
                transform=ax.transAxes)
        add_line(ax, pos * scale, ypos)
    #pdf.plot_teardown(pdfpage, fig)
    fig.savefig(folder_path + '/results/microLatency.pdf', bbox_inches='tight')

    plt.show()


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'usage: ' + sys.argv[0] + ' folder_path'
        exit()
    main(sys.argv[1])
