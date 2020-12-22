#include "hooks.h"

#include <stdio.h>

const char * const OpcodeString[] = {
  "GET",
  "IADD",
  "FADD",
  "DADD",
  "IMULT",
  "FMULT",
  "DMULT",
  "IFMULT",
  "IDMULT",
  "FDMULT",
  "DIDIV",
  "DEXP",
  // PEI
  "IPEI_DOT",
  "FPEI_DOT",
  "DPEI_DOT",
  "IFPEI_DOT",
  "IDPEI_DOT",
  "FDPEI_DOT",
  "IPEI_ATOMIC",
  "FPEI_ATOMIC",
  "DPEI_ATOMIC"
};

void roi_begin()
{
  printf("[" HOOKS_STR "] ROI begin\n");
  magic_op(MAGIC_OP_ROI_BEGIN);
}

void roi_end()
{
  magic_op(MAGIC_OP_ROI_END);
  printf("[" HOOKS_STR "] ROI end\n");
}

void active_begin()
{
  printf("[" HOOKS_STR "] Active begin\n");
  magic_op(MAGIC_OP_ACTIVE_BEGIN);
}

void active_end()
{
  magic_op(MAGIC_OP_ACTIVE_END);
  printf("[" HOOKS_STR "] Active end\n");
}

void mcsim_skip_instrs_begin()
{
  magic_op(MAGIC_OP_SKIP_INSTR_BEGIN);
}
void mcsim_skip_instrs_end()
{
  magic_op(MAGIC_OP_SKIP_INSTR_END);
}
void mcsim_spinning_begin()
{
  magic_op(MAGIC_OP_SPIN_BEGIN);
}
void mcsim_spinning_end()
{
  magic_op(MAGIC_OP_SPIN_END);
}

void UpdatePage(void *src_addr, uint32_t lines, void *dest_addr, eOpcode op)
{
  //magic_op(MAGIC_OP_UPDATE);
}

void UpdateRRPage(void *src_addr1, void *src_addr2, void *dest_addr, uint32_t lines, eOpcode op)
{
  //magic_op(MAGIC_OP_UPDATE);
}

void UpdateRR(void *src_addr1, void *src_addr2, void *dest_addr, eOpcode op)
{
  //magic_op(MAGIC_OP_UPDATE);
}
void UpdateRI(void *src_addr1, void *src_addr2, void *dest_addr, eOpcode op)
{
  //magic_op(MAGIC_OP_UPDATE);
}
void UpdateII(void *src_addr1, void *src_addr2, void *dest_addr, eOpcode op)
{
  //magic_op(MAGIC_OP_UPDATE);
}
void Update(void *src_addr1, void *src_addr2, void *dest_addr, eOpcode op)
{
  //magic_op(MAGIC_OP_UPDATE);
}
void Gather(void *src_addr1, void *src_addr2, void *dest_addr, int nthreads)
{
  //magic_op(MAGIC_OP_GATHER);
}

int testFunc(int tid)
{
  return 1;
}
