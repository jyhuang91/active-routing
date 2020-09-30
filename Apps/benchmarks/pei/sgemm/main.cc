/***************************************************************************
 *cr
 *cr            (C) Copyright 2010 The Board of Trustees of the
 *cr                        University of Illinois
 *cr                         All Rights Reserved
 *cr
 ***************************************************************************/

/* 
 * Main entry of dense matrix-matrix multiplication kernel
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>
#include <vector>
#include <iostream>
#include "parboil.h"
#include "hooks.h"

//extern void basicSgemm( char transa, char transb, int m, int n, int k, float alpha, const float *A, int lda, const float *B, int ldb, float beta, float *C, int ldc, int nthreads );
extern void basicSgemm( char transa, char transb, int m, int n, int k, float alpha, float *A, int lda, float *B, int ldb, float beta, float *C, int ldc, int nthreads, int niteration, int niteration2);

// I/O routines
extern bool generateMatrix(int &nr_row, int &nr_col, float **v);
extern bool readColMajorMatrixFile(const char *fn, int &nr_row, int &nr_col, std::vector<float>&v);
extern bool writeColMajorMatrixFile(const char *fn, int, int, std::vector<float>&);

int
main (int argc, char *argv[]) {

  struct pb_Parameters *params;
  struct pb_TimerSet timers;

  int matArow, matAcol;
  int matBrow, matBcol;
  //std::vector<float> matA, matBT;
  float *matA, *matBT;
  int i;

  /*printf("executed command line:\n");
  for (i = 0; i < argc; i++) {
    printf("%s ", argv[i]);
  }
  printf("\n\n");*/

  pb_InitializeTimerSet(&timers);

  /* Read command line. Expect 3 inputs: A, B and B^T 
     in column-major layout*/
  params = pb_ReadParameters(&argc, argv);
  if ((params->inpFiles[0] == NULL) 
      || (params->inpFiles[1] == NULL)
      || (params->inpFiles[2] == NULL)
      || (params->inpFiles[3] != NULL))
  {
    fprintf(stderr, "Expecting three input filenames:\n");
    fprintf(stderr, "\t./sgemm -i input1,intput2,input3 -o output -n nthreads\n");
    exit(-1);
  }

  if (params->nthreads == 0)
  {
    fprintf(stderr, "Expecting number of threads:\n");
    fprintf(stderr, "\t./sgemm -i input1,intput2,input3 -o output -n nthreads\n");
    exit(-1);
  }

  /* Read in data */
  pb_SwitchToTimer(&timers, pb_TimerID_IO);

  matArow = atoi(params->inpFiles[0]);
  matAcol = atoi(params->inpFiles[1]);
  matBrow = matAcol;
  matBcol = atoi(params->inpFiles[2]);

  // generate row-major A
  generateMatrix(matArow, matAcol, &matA);

  // generate row-major B^T
  generateMatrix(matBcol, matBrow, &matBT);

/*
  // load A
  readColMajorMatrixFile(params->inpFiles[0],
      matArow, matAcol, matA);

  // load B^T
  readColMajorMatrixFile(params->inpFiles[2],
      matBcol, matBrow, matBT);
*/
  pb_SwitchToTimer( &timers, pb_TimerID_COMPUTE );

  // allocate space for C
  std::vector<float> matC(matArow*matBcol);

  // Use standard sgemm interface
  roi_begin();
  basicSgemm('N', 'T', matArow, matBcol, matAcol, 1.0f,
      //&matA.front(), matArow, &matBT.front(), matBcol, 0.0f, &matC.front(),
      matA, matAcol, matBT, matBrow, 0.0f, &matC.front(),
      matBcol, params->nthreads, params->niteration, params->niteration2);
  roi_end();

  if (params->outFile) {
    /* Write C to file */
    pb_SwitchToTimer(&timers, pb_TimerID_IO);
    writeColMajorMatrixFile(params->outFile, matArow, matBcol, matC); 
  }

  pb_SwitchToTimer(&timers, pb_TimerID_NONE);

  double CPUtime = pb_GetElapsedTime(&(timers.timers[pb_TimerID_COMPUTE]));
  std::cout<< "GFLOPs = " << 2.* matArow * matBcol * matAcol/CPUtime/1e9 << std::endl;
  pb_PrintTimerSet(&timers);
  pb_FreeParameters(params);
  return 0;
}
