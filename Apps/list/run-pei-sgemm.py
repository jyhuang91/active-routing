# #_threads  #_skip_1st_instrs   dir  prog_n_argv
# small input
16 0 true ../Apps/benchmarks/pei/sgemm ./sgemm -i 128,96,160 -n 16
#16 0 ../Apps/benchmarks/sgemm ./sgemm -i ./datasets/small/input/matrix1.txt,./datasets/small/input/matrix2.txt,./datasets/small/input/matrix2t.txt -n 16
# medium input
#16 0 ../Apps/benchmarks/sgemm ./sgemm -i datasets/medium/input/matrix1.txt,./datasets/medium/input/matrix2t.txt,./datasets/medium/input/matrix2t.txt -n 16
#16 0 ../Apps/benchmarks/sgemm ./sgemm -i 1024,992,1056 -n 16
# self-defined
#16 0 ../Apps/benchmarks/sgemm ./sgemm -i 512,256,512 -n 16
