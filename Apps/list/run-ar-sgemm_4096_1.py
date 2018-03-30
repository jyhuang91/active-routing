# num_threads  num_skip_1st_instrs  run_roi  dir  prog_n_argv
# run_roi is true, ROI Hooks needs to instrumented to capture the ROI
16 0 true ../Apps/benchmarks/active_routing/sgemm ./sgemm -i 4096,4096,4096 -n 16 -s 1

