mkdir -p dfly_log

# benchmarks
#for bench in backprop lud pagerank spmv sgemm
for bench in backprop_2097152 lud_4096_0.75_1 pagerank_web-Google sgemm_4096_1 spmv_4096_0.3
do
  ./mcsim -mdfile ../Apps/md/md-16o3core2GHz-mesh-hmc.py -runfile ../Apps/list/run-ar-${bench}.py -benchname dfly_arfaddr_${bench} > dfly_log/arfaddr_${bench}.log 2>&1 &
done

# microbenchmarks
#for microbench in reduce rand_reduce mac rand_mac
#do
#  ./mcsim -mdfile ../Apps/md/md-16o3core2GHz-mesh-hmc.py -runfile ../Apps/list/run-ar-${microbench}.py -benchname dfly_arfaddr_${microbench} > dfly_log/arfaddr_${microbench}.log 2>&1 &
#done
