mkdir -p dfly_log

# benchmarks
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-pagerank.py -benchname dfly_arftid_pagerank > dfly_log/arftid_pagerank.log 2>&1 &

#microbenchmarks
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-reduce.py -benchname dfly_arftid_reduce > dfly_log/arftid_reduce.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-rand_reduce.py -benchname dfly_arftid_rand_reduce > dfly_log/arftid_rand_reduce.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-mac.py -benchname dfly_arftid_mac > dfly_log/arftid_mac.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-rand_mac.py -benchname dfly_arftid_rand_mac > dfly_log/arftid_rand_mac.log 2>&1 &
