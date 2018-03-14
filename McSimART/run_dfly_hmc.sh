mkdir -p dfly_log

# benchmarks
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-pagerank.py -benchname dfly_hmc_pagerank > dfly_log/hmc_pagerank.log 2>&1 &

#microbenchmarks
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-reduce.py -benchname dfly_hmc_reduce > dfly_log/hmc_reduce.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-rand_reduce.py -benchname dfly_hmc_rand_reduce > dfly_log/hmc_rand_reduce.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-mac.py -benchname dfly_hmc_mac > dfly_log/hmc_mac.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-rand_mac.py -benchname dfly_hmc_rand_mac > dfly_log/hmc_rand_mac.log 2>&1 &
