mkdir -p dfly_log

# benchmarks
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-pagerank.py -benchname dfly_art_pagerank > dfly_log/art_pagerank.log 2>&1 &

#microbenchmarks
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-reduce.py -benchname dfly_art_reduce > dfly_log/art_reduce.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-rand_reduce.py -benchname dfly_art_rand_reduce > dfly_log/art_rand_reduce.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-mac.py -benchname dfly_art_mac > dfly_log/art_mac.log 2>&1 &
./mcsim -mdfile ../Apps/md/md-16o3core-mesh-hmc.py -runfile ../Apps/list/run-ar-rand_mac.py -benchname dfly_art_rand_mac > dfly_log/art_rand_mac.log 2>&1 &
