mkdir -p dfly_log

# benchmarks
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-ddr.py -runfile ../Apps/list/run-pagerank.py > dfly_log/ddr_pagerank.log 2>&1 &

# microbenchmarks
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-ddr.py -runfile ../Apps/list/run-reduce.py > dfly_log/ddr_reduce.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-ddr.py -runfile ../Apps/list/run-rand_reduce.py > dfly_log/ddr_rand_reduce.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-ddr.py -runfile ../Apps/list/run-mac.py > dfly_log/ddr_mac.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-ddr.py -runfile ../Apps/list/run-rand_mac.py > dfly_log/ddr_rand_mac.log 2>&1 &
