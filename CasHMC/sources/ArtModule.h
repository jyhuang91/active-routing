#pragma once

namespace CasHMC
{
  typedef uint64_t FlowID;

  enum Opcode {
    ADD,
    MAC,
    INVALID
  };

  struct FlowEntry {
    Opcode   opcode;                    // function code: ADD, MAC, etc.
    double   result;                    // partial result
    uint64_t req_count;                 // number of requests
    uint64_t rep_count;                 // number of responses
    int      parent;                    // parent cubeID of ARTree
    int      children_count[NUM_LINKS]; // number of updates sent to children
    bool     children_gflag[NUM_LINKS];
    bool     g_flag;                    // flag indicating gather command received or not
    int      vault_count[NUM_VAULTS];   // VLP: how many requests were sent to each vault
    bool     vault_gflag[NUM_VAULTS];   // VLP: how many get requests were sent to each vault

    FlowEntry() : opcode(INVALID), result(0), req_count(0), rep_count(0), parent(-1), g_flag(false) {
      for (int i = 0; i < NUM_LINKS; i++) {
        children_count[i] = 0;
        children_gflag[i] = false;
      }
      // for VLP
      for (int i = 0; i < NUM_VAULTS; i++) {
        vault_count[i] = 0;
        vault_gflag[i] = false;
      }
    }
    FlowEntry(Opcode op) : opcode(op), result(0), req_count(0), rep_count(0), parent(-1), g_flag(false) {
      for (int i = 0; i < NUM_LINKS; i++) {
        children_count[i] = 0;
        children_gflag[i] = false;
      }
      // for VLP
      for (int i = 0; i < NUM_VAULTS; i++) {
        vault_count[i] = 0;
        vault_gflag[i] = false;
      }
    }
  };

  struct OperandEntry {
    FlowID   flowID;
    uint64_t src_addr1;
    uint64_t src_addr2;
    bool     op1_ready;
    bool     op2_ready;
    bool     ready;
    char     multStageCounter;
    bool     counted;
    unsigned vault;             // VLP: for each vault, an operand gives a partial result from that vault

    OperandEntry() : flowID(0), src_addr1(0), op1_ready(false), src_addr2(0), op2_ready(false), multStageCounter(5), ready(false), counted(false), vault(-1) {}
    OperandEntry(char initMultStage) : flowID(0), src_addr1(0), op1_ready(false), src_addr2(0), op2_ready(false), multStageCounter(initMultStage), ready(false), counted(false), vault(-1) {}
  };

  struct VaultFlowEntry {
    Opcode   opcode;                    // function code: ADD, MAC, etc.
    double   result;                    // partial result
    uint64_t req_count;                 // number of requests
    uint64_t rep_count;                 // number of responses
    int      parent;                    // parent cubeID of ARTree (all have ARE as parent for now)
    bool     g_flag;                    // flag indicating gather command received or not

    VaultFlowEntry() : opcode(INVALID), result(0), req_count(0), rep_count(0), g_flag(false) {}
    VaultFlowEntry(Opcode op) : opcode(op), result(0), req_count(0), rep_count(0), parent(-1), g_flag(false) {}
  };

  struct VaultOperandEntry {
    FlowID   flowID;
    uint64_t src_addr1;
    uint64_t src_addr2;
    bool     op1_ready;
    bool     op2_ready;
    bool     ready;
    char     multStageCounter;
    bool     counted;

    VaultOperandEntry() : flowID(0), src_addr1(0), op1_ready(false), src_addr2(0), op2_ready(false), multStageCounter(5), ready(false), counted(false) {}
    VaultOperandEntry(char initMultStage) : flowID(0), src_addr1(0), op1_ready(false), src_addr2(0), op2_ready(false), multStageCounter(initMultStage), ready(false), counted(false) {}
  };

}
