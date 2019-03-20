/*
 ******************************************************************
 * HISTORY
 * 15-Oct-94  Jeff Shufelt (js), Carnegie Mellon University
 *	Prepared for 15-681, Fall 1994.
 * Modified by Shuai Che
 ******************************************************************
 */

//#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include "backprop.h"
#include <math.h>
//#define OPEN

#include <pthread.h>
#include <hooks.h>

#define ABS(x)          (((x) > 0.0) ? (x) : (-(x)))

#define fastcopy(to,from,len)\
{\
  register char *_to,*_from;\
  register int _i,_l;\
  _to = (char *)(to);\
  _from = (char *)(from);\
  _l = (len);\
  for (_i = 0; _i < _l; _i++) *_to++ = *_from++;\
}

extern int nthreads;

/*** Return random number between 0.0 and 1.0 ***/
float drnd()
{
  return ((float) rand() / (float) BIGRND);
}

/*** Return random number between -1.0 and 1.0 ***/
float dpn1()
{
  return ((drnd() * 2.0) - 1.0);
}

/*** The squashing function.  Currently, it's a sigmoid. ***/

float squash(x)
float x;
{
  float m;
  //x = -x;
  //m = 1 + x + x*x/2 + x*x*x/6 + x*x*x*x/24 + x*x*x*x*x/120;
  //return(1.0 / (1.0 + m));
  return (1.0 / (1.0 + exp(-x)));
}


/*** Allocate 1d array of floats ***/

float *alloc_1d_dbl(n)
int n;
{
  float *new;

  //new = (float *) malloc ((unsigned) (n * sizeof (float)));
  //if (new == NULL)
  if (posix_memalign((void **) &new, CACHELINE_SIZE, n * sizeof(float)))
  {
    printf("ALLOC_1D_DBL: Couldn't allocate array of floats\n");
    return (NULL);
  }
  return (new);
}


/*** Allocate 2d array of floats ***/

float **alloc_2d_dbl(m, n)
int m, n;
{
  int i;
  float **new;

  //new = (float **) malloc ((unsigned) (m * sizeof (float *)));
  //if (new == NULL)
  if (posix_memalign((void **) &new, CACHELINE_SIZE, m * sizeof(float *)))
  {
    printf("ALLOC_2D_DBL: Couldn't allocate array of dbl ptrs\n");
    return (NULL);
  }

  for (i = 0; i < m; i++) {
    new[i] = alloc_1d_dbl(n);
  }

  return (new);
}


bpnn_randomize_weights(w, m, n)
float **w;
int m, n;
{
  int i, j;

  for (i = 0; i <= m; i++) {
    for (j = 0; j <= n; j++) {
     w[i][j] = (float) rand()/RAND_MAX;
    //  w[i][j] = dpn1();
    }
  }
}

bpnn_randomize_row(w, m)
float *w;
int m;
{
	int i;
	for (i = 0; i <= m; i++) {
    w[i] = (float) rand()/RAND_MAX;
  }
}


bpnn_zero_weights(w, m, n)
float **w;
int m, n;
{
  int i, j;

  for (i = 0; i <= m; i++) {
    for (j = 0; j <= n; j++) {
      w[i][j] = 0.0;
    }
  }
}


void bpnn_initialize(seed)
{
  printf("Random number generator seed: %d\n", seed);
  srand(seed);
}


BPNN *bpnn_internal_create(n_in, n_hidden, n_out)
int n_in, n_hidden, n_out;
{
  BPNN *newnet;

  newnet = (BPNN *) malloc (sizeof (BPNN));
  if (newnet == NULL) {
    printf("BPNN_CREATE: Couldn't allocate neural network\n");
    return (NULL);
  }

  newnet->input_n = n_in;
  newnet->hidden_n = n_hidden;
  newnet->output_n = n_out;
  newnet->input_units = alloc_1d_dbl(n_in + 1);
  newnet->hidden_units = alloc_1d_dbl(n_hidden + 1);
  newnet->output_units = alloc_1d_dbl(n_out + 1);

  newnet->hidden_delta = alloc_1d_dbl(n_hidden + 1);
  newnet->output_delta = alloc_1d_dbl(n_out + 1);
  newnet->target = alloc_1d_dbl(n_out + 1);

  newnet->input_weights = alloc_2d_dbl(n_in + 1, n_hidden + 1);
  newnet->hidden_weights = alloc_2d_dbl(n_hidden + 1, n_out + 1);

  newnet->input_prev_weights = alloc_2d_dbl(n_in + 1, n_hidden + 1);
  newnet->hidden_prev_weights = alloc_2d_dbl(n_hidden + 1, n_out + 1);

  return (newnet);
}


void bpnn_free(net)
BPNN *net;
{
  int n1, n2, i;

  n1 = net->input_n;
  n2 = net->hidden_n;

  free((char *) net->input_units);
  free((char *) net->hidden_units);
  free((char *) net->output_units);

  free((char *) net->hidden_delta);
  free((char *) net->output_delta);
  free((char *) net->target);

  for (i = 0; i <= n1; i++) {
    free((char *) net->input_weights[i]);
    free((char *) net->input_prev_weights[i]);
  }
  free((char *) net->input_weights);
  free((char *) net->input_prev_weights);

  for (i = 0; i <= n2; i++) {
    free((char *) net->hidden_weights[i]);
    free((char *) net->hidden_prev_weights[i]);
  }
  free((char *) net->hidden_weights);
  free((char *) net->hidden_prev_weights);

  free((char *) net);
}


/*** Creates a new fully-connected network from scratch,
     with the given numbers of input, hidden, and output units.
     Threshold units are automatically included.  All weights are
     randomly initialized.

     Space is also allocated for temporary storage (momentum weights,
     error computations, etc).
***/

BPNN *bpnn_create(n_in, n_hidden, n_out)
int n_in, n_hidden, n_out;
{

  BPNN *newnet;

  newnet = bpnn_internal_create(n_in, n_hidden, n_out);

#ifdef INITZERO
  bpnn_zero_weights(newnet->input_weights, n_in, n_hidden);
#else
  bpnn_randomize_weights(newnet->input_weights, n_in, n_hidden);
#endif
  bpnn_randomize_weights(newnet->hidden_weights, n_hidden, n_out);
  bpnn_zero_weights(newnet->input_prev_weights, n_in, n_hidden);
  bpnn_zero_weights(newnet->hidden_prev_weights, n_hidden, n_out);
  bpnn_randomize_row(newnet->target, n_out);
  return (newnet);
}


void *bpnn_pthread_worker(void *args)
{
  volatile thread_arg_t *arg = (thread_arg_t *) args;

  int in = arg->in;
  int hid = arg->hid;
  int out = arg->out;

  float *il = arg->il;
  float *hl = arg->hl;
  float *ol = arg->ol;

  float *delta_h = arg->delta_h;
  float *delta_o = arg->delta_o;

  float *target = arg->target;

  float **iw = arg->iw;
  float **hw = arg->hw;

  float **old_iw = arg->old_iw;
  float **old_hw = arg->old_hw;

  float *hid_err = arg->hid_err;
  float *out_err = arg->out_err;

  int tid = arg->tid;
  pthread_mutex_t *lock = arg->lock;
  pthread_barrier_t *barrier = arg->barrier;

  float sum, o, t, h, new_dw;
  int j, k;
  //float out_err, hid_err;

  double start, end;
  int start_hid, end_hid, start_out, end_out;

  start = tid * (hid / nthreads);
  end = (tid + 1.0) * (hid / nthreads);
  start_hid = (hid < nthreads) ?
    (tid < hid) ? tid + 1 : tid
    : start + 1;
  end_hid = (hid < nthreads) ?
    (tid < hid) ? start_hid + 1: tid
    : end + 1;

  start = tid * (out / nthreads);
  end = (tid + 1.0) * (out / nthreads);
  start_out = (out < nthreads) ?
    (tid < out) ? tid + 1 : tid
    : start + 1;
  end_out = (out < nthreads) ?
    (tid < out) ? start_out + 1 : tid
    : end + 1; 

  //printf("thread %d: start_hid<%d> end_hid<%d> start_out<%d> end_out<%d>\n",
  //    tid, start_hid, end_hid, start_out, end_out);

  /*** Forward propagation from input layer to hidden layer ***/
  /*** Set up thresholding unit ***/
  if (tid == 0) il[0] = 1.0;
  pthread_barrier_wait(barrier);

  /*** For each unit in second layer ***/
  for (j = start_hid; j < end_hid; j++) {
    /*** Compute weighted sum of its inputs ***/
    hl[j] = 0.0;
    for (k = 0; k <= in; k++) {
      UpdateII(&iw[k][j], &il[k], &hl[j], FMULT);
      /*mcsim_skip_instrs_begin();
      hl[j] += iw[k][j] * il[k];
      mcsim_skip_instrs_end();*/
    }
    Gather(0, 0, &hl[j], 1);
    hl[j] = squash(hl[j]);
    /*sum = 0.0;
    for (k = 0; k <= in; k++) {
      sum += iw[k][j] * il[k];
    }
    hl[j] = squash(sum);*/
    //printf("hl[%d]: %f\n", j, hl[j]);
  }

  /*** Forward propagation from hidden layer to output layer ***/
  /*** Set up threasholding unit ***/
  if (tid == 0) hl[0] = 1.0;
  pthread_barrier_wait(barrier);
  /*** For each unit in second layer ***/
  for (j = start_out; j < end_out; j++) {
    /*** Compute weighted sum of its inputs ***/
    ol[j] = 0.0;
    for (k = 0; k <= hid; k++) {
      UpdateII(&hw[k][j], &hl[k], &ol[j], FMULT);
      /*mcsim_skip_instrs_begin();
      ol[j] += hw[k][j] * hl[k];
      mcsim_skip_instrs_end();*/
    }
    Gather(0, 0, &ol[j], 1);
    ol[j] = squash(ol[j]);
    /*sum = 0.0;
    for (k = 0; k <= hid; k++) {
      sum += hw[k][j] * hl[k];
    }
    ol[j] = squash(sum);
    */
    //printf("ol[%d]: %f\n", j, ol[j]);
  }

  pthread_barrier_wait(barrier);

  if (tid == 0) {
    /*** Backpropagate for output error ***/
    *out_err = 0.0;
    for (j = 1; j <= out; j++) {
      o = ol[j];
      t = target[j];
      delta_o[j] = o * (1.0 - o) * (t - o);
      *out_err += ABS(delta_o[j]);
    }

    /*** Backpropagate for hidden error ***/
    *hid_err = 0.0;
    for (j = 1; j <= hid; j++) {
      h = hl[j];
      sum = 0.0;
      for (k = 1; k <= out; k++) {
        sum += delta_o[k] * hw[j][k];
      }
      delta_h[j] = h * (1.0 - h) * sum;
      *hid_err += ABS(delta_h[j]);
    }
  }

  /*** Adjust hidden to output weights ***/
  if (tid == 0) hl[0] = 1.0;
  pthread_barrier_wait(barrier);
  for (j = start_out; j < end_out; j++) {
    for (k = 0; k <= hid; k++) {
      new_dw = ((ETA * delta_o[j] * hl[k]) + (MOMENTUM * old_hw[j][j]));
      hw[k][j] += new_dw;
      old_hw[k][j] = new_dw;
    }
  }

  /*** Adjust input to hidden weights ***/
  if (tid == 0) il[0] = 0;
  pthread_barrier_wait(barrier);
  for (j = start_hid; j < end_hid; j++) {
    for (k = 0; k <= in; k++) {
      new_dw = ((ETA * delta_h[j] * il[k]) + (MOMENTUM * old_iw[j][j]));
      iw[k][j] += new_dw;
      old_iw[k][j] = new_dw;
    }
  }
}

void bpnn(il, hl, ol, iw, hw, in, hid, out, delta_o, target, delta_h, old_hw, old_iw)
float *il, *hl, *ol, **iw, **hw, *delta_o, *target, *delta_h, **old_hw, **old_iw;
int in, hid, out;
{
  int i;
  float hid_err, out_err;

  /* allocate memory for thread argument */
  thread_arg_t *thread_arg = (thread_arg_t *) malloc(sizeof(thread_arg_t) * nthreads);
  pthread_t *thread_handle = (pthread_t *) malloc(sizeof(pthread_t) * nthreads);
  pthread_mutex_t lock;
  pthread_barrier_t barrier;

  pthread_mutex_init(&lock, NULL);
  pthread_barrier_init(&barrier, NULL, nthreads);

  for (i = 0; i < nthreads; i++) {
    thread_arg[i].in = in;
    thread_arg[i].hid = hid;
    thread_arg[i].out = out;

    thread_arg[i].il = il;
    thread_arg[i].hl = hl;
    thread_arg[i].ol = ol;

    thread_arg[i].delta_h = delta_h;
    thread_arg[i].delta_o = delta_o;

    thread_arg[i].target = target;

    thread_arg[i].iw = iw;
    thread_arg[i].hw = hw;

    thread_arg[i].old_iw = old_iw;
    thread_arg[i].old_hw = old_hw;

    thread_arg[i].hid_err = &hid_err;
    thread_arg[i].out_err = &out_err;

    thread_arg[i].tid = i;
    thread_arg[i].lock = &lock;
    thread_arg[i].barrier = &barrier;
  }

  printf("Total threads created = %d\n", nthreads);

  roi_begin();

  for (i = 1; i < nthreads; i++) {
    pthread_create(thread_handle+i,
        NULL,
        bpnn_pthread_worker,
        (void *) &thread_arg[i]);
  }
  bpnn_pthread_worker((void *) &thread_arg[0]);

  printf("Threads returned!\n");

  for (i = 1; i < nthreads; i++) {
    pthread_join(thread_handle[i], NULL);
  }

  roi_end();

  printf("(From bpnn) Output err: %f and hidden err: %f\n", out_err, hid_err);

  free(thread_arg);
  free(thread_handle);
}

//void bpnn(il, hl, ol, iw, hw, in, hid, out, delta_o, target, delta_h, old_hw, old_iw)
//float *il, *hl, *ol, **iw, **hw, *delta_o, *target, *delta_h, **old_hw, **old_iw;
//int in, hid, out;
//{
//  float sum, o, t, h, new_dw;
//  int j, k;
//  float out_err, hid_err;
//
//  /*** Forward propagation from input layer to hidden layer ***/
//  /*** Set up thresholding unit ***/
//  il[0] = 1.0;
//#ifdef OPEN
//  omp_set_num_threads(NUM_THREAD);
//  #pragma omp parallel for shared(iw, in, hid, il) private(k, j) reduction(+: sum) schedule(static)
//#endif
//  /*** For each unit in second layer ***/
//  for (j = 1; j <= hid; j++) {
//    /*** Compute weighted sum of its inputs ***/
//    sum = 0.0;
//    for (k = 0; k <= in; k++) {
//      sum += iw[k][j] * il[k];
//    }
//    hl[j] = squash(sum);
//  }
//
//  /*** Forward propagation from hidden layer to output layer ***/
//  /*** Set up threasholding unit ***/
//  hl[0] = 1.0;
//#ifdef OPEN
//  omp_set_num_threads(NUM_THREAD);
//  #pragma omp parallel for shared(hw, hid, out, hl) private(k, j) reduction(+: sum) schedule(static)
//#endif
//  /*** For each unit in second layer ***/
//  for (j = 1; j <= out; j++) {
//    /*** Compute weighted sum of its inputs ***/
//    sum = 0.0;
//    for (k = 0; k <= hid; k++) {
//      sum += hw[k][j] * hl[k];
//    }
//    ol[j] = squash(sum);
//  }
//
//  /*** Backpropagate for output error ***/
//  out_err = 0.0;
//  for (j = 1; j <= out; j++) {
//    o = ol[j];
//    t = target[j];
//    delta_o[j] = o * (1.0 - o) * (t - o);
//    out_err += ABS(delta_o[j]);
//  }
//
//  /*** Backpropagate for hidden error ***/
//  hid_err = 0.0;
//  for (j = 1; j <= hid; j++) {
//    h = hl[j];
//    sum = 0.0;
//    for (k = 1; k <= out; k++) {
//      sum += delta_o[k] * hw[j][k];
//    }
//    delta_h[j] = h * (1.0 - h) * sum;
//    hid_err += ABS(delta_h[j]);
//  }
//
//  /*** Adjust hidden to output weights ***/
//  hl[0] = 1.0;
//#ifdef OPEN
//  omp_set_num_threads(NUM_THREAD);
//  #pragma omp parallel for  \
//    shared(old_hw, hw, delta_o)  \
//    private(j, k, new_dw) \
//    firstprivate(out, hid)
//#endif
//  for (j = 1; j <= out; j++) {
//    for (k = 0; k <= hid; k++) {
//      new_dw = ((ETA * delta_o[j] * hl[k]) + (MOMENTUM * old_hw[j][j]));
//      hw[k][j] += new_dw;
//      old_hw[k][j] = new_dw;
//    }
//  }
//
//  /*** Adjust input to hidden weights ***/
//  il[0] = 0;
//#ifdef OPEN
//  omp_set_num_threads(NUM_THREAD);
//  #pragma omp parallel for  \
//    shared(old_iw, iw, delta_h)  \
//    private(j, k, new_dw) \
//    firstprivate(hid, in)
//#endif
//  for (j = 1; j <= hid; j++) {
//    for (k = 0; k <= in; k++) {
//      new_dw = ((ETA * delta_h[j] * il[k]) + (MOMENTUM * old_iw[j][j]));
//      iw[k][j] += new_dw;
//      old_iw[k][j] = new_dw;
//    }
//  }
//
//  printf("(From bpnn) Output err: %f and hidden err: %f\n", out_err, hid_err);
//}

void bpnn_layerforward(l1, l2, conn, n1, n2)
float *l1, *l2, **conn;
int n1, n2;
{
  float sum;
  int j, k;

  /*** Set up thresholding unit ***/
  l1[0] = 1.0;
#ifdef OPEN
  omp_set_num_threads(NUM_THREAD);
  #pragma omp parallel for shared(conn, n1, n2, l1) private(k, j) reduction(+: sum) schedule(static)
#endif 
  /*** For each unit in second layer ***/
  for (j = 1; j <= n2; j++) {

    /*** Compute weighted sum of its inputs ***/
    sum = 0.0;
    for (k = 0; k <= n1; k++) {	
      sum += conn[k][j] * l1[k]; 
    }
    l2[j] = squash(sum);
    //printf("l2[%d]: %f\n", j, l2[j]);
  }
}

//extern "C"
void bpnn_output_error(delta, target, output, nj, err)  
float *delta, *target, *output, *err;
int nj;
{
  int j;
  float o, t, errsum;
  errsum = 0.0;
  for (j = 1; j <= nj; j++) {
    o = output[j];
    t = target[j];
    delta[j] = o * (1.0 - o) * (t - o);
    errsum += ABS(delta[j]);
  }
  *err = errsum;
}


void bpnn_hidden_error(delta_h,   
					   nh, 
					   delta_o, 
					   no, 
					   who, 
					   hidden, 
					   err)
float *delta_h, *delta_o, *hidden, **who, *err;
int nh, no;
{
  int j, k;
  float h, sum, errsum;

  errsum = 0.0;
  for (j = 1; j <= nh; j++) {
    h = hidden[j];
    sum = 0.0;
    for (k = 1; k <= no; k++) {
      sum += delta_o[k] * who[j][k];
    }
    delta_h[j] = h * (1.0 - h) * sum;
    errsum += ABS(delta_h[j]);
  }
  *err = errsum;
}


void bpnn_adjust_weights(delta, ndelta, ly, nly, w, oldw)
float *delta, *ly, **w, **oldw;
{
  float new_dw;
  int k, j;
  ly[0] = 1.0;
  //eta = 0.3;
  //momentum = 0.3;

#ifdef OPEN
  omp_set_num_threads(NUM_THREAD);
  #pragma omp parallel for  \
      shared(oldw, w, delta) \
	  private(j, k, new_dw) \
	  firstprivate(ndelta, nly) 
#endif 
  for (j = 1; j <= ndelta; j++) {
    for (k = 0; k <= nly; k++) {
      new_dw = ((ETA * delta[j] * ly[k]) + (MOMENTUM * oldw[k][j]));
	  w[k][j] += new_dw;
	  oldw[k][j] = new_dw;
    }
  }
}


void bpnn_feedforward(net)
BPNN *net;
{
  int in, hid, out;

  in = net->input_n;
  hid = net->hidden_n;
  out = net->output_n;

  /*** Feed forward input activations. ***/
  bpnn_layerforward(net->input_units, net->hidden_units,
      net->input_weights, in, hid);
  bpnn_layerforward(net->hidden_units, net->output_units,
      net->hidden_weights, hid, out);

}


void bpnn_train(net, eo, eh)
BPNN *net;
float *eo, *eh;
{
  int in, hid, out;
  float out_err, hid_err;

  in = net->input_n;
  hid = net->hidden_n;
  out = net->output_n;

  /*** Feed forward input activations. ***/
  bpnn_layerforward(net->input_units, net->hidden_units,
      net->input_weights, in, hid);
  bpnn_layerforward(net->hidden_units, net->output_units,
      net->hidden_weights, hid, out);

  /*** Compute error on output and hidden units. ***/
  bpnn_output_error(net->output_delta, net->target, net->output_units,
      out, &out_err);
  bpnn_hidden_error(net->hidden_delta, hid, net->output_delta, out,
      net->hidden_weights, net->hidden_units, &hid_err);
  *eo = out_err;
  *eh = hid_err;

  /*** Adjust input and hidden weights. ***/
  bpnn_adjust_weights(net->output_delta, out, net->hidden_units, hid,
      net->hidden_weights, net->hidden_prev_weights);
  bpnn_adjust_weights(net->hidden_delta, hid, net->input_units, in,
      net->input_weights, net->input_prev_weights);

}




void bpnn_save(net, filename)
BPNN *net;
char *filename;
{
  int n1, n2, n3, i, j, memcnt;
  float dvalue, **w;
  char *mem;
  ///add//
  FILE *pFile;
  pFile = fopen( filename, "w+" );
  ///////
  /*
  if ((fd = creat(filename, 0644)) == -1) {
    printf("BPNN_SAVE: Cannot create '%s'\n", filename);
    return;
  }
  */

  n1 = net->input_n;  n2 = net->hidden_n;  n3 = net->output_n;
  printf("Saving %dx%dx%d network to '%s'\n", n1, n2, n3, filename);
  //fflush(stdout);

  //write(fd, (char *) &n1, sizeof(int));
  //write(fd, (char *) &n2, sizeof(int));
  //write(fd, (char *) &n3, sizeof(int));

  fwrite( (char *) &n1 , sizeof(char), sizeof(char), pFile);
  fwrite( (char *) &n2 , sizeof(char), sizeof(char), pFile);
  fwrite( (char *) &n3 , sizeof(char), sizeof(char), pFile);

  

  memcnt = 0;
  w = net->input_weights;
  mem = (char *) malloc ((unsigned) ((n1+1) * (n2+1) * sizeof(float)));
  for (i = 0; i <= n1; i++) {
    for (j = 0; j <= n2; j++) {
      dvalue = w[i][j];
      fastcopy(&mem[memcnt], &dvalue, sizeof(float));
      memcnt += sizeof(float);
    }
  }
  //write(fd, mem, (n1+1) * (n2+1) * sizeof(float));
  fwrite( mem , (unsigned)(sizeof(float)), (unsigned) ((n1+1) * (n2+1) * sizeof(float)) , pFile);
  free(mem);

  memcnt = 0;
  w = net->hidden_weights;
  mem = (char *) malloc ((unsigned) ((n2+1) * (n3+1) * sizeof(float)));
  for (i = 0; i <= n2; i++) {
    for (j = 0; j <= n3; j++) {
      dvalue = w[i][j];
      fastcopy(&mem[memcnt], &dvalue, sizeof(float));
      memcnt += sizeof(float);
    }
  }
  //write(fd, mem, (n2+1) * (n3+1) * sizeof(float));
  fwrite( mem , sizeof(float), (unsigned) ((n2+1) * (n3+1) * sizeof(float)) , pFile);
  free(mem);

  fclose(pFile);
  return;
}


BPNN *bpnn_read(filename)
char *filename;
{
  char *mem;
  BPNN *new;
  int fd, n1, n2, n3, i, j, memcnt;

  if ((fd = open(filename, 0, 0644)) == -1) {
    return (NULL);
  }

  printf("Reading '%s'\n", filename);  //fflush(stdout);

  read(fd, (char *) &n1, sizeof(int));
  read(fd, (char *) &n2, sizeof(int));
  read(fd, (char *) &n3, sizeof(int));
  new = bpnn_internal_create(n1, n2, n3);

  printf("'%s' contains a %dx%dx%d network\n", filename, n1, n2, n3);
  printf("Reading input weights...");  //fflush(stdout);

  memcnt = 0;
  mem = (char *) malloc ((unsigned) ((n1+1) * (n2+1) * sizeof(float)));
  read(fd, mem, (n1+1) * (n2+1) * sizeof(float));
  for (i = 0; i <= n1; i++) {
    for (j = 0; j <= n2; j++) {
      fastcopy(&(new->input_weights[i][j]), &mem[memcnt], sizeof(float));
      memcnt += sizeof(float);
    }
  }
  free(mem);

  printf("Done\nReading hidden weights...");  //fflush(stdout);

  memcnt = 0;
  mem = (char *) malloc ((unsigned) ((n2+1) * (n3+1) * sizeof(float)));
  read(fd, mem, (n2+1) * (n3+1) * sizeof(float));
  for (i = 0; i <= n2; i++) {
    for (j = 0; j <= n3; j++) {
      fastcopy(&(new->hidden_weights[i][j]), &mem[memcnt], sizeof(float));
      memcnt += sizeof(float);
    }
  }
  free(mem);
  close(fd);

  printf("Done\n");  //fflush(stdout);

  bpnn_zero_weights(new->input_prev_weights, n1, n2);
  bpnn_zero_weights(new->hidden_prev_weights, n2, n3);

  return (new);
}
