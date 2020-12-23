#!/bin/sh

net_dim=4 # 4 or 8
logdir=dfly${net_dim}dim_log

mkdir -p ${logdir}

# benchmarks
#for bench in backprop_2097152 sgemm_4096_1
for bench in sgemm_4096_1
do
  # art-tid
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-tid.py \
    -runfile ../Apps/list/run-page-art-${bench}.py \
    -benchname dfly${net_dim}dim_page_art_tid_${bench} \
    -net_dim ${net_dim} \
    > ${logdir}/page_art_tid_${bench}.log 2>&1 &

  # art-addr
  #./mcsim \
  #  -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-addr.py \
  #  -runfile ../Apps/list/run-page-art-${bench}.py \
  #  -benchname dfly${net_dim}dim_art_addr_${bench} \
  #  -net_dim ${net_dim} \
  #  > ${logdir}/page_art_addr_${bench}.log 2>&1 &
done
