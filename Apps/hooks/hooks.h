#ifndef __HOOKS_H__
#define __HOOKS_H__

// Acknoledgement, code snippet from zsim

#include <stdint.h>
//#include <stdio.h>

// Avoid optimizing compilers moving code around this barrier
#define COMPILER_BARRIER() { __asm__ __volatile__("" ::: "memory"); }

#define MAGIC_OP_ROI_BEGIN         (1025)
#define MAGIC_OP_ROI_END           (1026)
#define MAGIC_OP_SKIP_INSTR_BEGIN  (1027)
#define MAGIC_OP_SKIP_INSTR_END    (1028)
#define MAGIC_OP_SPIN_BEGIN        (1029)
#define MAGIC_OP_SPIN_END          (1030)

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

void mcsim_skip_instrs_begin();
void mcsim_skip_instrs_end();
void mcsim_spinning_begin();
void mcsim_spinning_end();

#ifdef __cplusplus
}
#endif

#endif // __HOOKS_H__
