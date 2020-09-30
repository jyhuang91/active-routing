#!/bin/sh

net_dim=4 # 4 or 8
logdir=dfly${net_dim}dim_log

mkdir -p ${logdir}

# benchmarks
for bench in backprop_2097152 lud_4096_0.75_1 pagerank_web-Google spmv_4096_0.3 sgemm_4096_1
do
  # naive-art
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-naive.py \
    -runfile ../Apps/list/run-naive-art-${bench}.py \
    -benchname dfly${net_dim}dim_naive_art_${bench} \
    -net_dim ${net_dim} \
    > ${logdir}/naive_art_${bench}.log 2>&1 &

  # static-art
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-naive.py \
    -runfile ../Apps/list/run-art-${bench}.py \
    -benchname dfly${net_dim}dim_static_art_${bench} \
    -net_dim ${net_dim} \
    > ${logdir}/static_art_${bench}.log 2>&1 &

  # art-tid
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-tid.py \
    -runfile ../Apps/list/run-art-${bench}.py \
    -benchname dfly${net_dim}dim_art_tid_${bench} \
    -net_dim ${net_dim} \
    > ${logdir}/art_tid_${bench}.log 2>&1 &

  # art-addr
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-addr.py \
    -runfile ../Apps/list/run-art-${bench}.py \
    -benchname dfly${net_dim}dim_art_addr_${bench} \
    -net_dim ${net_dim} \
    > ${logdir}/art_addr_${bench}.log 2>&1 &
done


# microbenchmarks
for microbench in reduce rand_reduce mac rand_mac
do
  # naive-art
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-naive.py \
    -runfile ../Apps/list/run-naive-art-${microbench}.py \
    -benchname dfly${net_dim}dim_naive_art_${microbench} \
    -net_dim ${net_dim} \
    > ${logdir}/naive_art_${microbench}.log 2>&1 &

  # static-art
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-naive.py \
    -runfile ../Apps/list/run-art-${microbench}.py \
    -benchname dfly${net_dim}dim_static_art_${microbench} \
    -net_dim ${net_dim} \
    > ${logdir}/static_art_${microbench}.log 2>&1 &

  # art-tid
  ./mcsim \
    -mdfile ../Apps/md/md-16o3core2GHz-mesh-art-tid.py \
    -runfile $PWD/../Apps/list/run-art-${microbench}.py \
    -benchname dfly${net_dim}dim_art_tid_${microbench} \
    -net_dim ${net_dim} \
    > ${logdir}/art_tid_${microbench}.log 2>&1 &

  # art-addr
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-addr.py \
    -runfile ../Apps/list/run-art-${microbench}.py \
    -benchname dfly${net_dim}dim_art_addr_${microbench} \
    -net_dim ${net_dim} \
    > ${logdir}/art_addr_${microbench}.log 2>&1 &
done
