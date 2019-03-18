#!/bin/python

import sys
import numpy as np
import re
import matplotlib.pyplot as plt
from scipy import stats
from easypyplot import barchart, color
from easypyplot import format as fmt


def main(folder_path):

    folders = ['McSimART', 'McSimARFtid',
               'McSimARFaddr']  # log for pei is in McSimART
    names = ['pei', 'arftid', 'arfaddr']
    schemes = ['PEI', 'ART-tid', 'ART-addr']
    benchmarks = ['backprop', 'lud', 'pagerank', 'sgemm', 'spmv']
    xlabels = ['backprop', 'lud', 'pagerank', 'sgemm', 'spmv', 'gmean']
    microbenchs = ['reduce', 'rand_reduce', 'mac', 'rand_mac']
    microxlabels = ['reduce', 'rand_reduce', 'mac', 'rand_mac', 'gmean']

    cycles = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    ipcs = np.zeros((int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    norm_cycles = np.zeros(
        (int(len(schemes)), int(len(xlabels))), dtype=np.float)
    norm_ipcs = np.zeros(
        (int(len(schemes)), int(len(xlabels))), dtype=np.float)
    norm_cycles2hmc = np.zeros(
        (int(len(schemes) - 1), int(len(xlabels))), dtype=np.float)

    microcycles = np.zeros(
        (int(len(schemes)), int(len(microbenchs))), dtype=np.float)
    microipcs = np.zeros(
        (int(len(schemes)), int(len(microbenchs))), dtype=np.float)
    norm_microcycles = np.zeros(
        (int(len(schemes)), int(len(microxlabels))), dtype=np.float)
    norm_microipcs = np.zeros(
        (int(len(schemes)), int(len(microxlabels))), dtype=np.float)
    norm_microcycles2hmc = np.zeros(
        (int(len(schemes) - 1), int(len(microxlabels))), dtype=np.float)

    for s, scheme in enumerate(schemes):
        for b, bench in enumerate(benchmarks):
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
            if scheme == 'DRAM':
                filename = folder_path + '/' + folders[s] + '/dram_log/' + names[s] + '_' + bench + '.log'
            else:
                filename = folder_path + '/' + folders[s] + '/dfly_log/' + names[s] + '_' + bench + '.log'

            infile = open(filename)
            for l, line in enumerate(infile):
                if '-- total number of fetched instructions' in line:
                    line = re.split(':|\(|\)|\ |\n', line)
                    ipcs[s][b] = line[-3]
                elif '-- total number of ticks' in line:
                    line = line.split()
                    cycles[s][b] = line[-1]
                else:
                    continue

            norm_ipcs[s][b] = ipcs[s][b] / ipcs[0][b]
            norm_cycles[s][b] = cycles[s][b] / cycles[0][b]
            if s > 0:
                norm_cycles2hmc[s - 1][b] = cycles[s][b] / cycles[1][b]

        for b, bench in enumerate(microbenchs):
            if scheme == 'DRAM':
                filename = folder_path + '/' + folders[s] + '/dram_log/' + names[s] + '_' + bench + '.log'
            else:
                filename = folder_path + '/' + folders[s] + '/dfly_log/' + names[s] + '_' + bench + '.log'

            infile = open(filename)
            for l, line in enumerate(infile):
                if '-- total number of fetched instructions' in line:
                    line = re.split(':|\(|\)|\ |\n', line)
                    microipcs[s][b] = line[-3]
                elif '-- total number of ticks' in line:
                    line = line.split()
                    microcycles[s][b] = line[-1]
                else:
                    continue

            norm_microipcs[s][b] = microipcs[s][b] / microipcs[0][b]
            norm_microcycles[s][b] = microcycles[s][b] / microcycles[0][b]
            if s > 0:
                norm_microcycles2hmc[s - 1][b] = microcycles[s][
                    b] / microcycles[1][b]

        norm_ipcs[s][-1] = stats.mstats.gmean(norm_ipcs[s][0:-1])
        norm_cycles[s][-1] = stats.mstats.gmean(norm_cycles[s][0:-1])
        norm_microipcs[s][-1] = stats.mstats.gmean(norm_microipcs[s][0:-1])
        norm_microcycles[s][-1] = stats.mstats.gmean(norm_microcycles[s][0:-1])
        norm_cycles2hmc[s - 1][-1] = stats.mstats.gmean(
            norm_cycles2hmc[s - 1][0:-1])
        norm_microcycles2hmc[s - 1][-1] = stats.mstats.gmean(
            norm_microcycles2hmc[s - 1][0:-1])

    speedup = 1 / norm_cycles
    micro_speedup = 1 / norm_microcycles
    speedup2hmc = 1 / norm_cycles2hmc
    micro_speedup2hmc = 1 / norm_microcycles2hmc

    result_file = open('performance.csv', mode='w')
    result_file.write('cycles:\n')
    result_file.write(',backprop,lud,pagerank,sgemm,spmv\n')
    for s, scheme in enumerate(schemes):
        row = scheme
        for b, benchmark in enumerate(benchmarks):
            row = row + ',' + str(cycles[s][b])
        row = row + '\n'
        result_file.write(row)
    result_file.write('\n')

    result_file.write('microcycles:\n')
    result_file.write(',reduce,rand_reduce,mac,rand_mac\n')
    for s, scheme in enumerate(schemes):
        row = scheme
        for b, benchmark in enumerate(microbenchs):
            row = row + ',' + str(microcycles[s][b])
        row = row + '\n'
        result_file.write(row)
    result_file.write('\n')
    result_file.close()

    colors = ['#f0f9e8', '#bae4bc',
              '#7bccc4']  #, '#43a2ca', '#0868ac', '#0796ac']
    plt.rc('legend', fontsize=18)
    plt.rc('font', size=18)

    # benchmarks over PEI
    data = [list(i) for i in zip(*speedup)]
    data = np.array(data, dtype=np.float64)
    fig = plt.figure(figsize=(8, 5.5))
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names=xlabels,
        entry_names=schemes,
        colors=colors,
        breakdown=False,
        legendloc='upper center',
        legendncol=3)
    fig.autofmt_xdate()
    #ax.set_ylim(0, 20)
    ax.yaxis.grid(True, linestyle='--')
    ax.set_ylabel('Runtime Speedup')
    ax.legend(
        hdls,
        schemes,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=3,
        frameon=False,
        handletextpad=0.1,
        columnspacing=0.5)
    #fmt.resize_ax_box(ax, hratio=0.8)
    fig.savefig(
        folder_path + '/results/benchSpeedup_pei.pdf', bbox_inches='tight')

    # microbenchmarks over PEI
    data = [list(i) for i in zip(*micro_speedup)]
    data = np.array(data, dtype=np.float64)
    #fig = plt.figure(figsize=(6.4, 4))
    fig = plt.figure(figsize=(8, 5.5))
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names=microxlabels,
        entry_names=schemes,
        colors=colors,
        breakdown=False,
        legendloc='upper center',
        legendncol=3)
    fig.autofmt_xdate()
    #ax.set_ylim(0, 8)
    ax.yaxis.grid(True, linestyle='--')
    ax.set_ylabel('Runtime Speedup')
    ax.legend(
        hdls,
        schemes,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=3,
        frameon=False,
        handletextpad=0.1,
        columnspacing=0.5)
    #fmt.resize_ax_box(ax, hratio=0.8)
    fig.savefig(
        folder_path + '/results/microSpeedup_pei.pdf', bbox_inches='tight')

    # benchmarks over HMC
    data = [list(i) for i in zip(*speedup2hmc)]
    data = np.array(data, dtype=np.float64)
    fig = plt.figure(figsize=(8, 5.5))
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names=xlabels,
        entry_names=schemes[1:],
        colors=colors,
        breakdown=False,
        legendloc='upper center',
        legendncol=3)
    fig.autofmt_xdate()
    #ax.set_ylim(0, 8)
    ax.yaxis.grid(True, linestyle='--')
    ax.set_ylabel('Runtime Speedup')
    ax.legend(
        hdls,
        schemes[1:],
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=2,
        frameon=False,
        handletextpad=0.1,
        columnspacing=0.5)
    fmt.resize_ax_box(ax, hratio=0.8)
    fig.savefig(
        folder_path + '/results/benchSpeedup2hmc_pei.pdf', bbox_inches='tight')

    # microbenchmarks over HMC
    data = [list(i) for i in zip(*micro_speedup2hmc)]
    data = np.array(data, dtype=np.float64)
    #fig = plt.figure(figsize=(6.4, 4))
    fig = plt.figure(figsize=(8, 5.5))
    #fig = plt.figure()
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names=microxlabels,
        entry_names=schemes[1:],
        colors=colors,
        breakdown=False,
        legendloc='upper center',
        legendncol=2)
    fig.autofmt_xdate()
    #ax.set_ylim(0, 8)
    ax.yaxis.grid(True, linestyle='--')
    ax.set_ylabel('Runtime Speedup')
    ax.legend(
        hdls,
        schemes[1:],
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=2,
        frameon=False,
        handletextpad=0.1,
        columnspacing=0.5)
    fmt.resize_ax_box(ax, hratio=0.8)
    fig.savefig(
        folder_path + '/results/microSpeedup2hmc_pei.pdf', bbox_inches='tight')

    plt.show()


if __name__ == '__main__':

    if len(sys.argv) != 2:
        print 'usage: ' + sys.argv[0] + ' folder_path'
        exit()
    main(sys.argv[1])