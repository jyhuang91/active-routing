#ifndef __HOOKS_H__
#define __HOOKS_H__

// Acknoledgement, code snippet from zsim

#include <stdint.h>

// TODO: add namespace to resolve possible conflicts, but
//       funciton name should be identified by Pthread tool

// Avoid optimizing compilers moving code around this barrier
#define COMPILER_BARRIER() { __asm__ __volatile__("" ::: "memory"); }

#define MAGIC_OP_ROI_BEGIN         (1025)
#define MAGIC_OP_ROI_END           (1026)
#define MAGIC_OP_SKIP_INSTR_BEGIN  (1027)
#define MAGIC_OP_SKIP_INSTR_END    (1028)
#define MAGIC_OP_SPIN_BEGIN        (1029)
#define MAGIC_OP_SPIN_END          (1030)
#define MAGIC_OP_UPDATE            (1031)
#define MAGIC_OP_GATHER            (1032)
#define MAGIC_OP_ACTIVE_BEGIN      (1033)
#define MAGIC_OP_ACTIVE_END        (1034)
#define MAGIC_OP_INSTRS_COUNT      (1035)

// Active operations
typedef enum eOpcode {
  GET = 0,
  IADD,
  FADD,
  DADD,
  IMULT,
  FMULT,
  DMULT,
  IFMULT,
  IDMULT,
  FDMULT,
  DIDIV,
  DEXP,
  // PEI
  IPEI_DOT,
  FPEI_DOT,
  DPEI_DOT,
  IFPEI_DOT,
  IDPEI_DOT,
  FDPEI_DOT,
  IPEI_ATOMIC,
  FPEI_ATOMIC,
  DPEI_ATOMIC
} Opcode;

typedef enum {
  ART_CATEGORY_GET = 128,
  ART_CATEGORY_ADD,
  ART_CATEGORY_MULT,
  PEI_CATEGORY_DOT,
  PEI_CATEGORY_ATOMIC
} ins_category_enum_t;

#define CACHELINE_SIZE 64 // Bytes
#define PEI_GRANULARITY 4 // elements

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __x86_64__
#define HOOKS_STR "MCSIM-HOOKS"
static inline void magic_op(uint64_t op) {
  COMPILER_BARRIER();
  __asm__ __volatile__("xchg %%rcx, %%rcx;" : : "c"(op));
  COMPILER_BARRIER();
}
#else
#define HOOKS_STR "MCSIM-NOP-HOOKS"
static inline void magic_op(uint64_t op) {
  // NOP
}
#endif

void roi_begin();
void roi_end();

void active_begin();
void active_end();

void mcsim_skip_instrs_begin();
void mcsim_skip_instrs_end();
void mcsim_spinning_begin();
void mcsim_spinning_end();

void UpdateRR(void *src_addr1, void *src_addr2, void *dest_addr, Opcode op);
void UpdateRI(void *src_addr1, void *src_addr2, void *dest_addr, Opcode op);
void UpdateII(void *src_addr1, void *src_addr2, void *dest_addr, Opcode op);
void Update(void *src_addr1, void *src_addr2, void *dest_addr, Opcode op);
void Gather(void *src_addr1, void *src_addr2, void *dest_addr, int nthreads);

int testFunc(int tid);
#ifdef __cplusplus
}
#endif

#endif // __HOOKS_H__
