#!/bin/sh

net_dim=4 # 4 or 8
logdir=dfly${net_dim}dim_log

mkdir -p ${logdir}

for bench in backprop_2097152 lud_4096_0.75_1 pagerank_web-Google spmv_4096_0.3 sgemm_4096_1
do
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-hmc.py \
    -runfile ../Apps/list/run-pei-${bench}.py \
    -benchname dfly${net_dim}dim_pei_${bench} \
    -net_dim ${net_dim} \
    > ${logdir}/pei_${bench}.log 2>&1 &
done

# microbenchmarks
for microbench in reduce rand_reduce mac rand_mac
do
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-hmc.py \
    -runfile ../Apps/list/run-pei-${microbench}.py \
    -benchname dfly${net_dim}dim_pei_${microbench} \
    -net_dim ${net_dim} \
    > ${logdir}/pei_${microbench}.log 2>&1 &
done
