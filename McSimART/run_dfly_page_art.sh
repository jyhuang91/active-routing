#!/bin/sh

net_dim=4 # 4 or 8
logdir=dfly${net_dim}dim_log

mkdir -p ${logdir}

for bench in reduce mac backprop_2097152 sgemm_4096_1
do
  # ART-tid
  setarch `uname -m` -R ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-tid.py \
    -runfile ../Apps/list/run-art-${bench}.py \
    -benchname debug_art_tid_${bench} \
    -net_dim ${net_dim} \
    > ${logdir}/debug_art_tid_${bench}.log 2>&1 &

  # ART-addr
  setarch `uname -m` -R ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-addr.py \
    -runfile ../Apps/list/run-art-${bench}.py \
    -benchname debug_art_addr_${bench} \
    -net_dim ${net_dim} \
    > ${logdir}/debug_art_addr_${bench}.log 2>&1 &

  # ART-tid-VLP
  setarch `uname -m` -R ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-tid.py \
    -runfile ../Apps/list/run-art-${bench}.py \
    -benchname debug_vlp_art_tid_${bench} \
    -net_dim ${net_dim} \
    -vault_level_parallelism true \
    > ${logdir}/debug_vlp_art_tid_${bench}.log 2>&1 &

  # ART-addr-VLP
  setarch `uname -m` -R ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-addr.py \
    -runfile ../Apps/list/run-art-${bench}.py \
    -benchname debug_vlp_art_addr_${bench} \
    -net_dim ${net_dim} \
    -vault_level_parallelism true \
    > ${logdir}/debug_vlp_art_addr_${bench}.log 2>&1 &

  # ART-tid-Page
  setarch `uname -m` -R ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-tid.py \
    -runfile ../Apps/list/run-page-art-${bench}.py \
    -benchname debug_page_art_tid_${bench} \
    -net_dim ${net_dim} \
    > ${logdir}/debug_page_art_tid_${bench}.log 2>&1 &

  # ART-addr-Page
  setarch `uname -m` -R ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-addr.py \
    -runfile ../Apps/list/run-page-art-${bench}.py \
    -benchname debug_page_art_addr_${bench} \
    -net_dim ${net_dim} \
    > ${logdir}/debug_page_art_addr_${bench}.log 2>&1 &

  # ART-tid-Page-VLP
  setarch `uname -m` -R ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-tid.py \
    -runfile ../Apps/list/run-page-art-${bench}.py \
    -benchname debug_page_vlp_art_tid_${bench} \
    -net_dim ${net_dim} \
    -vault_level_parallelism true \
    > ${logdir}/debug_page_vlp_art_tid_${bench}.log 2>&1 &

  # ART-addr-Page-VLP
  setarch `uname -m` -R ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-addr.py \
    -runfile ../Apps/list/run-page-art-${bench}.py \
    -benchname debug_page_vlp_art_addr_${bench} \
    -net_dim ${net_dim} \
    -vault_level_parallelism true \
    > ${logdir}/debug_page_vlp_art_addr_${bench}.log 2>&1 &
done
