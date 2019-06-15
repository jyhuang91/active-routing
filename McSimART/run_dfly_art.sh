mkdir -p dfly_log

# benchmarks
for bench in backprop_2097152 lud_4096_0.75_1 pagerank_web-Google spmv_4096_0.3 sgemm_4096_1
do
  # naive-art
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-naive.py \
    -runfile ../Apps/list/run-naive-art-${bench}.py \
    -benchname dfly_naive_art_${bench} \
    > dfly_log/naive_art_${bench}.log 2>&1 &

  # static-art
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-naive.py \
    -runfile ../Apps/list/run-art-${bench}.py \
    -benchname dfly_static_art_${bench} \
    > dfly_log/static_art_${bench}.log 2>&1 &

  # art-tid
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-tid.py \
    -runfile ../Apps/list/run-art-${bench}.py \
    -benchname dfly_art_tid_${bench} \
    > dfly_log/art_tid_${bench}.log 2>&1 &

  # art-addr
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-addr.py \
    -runfile ../Apps/list/run-art-${bench}.py \
    -benchname dfly_art_addr_${bench} \
    > dfly_log/art_addr_${bench}.log 2>&1 &
done


# microbenchmarks
for microbench in reduce rand_reduce mac rand_mac
do
  # naive-art
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-naive.py \
    -runfile ../Apps/list/run-naive-art-${microbench}.py \
    -benchname dfly_naive_art_${microbench} \
    > dfly_log/naive_art_${microbench}.log 2>&1 &

  # static-art
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-naive.py \
    -runfile ../Apps/list/run-art-${microbench}.py \
    -benchname dfly_static_art_${microbench} \
    > dfly_log/static_art_${microbench}.log 2>&1 &

  # art-tid
  ./mcsim \
    -mdfile ../Apps/md/md-16o3core2GHz-mesh-art-tid.py \
    -runfile $PWD/../Apps/list/run-art-${microbench}.py \
    -benchname dfly_art_tid_${microbench} \
    > dfly_log/art_tid_${microbench}.log 2>&1 &

  # art-addr
  ./mcsim \
    -mdfile $PWD/../Apps/md/md-16o3core2GHz-mesh-art-addr.py \
    -runfile ../Apps/list/run-art-${microbench}.py \
    -benchname dfly_art_addr_${microbench} \
    > dfly_log/art_addr_${microbench}.log 2>&1 &
done
