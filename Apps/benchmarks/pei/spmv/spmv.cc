#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> 
#include <hooks.h>

#define WEIGHT_RANGE 2.0

int nnz = 0; 
typedef struct{
  int num_rows; 
  int num_cols; 
  int nnz; 
  int max_row; 
  int* row_ptr;  
  int* col_ind; 
  float* vals;
}spm_t;

typedef struct{ 
  spm_t *spm; 
  float *inVec; 
  float *outVec;
  int dim;
  int tid;
  int num_threads;
}arg_t; 

pthread_t   thread_handle[1024];   //MAX threads and pthread handlers
arg_t thread_arg[1024];

float** CreateSparseMatrix(int matrix_dim);  
float* CreateVector(int dim);  
spm_t* CompressMatrix(float **matrix, int matrix_dim); 
void DeleteMatrix(float** matrix, int matrix_dim);
void DeleteVector(float* vec);
void DeleteSparseMatrix(spm_t* spm);
void printSparseMat(spm_t* sparse_mat);
void* matVecMul(void *args);
float THRESHOLD = 0.2;

int main(int argc, char* argv[]){
  int matrix_dim = 32; /*default*/
  int num_threads = 2; 
  if(argc > 1){
    matrix_dim = atoi(argv[1]);
    num_threads = atoi(argv[2]);  
    THRESHOLD = atof(argv[3]); 
  } 
  float **matrix = CreateSparseMatrix(matrix_dim);
  spm_t *sparse_mat = CompressMatrix(matrix, matrix_dim);
  float* vec = CreateVector(matrix_dim); 
  float* out = CreateVector(matrix_dim); 
  for(int i = 0; i< num_threads; i++){
    thread_arg[i].spm = sparse_mat; 
    thread_arg[i].inVec = vec; 
    thread_arg[i].outVec = out; 
    thread_arg[i].dim = matrix_dim;  
    thread_arg[i].tid = i;  
    thread_arg[i].num_threads = num_threads;  
  }
  roi_begin();
  for(int i = 1; i<num_threads; i++){
    pthread_create(thread_handle + i, NULL, matVecMul, (void *)&thread_arg[i]);
  }
  matVecMul((void *) &thread_arg[0]);
  for(int i = 1; i<num_threads; i++){
    pthread_join(thread_handle[i], NULL);
  } 
  roi_end();
  if(matrix_dim <= 8){
    for(int i = 0; i < matrix_dim; i++){
      for(int j=0; j < matrix_dim; j++){
        printf("%f\t", matrix[i][j]);
      }
      printf("\t\t%f\t\t%f\n", vec[i], out[i]);
    } 
    printSparseMat(sparse_mat);
  }
  DeleteMatrix(matrix, matrix_dim); 
  DeleteVector(vec);
  DeleteSparseMatrix(sparse_mat);
  return 0; 
}

void* matVecMul(void *args){
  arg_t *arg = (arg_t *)args; 
  int dim = arg->dim; 
  int tid = arg->tid;
  int num_threads = arg->num_threads;
  spm_t* spm = arg->spm; 
  float* inVec = arg->inVec;
  float* outVec = arg->outVec;
  void* addr_arr[4];
  
  for(int i = tid; i < dim; i += num_threads){
    outVec[i] = 0; 
  //  if(spm->row_ptr[i+1] - spm->row_ptr[i] < 4){
  //    for(int k =spm->row_ptr[i]; k < spm->row_ptr[i+1]; k++)outVec[i] += inVec[spm->col_ind[k]]*spm->vals[k];
  //  }else{
      for(int j = spm->row_ptr[i]; j<spm->row_ptr[i+1]; j+=4){
        addr_arr[0] = (inVec + spm->col_ind[j]);
        addr_arr[1] = (inVec + spm->col_ind[j+1]);
        addr_arr[2] = (inVec + spm->col_ind[j+2]);
        addr_arr[3] = (inVec + spm->col_ind[j+3]);
        UPDATE(addr_arr, spm->vals +j, outVec + i, PEI_DOT); 
        //outVec[i] += inVec[spm->col_ind[j]]*spm->vals[j];
   //   }
    }
    //if(spm->row_ptr[i] < spm->row_ptr[i+1]) GATHER(NULL, NULL, outVec + i, 1); 
  }
  return NULL; 
} 

float **CreateSparseMatrix(int matrix_dim){
  float **mat = (float **)malloc(matrix_dim*(sizeof(float *)));
  for(int i = 0; i< matrix_dim; i++){
    mat[i] = (float *)malloc(matrix_dim*(sizeof(float)));
  }
  for(int i=0; i< matrix_dim; i++){
    for(int j =0; j<matrix_dim; j++){
      if(rand() > RAND_MAX*THRESHOLD) mat[i][j] = 0;
      else{ 
        mat[i][j] = ((float)rand()/(float)(RAND_MAX)) * WEIGHT_RANGE - WEIGHT_RANGE/2.0; 
        nnz++; 
      }
    }
  }
  printf("CREATED matrix with %d non zero elements\n", nnz); 
  return mat;
}

float* CreateVector(int dim){
  float* vec = (float *)malloc(dim * sizeof(float));
  for(int i = 0; i < dim; i++) vec[i] =  ((float)rand()/(float)(RAND_MAX)) * WEIGHT_RANGE - WEIGHT_RANGE/2.0; 
  return vec;
};

spm_t *CompressMatrix(float **matrix, int matrix_dim){
  spm_t *sparse_mat = (spm_t *)malloc(sizeof(spm_t));
  int row_ind = 0; 
  int *row_ptr = (int *)malloc((matrix_dim + 1)*sizeof(int)); 
//  for(int i = 0; i < matrix_dim; i++){
//    row_ptr[i] = -1; 
//  }
  int val_ind = 0; 
  int *col_ind = (int *)malloc((nnz)*sizeof(int)); 
  float *vals = (float *)malloc((nnz)*sizeof(float));
  for(int i = 0; i < matrix_dim; i++){
    row_ptr[row_ind] = val_ind;
    row_ind++; 
    for(int j=0; j < matrix_dim; j++){
      if(matrix[i][j] != 0){ 
        vals[val_ind] = matrix[i][j];
        col_ind[val_ind] = j; 
        if(val_ind > nnz) printf("%d ERROR",nnz); 
        val_ind++; 
      }
    }
  }
  row_ptr[row_ind] = val_ind;
  sparse_mat->num_rows = matrix_dim;
  sparse_mat->num_cols = matrix_dim;
  sparse_mat->nnz      = nnz;
  sparse_mat->max_row  = matrix_dim;
  sparse_mat->row_ptr  = row_ptr; 
  sparse_mat->col_ind  = col_ind;
  sparse_mat->vals     = vals;
  return sparse_mat;
}

void DeleteVector(float* vec){
  free(vec);
  return; 
}
 
void DeleteMatrix(float** matrix, int matrix_dim){
  for(int i= 0; i< matrix_dim; i++){
    free(matrix[i]); 
  }
  free(matrix);
  return;
}

void DeleteSparseMatrix(spm_t* spm){
  free(spm->row_ptr); 
  free(spm->col_ind); 
  free(spm->vals); 
  free(spm); 
  return;
}

void printSparseMat(spm_t* sparse_mat){
  printf("Mat.num_rows = %d\n", sparse_mat->num_rows);
  printf("Mat.num_cols = %d\n", sparse_mat->num_cols);
  printf("Mat.nnz = %d\n", sparse_mat->nnz);
  printf("Mat.row_ptr = [");
  for(int i = 0 ; i <= sparse_mat->num_rows; i++) printf("%d, ",sparse_mat->row_ptr[i]); 
  printf("]\n");
  printf("Mat.col_ind = [");
  for(int i = 0 ; i < sparse_mat->nnz; i++) printf("%d, ",sparse_mat->col_ind[i]); 
  printf("]\n");
  printf("Mat.vals = [");
  for(int i = 0 ; i < sparse_mat->nnz; i++) printf("%f, ",sparse_mat->vals[i]); 
  printf("]\n");
}

