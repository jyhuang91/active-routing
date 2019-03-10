#!/usr/bin/python

import sys
import numpy as np
import re
import matplotlib.pyplot as plt
from scipy import stats
from easypyplot import barchart, color
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

    folders = ['McSimART', 'McSimARFtid', 'McSimARFaddr']
    names = ['pei', 'arftid', 'arfaddr']
    schemes = ['PEI', 'ART-tid', 'ART-addr']
    benchmarks = ['backprop', 'lud', 'pagerank', 'sgemm', 'spmv']
    xlabels = ['backprop', 'lud', 'pagerank', 'sgemm', 'spmv', 'gmean']
    microbenches = ['reduce', 'rand_reduce', 'mac', 'rand_mac']
    microxlabels = ['reduce', 'rand_reduce', 'mac', 'rand_mac']
    entry_names = ['norm_req', 'active_req', 'norm_resp', 'active_resp']
    eff_entry_names = ['request', 'response']
    group_names = []
    micro_group_names = []

    read_request_size = 16  # bytes, 1 flit packet
    read_response_size = 64 + 16  # bytes, 64B data payload + 1 head-tail flit
    write_request_size = 64 + 16  # bytes, 64B data payload + 1 head-tail flit
    write_response_size = 16  # bytes, 1 flit packet
    update_request_size = 16  # bytes
    update_request_size = [[64, 64, 16, 64, 64], [16, 64, 16, 16, 64],
                           [16, 64, 16, 16, 64]]  # bytes
    micro_update_request_size = [[16, 16, 64, 16], [16, 16, 16, 16],
                                 [16, 16, 16, 16]]
    gather_request_size = 16  # bytes
    gather_responze_size = 16  # bytes

    read_request = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    write_request = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    update_request = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    gather_request = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    gather_response = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    total_response = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    bw_breakdown = np.zeros(
        (4, int(len(benchmarks) * len(schemes))), dtype=np.float)
    norm_bw_breakdown = np.zeros(
        (4, int(len(benchmarks) * len(schemes))), dtype=np.float)

    micro_read_request = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_write_request = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_update_request = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_gather_request = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_gather_response = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_total_response = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_bw_breakdown = np.zeros(
        (4, int(len(microbenches) * len(schemes))), dtype=np.float)
    norm_micro_bw_breakdown = np.zeros(
        (4, int(len(microbenches) * len(schemes))), dtype=np.float)

    # benchmark
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
            filename = folder_path + '/' + folders[s] + '/result/CasHMC_dfly_' + names[s] + '_' + bench + '_result.log'

            infile = open(filename)
            req_skip = True
            resp_skip = True
            for l, line in enumerate(infile):
                if 'LK_U' in line:
                    assert resp_skip == True
                    resp_skip = False
                elif resp_skip == False:
                    if 'Response per link' in line:
                        line = line.split()
                        total_response[s][b] += np.float(line[5])
                        resp_skip = True
                elif 'HC' in line and 'LK_D' in line:
                    assert req_skip == True
                    req_skip = False
                elif req_skip == False:
                    if 'Read per link' in line:
                        line = line.split()
                        read_request[s][b] += np.float(line[5])
                    elif 'Write per link' in line:
                        line = line.split()
                        write_request[s][b] += np.float(line[5])
                    elif 'Atomic per link' in line:
                        line = line.split()
                        update_request[s][b] += np.float(line[5])
                        req_skip = True
                else:
                    continue

            gathers = total_response[s][b] - read_request[s][b] - write_request[s][b]
            gather_request[s][b] = gathers
            gather_response[s][b] = gathers

            bw_breakdown[0][
                b * len(schemes) +
                s] = read_request[s][b] * read_request_size + write_request[s][b] * write_request_size
            bw_breakdown[1][
                b * len(schemes) +
                s] = update_request[s][b] * update_request_size[s][b] + gather_request[s][b] * gather_request_size
            bw_breakdown[2][
                b * len(schemes) +
                s] = read_request[s][b] * read_response_size + write_request[s][b] * write_response_size
            bw_breakdown[3][b * len(schemes)
                            + s] = gather_response[s][b] * gather_responze_size

    # normalization
    for b, bench in enumerate(benchmarks):
        for s, scheme in enumerate(schemes):
            bw = bw_breakdown[0][b * len(schemes)] + bw_breakdown[1][b * len(
                schemes)] + bw_breakdown[2][b * len(
                    schemes)] + bw_breakdown[3][b * len(schemes)]
            for e, entry in enumerate(entry_names):
                norm_bw_breakdown[e][b * len(schemes) + s] = bw_breakdown[e][
                    b * len(schemes) + s] / bw

    # microbenchmark
    for b, microbench in enumerate(microbenches):
        for s, scheme in enumerate(schemes):
            micro_group_names.append(scheme)
            filename = folder_path + '/' + folders[s] + '/result/CasHMC_dfly_' + names[s] + '_' + microbench + '_result.log'

            infile = open(filename)
            req_skip = True
            resp_skip = True
            for l, line in enumerate(infile):
                if 'LK_U' in line:
                    assert resp_skip == True
                    resp_skip = False
                elif resp_skip == False:
                    if 'Response per link' in line:
                        line = line.split()
                        micro_total_response[s][b] += np.float(line[5])
                        resp_skip = True
                elif 'HC' in line and 'LK_D' in line:
                    assert req_skip == True
                    req_skip = False
                elif req_skip == False:
                    if 'Read per link' in line:
                        line = line.split()
                        micro_read_request[s][b] += np.float(line[5])
                    elif 'Write per link' in line:
                        line = line.split()
                        micro_write_request[s][b] += np.float(line[5])
                    elif 'Atomic per link' in line:
                        line = line.split()
                        micro_update_request[s][b] += np.float(line[5])
                        req_skip = True
                else:
                    continue

            micro_gathers = micro_total_response[s][b] - micro_read_request[s][b] - micro_write_request[s][b]
            micro_gather_request[s][b] = gathers
            micro_gather_response[s][b] = gathers

            micro_bw_breakdown[0][
                b * len(schemes) +
                s] = micro_read_request[s][b] * read_request_size + micro_write_request[s][b] * write_request_size
            micro_bw_breakdown[1][
                b * len(schemes) +
                s] = micro_update_request[s][b] * micro_update_request_size[s][b] + micro_gather_request[s][b] * gather_request_size
            micro_bw_breakdown[2][
                b * len(schemes) +
                s] = micro_read_request[s][b] * read_response_size + micro_write_request[s][b] * write_response_size
            micro_bw_breakdown[3][b * len(schemes)
                                  + s] = micro_gather_response[s][
                                      b] * gather_responze_size

    # normalization
    for b, microbench in enumerate(microbenches):
        for s, scheme in enumerate(schemes):
            bw = micro_bw_breakdown[0][b * len(
                schemes)] + micro_bw_breakdown[1][b * len(
                    schemes)] + micro_bw_breakdown[2][b * len(
                        schemes)] + micro_bw_breakdown[3][b * len(schemes)]
            for e, entry in enumerate(entry_names):
                norm_micro_bw_breakdown[e][b * len(schemes)
                                           + s] = micro_bw_breakdown[e][
                                               b * len(schemes) + s] / bw

    # write to csv file
    result_file = open('data_movement.csv', mode='w')
    result_file.write('data movement:\n')
    result_file.write(',,norm_req,active_req,norm_resp,active_resp')
    for b, benchmark in enumerate(benchmarks):
        row = '\n' + benchmark + ',PEI,' + str(
            norm_bw_breakdown[0][b * len(schemes)]) + ',' + str(
                norm_bw_breakdown[1][b * len(schemes)]) + ',' + str(
                    norm_bw_breakdown[2][b * len(schemes)]) + ',' + str(
                        norm_bw_breakdown[3][b * len(schemes)]) + '\n'
        result_file.write(row)
        row = ',ART-tid,' + str(
            norm_bw_breakdown[0][b * len(schemes) + 1]) + ',' + str(
                norm_bw_breakdown[1][b * len(schemes) + 1]) + ',' + str(
                    norm_bw_breakdown[2][b * len(schemes) + 1]) + ',' + str(
                        norm_bw_breakdown[3][b * len(schemes) + 1]) + '\n'
        result_file.write(row)
        row = ',ART-addr,' + str(
            norm_bw_breakdown[0][b * len(schemes) + 2]) + ',' + str(
                norm_bw_breakdown[1][b * len(schemes) + 2]) + ',' + str(
                    norm_bw_breakdown[2][b * len(schemes) + 2]) + ',' + str(
                        norm_bw_breakdown[3][b * len(schemes) + 2]) + '\n\n'
        result_file.write(row)

    result_file.write(',,norm_req,active_req,norm_resp,active_resp')
    for b, benchmark in enumerate(microbenches):
        row = '\n' + benchmark + ',PEI,' + str(
            norm_micro_bw_breakdown[0][b * len(schemes)]) + ',' + str(
                norm_micro_bw_breakdown[1][b * len(schemes)]) + ',' + str(
                    norm_micro_bw_breakdown[2][b * len(schemes)]) + ',' + str(
                        norm_bw_breakdown[3][b * len(schemes)]) + '\n'
        result_file.write(row)
        row = ',ART-tid,' + str(
            norm_micro_bw_breakdown[0][b * len(schemes) + 1]) + ',' + str(
                norm_micro_bw_breakdown[1][b * len(schemes) + 1]) + ',' + str(
                    norm_micro_bw_breakdown[2][b * len(schemes) + 1]
                ) + ',' + str(
                    norm_micro_bw_breakdown[3][b * len(schemes) + 1]) + '\n'
        result_file.write(row)
        row = ',ART-addr,' + str(
            norm_micro_bw_breakdown[0][b * len(schemes) + 2]) + ',' + str(
                norm_micro_bw_breakdown[1][b * len(schemes) + 2]) + ',' + str(
                    norm_micro_bw_breakdown[2][b * len(schemes) + 2]
                ) + ',' + str(
                    norm_micro_bw_breakdown[3][b * len(schemes) + 2]) + '\n\n'
        result_file.write(row)

    # plot setting
    colors = ['#f0f9e8', '#bae4bc', '#7bccc4', '#2b8cbe']
    plt.rc('legend', fontsize=18)
    plt.rc('font', size=18)

    # benchmark  bandwidth
    xticks = []
    for i in range(0, len(benchmarks)):
        for j in range(0, len(schemes)):
            xticks.append(i * (len(schemes) + 1) + j)
    data = [list(i) for i in zip(*norm_bw_breakdown)]
    data = np.array(data, dtype=np.float64)

    #fig = plt.figure(figsize=(6.4, 4))
    fig = plt.figure(figsize=(8.7, 5.5))
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
        legendncol=4,
        xticklabelfontsize=16,
        xticklabelrotation=90,
        log=False)
    #fig.autofmt_xdate()
    #ax.set_ylabel('Bandwidth (GB)')
    #ax.set_ylim([0, 3])
    ax.set_ylabel('Normalized Data Movement')
    ax.yaxis.grid(True, linestyle='--')
    ax.legend(
        hdls,
        entry_names,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=4,
        frameon=False,
        handletextpad=0.1,
        columnspacing=0.3)
    fmt.resize_ax_box(ax, hratio=0.8)

    ly = len(benchmarks)
    scale = 1. / ly
    ypos = -.4
    pos = 0
    for pos in xrange(ly + 1):
        lxpos = (pos + 0.5) * scale
        if pos < ly:
            ax.text(
                lxpos, ypos, xlabels[pos], ha='center', transform=ax.transAxes)
        add_line(ax, pos * scale, ypos)
    fig.savefig(folder_path + '/results/benchBW.pdf', bbox_inches='tight')

    # microbenchmark  bandwidth
    xticks = []
    for i in range(0, len(microbenches)):
        for j in range(0, len(schemes)):
            xticks.append(i * (len(schemes) + 1) + j)
    data = [list(i) for i in zip(*norm_micro_bw_breakdown)]
    data = np.array(data, dtype=np.float64)

    #fig = plt.figure(figsize=(6.4, 4))
    fig = plt.figure(figsize=(8.7, 5.5))
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
        legendncol=4,
        xticklabelfontsize=16,
        xticklabelrotation=90,
        log=False)
    #fig.autofmt_xdate()
    #ax.set_ylabel('Bandwidth (GB)')
    #ax.set_ylim([0, 3])
    ax.set_ylabel('Normalized Data Movement')
    ax.yaxis.grid(True, linestyle='--')
    ax.legend(
        hdls,
        entry_names,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=4,
        frameon=False,
        handletextpad=0.1,
        columnspacing=0.3)
    fmt.resize_ax_box(ax, hratio=0.8)

    ly = len(microbenches)
    scale = 1. / ly
    ypos = -.4
    pos = 0
    for pos in xrange(ly + 1):
        lxpos = (pos + 0.5) * scale
        if pos < ly:
            ax.text(
                lxpos, ypos, xlabels[pos], ha='center', transform=ax.transAxes)
        add_line(ax, pos * scale, ypos)
    fig.savefig(folder_path + '/results/microBW.pdf', bbox_inches='tight')

    plt.show()


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'usage: ' + sys.argv[0] + ' folder_path'
        exit()
    main(sys.argv[1])
