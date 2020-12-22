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
map<uint64_t, pair<pair<int, int>, vector<int> > > PTSHMCController::gather_barrier;
map<uint64_t, vector<bool> > PTSHMCController::active_forests;


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
  num_mcs_log2 = log2(mcsim->pts->get_param_uint64("pts.num_mcs", 2));
  kernel_offloading = get_param_bool("kernel_offloading", "pts.", false);

  uint32_t num_mcs = get_param_uint64("num_mcs", "pts.", 4);
  uint32_t net_dim = get_param_uint64("net_dim", "pts.", 4);
  cubes_per_mc = net_dim * net_dim / num_mcs;

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
  /*if (!pending_active_updates.empty())
  {
    multimap<uint64_t, LocalQueueElement *>::iterator iter = pending_active_updates.begin();
    cout << "remaining pending updates:" << endl;
    for (; iter != pending_active_updates.end(); iter++)
    {
      switch (mcsim->art_scheme)
      {
        case art_naive:
          cout << "flow " << hex << iter->first << dec << endl;
          break;
        case art_tid:
        case art_addr:
          cout << "subflow (flow_id): " << hex << iter->first << ", flow (dest_addr): " << (iter->first >> num_mcs_log2) << dec << endl;
          break;
        default:
          assert(0);
    }
  }*/

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

  if (lqele->type == et_art_mult)
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
    case MEM_READ:  tranType = DATA_READ;   data_size = 64;/* size can vary from 32 to 256 */  break;
    case MEM_WRITE: tranType = DATA_WRITE;  data_size = 64; break;
    case MEM_EVICT: tranType = DATA_WRITE;  data_size = 64;  break;
    case ART_GET:   tranType = ACTIVE_GET;  data_size = 8;  break;
    case ART_ADD:   tranType = ACTIVE_ADD;  data_size = lqele->rlen;  break;
    case ART_MULT:  tranType = ACTIVE_MULT; data_size = lqele->rlen;  break;
    case ART_DOT:   tranType = ACTIVE_DOT;  data_size = lqele->rlen;  break;
    case PEI_DOT:   tranType = PIM_DOT;     data_size = lqele->rlen;  break;
    case PEI_ATOMIC:tranType = PIM_ATOMIC;  data_size = lqele->rlen;  break;
    default:
      cerr << "Error: Unknown transaction type" << endl;
      assert(0);
  }

  int src_cube = get_src_cubeID(num);
  Transaction *newTran = NULL;
  uint64_t flow_id;
  switch (mcsim->art_scheme)
  {
    case art_naive:
      flow_id = lqele->dest_addr;
      break;
    case art_tid:
    case art_addr:
      flow_id = (lqele->dest_addr << num_mcs_log2) | num;
      break;
    default:
      assert(0);
  }
  int nthreads = 0;
  map<uint64_t, pair<pair<int, int>, vector<int> > >::iterator it = gather_barrier.find(lqele->dest_addr);

  switch (tranType)
  {
    case DATA_READ:
    case DATA_WRITE:
    case PIM_DOT:
      newTran = new Transaction(tranType, lqele->address, data_size, hmc_net, src_cube, dest_cube);
      break;
    case PIM_ATOMIC:
      dest_cube = get_cube_num(lqele->dest_addr);
      newTran = new Transaction(tranType, lqele->dest_addr, data_size, hmc_net, src_cube, dest_cube);
      break;

    case ACTIVE_GET:
      active_gather_event.insert(make_pair(lqele->dest_addr, lqele));
      num_gather++;
      nthreads = lqele->nthreads;
      if (it != gather_barrier.end())
      {
        assert(nthreads == it->second.first.first && it->second.first.second < nthreads);
        gather_barrier[lqele->dest_addr].first.second++;
        gather_barrier[lqele->dest_addr].second[num]++;
#ifdef DEBUG_GATHER
        cout << "Receive Gather for flow " << hex << lqele->dest_addr << dec << " at hmc controller " << num
          << ", total gathers at this port: " << (gather_barrier[lqele->dest_addr].second)[num] << endl;
#endif
      }
      else
      {
        gather_barrier.insert(make_pair(lqele->dest_addr, make_pair(make_pair(nthreads, 1),
                vector<int>(mcsim->hmcs.size(), 0))));
        gather_barrier[lqele->dest_addr].second[num]++;
#ifdef DEBUG_GATHER
        cout << "(New) Receive Gather for flow " << hex << lqele->dest_addr << dec << " at hmc controller " << num
          << ", total gathers at this port: " << (gather_barrier[lqele->dest_addr].second)[num] << endl;
#endif
      }
      if (gather_barrier[lqele->dest_addr].first.second == nthreads)
      {
#ifdef DEBUG_GATHER
        cout << "make gather transactions for flow " << hex << lqele->dest_addr << dec << " at hmccontroller " << num << ":";
#endif
        gather_barrier[lqele->dest_addr].first.second = 0;
        for (uint32_t i = 0; i < mcsim->hmcs.size(); i++)
        {
          gather_barrier[lqele->dest_addr].second[i] = 0;
          if (active_forests[lqele->dest_addr][i] == true)
          {
            gather_barrier[lqele->dest_addr].first.second++;
            gather_barrier[lqele->dest_addr].second[i] = 1;
            bool send_dummy_gather = true;
            for (multimap<uint64_t, LocalQueueElement *>::iterator lqe_it = mcsim->hmcs[i]->active_gather_event.begin();
                lqe_it != mcsim->hmcs[i]->active_gather_event.end(); lqe_it++)
            {
              if (lqe_it->first != lqele->dest_addr) continue;

              switch (mcsim->art_scheme)
              {
                case art_naive:
                  flow_id = lqele->dest_addr;
                  break;
                case art_tid:
                case art_addr:
                  flow_id = (lqele->dest_addr << num_mcs_log2) | i;
                  break;
                default:
                  assert(0);
              }
              newTran = new Transaction(ACTIVE_GET, flow_id, lqe_it->second->src_addr1, data_size, hmc_net,
                  get_src_cubeID(i), get_src_cubeID(i));
              assert(lqe_it->second->dummy == false);
              newTran->address = lqele->dest_addr;
              newTran->nthreads = 1;
              mcsim->hmcs[i]->tran_buf.insert(make_pair(event_time, newTran));
              send_dummy_gather = false;
              //geq->add_event(event_time, mcsim->hmcs[i]);
              break; // only need to send one
            }

            if (send_dummy_gather == true)
            {
              assert(mcsim->art_scheme != art_naive);
              LocalQueueElement * dummy_lqe = new LocalQueueElement();
              dummy_lqe->from.push(this);
              dummy_lqe->address = lqele->address;
              dummy_lqe->dest_addr = lqele->dest_addr;
              dummy_lqe->type = et_art_get;
              dummy_lqe->nthreads = 1;
              dummy_lqe->dummy = true;
              mcsim->hmcs[i]->active_gather_event.insert(make_pair(lqele->dest_addr, dummy_lqe));
              geq->add_event(event_time, mcsim->hmcs[i]);

              flow_id = (lqele->dest_addr << num_mcs_log2) | i;
              newTran = new Transaction(ACTIVE_GET, flow_id, lqele->src_addr1, data_size, hmc_net,
                  get_src_cubeID(i), get_src_cubeID(i));
              newTran->address = lqele->dest_addr;
              newTran->nthreads = 1;
              mcsim->hmcs[i]->tran_buf.insert(make_pair(event_time, newTran));
            }
#ifdef DEBUG_GATHER
            cout << " hmc" << i << "-(" << hex << flow_id << ")" << (send_dummy_gather == true ? "-dummy " : "");
#endif
          }
        }
#ifdef DEBUG_GATHER
        cout << endl;
#endif
      }
      break;

    case ACTIVE_DOT:
      if (active_forests.find(lqele->dest_addr) == active_forests.end())
      {
        active_forests.insert(make_pair(lqele->dest_addr, vector<bool>(4, false)));
      }
      active_forests[lqele->dest_addr][num] = true;
      dest_cube = get_active_cube_num(lqele->src_addr2);
      newTran = new Transaction(ACTIVE_DOT, flow_id, lqele->src_addr2, data_size, hmc_net, src_cube, dest_cube);
      newTran->address = lqele->dest_addr;
      assert(lqele->nthreads == -1);
      break;
    case ACTIVE_ADD:
      if (active_forests.find(lqele->dest_addr) == active_forests.end())
      {
        active_forests.insert(make_pair(lqele->dest_addr, vector<bool>(4, false)));
      }
      active_forests[lqele->dest_addr][num] = true;
      dest_cube = get_active_cube_num(lqele->src_addr1);
      newTran = new Transaction(ACTIVE_ADD, flow_id, lqele->src_addr1, data_size, hmc_net, src_cube, dest_cube);
      newTran->address = lqele->dest_addr;
      newTran->lines = lqele->lines;
      assert(lqele->nthreads == -1);
      break;
    case ACTIVE_MULT:
      if (active_forests.find(lqele->dest_addr) == active_forests.end())
      {
        active_forests.insert(make_pair(lqele->dest_addr, vector<bool>(4, false)));
      }
      active_forests[lqele->dest_addr][num] = true;
      uint32_t dest_cube1 = get_cube_num(lqele->src_addr1);
      uint32_t dest_cube2 = get_cube_num(lqele->src_addr2);
      newTran = new Transaction(ACTIVE_MULT, flow_id, lqele->src_addr1, lqele->src_addr2, data_size,
          hmc_net, src_cube, dest_cube1, dest_cube2);
      newTran->address = lqele->dest_addr;
      newTran->lines = lqele->lines;
      assert(lqele->nthreads == -1);
      break;

  }

  uint64_t req_id = -1;
  if (lqele->type != et_art_get)
  {
    req_id = hmc_net->get_tran_tag(newTran);
    tran_buf.insert(make_pair(event_time, newTran));
  }

  if (lqele->type == et_art_add ||
      lqele->type == et_art_mult ||
      lqele->type == et_art_dot)
  {
    active_update_event.insert(make_pair(req_id, lqele));
    num_update++;
    total_update_req_time += geq->curr_time - lqele->issue_time;
  }
  else if (lqele->type != et_art_get)
  {
    req_event.insert(pair<uint64_t, LocalQueueElement *>(req_id, lqele));
    num_reqs++;
    if (lqele->type == et_pei_dot || lqele->type == et_pei_atomic)
    {
      num_update++;
      total_update_req_time += geq->curr_time - lqele->issue_time;
    }
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
  if (event_time % process_interval != 0)
  {
    event_time += process_interval - event_time%process_interval;
  }
  if (outstanding_req.empty() && tran_buf.empty() && active_update_event.empty() &&
      req_event.empty() && active_gather_event.empty() && resp_queue.empty())
      //active_gather_event.empty() && resp_queue.empty() && pending_active_updates.empty())
  {
    geq->add_event(event_time, this);
  }

  uint64_t dest_addr = lqele->dest_addr;

  map<uint64_t, pair<pair<int, int>, vector<int> > >::iterator it = gather_barrier.find(dest_addr);
  assert(it != gather_barrier.end());

  it->second.first.second -= it->second.second[num];
  it->second.second[num] = -1;
  if (it->second.first.second == 0)
  {
#ifdef DEBUG_GATHER
    cout << "Reply Gather for flow: " << hex << dest_addr << dec << endl;
#endif
    for (int i = 0; i < mcsim->hmcs.size(); i++)
    {
      assert(it->second.second[i] == -1 || it->second.second[i] == 0);
      uint64_t flow_id;
      switch (mcsim->art_scheme)
      {
        case art_naive:
          flow_id = lqele->dest_addr;
          break;
        case art_tid:
        case art_addr:
          flow_id = (lqele->dest_addr << num_mcs_log2) | i;
          break;
        default:
          assert(0);
      }
      multimap<uint64_t, LocalQueueElement *>::iterator get_it = mcsim->hmcs[i]->active_gather_event.begin();
      bool found = false;
      while (get_it != mcsim->hmcs[i]->active_gather_event.end())
      {
        if (get_it->first == dest_addr && get_it->second->dummy == false)
        {
          found = true;
          mcsim->hmcs[i]->resp_queue.push_back(get_it->second);
          geq->add_event(event_time, mcsim->hmcs[i]);
        }
        else if (get_it->first == dest_addr)
        {
          assert(get_it->second->from.top()->type == ct_hmc_controller);
          delete get_it->second;
        }
        ++get_it;
      }
#ifdef DEBUG_GATHER
      if (found)
      {
        cout << " - process for hmccontroller " << i << ", subflow_id " << hex << flow_id << dec;
        if (active_forests[dest_addr][i] == true)
        {
          cout << " (real subflow)" << endl;
          assert(mcsim->hmcs[i]->pending_active_updates.find(flow_id) != mcsim->hmcs[i]->pending_active_updates.end());
        }
        else
        {
          cout << " (fake subflow -- no updates but receive gather)" << endl;
        }
      }
#endif
      mcsim->hmcs[i]->pending_active_updates.erase(flow_id);
      mcsim->hmcs[i]->active_gather_event.erase(dest_addr);

      if (mcsim->art_scheme == art_naive)
        break;
    }
    active_forests.erase(lqele->dest_addr);
    gather_barrier.erase(it);
  }
  delete lqele;
}


uint32_t PTSHMCController::process_event(uint64_t curr_time)
{

  assert(curr_time % process_interval == 0);

  // synchronize memory clock with cpu clock
  if (last_process_time + process_interval < curr_time)
  {
#ifdef CASHMC_FASTSYNC
    uint64_t c = (curr_time - last_process_time - process_interval) / process_interval;
    update_hmc(c);
#else
    for (uint64_t c = last_process_time+process_interval; c < curr_time; c = c + process_interval)
    {
      update_hmc(c);
    }
#endif
  }

  if (tran_buf.empty() == false && tran_buf.begin()->first <= curr_time)
  {
    Transaction *tran = tran_buf.begin()->second;
    if (hmc_net->ReceiveTran(tran, net_num))
    {
      uint64_t flow_id = hmc_net->get_tran_addr(tran);
      uint64_t req_id = hmc_net->get_tran_tag(tran);
      if (tran->transactionType == ACTIVE_ADD ||
          tran->transactionType == ACTIVE_MULT ||
          tran->transactionType == ACTIVE_DOT)
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
          if (kernel_offloading)
          {
            lqele->from.top()->add_rep_event(curr_time + hmc_to_noc_t, lqele, this);
          }
          else
          {
            noc->add_rep_event(curr_time + hmc_to_noc_t, lqele, this);
          }
          pending_active_updates.insert(make_pair(flow_id, lqele));
          active_update_event.erase(req_id);
        }
        else
        {
          cerr << "ERROR: " << *tran << " req_id (" << req_id << ") lqele not found in active_update_event" << endl;
          assert(0);
        }
      }
      else if (tran->transactionType == ACTIVE_GET)
      {
#ifdef DEBUG_GATHER
        cout << "send Gather for flow " << hex << flow_id << dec << endl;
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
        else if(tran->transactionType == DATA_WRITE)
        {
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
        else
        {
          assert(tran->transactionType == PIM_DOT || tran->transactionType == PIM_ATOMIC);
          num_update_sent++;
          total_update_stall_time += curr_time - tran_buf.begin()->first;
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
      uint64_t flow_id = served_trans[0].first;
#ifdef DEBUG_GATHER
      multimap<uint64_t, LocalQueueElement *>::iterator it = pending_active_updates.find(flow_id);
      assert(it != pending_active_updates.end());
      cout << "Get active response (should be gather) flowID: " << hex << flow_id << ", dest_addr: "
        <<  (mcsim->art_scheme == art_naive ? flow_id : (flow_id >> num_mcs_log2)) << dec
        << " at hmccontroller " << num << endl;
#endif
      LocalQueueElement *lqe = new LocalQueueElement();
      lqe->from.push(this);
      lqe->type = et_art_get;
      switch (mcsim->art_scheme)
      {
        case art_naive:
          lqe->dest_addr = flow_id;
          break;
        case art_tid:
        case art_addr:
          lqe->dest_addr = flow_id >> num_mcs_log2;
          break;
        default:
          assert(0);
      }
      add_rep_event(curr_time, lqe, this);
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
      assert(tran_it->second != et_art_get &&
          tran_it->second != et_art_add &&
          tran_it->second != et_art_mult &&
          tran_it->second != et_art_dot);
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
        case MEM_READ: // read
          total_rd_mem_time += curr_time - issue_time;
          break;
        case MEM_WRITE: // write
          total_wr_mem_time += curr_time - issue_time;
          break;
        case MEM_EVICT: // evict
          total_ev_mem_time += curr_time - issue_time;
          break;
        case PEI_DOT: // pei
        case PEI_ATOMIC:
          break;
        default:
          cerr << "Unknown reply transaction type: " << tran_type  << endl;
          exit(1);
      }
    }

    served_trans.erase(served_trans.begin());
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
        case et_art_get:
        case et_pei_dot:
        case et_pei_atomic:
          if (kernel_offloading)
          {
            (*iter)->from.top()->add_rep_event(curr_time + hmc_to_noc_t, *iter, this);
          }
          else
          {
            noc->add_rep_event(curr_time + hmc_to_noc_t, *iter, this);
          }
          resp_queue.erase(iter);
          break;
        default:
          cerr << "what is this" << endl;
          assert(0);
      }
    }
    break; //as of now process only one in one cycle
  }

  update_hmc(1);
  last_process_time = curr_time; //last response time

  if (!req_event.empty() || !outstanding_req.empty() ||
      !tran_buf.empty() || !active_update_event.empty() ||
      //!tran_buf.empty() || !active_update_event.empty() || !pending_active_updates.empty() ||
      !active_gather_event.empty() || !resp_queue.empty())
  {
    geq->add_event(curr_time + process_interval, this);
  }
  else
  {
    assert(req_event.empty() && resp_queue.empty());
  }

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
  int src_cube = -1;
  switch (cubes_per_mc) {
    case 4:
      src_cube = num * 5;
      break;
    case 16:
      src_cube = num * 21;
      break;
    default:
      break;
  }
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
      return MEM_READ;//type 1 is READ

    case et_write:
    case et_write_nd:
    case et_s_rd_wr:
      return MEM_WRITE;

    case et_evict:
    case et_evict_nd:
    case et_dir_evict:
      return MEM_EVICT; // both write and evict are DATA_WRITE

    case et_art_get:
      return ART_GET;

    case et_art_add:
      return ART_ADD; // Jiayi, type 5 is ACTIVE_ADD

    case et_art_mult:
      return ART_MULT; // Jiayi, type 6 is ACTIVE_MULT

    case et_art_dot:
      return ART_DOT; // Jiayi, type 6 is ACTIVE_PEI

    case et_pei_dot:
      return PEI_DOT;

    case et_pei_atomic:
      return PEI_ATOMIC;

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

