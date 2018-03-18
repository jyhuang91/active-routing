#include"PTSHMCController.h"
#include "PTSCache.h" 
#include "PTSRBoL.h" 
#include "PTSXbar.h"
#include <assert.h>
#include <iomanip>
#include <cmath>

using namespace std;
using namespace PinPthread;

extern ostream& operator<<(ostream & output, component_type ct);

uint64_t  PTSHMCController::last_process_time = 0;


PTSHMCController::PTSHMCController(component_type type_, uint32_t num_,
    McSim * mcsim_, Network *hmc_net_) : Component(type_, num_, mcsim_), noc()
{
  last_process_time = 0;
  page_sz_base_bit = get_param_uint64("page_sz_base_bit", 12);
  display_os_page_usage = get_param_str("display_os_page_usage") == "true" ? true : false;
  process_interval = get_param_uint64("process_interval", 10);
  mc_to_dir_t = get_param_uint64("to_dir_t", 1000);
  hmc_to_noc_t = get_param_uint64("to_noc_t", 50);
  interleave_xor_base_bit = get_param_uint64("interleave_xor_base_bit", 20);
  cube_interleave_base_bit = get_param_uint64("cube_interleave_base_bit", 32);

  hmc_net = hmc_net_;

  if (mcsim->hmc_topology == DFLY)
  {
    net_num = num;
    hmc_top = DFLY;
  }
  else if (mcsim->hmc_topology == MESH)
  {
    hmc_top = MESH;
    if (num == 1 || num == 2)
    {
      net_num = 7 + num*num;
    }
    else
    {
      net_num = num;
    }
  }
  else
  {
    assert("Unsupported configuaration" == 0);
  }

  num_reqs        = 0;

  num_read        = 0;
  num_write       = 0;
  num_evict       = 0;
  num_update      = 0;
  num_gather      = 0;
  num_update_sent = 0;

  total_rd_stall_time     = 0.0;
  total_wr_stall_time     = 0.0;
  total_ev_stall_time     = 0.0;
  total_rd_mem_time       = 0.0;
  total_wr_mem_time       = 0.0;
  total_ev_mem_time       = 0.0;
  total_update_req_time   = 0.0;
  total_update_stall_time = 0.0;

  outstanding_req.clear();
  active_update_event.clear();
  active_gather_event.clear();
  pending_active_updates.clear();
  active_mult_twins.clear();
}

PTSHMCController::~PTSHMCController()
{
  cout << "  -- HMCCtrl [" << num << "] : (rd, wr, evict, update, gather) = ("
    << setw(8) << num_read << ", " << setw(8) << num_write << ", " << setw(8) << num_evict << ", "
    << setw(8) << num_update << ", " << setw(8) << num_gather << "), ";
  cout << "(avg_rd_stall, avg_wr_stall, avg_ev_stall, avg_rd_mem, avg_wr_mem, avg_ev_mem [cycles]) = ("
    << setw(8) << total_rd_stall_time / num_read / process_interval << ", "
    << setw(8) << total_wr_stall_time / num_write / process_interval << ", "
    << setw(8) << total_ev_stall_time / num_evict / process_interval << ", "
    << setw(8) << total_rd_mem_time / num_read / process_interval << ", "
    << setw(8) << total_wr_mem_time / num_write / process_interval << ", "
    << setw(8) << total_ev_mem_time / num_evict / process_interval << "), ";
  cout << "(avg_update_req, avg_update_stall [cycles]) = ("
    << setw(8) << total_update_req_time / num_update / process_interval << ", "
    << setw(8) << total_update_stall_time / num_update_sent / process_interval << ")" << endl;
  display();
  if (!pending_active_updates.empty())
  {
    multimap<uint64_t, LocalQueueElement *>::iterator iter = pending_active_updates.begin();
    cout << "remaining pending updates:" << endl;
    for (; iter != pending_active_updates.end(); iter++)
    {
      cout << "flow " << hex << iter->first << dec << endl;
    }
  }

  assert(tran_buf.empty());
  tran_buf.clear();
  active_update_event.clear();
  active_gather_event.clear();
}

void PTSHMCController::add_req_event(uint64_t event_time, LocalQueueElement * lqele, Component * from)
{
  
  if (event_time % process_interval != 0)
  {
    event_time += process_interval - event_time % process_interval;
  }

  if (outstanding_req.empty() && tran_buf.empty() && active_update_event.empty() &&
      req_event.empty() && active_gather_event.empty() && resp_queue.empty())
      //active_gather_event.empty() && resp_queue.empty() && pending_active_updates.empty())
  {
    geq->add_event(event_time, this);
  }

  if (lqele->type == et_hmc_update_mult)
  {
    if (lqele->twin_lqe1 == NULL &&
        active_mult_twins.find(lqele->twin_lqe2) == active_mult_twins.end())
    {
      active_mult_twins.insert(make_pair(lqele, lqele->twin_lqe2));
      return;
    }
    else if (lqele->twin_lqe2 == NULL &&
        active_mult_twins.find(lqele->twin_lqe1) == active_mult_twins.end())
    {
      active_mult_twins.insert(make_pair(lqele, lqele->twin_lqe1));
      return;
    }
    else
    {
      LocalQueueElement *twin_lqe = (lqele->twin_lqe1 == NULL) ?
        lqele->twin_lqe2 : lqele->twin_lqe1;
      assert(active_mult_twins[twin_lqe] == lqele);
      active_mult_twins.erase(active_mult_twins.find(twin_lqe));
      delete twin_lqe;
    }
  }

  int transaction_type = hmc_transaction_type(lqele);
  int data_size = 32;
  uint32_t dest_cube = get_cube_num(lqele->address);

  switch (transaction_type)
  {
    case 1: tranType = DATA_READ;   data_size = 32;/* size can vary from 32 to 256 */  break;
    case 2: tranType = DATA_WRITE;  data_size = 128; break;
    case 3: tranType = DATA_WRITE;  data_size = 32;  break;
    case 4: tranType = ACTIVE_GET;  data_size = 32;  break;
    case 5: tranType = ACTIVE_ADD;  data_size = 32;  break;
    case 6: tranType = ACTIVE_MULT; data_size = 32;  break;
    default:
      cerr << "Error: Unknown transaction type" << endl;
      assert(0);
  }

  int src_cube = get_src_cubeID(num);
  Transaction *newTran = NULL;
  switch (tranType)
  {
    case DATA_READ:
    case DATA_WRITE:
      newTran = new Transaction(tranType, lqele->address, data_size, hmc_net, src_cube, dest_cube);
      break;

    case ACTIVE_ADD:
      dest_cube = get_active_cube_num(lqele->src_addr1);
      newTran = new Transaction(ACTIVE_ADD, lqele->dest_addr, lqele->src_addr1, data_size, hmc_net, 0, dest_cube);
      assert(lqele->nthreads == -1);
      break;
    case ACTIVE_GET:
      newTran = new Transaction(ACTIVE_GET, lqele->dest_addr, lqele->src_addr1, data_size, hmc_net, 0, 0);
      assert(lqele->nthreads >= 1);
      //cout << "Gather's nthreads: " << lqele->nthreads << endl;
      newTran->nthreads = lqele->nthreads;
      break;
    case ACTIVE_MULT:
      uint32_t dest_cube1 = get_cube_num(lqele->src_addr1);
      uint32_t dest_cube2 = get_cube_num(lqele->src_addr2);
      newTran = new Transaction(ACTIVE_MULT, lqele->dest_addr, lqele->src_addr1, lqele->src_addr2, data_size,
          hmc_net, 0, dest_cube1, dest_cube2);
      assert(lqele->nthreads == -1);
      break;

  }
 
  uint64_t req_id = hmc_net->get_tran_tag(newTran);
  tran_buf.insert(make_pair(event_time, newTran));

  if (lqele->type == et_hmc_update_add || lqele->type == et_hmc_update_mult)
  {
    active_update_event.insert(make_pair(req_id, lqele));
    num_update++;
    total_update_req_time += geq->curr_time - lqele->issue_time;
  }
  else if (lqele->type == et_hmc_gather)
  {
    //active_gather_event.insert(make_pair(req_id, lqele));  // not neccessary though
    active_gather_event.insert(make_pair(lqele->dest_addr, lqele));
    num_gather++;
  }
  else
  {
    req_event.insert(pair<uint64_t, LocalQueueElement *>(req_id, lqele));
    num_reqs++;
  }

  uint64_t page_num = (lqele->address >> page_sz_base_bit);
  map<uint64_t, uint64_t>::iterator p_iter = os_page_acc_dist_curr.find(page_num);

  if (p_iter != os_page_acc_dist_curr.end())
  {
    (p_iter->second)++;
  }
  else
  {
    os_page_acc_dist_curr.insert(pair<uint64_t, uint64_t>(page_num, 1));
  }
}


void PTSHMCController::add_rep_event(uint64_t event_time, LocalQueueElement * lqele, Component * from)
{

  if (outstanding_req.empty() && tran_buf.empty() && active_update_event.empty() &&
      req_event.empty() && active_gather_event.empty() && resp_queue.empty())
      //active_gather_event.empty() && resp_queue.empty() && pending_active_updates.empty())
  {
    geq->add_event(event_time, this);
  }

  rep_event.insert(pair<uint64_t, LocalQueueElement *>(event_time, lqele));
}


uint32_t PTSHMCController::process_event(uint64_t curr_time)
{

  assert(curr_time % process_interval == 0);

  if (tran_buf.empty() == false && tran_buf.begin()->first <= curr_time)
  {
    Transaction *tran = tran_buf.begin()->second;
    if (hmc_net->ReceiveTran(tran, net_num))
    {
      uint64_t addr = hmc_net->get_tran_addr(tran);
      uint64_t req_id = hmc_net->get_tran_tag(tran);
      if (tran->transactionType == ACTIVE_ADD || tran->transactionType == ACTIVE_MULT)
      {
        if (active_update_event.find(req_id) != active_update_event.end())
        {
          multimap<uint64_t, LocalQueueElement *>::iterator it = active_update_event.find(req_id);
          //assert(it == active_update_event.begin());
          LocalQueueElement *lqele = (it->second);
          assert(lqele);
          assert(lqele->from.top());
          num_update_sent++;
          total_update_stall_time += curr_time - tran_buf.begin()->first;
          noc->add_rep_event(curr_time + hmc_to_noc_t, lqele, this);
          pending_active_updates.insert(make_pair(addr, lqele));
          active_update_event.erase(req_id);
        }
        else
        {
          cerr << "ERROR: " << (tran->transactionType == ACTIVE_ADD ? "ACTIVE_ADD" : "ACTIVE_MULT")
            << " req_id (" << req_id << ") lqele not found in active_update_event" << endl;
          assert(0);
        }
      }
      else if (tran->transactionType == ACTIVE_GET)
      {
#ifdef DEBUG_GATHER
        cout << "send Gather for flow " << hex << addr << dec << endl;
#endif
        //display();
        // do nothing
      }
      else // normal memory requests
      {
        if (tran->transactionType == DATA_READ)
        {
          num_read++;
          total_rd_stall_time += curr_time - tran_buf.begin()->first;
        }
        else
        {
          assert(tran->transactionType == DATA_WRITE);
          LocalQueueElement *lqele = req_event.find(req_id)->second;
          if (lqele->type == et_write || lqele->type == et_write_nd ||
              lqele->type == et_s_rd_wr)
          {
            num_write++;
            total_wr_stall_time += curr_time - tran_buf.begin()->first;
          }
          else
          {
            assert(lqele->type == et_evict || lqele->type == et_evict_nd || lqele->type == et_dir_evict);
            num_evict++;
            total_ev_stall_time += curr_time - tran_buf.begin()->first;
          }
        }
        outstanding_req.insert(make_pair(req_id, curr_time));
      }
      tran_buf.erase(tran_buf.begin());
    }
  }

  vector<pair<uint64_t, PacketCommandType> > &served_trans = hmc_net->get_serv_trans(net_num);
  bool have_trans = !served_trans.empty();
  
  if (have_trans)
  {
    bool is_active = false;
    if (served_trans[0].second == ACT_GET)
    {
      is_active = true;
      uint64_t get_addr = served_trans[0].first;
      multimap<uint64_t, LocalQueueElement *>::iterator it = pending_active_updates.find(get_addr);
      assert(it != pending_active_updates.end());
      uint64_t dest_addr = it->first;
      assert(dest_addr == get_addr);
#ifdef DEBUG_GATHER
      cout << "Get active response (should be gather) dest addr: " << hex << dest_addr << dec << ", type: ACT_GET" << endl;
#endif

      multimap<uint64_t, LocalQueueElement *>::iterator get_it = active_gather_event.begin();
      while (get_it != active_gather_event.end())
      {
        if (get_it->first == dest_addr)
        {
          //get_it->second->display();
          resp_queue.push_back(get_it->second);
        }
        ++get_it;
      }
      pending_active_updates.erase(dest_addr);
      active_gather_event.erase(dest_addr);
    }

    multimap<uint64_t,uint64_t>::iterator tran_it = outstanding_req.begin();

#ifdef VERIFY
    int same_trans = 0;
    while (tran_it != outstanding_req.end() && !is_active)
    {
      if (tran_it->first == served_trans[0].first) ++same_trans;
      ++tran_it;
    }
    if (same_trans > 1)
    {
      cerr << same_trans << " transactions have the same key " << tran_it->first << endl;
      assert(0);
    }
#endif

    tran_it = outstanding_req.find(served_trans[0].first);
    if (!is_active)
    {
      assert(tran_it != outstanding_req.end());
      assert(tran_it->second != et_hmc_update_add &&
          tran_it->second != et_hmc_update_mult &&
          tran_it->second != et_hmc_gather);
      uint64_t req_id = tran_it->first;
      outstanding_req.erase(tran_it);
      served_trans[0].first = -1;

      multimap<uint64_t, LocalQueueElement *>::iterator req_event_iter = req_event.find(req_id);
      assert(req_event_iter != req_event.end());
      resp_queue.push_back(req_event_iter->second);
      req_event.erase(req_id);

      int tran_type = hmc_transaction_type(req_event_iter->second);
      int issue_time = tran_it->second;
      switch (tran_type)
      {
        case 1: // read
          total_rd_mem_time += curr_time - issue_time;
          break;
        case 2: // write
          total_wr_mem_time += curr_time - issue_time;
          break;
        case 3: // evict
          total_ev_mem_time += curr_time - issue_time;
          break;
        default:
          cerr << "Unknown reply transaction type: " << tran_type  << endl;
          exit(1);
      }
    }

#ifdef DEBUG_GATEHR
    if (served_trans[0].second == ACT_GET) {
      cout << "outstanding requests: " << outstanding_req.size() << endl;
      cout << "Get the gather response at cycle " << geq->curr_time << endl;
    }
#endif
    
    served_trans.erase(served_trans.begin());
  }

  if (!req_event.empty() || !outstanding_req.empty() ||
      !tran_buf.empty() || !active_update_event.empty() ||
      //!tran_buf.empty() || !active_update_event.empty() || !pending_active_updates.empty() ||
      !active_gather_event.empty() || !resp_queue.empty())
  {
    geq->add_event(curr_time+process_interval, this);
  }
  else
  {
    assert(req_event.empty() && resp_queue.empty());
  }

  if (last_process_time < curr_time)
  {
#ifdef CASHMC_FASTSYNC
    uint64_t c = (curr_time - last_process_time) / process_interval;
    update_hmc(c);
#else
    for (uint64_t c = last_process_time+process_interval; c <= curr_time; c = c + process_interval)
    {
      update_hmc(c);
    }
#endif
  }

  vector<LocalQueueElement *>::iterator iter;
  for (iter = resp_queue.begin(); iter != resp_queue.end(); ++iter)
  {
    if ((*iter) != NULL)
    {
      switch ((*iter)->type)
      {
        case et_rd_dir_info_req:
        case et_rd_dir_info_rep:
        case et_read:
        case et_e_rd:
        case et_s_rd:
          directory->add_rep_event(curr_time+mc_to_dir_t, *iter,this);
          resp_queue.erase(iter);
          break;
        case et_evict:
        case et_dir_evict:
        case et_s_rd_wr:
          if ((*iter)->type == et_s_rd_wr)
          {
            (*iter)->type = et_s_rd;
            directory->add_rep_event(curr_time+mc_to_dir_t, *iter,this);
          }
          else
          {
            delete *iter;
          }
          resp_queue.erase(iter);
          break;
        case et_hmc_gather:
          noc->add_rep_event(curr_time + hmc_to_noc_t, *iter, this);
          resp_queue.erase(iter);
          break;
        default:
          cerr << "what is this" << endl;
          assert(0);
      }
    }
    break; //as of now process only one in one cycle 
  }

  last_process_time = curr_time; //last response time 
  return 0;
}


void PTSHMCController::update_hmc(uint64_t cycles)
{
#ifdef CASHMC_FASTSYNC
  if (cycles > 1) hmc_net->MultiStep(cycles - 1);
#endif
  hmc_net->Update();
}

int PTSHMCController::get_src_cubeID(int num)
{
  int src_cube = num * 5;
  if (hmc_top == MESH) //Works only for 4x4;
  {
    switch (num)
    {
      case 0: src_cube = 0; break;
      case 1: src_cube = 3; break;
      case 2: src_cube = 15; break;
      case 3: src_cube = 12; break;
      defualt:
        cerr << "ERROR: HMC Configuaration unclear" << endl;
        assert(0);
    }
  }

  return src_cube;
}

int PTSHMCController::hmc_transaction_type(LocalQueueElement * lqe){

  switch(lqe->type)
  {

    case et_read:
    case et_dir_rd:
    case et_dir_rd_nd:
    case et_e_rd:
    case et_s_rd:
    case et_tlb_rd:
    case et_rd_dir_info_req:
    case et_rd_dir_info_rep:
      return 1;//type 1 is READ

    case et_write:
    case et_write_nd:
    case et_s_rd_wr:
      return 2;//type 2 is WRITE

    case et_evict:
    case et_evict_nd:
    case et_dir_evict:
      return 3;//type 3 is EVICT

    case et_hmc_gather:
      return 4; // Jiayi, type 4 is ACTIVE_GET

    case et_hmc_update_add:
      return 5; // Jiayi, type 5 is ACTIVE_ADD

    case et_hmc_update_mult:
      return 6; // Jiayi, type 6 is ACTIVE_MULT

    case et_rd_bypass:   // this read is older than what is in the cache
    case et_e_to_m:
    case et_e_to_i:
    case et_e_to_s:
    case et_e_to_s_nd:
    case et_m_to_s:
    case et_m_to_m:
    case et_m_to_i:
    case et_s_to_s:
    case et_s_to_s_nd:
    case et_m_to_e:
    case et_nack:
    case et_invalidate:  // originated from a directory
    case et_invalidate_nd:
    case et_i_to_e:
    case et_nop:
    case et_hmc_gather_rep: // Jiayi added, 03/25/17
    default:
      cerr << lqe->type << " should not be sent to HMC network" << endl;
      assert(0);
  }
}


void PTSHMCController::update_acc_dist()
{
  map<uint64_t, uint64_t>::iterator p_iter, c_iter;
  for (c_iter = os_page_acc_dist_curr.begin(); c_iter != os_page_acc_dist_curr.end(); ++c_iter)
  {
    p_iter = os_page_acc_dist.find(c_iter->first);
    if (p_iter == os_page_acc_dist.end())
    {
      os_page_acc_dist.insert(pair<uint64_t, uint64_t>(c_iter->first, 1));
    }
    else
    {
      p_iter->second += c_iter->second;
    }
  }
  os_page_acc_dist_curr.clear();
}


void PTSHMCController::display()
{
  cout << "(" << (component_type) type << ", " << num << ")";
  cout << " [req_event: " << req_event.size()
    << "] [outstanding_req: " << outstanding_req.size()
    << "] [tran_buf: " << tran_buf.size()
    << "] [active_update_event: " << active_update_event.size()
    << "] [active_gather_event: " << active_gather_event.size()
    << "] [pending_active_updates: " << pending_active_updates.size()
    << "] [resp_queue: " << resp_queue.size() << "]" << endl;
  multimap<uint64_t, uint64_t>::iterator iter = outstanding_req.begin();
  //while (iter != outstanding_req.end()) {
  //  assert(req_event.find(iter->first) != req_event.end());
  //  req_event.find(iter->first)->second->display();
  //  iter++;
  //}
}

