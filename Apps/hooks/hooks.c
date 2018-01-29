#include "hooks.h"

#include <stdio.h>

void roi_begin()
{
  printf("[" HOOKS_STR "] ROI_begin\n");
  magic_op(MAGIC_OP_ROI_BEGIN);
}

void roi_end()
{
  magic_op(MAGIC_OP_ROI_END);
  printf("[" HOOKS_STR "] ROI_end\n");
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

