#!/usr/bin/python

import sys
import numpy as np
import re, string, fpformat
import matplotlib.pyplot as plt
from scipy import stats
from easypyplot import barchart, color
from easypyplot import format as fmt
from optparse import OptionParser
from pinsim_log_parser import *


def add_line(ax, xpos, ypos):
    line = plt.Line2D(
        [xpos, xpos], [0, ypos],
        transform=ax.transAxes,
        color='black',
        linewidth=1)
    line.set_clip_on(False)
    ax.add_line(line)


def main(folder_path):

    usage = 'usage: %prog [options]'
    parser = OptionParser(usage)

    parser.add_option(
        "--mdfile",
        action="store",
        type="string",
        default="../Apps/md/md-16o3core2GHz-mesh-hmc.py",
        dest="mdfile",
        help="specify the machine description file (default = /dev/null)")
    parser.add_option(
        "--ipcfile",
        action="store",
        type="string",
        default="/dev/null",
        dest="ipcfile",
        help="specify the output ipc trace file (default = /dev/null)")
    parser.add_option(
        "--ticks_per_cycle",
        action="store",
        type="string",
        default="10",
        dest="ticks_per_cycle",
        help=
        "specify the number of ticks per cycle (default = 10, valid only if mdfile == /dev/null"
    )

    (options, args) = parser.parse_args()
    md = MD(options.mdfile, options.ticks_per_cycle)

    folders = ['McSimART', 'McSimARFtid', 'McSimARFaddr']
    names = ['pei', 'arftid', 'arfaddr']
    schemes = ['PEI', 'ART-tid', 'ART-addr']
    benchmarks = ['backprop', 'lud', 'pagerank', 'sgemm', 'spmv']
    benchnames = ['backprop', 'lud', 'pagerank', 'sgemm', 'spmv', 'gmean']
    update_granularities = [[4, 4, 1, 4, 4], [16, 2, 1, 16, 2],
                            [16, 2, 1, 16, 2]]
    microbenches = ['reduce', 'rand_reduce', 'mac', 'rand_mac']
    micronames = ['reduce', 'rand_reduce', 'mac', 'rand_mac', 'gmean']
    micro_update_granularities = [[1, 1, 4, 1], [16, 1, 16, 1], [16, 1, 16, 1]]

    # energy values
    # - hmc
    LinkAccEnergy = 5  # 5 pJ/bit
    HMCAccEnergy = 12  # 12 pJ/bit
    # - dram
    DramAccEnergy = 39  # 39 pJ/bit
    DramAccSize = 64  # bytes

    # link data size
    LkCtrlSize = 32  # bytes
    LkDataSize = 96  # bytes
    ActLkDataSize = 32  # bytes

    # memory access size
    NormMemAccSize = 64  # bytes
    UpdateTwoOpSize = 8  # bytes
    UpdateOneOpSize = 4  # bytes
    PeiUpdateOpSize = 64  # bytes

    # cpu statistic arrays
    clk = 0.25e-9  #0.5e-9
    # - benchmark
    cache_power = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    cache_energy = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    cycles = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    runtime = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    # - microbenchmark
    micro_cache_power = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_cache_energy = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_cycles = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_runtime = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)

    # memory statistic arrays
    # - benchmark
    reads = np.zeros((int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    writes = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    updates = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    gathers = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    actives = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    mem_acc_energy = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    mem_acc_power = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    # - microbenchmark
    micro_reads = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_writes = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_updates = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_gathers = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_actives = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_mem_acc_energy = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_mem_acc_power = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)

    # link statistic arrays
    # - benchmark
    link_read_req = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    link_write_req = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    link_resp = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    link_active_req = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    link_active_resp = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    link_energy = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    link_power = np.zeros(
        (int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    # - microbenchmark
    micro_link_read_req = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_link_write_req = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_link_resp = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_link_active_req = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_link_active_resp = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_link_energy = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    micro_link_power = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)

    # energy-delay product (edp)
    # - benchmark
    edp = np.zeros((int(len(schemes)), int(len(benchmarks))), dtype=np.float)
    norm_edp = np.zeros(
        (int(len(schemes)), int(len(benchmarks) + 1)), dtype=np.float)
    normhmc_edp = np.zeros(
        (int(len(schemes) - 1), int(len(benchmarks) + 1)), dtype=np.float)
    # - microbenchmark
    micro_edp = np.zeros(
        (int(len(schemes)), int(len(microbenches))), dtype=np.float)
    norm_micro_edp = np.zeros(
        (int(len(schemes)), int(len(microbenches) + 1)), dtype=np.float)
    normhmc_micro_edp = np.zeros(
        (int(len(schemes) - 1), int(len(microbenches) + 1)), dtype=np.float)

    # normalized power breakdown (cache, mem, link), energy breakdown
    # - benchmark
    power_breakdown = np.zeros(
        (3, int(len(schemes) * len(benchmarks))), dtype=np.float)
    energy_breakdown = np.zeros(
        (3, int(len(schemes) * len(benchmarks))), dtype=np.float)
    # - microbenchmark
    micro_power_breakdown = np.zeros(
        (3, int(len(schemes) * len(microbenches))), dtype=np.float)
    micro_energy_breakdown = np.zeros(
        (3, int(len(schemes) * len(microbenches))), dtype=np.float)

    # benchmark
    for s, scheme in enumerate(schemes):
        for b, bench in enumerate(benchmarks):
            if bench == 'lud':
                #bench = bench + '_s128'
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
                logfile = folder_path + '/' + folders[s] + '/dram_log/' + names[s] + '_' + bench + '.log'
            else:
                logfile = folder_path + '/' + folders[s] + '/dfly_log/' + names[s] + '_' + bench + '.log'
            hmcfile = folder_path + '/' + folders[s] + '/result/CasHMC_dfly_' + names[s] + '_' + bench + '_result.log'

            stat = Stat(md)
            stat.parse(logfile, options.ipcfile)
            cpu_stat = stat.compute_energydelay()

            cache_power[s][b] = cpu_stat[-1]  # in W
            cycles[s][b] = cpu_stat[10]
            runtime[s][b] = cycles[s][b] * clk  # in second

            infile = open(logfile)
            for l, line in enumerate(infile):
                if 'rd, wr, act, pre' in line:
                    line = line.split()
                    reads[s][b] += np.float(filter(str.isdigit, line[11]))
                    writes[s][b] += np.float(filter(str.isdigit, line[12]))
                elif '-- total number of updates' in line:
                    line = line.split()
                    updates[s][b] = np.float(
                        line[6]) * update_granularities[s][b]
                elif '-- total number of gathers' in line:
                    line = line.split()
                    gathers[s][b] = np.float(line[6])
                else:
                    continue

            if scheme == 'DRAM':
                continue

            infile = open(hmcfile)
            hc_skip = True
            hmc_skip = True
            for l, line in enumerate(infile):
                if 'HC' in line and 'LK_D' in line:
                    assert (hc_skip == True)
                    hc_skip = False
                elif 'HMC' in line and 'LK' in line:
                    assert (hmc_skip == True)
                    hmc_skip = False
                elif hc_skip == False:
                    if 'Read per link' in line:
                        line = line.split()
                        reads[s][b] += np.float(line[5])
                        link_read_req[s][b] += np.float(line[5])
                    elif 'Write per link' in line:
                        line = line.split()
                        writes[s][b] += np.float(line[5])
                        link_write_req[s][b] += np.float(line[5])
                    elif 'Atomic per link' in line:
                        line = line.split()
                        actives[s][b] += np.float(line[5])
                        link_active_req[s][b] += np.float(line[5])
                        hc_skip = True
                    else:
                        continue
                elif hmc_skip == False:
                    if 'Read per link' in line:
                        line = line.split()
                        link_read_req[s][b] += np.float(line[5])
                    elif 'Write per link' in line:
                        line = line.split()
                        link_write_req[s][b] += np.float(line[5])
                    elif 'Atomic per link' in line:
                        line = line.split()
                        link_active_req[s][b] += np.float(line[5])
                    elif 'Response per link' in line:
                        line = line.split()
                        link_resp[s][b] += np.float(line[5])
                        hmc_skip = True
                    else:
                        continue

    # microbenchmark
    for s, scheme in enumerate(schemes):
        for b, bench in enumerate(microbenches):
            if scheme == 'DRAM':
                logfile = folder_path + '/' + folders[s] + '/dram_log/' + names[s] + '_' + bench + '.log'
            else:
                logfile = folder_path + '/' + folders[s] + '/dfly_log/' + names[s] + '_' + bench + '.log'
            hmcfile = folder_path + '/' + folders[s] + '/result/CasHMC_dfly_' + names[s] + '_' + bench + '_result.log'

            stat = Stat(md)
            stat.parse(logfile, options.ipcfile)
            micro_cpu_stat = stat.compute_energydelay()

            micro_cache_power[s][b] = micro_cpu_stat[-1]  # in W l2$
            micro_cycles[s][b] = micro_cpu_stat[10]
            micro_runtime[s][b] = micro_cycles[s][b] * clk  # in second

            infile = open(logfile)
            for l, line in enumerate(infile):
                if 'rd, wr, act, pre' in line:
                    line = line.split()
                    micro_reads[s][b] += np.float(
                        filter(str.isdigit, line[11]))
                    micro_writes[s][b] += np.float(
                        filter(str.isdigit, line[12]))
                elif '-- total number of updates' in line:
                    line = line.split()
                    micro_updates[s][b] = np.float(
                        line[6]) * micro_update_granularities[s][b]
                elif '-- total number of gathers' in line:
                    line = line.split()
                    micro_gathers[s][b] = np.float(line[6])
                else:
                    continue

            if scheme == 'DRAM':
                continue

            infile = open(hmcfile)
            hc_skip = True
            hmc_skip = True
            for l, line in enumerate(infile):
                if 'HC' in line and 'LK_D' in line:
                    assert (hc_skip == True)
                    hc_skip = False
                elif 'HMC' in line and 'LK' in line:
                    assert (hmc_skip == True)
                    hmc_skip = False
                elif hc_skip == False:
                    if 'Read per link' in line:
                        line = line.split()
                        micro_reads[s][b] += np.float(line[5])
                        micro_link_read_req[s][b] += np.float(line[5])
                    elif 'Write per link' in line:
                        line = line.split()
                        micro_writes[s][b] += np.float(line[5])
                        micro_link_write_req[s][b] += np.float(line[5])
                    elif 'Atomic per link' in line:
                        line = line.split()
                        micro_actives[s][b] += np.float(line[5])
                        micro_link_active_req[s][b] += np.float(line[5])
                        hc_skip = True
                    else:
                        continue
                elif hmc_skip == False:
                    if 'Read per link' in line:
                        line = line.split()
                        micro_link_read_req[s][b] += np.float(line[5])
                    elif 'Write per link' in line:
                        line = line.split()
                        micro_link_write_req[s][b] += np.float(line[5])
                    elif 'Atomic per link' in line:
                        line = line.split()
                        micro_link_active_req[s][b] += np.float(line[5])
                    elif 'Response per link' in line:
                        line = line.split()
                        micro_link_resp[s][b] += np.float(line[5])
                        hmc_skip = True
                    else:
                        continue

    # cache energy in J
    # - benchmark
    cache_energy = cache_power * runtime
    # - microbenchmark
    micro_cache_energy = micro_cache_power * micro_runtime

    # memory access energy in J
    # - benchmark
    #mem_acc_energy[0] = (
    #    reads[0] + writes[0]) * DramAccSize * 8 * DramAccEnergy
    mem_acc_energy[0] = (
        (reads[0] + writes[0]) * NormMemAccSize + updates[0] * UpdateOneOpSize
    ) * 8 * HMCAccEnergy
    mem_acc_energy[1:, :] = ((reads[1:, :] + writes[1:, :]) * NormMemAccSize +
                             updates[1:, :] * UpdateTwoOpSize
                             ) * 8 * HMCAccEnergy  # cache granularity
    mem_acc_energy = mem_acc_energy * 1e-12
    # - microbenchmark
    micro_mem_acc_energy[0] = (
        micro_reads[0] + micro_writes[0]) * DramAccSize * 8 * DramAccEnergy
    micro_mem_acc_energy[1:, :] = (
        (micro_reads[1:, :] + micro_writes[1:, :]) * NormMemAccSize +
        micro_updates[1:, :] * UpdateTwoOpSize) * 8 * HMCAccEnergy
    micro_mem_acc_energy = micro_mem_acc_energy * 1e-12

    # link energy in J
    # - benchmark
    link_active_resp = link_resp - link_read_req - link_write_req
    link_energy = ((link_read_req + link_write_req) *
                   (LkCtrlSize + LkDataSize) +
                   (link_active_req + link_active_resp) * ActLkDataSize
                   ) * 8 * LinkAccEnergy
    link_energy = link_energy * 1e-12
    # - microbenchmark
    micro_link_active_resp = micro_link_resp - micro_link_read_req - micro_link_write_req
    micro_link_energy = (
        (micro_link_read_req + micro_link_write_req) *
        (LkCtrlSize + LkDataSize) +
        (micro_link_active_req + micro_link_active_resp) * ActLkDataSize
    ) * 8 * LinkAccEnergy
    micro_link_energy = micro_link_energy * 1e-12

    # power in W
    # - benchmark
    mem_acc_power = mem_acc_energy / runtime
    link_power = link_energy / runtime
    # - microbenchmark
    micro_mem_acc_power = micro_mem_acc_energy / micro_runtime
    micro_link_power = micro_link_energy / micro_runtime

    # energy-delay product
    # - benchmark
    edp = (cache_energy + mem_acc_energy + link_energy) * runtime
    norm_edp[:, 0:-1] = edp / np.tile(edp[0][:], (len(schemes), 1))
    normhmc_edp[:, 0:-1] = edp[1:, 0:] / np.tile(edp[1][:],
                                                 (len(schemes) - 1, 1))
    # - microbenchmark
    micro_edp = (micro_cache_energy + micro_mem_acc_energy + micro_link_energy
                 ) * micro_runtime
    norm_micro_edp[:, 0:-1] = micro_edp / np.tile(micro_edp[0][:],
                                                  (len(schemes), 1))
    normhmc_micro_edp[:, 0:-1] = micro_edp[1:, 0:] / np.tile(
        micro_edp[1][:], (len(schemes) - 1, 1))
    # - geomean for both benchmark and microbenchmark
    for s in xrange(len(schemes)):
        norm_edp[s][-1] = stats.mstats.gmean(norm_edp[s][0:-1])
        norm_micro_edp[s][-1] = stats.mstats.gmean(norm_micro_edp[s][0:-1])
        if s < len(schemes) - 1:
            normhmc_edp[s][-1] = stats.mstats.gmean(norm_edp[s][0:-1])
            normhmc_micro_edp[s][-1] = stats.mstats.gmean(
                norm_micro_edp[s][0:-1])

    # normalized breakdown
    # - benchmark
    total_power = np.zeros(
        (1, int(len(schemes) * len(benchmarks))), dtype=np.float)
    total_energy = np.zeros(
        (1, int(len(schemes) * len(benchmarks))), dtype=np.float)
    for b, bench in enumerate(benchmarks):
        for s, scheme in enumerate(schemes):
            power_breakdown[0][b * len(schemes) + s] = cache_power[s][b]
            power_breakdown[1][b * len(schemes) + s] = mem_acc_power[s][b]
            power_breakdown[2][b * len(schemes) + s] = link_power[s][b]

            energy_breakdown[0][b * len(schemes) + s] = cache_energy[s][b]
            energy_breakdown[1][b * len(schemes) + s] = mem_acc_energy[s][b]
            energy_breakdown[2][b * len(schemes) + s] = link_energy[s][b]

            if scheme == 'PEI':
                tmp_power = cache_power[s][b] + mem_acc_power[s][b] + link_power[s][b]
                tmp_energy = cache_energy[s][b] + mem_acc_energy[s][b] + link_energy[s][b]
                total_power[0][b * len(schemes)] = tmp_power
                total_energy[0][b * len(schemes)] = tmp_energy
            else:
                total_power[0][b * len(schemes)
                               + s] = total_power[0][b * len(schemes)]
                total_energy[0][b * len(schemes)
                                + s] = total_energy[0][b * len(schemes)]

    power_breakdown = power_breakdown / np.tile(total_power, (3, 1))
    energy_breakdown = energy_breakdown / np.tile(total_energy, (3, 1))
    log_norm_edp = np.log10(norm_edp[1:][:])

    # - microbenchmark
    micro_total_power = np.zeros(
        (1, int(len(schemes) * len(microbenches))), dtype=np.float)
    micro_total_energy = np.zeros(
        (1, int(len(schemes) * len(microbenches))), dtype=np.float)
    for b, bench in enumerate(microbenches):
        for s, scheme in enumerate(schemes):
            micro_power_breakdown[0][b * len(schemes)
                                     + s] = micro_cache_power[s][b]
            micro_power_breakdown[1][b * len(schemes)
                                     + s] = micro_mem_acc_power[s][b]
            micro_power_breakdown[2][b * len(schemes)
                                     + s] = micro_link_power[s][b]

            micro_energy_breakdown[0][b * len(schemes)
                                      + s] = micro_cache_energy[s][b]
            micro_energy_breakdown[1][b * len(schemes)
                                      + s] = micro_mem_acc_energy[s][b]
            micro_energy_breakdown[2][b * len(schemes)
                                      + s] = micro_link_energy[s][b]

            if scheme == 'PEI':
                tmp_power = micro_cache_power[s][b] + micro_mem_acc_power[s][b] + micro_link_power[s][b]
                tmp_energy = micro_cache_energy[s][b] + micro_mem_acc_energy[s][b] + micro_link_energy[s][b]
                micro_total_power[0][b * len(schemes)] = tmp_power
                micro_total_energy[0][b * len(schemes)] = tmp_energy
            else:
                micro_total_power[0][b * len(schemes) + s] = micro_total_power[
                    0][b * len(schemes)]
                micro_total_energy[0][b * len(schemes)
                                      + s] = micro_total_energy[0][b * len(
                                          schemes)]

    micro_power_breakdown = micro_power_breakdown / np.tile(
        micro_total_power, (3, 1))
    micro_energy_breakdown = micro_energy_breakdown / np.tile(
        micro_total_energy, (3, 1))
    log_norm_micro_edp = np.log10(norm_micro_edp[1:][:])

    result_file = open('power_energy.csv', mode='w')

    result_file.write('power:\n')
    result_file.write(',,cache,memory,network')
    for b, benchmark in enumerate(benchmarks):
        row = '\n' + benchmark + ',PEI,' + str(
            power_breakdown[0][b * len(schemes)]) + ',' + str(
                power_breakdown[1][b * len(schemes)]) + ',' + str(
                    power_breakdown[2][b * len(schemes)]) + '\n'
        result_file.write(row)
        row = ',ART-tid,' + str(
            power_breakdown[0][b * len(schemes) + 1]) + ',' + str(
                power_breakdown[1][b * len(schemes) + 1]) + ',' + str(
                    power_breakdown[2][b * len(schemes) + 1]) + '\n'
        result_file.write(row)
        row = ',ART-tid,' + str(
            power_breakdown[0][b * len(schemes) + 2]) + ',' + str(
                power_breakdown[1][b * len(schemes) + 2]) + ',' + str(
                    power_breakdown[2][b * len(schemes) + 2]) + '\n\n'
        result_file.write(row)

    result_file.write(',,cache,memory,network')
    for b, benchmark in enumerate(microbenches):
        row = '\n' + benchmark + ',PEI,' + str(
            micro_power_breakdown[0][b * len(schemes)]) + ',' + str(
                micro_power_breakdown[1][b * len(schemes)]) + ',' + str(
                    micro_power_breakdown[2][b * len(schemes)]) + '\n'
        result_file.write(row)
        row = ',ART-tid,' + str(
            micro_power_breakdown[0][b * len(schemes) + 1]) + ',' + str(
                micro_power_breakdown[1][b * len(schemes) + 1]) + ',' + str(
                    micro_power_breakdown[2][b * len(schemes) + 1]) + '\n'
        result_file.write(row)
        row = ',ART-tid,' + str(
            micro_power_breakdown[0][b * len(schemes) + 2]) + ',' + str(
                micro_power_breakdown[1][b * len(schemes) + 2]) + ',' + str(
                    micro_power_breakdown[2][b * len(schemes) + 2]) + '\n\n'
        result_file.write(row)

    result_file.write('energy:\n')
    result_file.write(',,cache,memory,network')
    for b, benchmark in enumerate(benchmarks):
        row = '\n' + benchmark + ',PEI,' + str(
            energy_breakdown[0][b * len(schemes)]) + ',' + str(
                energy_breakdown[1][b * len(schemes)]) + ',' + str(
                    energy_breakdown[2][b * len(schemes)]) + '\n'
        result_file.write(row)
        row = ',ART-tid,' + str(
            energy_breakdown[0][b * len(schemes) + 1]) + ',' + str(
                energy_breakdown[1][b * len(schemes) + 1]) + ',' + str(
                    energy_breakdown[2][b * len(schemes) + 1]) + '\n'
        result_file.write(row)
        row = ',ART-tid,' + str(
            energy_breakdown[0][b * len(schemes) + 2]) + ',' + str(
                energy_breakdown[1][b * len(schemes) + 2]) + ',' + str(
                    energy_breakdown[2][b * len(schemes) + 2]) + '\n\n'
        result_file.write(row)

    result_file.write(',,cache,memory,network')
    for b, benchmark in enumerate(microbenches):
        row = '\n' + benchmark + ',PEI,' + str(
            micro_energy_breakdown[0][b * len(schemes)]) + ',' + str(
                micro_energy_breakdown[1][b * len(schemes)]) + ',' + str(
                    micro_energy_breakdown[2][b * len(schemes)]) + '\n'
        result_file.write(row)
        row = ',ART-tid,' + str(
            micro_energy_breakdown[0][b * len(schemes) + 1]) + ',' + str(
                micro_energy_breakdown[1][b * len(schemes) + 1]) + ',' + str(
                    micro_energy_breakdown[2][b * len(schemes) + 1]) + '\n'
        result_file.write(row)
        row = ',ART-tid,' + str(
            micro_energy_breakdown[0][b * len(schemes) + 2]) + ',' + str(
                micro_energy_breakdown[1][b * len(schemes) + 2]) + ',' + str(
                    micro_energy_breakdown[2][b * len(schemes) + 2]) + '\n\n'
        result_file.write(row)

    result_file.write('normalized energy-delay-product:\n')
    result_file.write(',backprop,lud,pagerank,sgemm,spmv,gmean\n')
    for s, scheme in enumerate(schemes):
        row = scheme
        for b, benchmark in enumerate(benchnames):
            row = row + ',' + str(norm_edp[s][b])
        row = row + '\n'
        result_file.write(row)
    result_file.write('\n')

    result_file.write(',reduce,rand_reduce,mac,rand_mac,gmean\n')
    for s, scheme in enumerate(schemes):
        row = scheme
        for b, benchmark in enumerate(micronames):
            row = row + ',' + str(norm_micro_edp[s][b])
        row = row + '\n'
        result_file.write(row)
    result_file.write('\n')

    result_file.write('normalized energy-delay-product (log):\n')
    result_file.write(',backprop,lud,pagerank,sgemm,spmv,gmean\n')
    for s, scheme in enumerate(schemes):
        if s == 0:
            continue
        row = scheme
        for b, benchmark in enumerate(benchnames):
            row = row + ',' + str(log_norm_edp[s - 1][b])
        row = row + '\n'
        result_file.write(row)
    result_file.write('\n')

    result_file.write(',reduce,rand_reduce,mac,rand_mac,gmean\n')
    for s, scheme in enumerate(schemes):
        if s == 0:
            continue
        row = scheme
        for b, benchmark in enumerate(micronames):
            row = row + ',' + str(log_norm_micro_edp[s - 1][b])
        row = row + '\n'
        result_file.write(row)
    result_file.write('\n')

    result_file.close()

    colors = ['#f0f9e8', '#bae4bc',
              '#7bccc4']  #, '#43a2ca', '#0868ac', '#0796ac']
    plt.rc('legend', fontsize=18)
    plt.rc('font', size=18)

    # - benchmark
    xticks = []
    group_names = []
    entry_names = ['cache', 'memory', 'network']
    for i in range(0, len(benchmarks)):
        for j in range(0, len(schemes)):
            xticks.append(i * (len(schemes) + 1) + j)
            group_names.append(schemes[j])

    # power breakdown
    data = [list(i) for i in zip(*power_breakdown)]
    data = np.array(data, dtype=np.float64)

    fig = plt.figure(figsize=(8.7, 5.5))
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names,
        entry_names=entry_names,
        breakdown=True,
        xticks=xticks,
        width=0.8,
        colors=colors,
        legendloc='upper center',
        legendncol=4,
        xticklabelfontsize=16,
        xticklabelrotation=90)
    ax.set_ylabel('Normalized Power Breakdown')
    ax.yaxis.grid(True, linestyle='--')
    ax.legend(
        hdls,
        entry_names,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=4,
        frameon=False,
        handletextpad=0.1)
    fmt.resize_ax_box(ax, hratio=0.8)

    ly = len(benchmarks)
    scale = 1. / ly
    ypos = -0.4
    for pos in xrange(ly + 1):
        lxpos = (pos + 0.5) * scale
        if pos < ly:
            ax.text(
                lxpos,
                ypos,
                benchnames[pos],
                ha='center',
                transform=ax.transAxes)
        add_line(ax, pos * scale, ypos)
    fig.savefig(folder_path + '/results/benchPower.pdf', bbox_inches='tight')

    # energy breakdown
    data = [list(i) for i in zip(*energy_breakdown)]
    data = np.array(data, dtype=np.float64)

    fig = plt.figure(figsize=(8.7, 5.5))
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names,
        entry_names=entry_names,
        breakdown=True,
        xticks=xticks,
        width=0.8,
        colors=colors,
        legendloc='upper center',
        legendncol=4,
        xticklabelfontsize=16,
        xticklabelrotation=90)
    ax.set_ylabel('Normalized Energy Breakdown')
    ax.yaxis.grid(True, linestyle='--')
    ax.legend(
        hdls,
        entry_names,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=4,
        frameon=False,
        handletextpad=0.1)
    fmt.resize_ax_box(ax, hratio=0.8)

    ly = len(benchmarks)
    scale = 1. / ly
    ypos = -0.4
    for pos in xrange(ly + 1):
        lxpos = (pos + 0.5) * scale
        if pos < ly:
            ax.text(
                lxpos,
                ypos,
                benchnames[pos],
                ha='center',
                transform=ax.transAxes)
        add_line(ax, pos * scale, ypos)
    fig.savefig(folder_path + '/results/benchEnergy.pdf', bbox_inches='tight')

    # energy-delay product
    data = [list(i) for i in zip(*norm_edp)]
    data = np.array(data, dtype=np.float64)
    fig = plt.figure(figsize=(8, 5.5))
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names=benchnames,
        entry_names=schemes,
        colors=colors,
        breakdown=False,
        legendloc='upper center',
        legendncol=6)
    fig.autofmt_xdate()
    ax.yaxis.grid(True, linestyle='--')
    ax.set_ylim([0, 1.2])
    ax.set_ylabel('Normalized Energy-Delay Product')
    ax.legend(
        hdls,
        schemes,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=6,
        frameon=False,
        handletextpad=0.1,
        columnspacing=0.3)
    fig.savefig(folder_path + '/results/benchEDP.pdf', bbox_inches='tight')

    # energy-delay product (log)
    logschemes = ['ART-tid', 'ART-addr']
    data = [list(i) for i in zip(*log_norm_edp)]
    fig = plt.figure(figsize=(8.7, 5.5))
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names=benchnames,
        entry_names=logschemes,
        colors=colors[1:],
        breakdown=False,
        legendloc='upper center',
        legendncol=6)
    fig.autofmt_xdate()
    ax.yaxis.grid(True, linestyle='--')
    ax.locator_params(nbins=10, axis='y')
    #ax.set_ylim([0, 1.2])
    ax.set_ylabel('Normalized Energy-Delay Product (log)')
    ax.legend(
        hdls,
        logschemes,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=6,
        frameon=False,
        handletextpad=0.1,
        columnspacing=0.3)
    fig.savefig(folder_path + '/results/benchLogEDP.pdf', bbox_inches='tight')

    # - microbenchmark
    xticks = []
    group_names = []
    entry_names = ['cache', 'memory', 'network']
    for i in range(0, len(microbenches)):
        for j in range(0, len(schemes)):
            xticks.append(i * (len(schemes) + 1) + j)
            group_names.append(schemes[j])

    # power breakdown
    data = [list(i) for i in zip(*micro_power_breakdown)]
    data = np.array(data, dtype=np.float64)

    fig = plt.figure(figsize=(8.7, 5.5))
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names,
        entry_names=entry_names,
        breakdown=True,
        xticks=xticks,
        width=0.8,
        colors=colors,
        legendloc='upper center',
        legendncol=3,
        xticklabelfontsize=16,
        xticklabelrotation=90)
    ax.set_ylabel('Normalized Power Breakdown')
    ax.yaxis.grid(True, linestyle='--')
    ax.legend(
        hdls,
        entry_names,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=3,
        frameon=False,
        handletextpad=0.1)
    fmt.resize_ax_box(ax, hratio=0.8)

    ly = len(microbenches)
    scale = 1. / ly
    ypos = -0.4
    for pos in xrange(ly + 1):
        lxpos = (pos + 0.5) * scale
        if pos < ly:
            ax.text(
                lxpos,
                ypos,
                micronames[pos],
                ha='center',
                transform=ax.transAxes)
        add_line(ax, pos * scale, ypos)
    fig.savefig(folder_path + '/results/microPower.pdf', bbox_inches='tight')

    # energy breakdown
    data = [list(i) for i in zip(*micro_energy_breakdown)]
    data = np.array(data, dtype=np.float64)

    fig = plt.figure(figsize=(8.7, 5.5))
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names,
        entry_names=entry_names,
        breakdown=True,
        xticks=xticks,
        width=0.8,
        colors=colors,
        legendloc='upper center',
        legendncol=3,
        xticklabelfontsize=16,
        xticklabelrotation=90)
    ax.set_ylabel('Normalized Energy Breakdown')
    ax.yaxis.grid(True, linestyle='--')
    ax.legend(
        hdls,
        entry_names,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=3,
        frameon=False,
        handletextpad=0.1)
    fmt.resize_ax_box(ax, hratio=0.8)

    ly = len(microbenches)
    scale = 1. / ly
    ypos = -0.4
    for pos in xrange(ly + 1):
        lxpos = (pos + 0.5) * scale
        if pos < ly:
            ax.text(
                lxpos,
                ypos,
                micronames[pos],
                ha='center',
                transform=ax.transAxes)
        add_line(ax, pos * scale, ypos)
    fig.savefig(folder_path + '/results/microEnergy.pdf', bbox_inches='tight')

    # energy-delay product
    data = [list(i) for i in zip(*norm_micro_edp)]
    data = np.array(data, dtype=np.float64)
    fig = plt.figure(figsize=(8, 5.5))
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names=micronames,
        entry_names=schemes,
        colors=colors,
        breakdown=False,
        legendloc='upper center',
        legendncol=5)
    fig.autofmt_xdate()
    ax.yaxis.grid(True, linestyle='--')
    ax.set_ylim([0, 1.2])
    ax.set_ylabel('Normalized Energy-Delay Product')
    ax.legend(
        hdls,
        schemes,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=6,
        frameon=False,
        handletextpad=0.1,
        columnspacing=0.3)
    fig.savefig(folder_path + '/results/microEDP.pdf', bbox_inches='tight')

    # energy-delay product (log)
    data = [list(i) for i in zip(*log_norm_micro_edp)]
    data = np.array(data, dtype=np.float64)
    fig = plt.figure(figsize=(8.7, 5.5))
    ax = fig.gca()
    hdls = barchart.draw(
        ax,
        data,
        group_names=micronames,
        entry_names=logschemes,
        colors=colors[1:],
        breakdown=False,
        legendloc='upper center',
        legendncol=5)
    fig.autofmt_xdate()
    ax.yaxis.grid(True, linestyle='--')
    #ax.set_ylim([0, 1.2])
    ax.set_ylabel('Normalized Energy-Delay Product (log)')
    ax.legend(
        hdls,
        logschemes,
        loc='upper center',
        bbox_to_anchor=(0.5, 1.18),
        ncol=6,
        frameon=False,
        handletextpad=0.1,
        columnspacing=0.3)
    fig.savefig(folder_path + '/results/microLogEDP.pdf', bbox_inches='tight')

    plt.show()


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'usage: ' + sys.argv[0] + ' folder_path'
        print '       type \"' + sys.argv[0] + ' -h\" for options'
        exit()
    main(sys.argv[1])
