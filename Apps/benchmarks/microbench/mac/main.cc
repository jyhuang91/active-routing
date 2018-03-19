#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <time.h>
#include <sys/timeb.h>
#include <string.h>

#include "hooks.h"

#define MAX            100000000
#define INT_MAX        100000000
#define BILLION 1E9

//Thread Structure
typedef struct
{
  double*   W;
  double*   X;
  int       tid;
  int       P;
  int       N;
  pthread_barrier_t* barrier;
} thread_arg_t;

//Global Variables
pthread_mutex_t lock;           //single lock
pthread_mutex_t locks[4194304]; //upper limit for locks, can be increased
int nodecount = 0;
double sum;
thread_arg_t thread_arg[1024];     // MAX threads
pthread_t     thread_handle[1024];  // pthread handlers

// Primary Parallel Function
void *do_work(void *args)
{
  volatile thread_arg_t* arg = (thread_arg_t*) args;
  int tid                    = arg->tid;
  double* W                  = arg->W;
  double* X                  = arg->X;
  const int N                = arg->N;
  int v                      = 0;      //variable for current vertex
  double N_real              = N;
  double tid_d = tid;
  double P_d = arg->P;

  // Allocate work among threads
  double start_d = tid_d * (N_real / P_d);
  double stop_d = (tid_d + 1.0) * (N_real / P_d);
  int i_start = start_d;
  int i_stop = stop_d;

  pthread_barrier_wait(arg->barrier);

  double local_sum = 0.0;
  for (v = i_start; v < i_stop; ++v) {
    local_sum += W[v] * X[v];
  }

  pthread_mutex_lock(&lock);
  sum += local_sum;
  pthread_mutex_unlock(&lock);

  pthread_barrier_wait(arg->barrier);

  return NULL;
}

// Main
int main(int args, char **argv)
{
  const int P = atoi(argv[1]);    // number of threads
  int N = atoi(argv[2]);

  pthread_barrier_t barrier;

  double *W = (double *) malloc(N * sizeof(double *));
  double *X = (double *) malloc(N * sizeof(double *));
  double ret = posix_memalign((void **) &W, 64, N * sizeof(double));
  if (ret != 0) {
    fprintf(stderr, "Could not allocate memory\n");
    exit(EXIT_FAILURE);
  }

  sum = 0;

  // Memory initialization
  //for (int i = 0; i < N; ++i) {
  //  W[i] = 1;
  //  X[i] = 1;
  //}

  // Synchronization parameters
  pthread_barrier_init(&barrier, NULL, P); 
  pthread_mutex_init(&lock, NULL);

  // Thread arguments
  for (int j = 0; j < P; ++j)
  {
    thread_arg[j].W       = W;
    thread_arg[j].X       = X;
    thread_arg[j].tid     = j;
    thread_arg[j].P       = P;
    thread_arg[j].N       = N;
    thread_arg[j].barrier = &barrier;
  }

  // start cpu clock
  struct timespec requestStart, requestEnd;
  clock_gettime(CLOCK_REALTIME, &requestStart);

  printf("\nSpawn Threads...");

  roi_begin();

  // Spawn threads
  for (int j = 1;j < P; ++j) {
    pthread_create(thread_handle+j,
        NULL,
        do_work,
        (void *) &thread_arg[j]);
  }
  do_work((void *) &thread_arg[0]); // Spawn master

  // Join threads
  for (int j = 1; j < P; ++j) {
    pthread_join(thread_handle[j], NULL);
  }

  roi_end();

  printf("\nThreads Joined!");

  // Read clock and print time
  clock_gettime(CLOCK_REALTIME, &requestEnd);
  double accum = ( requestEnd.tv_sec - requestStart.tv_sec ) + 
    ( requestEnd.tv_nsec - requestStart.tv_nsec ) / BILLION;
  printf( "\nTime:%lf seconds\n", accum );

  printf("Array size: %d, sum: %f\n", N, sum);

  return 0;
}

