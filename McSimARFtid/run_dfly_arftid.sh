mkdir -p dfly_log

# benchmarks
for bench in backprop lud pagerank spmv sgemm
do
  ./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-${bench}.py -benchname dfly_arftid_${bench} > dfly_log/arftid_${bench}.log 2>&1 &
done

#microbenchmarks
for microbench in reduce rand_reduce mac rand_mac
do
  ./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-${microbench}.py -benchname dfly_arftid_${microbench} > dfly_log/arftid_${microbench}.log 2>&1 &
done
