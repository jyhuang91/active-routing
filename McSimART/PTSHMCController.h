#include "McSim.h"
#include <list>
#include <stack>
#include <queue>
#include <set>

#include "Network.h"
#include "Transaction.h"

using namespace CasHMC;

namespace PinPthread
{
  class PTSHMCController : public Component
  {
    public:
      Network *hmc_net;
      TransactionType tranType;

      PTSHMCController(component_type type_, uint32_t num_, McSim * mcsim_, Network * hmc_net_);
      ~PTSHMCController();

      void add_req_event(uint64_t, LocalQueueElement *, Component * from = NULL);//we have to see what to pass
      void add_rep_event(uint64_t, LocalQueueElement *, Component * from = NULL);
      uint32_t process_event(uint64_t curr_time);
      void update_hmc(uint64_t cycles);
      bool pre_processing(uint64_t curr_time);
      int hmc_transaction_type(LocalQueueElement *);
      int get_src_cubeID(int num); 
      void display();

      //elements
      Component * directory;  // uplink
      NoC * noc;
      vector<LocalQueueElement *> resp_queue;
      std::multimap<uint64_t, LocalQueueElement *> active_update_event;  // Jiayi, <tran_id, lqe>, 03/28/17 
      std::multimap<uint64_t, LocalQueueElement *> active_gather_event;  // Jiayi, <tran_id, lqe>, 03/28/17
      std::multimap<uint64_t, LocalQueueElement *> pending_active_updates;
      std::map<LocalQueueElement *, LocalQueueElement *> active_mult_twins;
      uint64_t last_transaction;
      TOPOLOGY hmc_top;
      int net_num;
      uint64_t mc_to_dir_t;
      uint64_t hmc_to_noc_t;
      std::multimap<uint64_t, uint64_t> outstanding_req;//this is to track the transactions sent to HMC: <addr, curr_cycle>
      std::multimap<uint64_t, Transaction *> tran_buf; // transaction buffer, hold transactions temporally
      void update_acc_dist();
      map<uint64_t, uint64_t> os_page_acc_dist;       // os page access distribution
      map<uint64_t, uint64_t> os_page_acc_dist_curr;  // os page access distribution
      bool display_os_page_usage_summary;
      bool display_os_page_usage;
      uint64_t num_reqs;
      uint64_t num_update_reqs;
      uint64_t num_gather_reqs;
      uint64_t page_sz_base_bit;

      uint32_t interleave_xor_base_bit; // Jiayi, for addressing, 04/16/17
      uint32_t cube_interleave_base_bit;

      static uint64_t num_updates;
      static double noc_latency;
      static uint64_t num_updates_sent;
      static double stall_ticks;
      static uint64_t last_process_time;

      inline uint32_t get_cube_num(uint64_t addr) {  // Jiayi, addressing
        return (num * 4) + ((addr >> cube_interleave_base_bit) ^ (addr >> interleave_xor_base_bit)) % 4;
      }

      inline uint32_t get_active_cube_num(uint64_t addr) {
        uint32_t base_num = geq->which_mc(addr);
        return (base_num * 4) + ((addr >> cube_interleave_base_bit) ^ (addr >> interleave_xor_base_bit)) % 4;
      }

  };
}
