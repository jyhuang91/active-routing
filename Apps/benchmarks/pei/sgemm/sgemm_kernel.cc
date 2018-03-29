/***************************************************************************
 *cr
 *cr            (C) Copyright 2010 The Board of Trustees of the
 *cr                        University of Illinois
 *cr                         All Rights Reserved
 *cr
 ***************************************************************************/

/* 
 * Base C implementation of MM
 */

#include <iostream>
#include <pthread.h>
#include "hooks.h"

/* thread arguments */
struct thread_arg_t {
  int m;
  int n;
  int k;
  int lda;
  int ldb;
  int ldc;
  float *A;
  float *B;
  float *C;
  float  alpha;
  float  beta;
  int nthreads;
  int tid;
  pthread_barrier_t *barrier;
};

void* work_func(void *thread_arg)
{
  volatile thread_arg_t *arg = (thread_arg_t *) thread_arg;

  int m = arg->m;
  int n = arg->n;
  int k = arg->k;
  int lda = arg->lda;
  int ldb = arg->ldb;
  int ldc = arg->ldc;
  float *A = arg->A;
  float *B = arg->B;
  float *C = arg->C;
  float  alpha = arg->alpha;
  float  beta  = arg->beta;
  int nthreads = arg->nthreads;
  int      tid = arg->tid;

  double m_d = m;
  double nthreads_d = nthreads;
  double tid_d = tid;
  double start_d = tid_d * (m_d / nthreads_d);
  double stop_d  = (tid_d + 1.0) * (m_d / nthreads_d);
  int start = start_d;
  int stop = stop_d;

  pthread_barrier_wait(arg->barrier);

  for (int mm = start; mm < stop; mm++) {
    for (int nn = 0; nn < n; nn++) {
      //float c = 0.0f;
      uint64_t flowID = mm+nn*ldc;
      for (int i = 0; i < k; i+=4) {
        /*float a = A[mm + i * lda];
        float b = B[nn + i * ldb];
        c += a * b;*/
        UPDATE(&A[mm + i * lda], &B[nn + i * ldb], (void *) flowID, PEI_DOT);
      }
      //GATHER(0, 0, (void *) flowID, 1);
      C[mm+nn*ldc] = C[mm+nn*ldc] * beta + alpha * flowID;
      //std::cout << "thread " << tid << " - flow ID " << flowID << std::endl;
    }
  }

  pthread_barrier_wait(arg->barrier);
}

//void basicSgemm( char transa, char transb, int m, int n, int k, float alpha, const float *A, int lda, const float *B, int ldb, float beta, float *C, int ldc, int nthreads )
void basicSgemm( char transa, char transb, int m, int n, int k, float alpha, float *A, int lda, float *B, int ldb, float beta, float *C, int ldc, int nthreads )
{
  if ((transa != 'N') && (transa != 'n')) {
    std::cerr << "unsupported value of 'transa' in regtileSgemm()" << std::endl;
    return;
  }
  
  if ((transb != 'T') && (transb != 't')) {
    std::cerr << "unsupported value of 'transb' in regtileSgemm()" << std::endl;
    return;
  }
  std::cout << "m: " << m << ", n: " << n << ", k: " << k << ", lda: " << lda
    << ", ldb: " << ldb << ", ldc: " << ldc << ", nthreads: " << nthreads << std::endl;

  thread_arg_t thread_arg[256];
  pthread_t    thread_handle[256];
  pthread_barrier_t barrier;

  pthread_barrier_init(&barrier, NULL, nthreads);

  for (int i = 0; i < nthreads; i++) {
    thread_arg[i].m = m;
    thread_arg[i].n = n;
    thread_arg[i].k = k;
    thread_arg[i].lda = lda;
    thread_arg[i].ldb = ldb;
    thread_arg[i].ldc = ldc;
    thread_arg[i].A = A;
    thread_arg[i].B = B;
    thread_arg[i].C = C;
    thread_arg[i].alpha = alpha;
    thread_arg[i].beta = beta;
    thread_arg[i].nthreads = nthreads;
    thread_arg[i].tid = i;
    thread_arg[i].barrier = &barrier;
  }

  for (int i = 1; i < nthreads; i++) {
    pthread_create(thread_handle+i,
                   NULL,
                   work_func,
                   (void *) &thread_arg[i]);
  }

  work_func((void *) &thread_arg[0]);

  for (int i = 1; i < nthreads; i++) {
    pthread_join(thread_handle[i], NULL);
  }
/*
  #pragma omp parallel for collapse (2)
  for (int mm = 0; mm < m; ++mm) {
    for (int nn = 0; nn < n; ++nn) {
      float c = 0.0f;
      for (int i = 0; i < k; ++i) {
        float a = A[mm + i * lda]; 
        float b = B[nn + i * ldb];
        c += a * b;
      }
      C[mm+nn*ldc] = C[mm+nn*ldc] * beta + alpha * c;
    }
  }
*/
}
