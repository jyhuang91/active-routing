Program for LUDecomposition, lud_omp.c contains the kernel for both omp and pthread implementations.
./lud_omp [-v] [-n no. of threads] [-s matrix_size|-i input_file]

Uncomment/Comment at lud.c +124 to switch between omp and pthread

