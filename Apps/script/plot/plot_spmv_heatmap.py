#!/usr/bin/python

import sys
import numpy as np
import matplotlib.pyplot as plt


def main(folder_path):

    folders = ['McSimARFtid', 'McSimARFaddr']
    names = ['arftid', 'arfaddr']
    schemes = ['ART-tid', 'ART-addr']
    benchmarks = ['spmv']
    cols = ['Operand buffer stalls', 'Update distribution']

    dfly_x = [0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 3, 3, 2, 2, 3, 3]
    dfly_y = [0, 1, 1, 0, 2, 3, 3, 2, 2, 3, 3, 2, 0, 1, 1, 0]

    plt.rc('legend', fontsize=14)
    plt.rc('font', size=14)
    #plt.rc('figure', figsize=(6, 5))

    result_file = open('spmv_heatmap.csv', 'w')
    for b, bench in enumerate(benchmarks):
        fig, axes = plt.subplots(2, 2, figsize=(8, 6))
        for s, scheme in enumerate(schemes):
            if bench == 'spmv':
                filename = folder_path + '/' + folders[s] + '/result/CasHMC_dfly_' + names[s] + '_' + bench + '_4096_0.3_result.log'
            else:
                filename = folder_path + '/' + folders[s] + '/result/CasHMC_dfly_' + names[s] + '_' + bench + '_result.log'

            infile = open(filename)
            for l, line in enumerate(infile):
                '''
                if 'Operand buffer stalls' in line:
                    line = [int(st) for st in line.split() if st.isdigit()]
                    heatmap = np.zeros((4, 4), dtype=np.int)
                    for i, num in enumerate(line):
                        heatmap[dfly_x[i]][dfly_y[i]] = num
                    heatmap = heatmap[::-1]
                    num = s * 3 + 1
                    ax = plt.subplot(2, 3, num)
                    print heatmap
                    im = ax.pcolormesh(heatmap, cmap='Reds')
                    major_ticks = np.arange(0, 4, 1)
                    ax.set_xticks(major_ticks)
                    ax.set_yticks(major_ticks)
                    ax.grid(True)
                    ax.set_xticklabels([])
                    ax.set_yticklabels([])
                    if num == 1:
                        ax.set_title('Operand buffer stalls\n', fontsize=14)
                        #ax.set_ylabel(scheme)
                        #ax.set_ylabel(scheme + ' (' + bench + ')')
                    #else:
                    #    ax.set_ylabel(scheme)
                    ax.set_ylabel(scheme + ' (' + bench + ')')
                    cb = fig.colorbar(im)
                    cb.formatter.set_powerlimits((0, 0))
                    cb.update_ticks()
                '''
                if 'Number of updates' in line:
                    line = [int(st) for st in line.split() if st.isdigit()]
                    heatmap = np.zeros((4, 4), dtype=np.int)
                    for i, num in enumerate(line):
                        heatmap[dfly_x[i]][dfly_y[i]] = num
                    result_file.write(scheme + ':\n')
                    result_file.write('compute point distribution:\n')
                    for row in range(0, 4):
                        for col in range(0, 4):
                            result_file.write(str(heatmap[row][col]) + ',')
                        result_file.write('\n')
                    result_file.write('\n')
                    heatmap = heatmap[::-1]
                    num = s * 2 + 1
                    ax = plt.subplot(2, 2, num)
                    im = ax.pcolormesh(
                        heatmap, cmap='Reds', vmin=0, vmax=400000)
                    major_ticks = np.arange(0, 4, 1)
                    ax.set_xticks(major_ticks)
                    ax.set_yticks(major_ticks)
                    ax.grid(True)
                    ax.set_xticklabels([])
                    ax.set_yticklabels([])
                    if num == 1:
                        ax.set_title(
                            'Compute point distribution\n', fontsize=14)
                    ax.set_ylabel(scheme + ' (' + bench + ')')
                    cb = fig.colorbar(im)
                    cb.formatter.set_powerlimits((0, 0))
                    cb.update_ticks()
                if 'Number of operands' in line:
                    line = [int(st) for st in line.split() if st.isdigit()]
                    heatmap = np.zeros((4, 4), dtype=np.int)
                    for i, num in enumerate(line):
                        heatmap[dfly_x[i]][dfly_y[i]] = num
                    result_file.write('operand distribution:\n')
                    for row in range(0, 4):
                        for col in range(0, 4):
                            result_file.write(str(heatmap[row][col]) + ',')
                        result_file.write('\n')
                    result_file.write('\n')
                    heatmap = heatmap[::-1]
                    num = s * 2 + 2
                    ax = plt.subplot(2, 2, num)
                    im = ax.pcolormesh(
                        heatmap, cmap='Reds', vmin=0, vmax=400000)
                    major_ticks = np.arange(0, 4, 1)
                    ax.set_xticks(major_ticks)
                    ax.set_yticks(major_ticks)
                    ax.grid(True)
                    ax.set_xticklabels([])
                    ax.set_yticklabels([])
                    if num == 2:
                        ax.set_title('Operand distribution\n', fontsize=14)
                    cb = fig.colorbar(im)
                    cb.formatter.set_powerlimits((0, 0))
                    cb.update_ticks()

        fig.savefig(
            folder_path + '/results/' + bench + 'Stalls.pdf',
            bbox_inches='tight')

    result_file.close()

    plt.show()


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'usage: ' + sys.argv[0] + ' folder_path'
        exit()
    main(sys.argv[1])
