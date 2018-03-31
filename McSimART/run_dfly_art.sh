mkdir -p dfly_log

# benchmarks
for bench in backprop lud pagerank spmv sgemm
do
  ./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-${bench}.py -benchname dfly_art_${bench} > dfly_log/art_${bench}.log 2>&1 &
done

# microbenchmarks
for microbench in reduce rand_reduce mac rand_mac
do
  ./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-${microbench}.py -benchname dfly_art_${microbench} > dfly_log/art_${microbench}.log 2>&1 &
done
