#include "mypthreadtool.h"
#include "stdio.h"
#include <hooks.h>

using namespace PinPthread;

/* ================================================================== */

int main(int argc, char** argv) 
{
    Init(argc, argv);
    PIN_InitSymbols();
    PIN_Init(argc, argv);
    IMG_AddInstrumentFunction(FlagImg, 0);
    RTN_AddInstrumentFunction(FlagRtn, 0);
    TRACE_AddInstrumentFunction(FlagTrace, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
    return 0;
}

/* ================================================================== */

namespace PinPthread
{

/* ------------------------------------------------------------------ */
/* Initalization & Clean Up                                           */
/* ------------------------------------------------------------------ */

VOID Init(uint32_t argc, char** argv) 
{
    pthreadsim = new PthreadSim(argc, argv);
    in_roi = false;
    ngather = 0;
}

VOID Fini(INT32 code, VOID* v) 
{
    delete pthreadsim;
    fprintf(stdout, "Pthread Tool ngather: %d\n", ngather);
}

VOID ProcessMemIns(
    CONTEXT * context,
    ADDRINT ip,
    ADDRINT raddr, ADDRINT raddr2, UINT32 rlen,
    ADDRINT waddr, UINT32  wlen,
    BOOL    isbranch,
    BOOL    isbranchtaken,
    UINT32  category,
    UINT32  rr0,
    UINT32  rr1,
    UINT32  rr2,
    UINT32  rr3,
    UINT32  rw0,
    UINT32  rw1,
    UINT32  rw2,
    UINT32  rw3)
{ // for memory address and register index, '0' means invalid

  if (pthreadsim->first_instrs == 0) {  // Jiayi
    cout << "initiating context at the begining ..." << endl;
    pthreadsim->initiate(context);
    pthreadsim->first_instrs++;
    cout << "finish context initialization ..." << endl;
    return;
  }
  if (pthreadsim->first_instrs < pthreadsim->skip_first)
  {
    pthreadsim->first_instrs++;
    return;
  }
  else if (pthreadsim->first_instrs == pthreadsim->skip_first)
  {
    pthreadsim->first_instrs++;
    //pthreadsim->initiate(context);  // Jiayi, for skip instructions
  }

  if ((pthreadsim->run_roi && in_roi) || pthreadsim->run_roi == false)
  {
    pthreadsim->process_ins(
        context,
        ip,
        raddr, raddr2, rlen,
        waddr,         wlen,
        isbranch,
        isbranchtaken,
        category,
        rr0, rr1, rr2, rr3,
        rw0, rw1, rw2, rw3);
  }
}


VOID NewPthreadSim(CONTEXT* ctxt)
{
  pthreadsim->set_stack(ctxt);
}


/* ------------------------------------------------------------------ */
/* Active-Routing Callback Routines                                   */
/* ------------------------------------------------------------------ */
#ifdef RUNTIME_KNOB
int gathers[] = {
    18877
    ,155543
    ,1906
    ,1919
    ,2492
    ,1252
    ,2444
    ,1840
    ,2389
    ,1799
    ,2339
    ,1179
    ,2300
    ,1732
    ,2258
    ,2255
    ,2210
    ,1662
    ,2169
    ,1636
    ,2127
    ,1606
    ,2622
    ,1576
    ,2055
    ,2066
    ,2018
    ,2027
    ,1982
    ,1993
    ,1953
    ,1962
    ,1919
    ,1928
    ,2363
    ,1896
    ,1859
    ,1868
    ,2288
    ,1836
    ,2255
    ,1808
    ,1772
    ,2223
    ,1753
    ,2190
    ,1726
    ,2159
    ,2122
    ,1707
    ,420
    ,415
    ,1255
    ,2100
    ,1653
    ,2069
    ,2033
    ,1633
    ,2414
    ,2015
    ,792
    ,392
    ,794
    ,1588
    ,2348
    ,1962
    ,1925
    ,1937
    ,1897
    ,1914
    ,2252
    ,1891
    ,1850
    ,1863
    ,2201
    ,1470
    ,2537
    ,1453
    ,723
    ,357
    ,1429
    ,1435
};
int testFunc(int tid)
{
  static int phase = 0;
  static int num = 0;
  static bool active = true;

  if (pthreadsim->active_mode != 2) return pthreadsim->active_mode;

  if (phase >= 82) return -1;

  if (num++ >= gathers[phase]) {
    fprintf(stderr, "[debug] change from %d to %d\n", active, !active);
    num = 0;
    phase++;
    active = !active;
  }
  return active;
}
#endif

// TODO: two operands may have different type, int/fp/double, may not aligned in cacheline
VOID UpdateRRAPI(CONTEXT *context, ADDRINT ip, VOID *a, VOID *b, VOID *c, Opcode opcode)
{
  uint32_t category = -1;
  uint32_t rlen = 4 * PEI_GRANULARITY;
  uint32_t wlen = 4; // Bytes
  switch (opcode)
  {
    case IADD:
    case FADD:
    case DADD:
      rlen = CACHELINE_SIZE;
      wlen = 0;
      //* ((float *)c) += * ((float *)a);
      category = ART_CATEGORY_ADD;
      break;
    case IMULT:
    case FMULT:
    case DMULT:
      rlen = CACHELINE_SIZE;
      wlen = 0;
      //* ((float *)c) += (*((float *)a)) * (*((float *)b));
      category = ART_CATEGORY_MULT;
      break;
    case DPEI_DOT:
      rlen = 8 * PEI_GRANULARITY;
      wlen = 8;
    case IPEI_DOT:
    case FPEI_DOT:
      category = PEI_CATEGORY_DOT;
      // load first half source operands on-chip
      for(int i = 0; i < PEI_GRANULARITY; i++) {
        pthreadsim->process_ins(
            context,
            ip,
            (ADDRINT) ((ADDRINT) a + 8*i), 0, rlen / PEI_GRANULARITY,
            0, 0,
            false, false,
            0,
            0, 0, 0, 0,
            0, 0, 0, 0);
      }
      break;
    default: fprintf(stderr, "Unknown active operation: %d\n", opcode); exit(1);
  }

  pthreadsim->process_ins(
      context,
      ip,
      (ADDRINT) a, (ADDRINT) b, rlen,
      (ADDRINT) c, wlen,
      false, false,
      category,
      0, 0, 0, 0,
      0, 0, 0, 0);
  //fprintf(stderr, " [UpdateRR API Pin: %p %p %p <%i> (tid: %d)]  \n", a, b, c, opcode, pthreadsim->scheduler->current->first);
}

VOID UpdateRIAPI(CONTEXT *context, ADDRINT ip, VOID *a, VOID *b, VOID *c, Opcode opcode)
{
  uint32_t category = -1;
  uint32_t rlen = 4 * PEI_GRANULARITY;
  uint32_t wlen = 4;
  uint32_t art_granularity = CACHELINE_SIZE / 4;
  switch (opcode)
  {
    case IADD:
    case FADD:
    case DADD:
      rlen = CACHELINE_SIZE;
      wlen = 0;
      //* ((float *)c) += * ((float *)a);
      category = ART_CATEGORY_ADD;
      break;
    case DMULT:
      art_granularity = CACHELINE_SIZE / 8;
    case IMULT:
    case FMULT:
      rlen = CACHELINE_SIZE;
      wlen = 0;
      //* ((float *)c) += (*((float *)a)) * (*((float *)b));
      category = ART_CATEGORY_DOT;
      for (int i = 0; i < art_granularity; i++)
      {
        pthreadsim->process_ins(
            context,
            ip,
            (ADDRINT) ((void **)a)[i], 0, rlen / art_granularity,
            0, 0,
            false, false,
            0,
            0, 0, 0, 0,
            0, 0, 0, 0);
      }
      break;
    case DPEI_DOT:
      rlen = 8 * PEI_GRANULARITY;
      wlen = 8;
    case IPEI_DOT:
    case FPEI_DOT:
      category = PEI_CATEGORY_DOT;
      // load first half source operands on-chip
      for(int i = 0; i < 4; i++) {
        pthreadsim->process_ins(
            context,
            ip,
            (ADDRINT) ((void **)a)[i], 0, rlen / PEI_GRANULARITY,
            0, 0,
            false, false,
            0,
            0, 0, 0, 0,
            0, 0, 0, 0);
      }      
      break;
    default: fprintf(stderr, "Unknown active operation: %d\n", opcode); exit(1);
  }

  pthreadsim->process_ins(
      context,
      ip,
      (ADDRINT) a, (ADDRINT) b, rlen,
      (ADDRINT) c, wlen,
      false, false,
      category,
      0, 0, 0, 0,
      0, 0, 0, 0);
  //fprintf(stderr, " [UPDATE API Pin: %p %p %p <%i> (tid: %d)]  \n", a, b, c, function, pthreadsim->scheduler->current->first);
}

VOID UpdateIIAPI(CONTEXT *context, ADDRINT ip, VOID *a, VOID *b, VOID *c, Opcode opcode)
{
  uint32_t category = -1;
  uint32_t rlen = 4;
  switch (opcode)
  {
    case DADD:
    case DIDIV:
      rlen = 8;
    case IADD:
    case FADD:
      //* ((float *)c) += * ((float *)a);
      category = ART_CATEGORY_ADD;
      break;
    case DMULT:
      rlen = 8;
    case IMULT:
    case FMULT:
      //* ((float *)c) += (*((float *)a)) * (*((float *)b));
      category = ART_CATEGORY_MULT;
      break;
    default: fprintf(stderr, "Unknown active operation: %d\n", opcode); exit(1);
  }

  pthreadsim->process_ins(
      context,
      ip,
      (ADDRINT) a, (ADDRINT) b, rlen,
      (ADDRINT) c, 0,
      false, false,
      category,
      0, 0, 0, 0,
      0, 0, 0, 0);
  //fprintf(stderr, " [UPDATE API Pin: %p %p %p <%i> (tid: %d)]  \n", a, b, c, function, pthreadsim->scheduler->current->first);
}

// Jiayi, 01/29/2018
VOID UpdateAPI(CONTEXT *context, ADDRINT ip, VOID *a, VOID *b, VOID *c, Opcode opcode)
{
  uint32_t category = -1;
  uint32_t rlen = 4;
  uint32_t wlen = 4;
  switch (opcode)
  {
    case DPEI_ATOMIC:
      rlen = 8;
      wlen = 8;
    case IPEI_ATOMIC:
    case FPEI_ATOMIC:
      category = PEI_CATEGORY_ATOMIC;
      // load source operand on-chip
      pthreadsim->process_ins(
         context,
         ip,
         (ADDRINT) (a), rlen, 0,
         0, 0,
         false, false,
         0,
         0, 0, 0, 0,
         0, 0, 0, 0);
      break;
    default: fprintf(stderr, "Unknown active operation: %d\n", opcode); exit(1);
  }

  pthreadsim->process_ins(
      context,
      ip,
      (ADDRINT) a, (ADDRINT) b, rlen,
      (ADDRINT) c, wlen,
      false, false,
      category,
      0, 0, 0, 0,
      0, 0, 0, 0);
  //fprintf(stderr, " [UPDATE API Pin: %p %p %p <%i> (tid: %d)]  \n", a, b, c, opcode, pthreadsim->scheduler->current->first);
}

VOID GatherAPI(CONTEXT *context, ADDRINT ip, VOID *a, VOID *b, VOID *c, int nthreads)
{
  pthreadsim->process_ins(
      context,
      ip,
      (ADDRINT) a, (ADDRINT) b, 0,
      (ADDRINT) c, nthreads,
      false, false,
      ART_CATEGORY_GET,
      0, 0, 0, 0,
      0, 0, 0, 0);
  ngather++;
  //fprintf(stderr, " [GATHER API Pin: %p %p %p <%i> (tid: %d)]  \n", a, b, c, nthreads, pthreadsim->scheduler->current->first);
}


/* ------------------------------------------------------------------ */
/* Graph preprocessing callback functions                             */
/* ------------------------------------------------------------------ */

VOID PRInitMemCall(INT32 N, INT32 *test, INT32 *exist, INT32 *test2, INT32 *dangling, INT32 *inlinks, INT32 *outlinks)
{
  std::cout << "Pagerank memory init in Pin Pthread Tool" << endl;
  for (int i = 0; i < N; i++) {
    test[i] = 0;
    exist[i] = 0;
    test2[i] = 0;
    dangling[i] = 0;
    inlinks[i] = 0;
    outlinks[i] = 0;
  }
}

VOID PRGraphFirstScanCall(FILE *fp, int *inlinks, int *outlinks, int *exist, int *test2, int *dangling)
{
  int number0 = -1;
  int number1 = -1;
  char line[256];

  std::cout << "Pagerank graph first scan in Pin Pthread Tool" << endl;
  while (fgets(line, sizeof(line), fp)) {
    if (line[0] == '#') continue;
    sscanf(line, "%d%*[^0-9]%d\n", &number0, &number1);
    inlinks[number1]++;
    outlinks[number0]++;
    exist[number0] = 1;
    exist[number1] = 1;
    test2[number0] = 1;
    dangling[number1] = 1;
  }
}

VOID PRGraphSecondScanCall(FILE *fp, int N, double ***W, int ***W_index, int *inlinks, int *outlinks, int *test, int *test2, int *dangling)
{
  int number0 = -1;
  int number1 = -1;
  int inter = -1;
  char line[256];

  std::cout << "Pagerank graph second scan in Pin Pthread Tool" << endl;
  (*W) = (double**) malloc(N*sizeof(double*));
  (*W_index) = (int**) malloc(N*sizeof(int*));
  for (int i = 0; i < N; i++)
  {
    int ret = posix_memalign((void**) &(*W)[i], 64, outlinks[i]*sizeof(double));
    int re1 = posix_memalign((void**) &(*W_index)[i], 64, inlinks[i]*sizeof(int));
    if (ret != 0 || re1!=0)
    {
      fprintf(stderr, "Could not allocate memory\n");
      exit(EXIT_FAILURE);
    }
  }

  rewind(fp);
  //nodecount = N;
  while (fgets(line, sizeof(line), fp)) {
    if (line[0] == '#') continue;
    sscanf(line, "%d%*[^0-9]%d\n", &number0, &number1);

    inter = test[number1];

    (*W_index)[number1][inter] = number0;
    test[number1]++;
  }

  for (int i = 0; i < N; i++)
  {
    if (test2[i] == 1 && dangling[i] == 1)
      dangling[i] = 0;
  }

  printf("\nLargest Vertex: %d\n", N);//nodecount);
  //N = nodecount;
}

VOID RandIndexCall(int *index, int start, int stop)
{
  *index = rand() % (stop - start) + start;
}


/* ------------------------------------------------------------------ */
/* Instrumentation Routines                                           */
/* ------------------------------------------------------------------ */

VOID FlagImg(IMG img, VOID* v) 
{
  RTN rtn;
#ifdef RUNTIME_KNOB
  rtn = RTN_FindByName(img, "testFunc");
  if (rtn != RTN_Invalid()) {
    RTN_ReplaceSignature(rtn, (AFUNPTR)testFunc,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
#endif

  rtn = RTN_FindByName(img, "roi_begin");
  if (rtn != RTN_Invalid()) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallROIBegin,
        IARG_ADDRINT, "ROI-Begin",
        IARG_END);
    RTN_Close(rtn);
  }
  rtn = RTN_FindByName(img, "roi_end");
  if (rtn != RTN_Invalid()) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallROIEnd,
        IARG_ADDRINT, "ROI-End",
        IARG_END);
    RTN_Close(rtn);
  }
  // Jiayi, 01/29/2018
  rtn = RTN_FindByName(img, "UpdateRR");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)UpdateRRAPI,
        IARG_CONTEXT,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "UpdateRI");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)UpdateRIAPI,
        IARG_CONTEXT,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "UpdateII");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)UpdateIIAPI,
        IARG_CONTEXT,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "Update");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)UpdateAPI,
        IARG_CONTEXT,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "Gather");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)GatherAPI,
        IARG_CONTEXT,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "active_begin");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallActiveBegin);
  }
  rtn = RTN_FindByName(img, "active_end");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallActiveEnd);
  }

  rtn = RTN_FindByName(img, "__kmp_get_global_thread_id");
  if (rtn != RTN_Invalid()) 
  {
    RTN_Replace(rtn, (AFUNPTR)CallPthreadSelf);
    //RTN_ReplaceWithUninstrumentedRoutine(rtn, (AFUNPTR)CallPthreadSelf);
  }
  rtn = RTN_FindByName(img, "__kmp_check_stack_overlap");
  if (rtn != RTN_Invalid()) 
  {
    RTN_Replace(rtn, (AFUNPTR)DummyFunc);
    //RTN_ReplaceWithUninstrumentedRoutine(rtn, (AFUNPTR)DummyFunc);
  }
  rtn = RTN_FindByName(img, "mcsim_skip_instrs_begin");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsBegin);
  }
  rtn = RTN_FindByName(img, "mcsim_skip_instrs_end");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsEnd);
  }
  rtn = RTN_FindByName(img, "mcsim_spinning_begin");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSpinningBegin);
  }
  rtn = RTN_FindByName(img, "mcsim_spinning_end");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSpinningEnd);
  }
  rtn = RTN_FindByName(img, "__parsec_bench_begin");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsBegin);
  }
  rtn = RTN_FindByName(img, "__parsec_roi_begin");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsEnd);
  }
  rtn = RTN_FindByName(img, "__parsec_roi_end");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsBegin);
  }
  rtn = RTN_FindByName(img, "__parsec_bench_end");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsBegin);
  }

  rtn = RTN_FindByName(img, "pthread_attr_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrDestroy,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_getdetachstate");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrGetdetachstate,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_getstackaddr");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrGetstackaddr,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_getstacksize");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrGetstacksize,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_getstack");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrGetstack,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_G_ARG2_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrInit,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_setdetachstate");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrSetdetachstate,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_setstackaddr");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrSetstackaddr,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_G_ARG2_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_setstacksize");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrSetstacksize,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_setstack");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrSetstack,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_G_ARG2_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cancel");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCancel,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cleanup_pop");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCleanupPop,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cleanup_push");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCleanupPush,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_condattr_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondattrDestroy,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_condattr_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondattrInit,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cond_broadcast");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondBroadcast,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cond_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondDestroy,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cond_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondInit,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cond_signal");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondSignal,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cond_timedwait");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondTimedwait,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_G_ARG2_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cond_wait");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondWait,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_create");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCreate,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_G_ARG2_CALLEE,
        IARG_G_ARG3_CALLEE,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_detach");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadDetach,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_equal");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadEqual,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_exit");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadExit,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_getattr");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadGetattr,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_getspecific");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadGetspecific,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_join");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadJoin,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,    
        IARG_G_ARG1_CALLEE,                       
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_key_create");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadKeyCreate,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE, 
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_key_delete");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadKeyDelete,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_kill");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadKill,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutexattr_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrDestroy,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutexattr_gettype");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrGetkind,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutexattr_getkind");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrGetkind,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutexattr_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrInit,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutexattr_settype");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrSetkind,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutexattr_setkind");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrSetkind,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutex_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexDestroy,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutex_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexInit,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutex_lock");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexLock,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutex_trylock");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexTrylock,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutex_unlock");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexUnlock,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_once");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadOnce,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_self");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSelf,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_setcancelstate");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSetcancelstate,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_setcanceltype");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSetcanceltype,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_setspecific");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSetspecific,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "libc_tsd_set");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSetspecific,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_testcancel");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadTestcancel,
        IARG_CONTEXT,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrier_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierInit,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_G_ARG2_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrier_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierDestroy,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrier_wait");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierWait,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrierattr_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierattrInit,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrierattr_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierattrDestory,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrierattr_getpshared");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierattrGetpshared,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrierattr_setpshared");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierattrSetpshared,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
}


VOID FlagRtn(RTN rtn, VOID* v) 
{
  RTN_Open(rtn);
  string * rtn_name = new string(RTN_Name(rtn));
#if VERYVERBOSE
  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)PrintRtnName,
      IARG_PTR, rtn_name,
      IARG_END);
#endif

  if (rtn_name->find("main") != string::npos)
  {
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)NewPthreadSim,
        IARG_CONTEXT,
        IARG_END);
  }
  else if (((rtn_name->find("__kmp") != string::npos) &&
        (rtn_name->find("yield") != string::npos)) ||
      (rtn_name->find("__sleep") != string::npos) ||
      (rtn_name->find("__kmp_wait_sleep") != string::npos))
  {
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)DoContextSwitch,
        IARG_CONTEXT,
        IARG_END);
  }
  /*else if ((rtn_name->find("__pthread_return_void") != string::npos) ||
    (rtn_name->find("pthread_mutex_t") != string::npos) ||
    (rtn_name->find("pthread_atfork") != string::npos))
  {
  }*/
  else if ((rtn_name->find("pthread") != string::npos) ||
      (rtn_name->find("sigwait") != string::npos) ||
      (rtn_name->find("tsd") != string::npos) ||
      ((rtn_name->find("fork") != string::npos) &&
       (rtn_name->find("__kmp") == string::npos)))
  {
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)WarnNYI,
        IARG_PTR, rtn_name,
        IARG_INST_PTR,
        IARG_END);
  }
  // Jiayi, 01/29/2018
  else if (rtn_name->find("pr_init_mem") != string::npos)
  {
    RTN_Replace(rtn, (AFUNPTR)PRInitMemCall);
  }
  else if (rtn_name->find("pr_graph_first_scan") != string::npos)
  {
    RTN_Replace(rtn, (AFUNPTR)PRGraphFirstScanCall);
  }
  else if (rtn_name->find("pr_graph_second_scan") != string::npos)
  {
    RTN_Replace(rtn, (AFUNPTR)PRGraphSecondScanCall);
  }
  else if (rtn_name->find("randindex") != string::npos)
  {
    RTN_Replace(rtn, (AFUNPTR)RandIndexCall);
  }
  RTN_Close(rtn);
}


VOID FlagTrace(TRACE trace, VOID* v) 
{
  bool unnecessary_art_call = false;
  if (TRACE_Address(trace) == (ADDRINT)pthread_exit) 
  {
    TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR)CallPthreadExit,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_END);
  }
  else if (!INS_IsAddedForFunctionReplacement(BBL_InsHead(TRACE_BblHead(trace)))) 
  {
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) 
    {
      for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) 
      {
        if (INS_IsCall(ins) && !INS_IsDirectBranchOrCall(ins))            // indirect call
        {
          INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessCall,
              IARG_BRANCH_TARGET_ADDR,
              IARG_G_ARG0_CALLER,
              IARG_G_ARG1_CALLER,
              IARG_BOOL, false,
              IARG_END);
        }
        else if (INS_IsDirectBranchOrCall(ins))                           // tail call or conventional call
        {
          ADDRINT target = INS_DirectBranchOrCallTargetAddress(ins);
          RTN src_rtn = INS_Rtn(ins);
          RTN dest_rtn = RTN_FindByAddress(target);

          if (INS_IsCall(ins) || (src_rtn != dest_rtn))
          {
            if (INS_IsCall(ins) && RTN_Valid(dest_rtn) &&
                (RTN_Name(dest_rtn) == "Update" ||
                 RTN_Name(dest_rtn) == "Gather" ||
                 RTN_Name(dest_rtn) == "UpdateRR" ||
                 RTN_Name(dest_rtn) == "UpdateRI" ||
                 RTN_Name(dest_rtn) == "UpdateII")) {
              unnecessary_art_call = true;
            } else {
              BOOL tailcall = !INS_IsCall(ins);
              INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessCall,
                  IARG_ADDRINT, target,
                  IARG_G_ARG0_CALLER,
                  IARG_G_ARG1_CALLER,
                  IARG_BOOL, tailcall,
                  IARG_END);
            }
          }
        }
        else if (INS_IsRet(ins))                                          // return
        {
          RTN rtn = INS_Rtn(ins);
          if (RTN_Valid(rtn) && (RTN_Name(rtn) != "_dl_runtime_resolve")) 
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessReturn,
                IARG_PTR, new string(RTN_Name(rtn)),
                IARG_END);
          }
        }

        if (unnecessary_art_call) {
          return;
        }

        if (((INS_Address(ins) - (ADDRINT)StartThreadFunc) < 0) ||
            ((INS_Address(ins) - (ADDRINT)StartThreadFunc) > 8 * sizeof(ADDRINT)))
        {
          bool is_mem_wr   = INS_IsMemoryWrite(ins);
          bool is_mem_rd   = INS_IsMemoryRead(ins);
          bool has_mem_rd2 = INS_HasMemoryRead2(ins);

          if (is_mem_wr && is_mem_rd && has_mem_rd2) 
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                IARG_CONTEXT,
                IARG_INST_PTR,
                IARG_MEMORYREAD_EA,
                IARG_MEMORYREAD2_EA,
                IARG_MEMORYREAD_SIZE,
                IARG_MEMORYWRITE_EA,
                IARG_MEMORYWRITE_SIZE,
                IARG_BOOL, INS_IsBranchOrCall(ins),
                IARG_BRANCH_TAKEN,
                IARG_UINT32,  INS_Category(ins),
                IARG_UINT32, INS_RegR(ins, 0),
                IARG_UINT32, INS_RegR(ins, 1),
                IARG_UINT32, INS_RegR(ins, 2),
                IARG_UINT32, INS_RegR(ins, 3),
                IARG_UINT32, INS_RegW(ins, 0),
                IARG_UINT32, INS_RegW(ins, 1),
                IARG_UINT32, INS_RegW(ins, 2),
                IARG_UINT32, INS_RegW(ins, 3),
                IARG_END);
          }
          else if (is_mem_wr && is_mem_rd && !has_mem_rd2) 
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                IARG_CONTEXT,
                IARG_INST_PTR,
                IARG_MEMORYREAD_EA,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_MEMORYREAD_SIZE,
                IARG_MEMORYWRITE_EA,
                IARG_MEMORYWRITE_SIZE,
                IARG_BOOL, INS_IsBranchOrCall(ins),
                IARG_BRANCH_TAKEN,
                IARG_UINT32,  INS_Category(ins),
                IARG_UINT32, INS_RegR(ins, 0),
                IARG_UINT32, INS_RegR(ins, 1),
                IARG_UINT32, INS_RegR(ins, 2),
                IARG_UINT32, INS_RegR(ins, 3),
                IARG_UINT32, INS_RegW(ins, 0),
                IARG_UINT32, INS_RegW(ins, 1),
                IARG_UINT32, INS_RegW(ins, 2),
                IARG_UINT32, INS_RegW(ins, 3),
                IARG_END);
          }
          else if (is_mem_wr && !is_mem_rd) 
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                IARG_CONTEXT,
                IARG_INST_PTR,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_UINT32, 0,
                IARG_MEMORYWRITE_EA,
                IARG_MEMORYWRITE_SIZE,
                IARG_BOOL, INS_IsBranchOrCall(ins),
                IARG_BRANCH_TAKEN,
                IARG_UINT32, INS_Category(ins),
                IARG_UINT32, INS_RegR(ins, 0),
                IARG_UINT32, INS_RegR(ins, 1),
                IARG_UINT32, INS_RegR(ins, 2),
                IARG_UINT32, INS_RegR(ins, 3),
                IARG_UINT32, INS_RegW(ins, 0),
                IARG_UINT32, INS_RegW(ins, 1),
                IARG_UINT32, INS_RegW(ins, 2),
                IARG_UINT32, INS_RegW(ins, 3),
                IARG_END);
          }
          else if (!is_mem_wr && is_mem_rd && has_mem_rd2)
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                IARG_CONTEXT,
                IARG_INST_PTR,
                IARG_MEMORYREAD_EA,
                IARG_MEMORYREAD2_EA,
                IARG_MEMORYREAD_SIZE,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_UINT32, 0,
                IARG_BOOL, INS_IsBranchOrCall(ins),
                IARG_BRANCH_TAKEN,
                IARG_UINT32, INS_Category(ins),
                IARG_UINT32, INS_RegR(ins, 0),
                IARG_UINT32, INS_RegR(ins, 1),
                IARG_UINT32, INS_RegR(ins, 2),
                IARG_UINT32, INS_RegR(ins, 3),
                IARG_UINT32, INS_RegW(ins, 0),
                IARG_UINT32, INS_RegW(ins, 1),
                IARG_UINT32, INS_RegW(ins, 2),
                IARG_UINT32, INS_RegW(ins, 3),
                IARG_END);
          }
          else if (!is_mem_wr && is_mem_rd && !has_mem_rd2) 
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                IARG_CONTEXT,
                IARG_INST_PTR,
                IARG_MEMORYREAD_EA,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_MEMORYREAD_SIZE,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_UINT32, 0,
                IARG_BOOL, INS_IsBranchOrCall(ins),
                IARG_BRANCH_TAKEN,
                IARG_UINT32, INS_Category(ins),
                IARG_UINT32, INS_RegR(ins, 0),
                IARG_UINT32, INS_RegR(ins, 1),
                IARG_UINT32, INS_RegR(ins, 2),
                IARG_UINT32, INS_RegR(ins, 3),
                IARG_UINT32, INS_RegW(ins, 0),
                IARG_UINT32, INS_RegW(ins, 1),
                IARG_UINT32, INS_RegW(ins, 2),
                IARG_UINT32, INS_RegW(ins, 3),
                IARG_END);
          }
          else
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                IARG_CONTEXT,
                IARG_INST_PTR,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_UINT32,  0,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_UINT32,  0,
                IARG_BOOL, INS_IsBranchOrCall(ins),
                IARG_BRANCH_TAKEN,
                IARG_UINT32, INS_Category(ins),
                IARG_UINT32, INS_RegR(ins, 0),
                IARG_UINT32, INS_RegR(ins, 1),
                IARG_UINT32, INS_RegR(ins, 2),
                IARG_UINT32, INS_RegR(ins, 3),
                IARG_UINT32, INS_RegW(ins, 0),
                IARG_UINT32, INS_RegW(ins, 1),
                IARG_UINT32, INS_RegW(ins, 2),
                IARG_UINT32, INS_RegW(ins, 3),
                IARG_END);
          }
        }
      }
    }
  }
}


/* ------------------------------------------------------------------ */
/* Pthread Hooks                                                      */
/* ------------------------------------------------------------------ */

int CallPthreadAttrDestroy(ADDRINT _attr) 
{
  return PthreadAttr::_pthread_attr_destroy((pthread_attr_t*)_attr);
}

int CallPthreadAttrGetdetachstate(ADDRINT _attr, ADDRINT _detachstate) 
{
  return PthreadAttr::_pthread_attr_getdetachstate((pthread_attr_t*)_attr, (int*)_detachstate);
}

int CallPthreadAttrGetstack(ADDRINT _attr, ADDRINT _stackaddr, ADDRINT _stacksize) 
{
  return PthreadAttr::_pthread_attr_getstack((pthread_attr_t*)_attr,
      (void**)_stackaddr, (size_t*)_stacksize);
}

int CallPthreadAttrGetstackaddr(ADDRINT _attr, ADDRINT _stackaddr) 
{
  return PthreadAttr::_pthread_attr_getstackaddr((pthread_attr_t*)_attr, (void**)_stackaddr);
}

int CallPthreadAttrGetstacksize(ADDRINT _attr, ADDRINT _stacksize) 
{
  return PthreadAttr::_pthread_attr_getstacksize((pthread_attr_t*)_attr, (size_t*)_stacksize);
}

int CallPthreadAttrInit(ADDRINT _attr) 
{
  return PthreadAttr::_pthread_attr_init((pthread_attr_t*)_attr);
}

int CallPthreadAttrSetdetachstate(ADDRINT _attr, ADDRINT _detachstate) 
{
  return PthreadAttr::_pthread_attr_setdetachstate((pthread_attr_t*)_attr, (int)_detachstate);
}

int CallPthreadAttrSetstack(ADDRINT _attr, ADDRINT _stackaddr, ADDRINT _stacksize) 
{
  return PthreadAttr::_pthread_attr_setstack((pthread_attr_t*)_attr,
      (void*)_stackaddr, (size_t)_stacksize);
}

int CallPthreadAttrSetstackaddr(ADDRINT _attr, ADDRINT _stackaddr) 
{
  return PthreadAttr::_pthread_attr_setstackaddr((pthread_attr_t*)_attr, (void*)_stackaddr);
}

int CallPthreadAttrSetstacksize(ADDRINT _attr, ADDRINT _stacksize) 
{
  return PthreadAttr::_pthread_attr_setstacksize((pthread_attr_t*)_attr, (size_t)_stacksize);
}

int CallPthreadCancel(ADDRINT _thread) 
{
  return pthreadsim->pthread_cancel((pthread_t)_thread);
}

VOID CallPthreadCleanupPop(CONTEXT* ctxt, ADDRINT _execute) 
{
  pthreadsim->pthread_cleanup_pop_((int)_execute, ctxt);
}

VOID CallPthreadCleanupPush(ADDRINT _routine, ADDRINT _arg) 
{
  pthreadsim->pthread_cleanup_push_(_routine, _arg);
}

int CallPthreadCondattrDestroy(ADDRINT _attr) 
{
  return 0;
}

int CallPthreadCondattrInit(ADDRINT _attr) 
{
  return 0;
}

int CallPthreadCondBroadcast(ADDRINT _cond) 
{
  return pthreadsim->pthread_cond_broadcast((pthread_cond_t*)_cond);
}

int CallPthreadCondDestroy(ADDRINT _cond) 
{
  return pthreadsim->pthread_cond_destroy((pthread_cond_t*)_cond);
}

int CallPthreadCondInit(ADDRINT _cond, ADDRINT _condattr) 
{
  return PthreadCond::pthread_cond_init((pthread_cond_t*)_cond, (pthread_condattr_t*)_condattr);
}

int CallPthreadCondSignal(ADDRINT _cond) 
{
  return pthreadsim->pthread_cond_signal((pthread_cond_t*)_cond);
}

VOID CallPthreadCondTimedwait(CONTEXT* context, ADDRINT _cond, ADDRINT _mutex, ADDRINT _abstime) 
{
  pthreadsim->pthread_cond_timedwait((pthread_cond_t*)_cond, (pthread_mutex_t*)_mutex,
      (const struct timespec*)_abstime, context);
}

VOID CallPthreadCondWait(CONTEXT* context, ADDRINT _cond, ADDRINT _mutex) 
{
  pthreadsim->pthread_cond_wait((pthread_cond_t*)_cond, (pthread_mutex_t*)_mutex, context);
}

VOID CallPthreadCreate(CONTEXT* ctxt,
    ADDRINT _thread, ADDRINT _attr, ADDRINT _func, ADDRINT _arg) 
{
  pthreadsim->pthread_create((pthread_t*)_thread, (pthread_attr_t*)_attr,
      ctxt, _func, _arg);
}

int CallPthreadDetach(ADDRINT _th) 
{
  return pthreadsim->pthread_detach((pthread_t)_th);
}

int CallPthreadEqual(ADDRINT _thread1, ADDRINT _thread2) 
{
  return pthreadsim->pthread_equal((pthread_t)_thread1, (pthread_t)_thread2);
}

VOID CallPthreadExit(CONTEXT* ctxt, ADDRINT _retval)  
{
  pthreadsim->pthread_exit((void*)_retval, ctxt);
}

int CallPthreadGetattr(ADDRINT _th, ADDRINT _attr) 
{
  return pthreadsim->pthread_getattr((pthread_t)_th, (pthread_attr_t*)_attr);
}

VOID* CallPthreadGetspecific(ADDRINT _key) 
{
  return pthreadsim->pthread_getspecific((pthread_key_t)_key);
}

VOID CallPthreadJoin(CONTEXT* ctxt,
    ADDRINT _th, ADDRINT _thread_return)
{
  pthreadsim->pthread_join((pthread_t)_th, (void**)_thread_return, ctxt);
}

int CallPthreadKeyCreate(ADDRINT _key, ADDRINT _func) 
{
  return pthreadsim->pthread_key_create((pthread_key_t*)_key, (void(*)(void*))_func);
}

int CallPthreadKeyDelete(ADDRINT _key) 
{
  return pthreadsim->pthread_key_delete((pthread_key_t)_key);
}

int CallPthreadKill(ADDRINT _thread, ADDRINT _signo) 
{
  return pthreadsim->pthread_kill((pthread_t)_thread, (int)_signo);
}

int CallPthreadMutexattrDestroy(ADDRINT _attr) 
{
  return PthreadMutexAttr::_pthread_mutexattr_destroy((pthread_mutexattr_t*) _attr);
}

int CallPthreadMutexattrGetkind(ADDRINT _attr, ADDRINT _kind) 
{
  return PthreadMutexAttr::_pthread_mutexattr_getkind((pthread_mutexattr_t*)_attr, (int*)_kind);
}

int CallPthreadMutexattrInit(ADDRINT _attr) 
{
  return PthreadMutexAttr::_pthread_mutexattr_init((pthread_mutexattr_t*)_attr);
}

int CallPthreadMutexattrSetkind(ADDRINT _attr, ADDRINT _kind) 
{
  return PthreadMutexAttr::_pthread_mutexattr_setkind((pthread_mutexattr_t*)_attr, (int)_kind);
}

int CallPthreadMutexDestroy(ADDRINT _mutex) 
{
  return PthreadMutex::_pthread_mutex_destroy((pthread_mutex_t*)_mutex);
}

int CallPthreadMutexInit(ADDRINT _mutex, ADDRINT _mutexattr) 
{
  return PthreadMutex::_pthread_mutex_init((pthread_mutex_t*)_mutex, (pthread_mutexattr_t*)_mutexattr);
}

VOID CallPthreadMutexLock(CONTEXT* context, ADDRINT _mutex) 
{
  pthreadsim->pthread_mutex_lock((pthread_mutex_t*)_mutex, context);
}

int CallPthreadMutexTrylock(ADDRINT _mutex) 
{
  return pthreadsim->pthread_mutex_trylock((pthread_mutex_t*)_mutex);
}

int CallPthreadMutexUnlock(ADDRINT _mutex) 
{
  return pthreadsim->pthread_mutex_unlock((pthread_mutex_t*)_mutex);
}

VOID CallPthreadOnce(CONTEXT* ctxt, ADDRINT _oncecontrol, ADDRINT _initroutine) 
{
  PthreadOnce::pthread_once((pthread_once_t*)_oncecontrol, _initroutine, ctxt);
}

pthread_t CallPthreadSelf() 
{
  return pthreadsim->pthread_self();
}

int CallPthreadSetcancelstate(ADDRINT _state, ADDRINT _oldstate) 
{
  return pthreadsim->pthread_setcancelstate((int)_state, (int*)_oldstate);
}

int CallPthreadSetcanceltype(ADDRINT _type, ADDRINT _oldtype) 
{
  return pthreadsim->pthread_setcanceltype((int)_type, (int*)_oldtype);
}

int CallPthreadSetspecific(ADDRINT _key, ADDRINT _pointer) 
{
  return pthreadsim->pthread_setspecific((pthread_key_t)_key, (VOID*)_pointer);
}

int CallPthreadBarrierInit(ADDRINT _barrier, ADDRINT _barrierattr, ADDRINT num) 
{
  return pthreadsim->pthread_barrier_init((pthread_barrier_t *)_barrier, (pthread_barrierattr_t *)_barrierattr, (unsigned int) num);
}

int CallPthreadBarrierDestroy(ADDRINT _barrier) 
{
  return pthreadsim->pthread_barrier_destroy((pthread_barrier_t *)_barrier);
}

int CallPthreadBarrierWait(CONTEXT* context, ADDRINT _barrier) 
{
  return pthreadsim->pthread_barrier_wait((pthread_barrier_t *)_barrier, context);
}

int CallPthreadBarrierattrInit(ADDRINT _barrierattr)
{
  return 0;  // not implemented yet
}

int CallPthreadBarrierattrDestory(ADDRINT _barrierattr)
{
  return 0;  // not implemented yet
}

int CallPthreadBarrierattrGetpshared(ADDRINT _barrierattr, ADDRINT value)
{
  return 0;  // not implemented yet
}

int CallPthreadBarrierattrSetpshared(ADDRINT _barrierattr, ADDRINT value)
{
  return 0;  // not implemented yet
}

VOID CallPthreadTestcancel(CONTEXT* ctxt) 
{
  pthreadsim->pthread_testcancel(ctxt);
}

VOID CallMcSimSkipInstrsBegin()
{
  pthreadsim->mcsim_skip_instrs_begin();
}

VOID CallMcSimSkipInstrsEnd()
{
  pthreadsim->mcsim_skip_instrs_end();
}

VOID CallMcSimSpinningBegin()
{
  pthreadsim->mcsim_spinning_begin();
}

VOID CallMcSimSpinningEnd()
{
  pthreadsim->mcsim_spinning_end();
}


/* ------------------------------------------------------------------ */
/* Thread-Safe Memory Allocation Support                              */
/* ------------------------------------------------------------------ */

VOID ProcessCall(ADDRINT target, ADDRINT arg0, ADDRINT arg1, BOOL tailcall) 
{
  PIN_LockClient();
  RTN rtn = RTN_FindByAddress(target);
  PIN_UnlockClient();
  if (RTN_Valid(rtn)) 
  {
    string temp_string(RTN_Name(rtn));
    pthreadsim->threadsafemalloc(true, tailcall, &temp_string);
  }
}

VOID ProcessReturn(const string* rtn_name) 
{
  ASSERTX(rtn_name != NULL);
  pthreadsim->threadsafemalloc(false, false, rtn_name);
}

/* ------------------------------------------------------------------ */
/* Thread Scheduler                                                   */
/* ------------------------------------------------------------------ */

VOID DoContextSwitch(CONTEXT* context) 
{
  pthreadsim->docontextswitch(context);
}

/* ------------------------------------------------------------------ */
/* Debugging Support                                                  */
/* ------------------------------------------------------------------ */

VOID WarnNYI(const string* rtn_name,
    ADDRINT ip) 
{
  std::cout << "NYI: " << *rtn_name << " at: 0x" << hex << ip << dec <<  "\n" << flush;
  //ASSERTX(0);
}

VOID PrintRtnName(const string* rtn_name) 
{
  std::cout << "RTN " << *rtn_name << "\n" << flush;
}


/* ------------------------------------------------------------------ */
/* Skip instruction hooks                                             */
/* ------------------------------------------------------------------ */

VOID CallROIBegin(CHAR *name)
{
  fprintf(stderr, "I am in ::::::::::::::::::::::::::::::::::: %s\n",name);
  in_roi = true;
}

VOID CallROIEnd(CHAR *name)
{
  fprintf(stderr, "I am in ::::::::::::::::::::::::::::::::::: %s\n",name);
  in_roi = false;
}

// Jiayi, 01/29/2018
VOID CallActiveBegin()
{
  fprintf(stderr, "[MCSIM-HOOKS] Active_begin\n");
}

VOID CallActiveEnd()
{
  fprintf(stderr, "[MCSIM-HOOKS] Active_end\n");
}

} // namespace PinPthread

