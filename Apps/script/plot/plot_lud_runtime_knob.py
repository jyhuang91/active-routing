#!/usr/bin/env python
import sys
import os
import numpy as np
import re
import matplotlib.pyplot as plt
from matplotlib import gridspec
from easypyplot import barchart, color
from easypyplot import format as fmt

if len(sys.argv) < 3:
    print 'usage: ' + sys.argv[0] + ' log1 log2 log3 ...'
    exit()
else:
    logfiles = sys.argv[1:-2]
    xmax = sys.argv[-2:-1]
    freq = sys.argv[-1:]

name_legends = ['ART-tid', 'ART-tid-Adaptive']
#for logfile in logfiles:
#    name_legends.append(os.path.basename(logfile))
'''
whatToDisplay = ['IPC', 'L1', 'L2', 'memaccs', 'updateaccs', 'gatheraccs',\
                 'load-amat', 'store-amat', 'req_noc_lat']
'''
whatToDisplay = ['cycles']
'''
  -- [     2529120]:    1000003 instrs so far,
  IPC=   3.953,
  L1 (acc, miss)=(  786744,   7104),
  L2 (acc, miss)=(  48749,    591),
  591 mem accs,
  (86,   86) touched pages (this time, 1stly),
  0 update accs,
  0 gather accs,
  avg_dd=  8.003,
  0 back-inv,
  133.728 req_noc_lat,
  0 noc_lat,
  0 hmc_stall_lat,
  0 update_rr_lat,
  0 gather_rr_lat,

  -- [    14930710]:   12000049 instrs so far,
  IPC=   3.988,
  L1 (acc, miss)=(  543831,    524),
  L2 (acc, miss)=(   5214,    226),
  226 mem accs,
  (   25,    0) touched pages (this time, 1stly),
  31377 update accs,
  1083 gather accs,
  avg_dd= 12.557,
  235 back-inv,
  39.2406 update-noc-lat,
  53.9485 update-stall-lat,
  117.33 update-roundtrip-lat,
  3512.81 gather-roundtrip-lat,
  12.4839 load-amat,
  28.2703 store-amat,
  158.355 req_noc_lat,
'''

patterns = {                                    \
        'IPC': "IPC=(\d+\.\d+),",               \
        'cycles': "cycles=(\d+),",               \
        'L1': "L1\(acc,miss\)=\((\d+),(\d+)\)", \
        'L2': "L2\(acc,miss\)=\((\d+),(\d+)\)", \
        'memaccs': "(\d+)memaccs",             \
        'touchedpages': "\((\d+),(\d+)\)touchedpages", \
        'updateaccs': "(\d+)updateaccs",       \
        'gatheraccs': "(\d+)gatheraccs",       \
        'avg_dd': "avg_dd=(\d+\.\d+)",          \
        'back-inv': "(\d+)back-inv",            \
        'update-noc-lat': "(\d+\.?\d*)update-noc-lat",              \
        'update-stall-lat': "(\d+\.?\d*)update-stall-lat",          \
        'update-roundtrip-lat': "(\d+\.?\d*)update-roundtrip-lat",  \
        'load-amat': "(\d+\.?\d*)load-amat",     \
        'store-amat': "(\d+\.?\d*)store-amat",   \
        'req_noc_lat': "(\d+\.?\d*)req_noc_lat", \
        }


def parse_stat(patterns, line):
    stat = {}
    line = line.strip('\n').replace(' ', '')

    for k, p in patterns.iteritems():
        m = re.search(p, line)
        if m:
            if len(m.groups()) > 1 and m.group(2) != None:
                stat[k] = m.groups()
            else:
                stat[k] = m.group(1)
    return stat


def parse_ipc(line):
    left, right = line.split('=')
    return [float((right.strip())[:-1])]


def hit_rate(pair):
    acc, miss = pair
    if float(acc) == 0.0:
        return 0.0
    return (float(acc) - float(miss)) / float(acc)


stats = {}
cycles = {}
for logfile in logfiles:
    cnt = 0
    stats[logfile] = []
    cycles[logfile] = 0
    for l in open(logfile, 'r'):
        if re.match("(.*)instrs so far,(.*)", l):
            s = parse_stat(patterns, l)
            stats[logfile].append(s)
            if cnt < int(xmax[0]):
                #print cnt
                cycles[logfile] += int(s['cycles'])
                cnt += 1
total_cycles = []
for logfile in logfiles:
    total_cycles.append(float(cycles[logfile]))

speedup = []
#print total_cycles
for cycle in total_cycles:
    speedup.append([float(total_cycles[0]) / cycle])

dataToDisplay = {}
for logfile in logfiles:
    dataToDisplay[logfile] = []
    for k in whatToDisplay:
        tmp = []
        for stat in stats[logfile]:
            #print stat
            #print k
            if k == 'L1' or k == 'L2':
                tmp.append(hit_rate(stat[k]))
            else:
                tmp.append(stat[k])
        dataToDisplay[logfile].append(tmp)

# dump csv file
result_file = open('lud_phase_analysis.csv', mode='w')
result_file.write('Phases,ART-tid,ART-tid-adaptive\n')
for logfile in logfiles:
    for cycle in dataToDisplay[logfile][0]:
        result_file.write(str(cycle) + ',')
    result_file.write('\n')
result_file.write('\n\nspeedup:\n')
result_file.write('ART-tid,ART-tid-adaptive\n')
for s in speedup:
    result_file.write(str(s[0]) + ',')
result_file.write('\n')
result_file.close()

# plotting preparation

f_font = {
    #'weight' : 'bold',
    'size': 18
}
f_axes = {
    #'titleweight': 'bold',
    'titlesize': 18,
    'labelsize': 18,
    #'labelweight':'bold'
}
f_xtick = {
    'labelsize': 18,
}
f_ytick = {
    'labelsize': 18,
}
f_legend = {
    'fontsize': 18,  #22,
}
f_figure = {
    'titlesize': 18,
    #'titleweight' : 'bold'
}
plt.rc('font', **f_font)  # controls default text sizes
plt.rc('axes', **f_axes)  # fontsize of the axes title
plt.rc('xtick', **f_xtick)  # fontsize of the tick labels
plt.rc('ytick', **f_ytick)  # fontsize of the tick labels
plt.rc('legend', **f_legend)  # legend fontsize
plt.rc('figure', **f_figure)  # fontsize of the figure title

#plt.rc('legend', fontsize=20)
#plt.rc('font', size=20)

fig = plt.figure(figsize=(13, 6.6))
fig.tight_layout()
#fig.set_size_inches(18.5, 10.5, forward=True)
plt.subplots_adjust(wspace=0.26, left=0.16, right=0.97, top=0.95)

#gs = gridspec.GridSpec(3, (len(whatToDisplay)/3) + (len(whatToDisplay) % 3))
gs = gridspec.GridSpec(1, 2, width_ratios=[5, 3])

#whatToDisplay         = ['IPC',                 'Mem accesses',           'L1 miss rate']
#log1                  = [ [v1,v2,...],           [v1,v2,v3...],           [v1, v2, v3, ...] ]

lines = []
plt.xticks(np.arange(0, int(xmax[0]), step=50))
#plt.xticks(np.arange(0, 200, step=15))
ax0 = plt.subplot(gs[0])
ax0.set_xlim([0, int(xmax[0])])
ax0.set_ylim([0, 2500000])
#ax0.set_xlim([0,200])
#ax0.set_yticks(np.arange(0, 1500001, step=500000))
for i, logfile in enumerate(logfiles):
    if (i != 1):
        line, = ax0.plot(
            map(float, dataToDisplay[logfile][0]),
            label=name_legends[i],
            linewidth=2,
            color='#8c96c6')  ## 0868ac   dd1c77
    else:
        line, = ax0.plot(
            map(float, dataToDisplay[logfile][0]),
            label=name_legends[i],
            linewidth=2,
            color='#dd1c77')

    lines.append(line)

ax0.yaxis.grid(True)
ax0.set_ylabel('Cycles', fontsize=24)
ax0.set_xlabel('Phases', fontsize=24)
ax0.legend(loc='upper left', fontsize=20)

#ax = plt.axes([.12,.93,.78,.05], frameon=True)
#plt.xticks([]), plt.yticks([])
#ax.legend(lines, name_legends, loc='center', mode='expand', ncol=2, frameon=False)

#====

plt.text(41, 900000, r'First Phase', fontsize=20)
plt.arrow(
    41, 900000, 0, -200000, head_width=1.5, head_length=30000, fc='k', ec='k')
plt.text(42, 30000, r'Second Phase', fontsize=20)
plt.arrow(
    42, 70000, 0, 90000, head_width=1.5, head_length=30000, fc='k', ec='k')

ax1 = plt.subplot(gs[1])
#colors = ['#e0f3db', '#a8ddb5', '#43a2ca']
colors = ['#a8ddb5', '#43a2ca']

gnames = ['ART-tid', 'ART-tid-Adaptive']

#print gnames
hdls = barchart.draw(
    ax1,
    speedup,
    group_names=gnames,
    colors=colors,
    breakdown=False,
    legendloc='upper center',
    xticklabelrotation=10)
ax1.yaxis.grid(True, linestyle='--')
ax1.set_ylabel('Speedup', fontsize=24)

f = ''
for logf in logfiles:
    tmp = os.path.basename(logf)
    tmp = os.path.splitext(tmp)[0]
    f = f + os.path.basename(tmp)
plt.savefig('hpcaLudPhase.pdf', format='pdf')
plt.show()
