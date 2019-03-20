#include <stdio.h>
//#include <omp.h>
#include <pthread.h>
#include <hooks.h>

extern int omp_num_threads;

//void lud_omp(float *a, int size)
//{
//     int i,j,k;
//     float sum;
//	 printf("num of threads = %d\n", omp_num_threads);
//     for (i=0; i <size; i++){
//	 omp_set_num_threads(omp_num_threads);
//#pragma omp parallel for default(none) \
//         private(j,k,sum) shared(size,i,a) 
//         for (j=i; j <size; j++){
//             sum=a[i*size+j];
//             for (k=0; k<i; k++) sum -= a[i*size+k]*a[k*size+j];
//             a[i*size+j]=sum;
//         }
//#pragma omp parallel for default(none) \
//         private(j,k,sum) shared(size,i,a) 
//         for (j=i+1;j<size; j++){
//             sum=a[j*size+i];
//             for (k=0; k<i; k++) sum -=a[j*size+k]*a[k*size+i];
//             a[j*size+i]=sum/a[i*size+i];
//         }
//     }
//}

//Pthread implmentation of the same function
typedef struct
{
  int tid; 
  int P;
  int num_iter; 
  int i; 
  int size; 
  float* shared_mat; 
  pthread_barrier_t* barrier;
  float fraction;
  int iteration;
}thread_arg_t;

thread_arg_t thread_arg[1024];
pthread_t   thread_handle[1024];   //MAX threads and pthread handlers

void* do_work(void* args){
  volatile thread_arg_t* arg = (thread_arg_t*) args;
  int tid           =  arg->tid;
  int P             =  arg->P;
//  int num_iter      = arg->num_iter; 
//  int i    = arg->i;
  int size          = arg->size; 
  float* shared_mat = arg->shared_mat; 
  int i;
  double P_d = P; 
  int stride = PEI_GRANULARITY;
  void* addr_arr[stride];

  int start = arg->fraction * size;
  int iteration = arg->iteration;
  for(i = start; i < start + iteration ;i++) {
    //divide work amoung threads; 
    float local_sum = 0;
    float local_product = 0;
    
    int j,k; 
    for(j = i+tid; j<size; j+=P){
      local_sum = shared_mat[i*size + j];
      for(k = 0; k < i - stride; k += stride){
        //local_sum -= shared_mat[i*size + k]*shared_mat[k*size + j]; 
        for (int c; c < stride; c++)
          addr_arr[c] = &shared_mat[(k+c)*size + j];
        UpdateRI(addr_arr, &shared_mat[i*size + k], &local_product, PEI_DOT);
        local_sum -= local_product;
      }
      // dealing with fragmentation, TODO: optimize it by applying masking
      for (; k < i; k++)
        local_sum -= shared_mat[i*size + k]*shared_mat[k*size + j]; 
      shared_mat[i*size + j] = local_sum;       //No lock required since j is different for each thread
    }
    
    pthread_barrier_wait(arg->barrier);
    
    for(j=i+tid; j<size; j+=P){
      if(tid == 0 && j == i + tid) continue;
      local_sum = shared_mat[j*size + i];
      for(k = 0; k < i - stride; k += stride){
        for (int c; c < stride; c++)
          addr_arr[c] = &shared_mat[(k+c)*size + i];
        UpdateRI(addr_arr, &shared_mat[j*size + k], &local_product, PEI_DOT);
        local_sum -= local_product;
      }
      for (; k < i; k++)
        local_sum -= shared_mat[j*size + k]*shared_mat[k*size + i];
      shared_mat[j*size + i] = local_sum / shared_mat[i*size + i];
    }
    pthread_barrier_wait(arg->barrier);
  }
  return NULL;
}

void lud_pthread(float *a, int size, float fraction, int iteration)
{
  int i = 0,j;
  //float sum;
  printf("num of threads = %d\n", omp_num_threads);
  pthread_barrier_t barrier;
  int num_threads = (size-i < 0)? 1:omp_num_threads;   //SHOULD Fine tune this??
  pthread_barrier_init(&barrier, NULL,num_threads);
//  for (i=0; i <size; i++){
    for (j=0; j < num_threads; j++){
      thread_arg[j].tid         = j;
      thread_arg[j].P           = num_threads;
//      thread_arg[j].num_iter    = size-i;
//      thread_arg[j].i  = i;
      thread_arg[j].size        = size;
      thread_arg[j].shared_mat  = a;
      thread_arg[j].barrier     = &barrier;
      thread_arg[j].fraction    = fraction;
      thread_arg[j].iteration   = iteration;
    }
    roi_begin();
    for (j=1; j < num_threads; j++){
      pthread_create(thread_handle + j,
                     NULL,
                     do_work,
                     (void*)&thread_arg[j]);
    }
    do_work((void*) &thread_arg[0]);
    
    for(j=1; j<num_threads; j++){
      pthread_join(thread_handle[j],NULL);
    }
    roi_end();
/*  
       for (j=i; j <size; j++){
             sum=a[i*size+j];
             for (k=0; k<i; k++) sum -= a[i*size+k]*a[k*size+j];
             a[i*size+j]=sum;
         }
         for (j=i+1;j<size; j++){
             sum=a[j*size+i];
             for (k=0; k<i; k++) sum -=a[j*size+k]*a[k*size+i];
             a[j*size+i]=sum/a[i*size+i];
         }
*///do_work handles this
//  }
 
}
