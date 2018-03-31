mkdir -p dfly_log

# benchmarks
for bench in backprop_2097152 lud_4096_0.75_1 pagerank_web-Google spmv_4096_0.3 sgemm_4096_1
do
  ./mcsim -mdfile ../Apps/md/md-16o3core2GHz-mesh-hmc.py -runfile ../Apps/list/run-${bench}.py -benchname dfly_hmc_${bench} > dfly_log/hmc_${bench}.log 2>&1 &
done

# microbenchmarks
for microbench in reduce rand_reduce mac rand_mac
do
  ./mcsim -mdfile ../Apps/md/md-16o3core2GHz-mesh-hmc.py -runfile ../Apps/list/run-${microbench}.py -benchname dfly_hmc_${microbench} > dfly_log/hmc_${microbench}.log 2>&1 &
done
