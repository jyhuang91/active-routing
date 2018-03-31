mkdir -p dram_log

# benchmarks
for bench in backprop lud pagerank spmv sgemm
do
  ./mcsim -mdfile ../Apps/md/md-16o3core-mesh-ddr.py -runfile ../Apps/list/run-${bench}.py > dram_log/ddr_${bench}.log 2>&1 &
done

# microbenchmarks
for microbench in reduce rand_reduce mac rand_mac
do
  ./mcsim -mdfile ../Apps/md/md-16o3core-mesh-ddr.py -runfile ../Apps/list/run-${microbench}.py > dram_log/ddr_${microbench}.log 2>&1 &
done
