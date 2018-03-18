mkdir -p dfly_log

# benchmarks
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-pagerank.py -benchname dfly_arfaddr_pagerank > dfly_log/arfaddr_pagerank.log 2>&1 &

#microbenchmarks
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-reduce.py -benchname dfly_arfaddr_reduce > dfly_log/arfaddr_reduce.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-rand_reduce.py -benchname dfly_arfaddr_rand_reduce > dfly_log/arfaddr_rand_reduce.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-mac.py -benchname dfly_arfaddr_mac > dfly_log/arfaddr_mac.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-rand_mac.py -benchname dfly_arfaddr_rand_mac > dfly_log/arfaddr_rand_mac.log 2>&1 &
