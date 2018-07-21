/*
 * Copyright (c) 2010 The Hewlett-Packard Development Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Jung Ho Ahn
 */

#include "PTSCache.h"
#include "PTSDirectory.h"
#include "PTSXbar.h"
#include <algorithm>
#include <iomanip>
#include <set>

using namespace PinPthread;
using namespace std;


extern ostream & operator << (ostream & output, coherence_state_type cs);
extern ostream & operator << (ostream & output, component_type ct);
extern ostream & operator << (ostream & output, event_type et);


Cache::Cache(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_):
  Component(type_, num_, mcsim_),
  num_rd_access(0), num_rd_miss(0),
  num_wr_access(0), num_wr_miss(0),
  num_ev_coherency(0), num_ev_capacity(0),
  num_coherency_access(0), num_upgrade_req(0),
  num_bypass(0), num_nack(0)
{
  num_banks = get_param_uint64("num_banks", 1);
  req_qs    = vector< queue<LocalQueueElement * > >(num_banks);
}


void Cache::display_event(uint64_t curr_time, LocalQueueElement * lqe, const string & postfix)
{
  if (lqe->address >= ((search_addr >> set_lsb) << set_lsb) &&
      lqe->address <  (((search_addr >> set_lsb) + 1) << set_lsb))
  {
    cout << "  -- [" << setw(7) << curr_time << "] " << type << postfix << " [" << num << "] "; 
    lqe->display();
    show_state(lqe->address);
  }
}


CacheL1::CacheL1(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
 :Cache(type_, num_, mcsim_),
  l1_to_lsu_t(get_param_uint64("to_lsu_t", 0)),
  l1_to_l2_t (get_param_uint64("to_l2_t", 45)),
  l1_to_xbar_t(get_param_uint64("to_xbar_t", 90)) // Jiayi, 06/06/17
{
  num_sets   = get_param_uint64("num_sets", 64);
  num_ways   = get_param_uint64("num_ways", 4);
  set_lsb    = get_param_uint64("set_lsb",  6);
  always_hit = (get_param_str("always_hit") == "true");
  process_interval = get_param_uint64("process_interval", 10);
  l2_set_lsb = get_param_uint64("set_lsb", "pts.l2$.", set_lsb);

  num_sets_per_subarray = get_param_uint64("num_sets_per_subarray", 1);

  //tags = vector< list< pair< uint64_t, coherence_state_type > > >(num_sets,
  //               list< pair< uint64_t, coherence_state_type > >(num_ways,
  //                     pair< uint64_t, coherence_state_type >(0, cs_invalid)));
  tags = new pair< uint64_t, coherence_state_type > ** [num_sets];
  for (uint32_t i = 0; i < num_sets; i++)
  {
    tags[i] = new pair< uint64_t, coherence_state_type > * [num_ways];
    for (uint32_t j = 0; j < num_ways; j++)
    {
      tags[i][j] = new pair< uint64_t, coherence_state_type > (0, cs_invalid);
    }
  }

  use_prefetch          = (get_param_str("use_prefetch") == "true");
  num_prefetch_requests = 0;
  num_prefetch_hits     = 0;
  num_pre_entries       = get_param_uint64("num_pre_entries", 64);
  oldest_pre_entry_idx  = 0;

  pres = new PrefetchEntry * [num_pre_entries];
  for (uint32_t i = 0; i < num_pre_entries; i++)
  {
    pres[i] = new PrefetchEntry();
  }

  num_mshrs = get_param_uint64("num_mshrs", 32);  // Jiayi, MSHR
  mshrs = new MSHREntry * [num_mshrs];
  for (uint32_t i = 0; i < num_mshrs; i++)
  {
    mshrs[i] = new MSHREntry();
  }
}


CacheL1::~CacheL1()
{
  if (num_rd_access > 0)
  {
    cout << "  -- L1$" << ((type == ct_cachel1d || type == ct_cachel1d_t1 || type == ct_cachel1d_t2) ? "D[" : "I[") << setw(3) << num << "] : RD (miss, access)=( "
         << setw(10) << num_rd_miss << ", " << setw(10) << num_rd_access << ")= "
         << setw(6) << setiosflags(ios::fixed) << setprecision(2) << 100.00*num_rd_miss/num_rd_access << "%, PRE (hit, reqs)=( "
         << num_prefetch_hits << ", " << num_prefetch_requests << " )" << endl;
  }
  if (num_wr_access > 0)
  {
    cout << "  -- L1$" << ((type == ct_cachel1d || type == ct_cachel1d_t1 || type == ct_cachel1d_t2) ? "D[" : "I[") << setw(3) << num << "] : WR (miss, access)=( "
         << setw(10) << num_wr_miss << ", " << setw(10) << num_wr_access << ")= "
         << setw(6) << setiosflags(ios::fixed) << setprecision(2) << 100.00*num_wr_miss/num_wr_access << "%" << endl;
  }

  if ((type == ct_cachel1d || type == ct_cachel1d_t1 || type == ct_cachel1d_t2) && (num_ev_coherency > 0 || num_ev_capacity > 0 || num_coherency_access > 0))
  {
    cout << "  -- L1$D[" << setw(3) << num
         << "] : (ev_coherency, ev_capacity, coherency_access, up_req, bypass, nack)=( "
         << setw(10) << num_ev_coherency << ", " << setw(10) << num_ev_capacity << ", " 
         << setw(10) << num_coherency_access << ", " << setw(10) << num_upgrade_req << ", "
         << setw(10) << num_bypass << ", " << setw(10) << num_nack << "), ";

    map<uint64_t, uint64_t> dirty_cl_per_offset;
    int32_t  addr_offset_lsb = get_param_uint64("addr_offset_lsb", "", 48);

    for (uint32_t j = 0; j < num_sets; j++)
    {
      for (uint32_t i = 0; i < num_ways; i++)
      {
        if (tags[j][i]->second == cs_modified)
        {
          uint64_t addr   = ((tags[j][i]->first * num_sets) << set_lsb);
          uint64_t offset = addr >> addr_offset_lsb;

          if (dirty_cl_per_offset.find(offset) == dirty_cl_per_offset.end())
          {
            dirty_cl_per_offset[offset] = 1;
          }
          else
          {
            dirty_cl_per_offset[offset]++;
          }
        }
      }
    }

    cout << "num_dirty_lines (pid:#) = ";

    for (map<uint64_t, uint64_t>::iterator m_iter = dirty_cl_per_offset.begin(); m_iter != dirty_cl_per_offset.end(); m_iter++)
    {
      cout << m_iter->first << " : " << m_iter->second << " , ";
    }
    cout << endl;
  }
  else if ((type == ct_cachel1i || type == ct_cachel1i_t1 || type == ct_cachel1i_t2) &&
           (num_ev_coherency > 0 || num_coherency_access > 0))
  {
    cout << "  -- L1$I[" << setw(3) << num << "] : (ev_coherency, coherency_access, bypass)=( "
         << setw(10) << num_ev_coherency << ", " << setw(10) << num_coherency_access << ", "
         << setw(10) << num_bypass << ")" << endl;
  }

  for (uint32_t i = 0; i < num_sets; i++)
  {
    for (uint32_t j = 0; j < num_ways; j++)
    {
      delete tags[i][j];
    }
    delete [] tags[i];
  }
  delete [] tags;
  for (uint32_t i = 0; i < num_pre_entries; i++)
  {
    delete pres[i];
  }
  delete [] pres;
  for (uint32_t i = 0; i < num_mshrs; i++)
  {
    delete mshrs[i];
  }
  delete [] mshrs;
}


void CacheL1::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  /*if (rep_q.empty() == false || req_q.empty() == false)
  {
    event_time += process_interval - event_time%process_interval;
  }*/
  geq->add_event(event_time, this);
  req_event.insert(pair<uint64_t, LocalQueueElement *>(event_time, local_event));
}


void CacheL1::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  /*if (rep_q.empty() == false || req_q.empty() == false)
  {
    event_time += process_interval - event_time%process_interval;
  }*/
  geq->add_event(event_time, this);
  rep_event.insert(pair<uint64_t, LocalQueueElement *>(event_time, local_event));
}


void CacheL1::show_state(uint64_t address)
{
  uint32_t set = (address >> set_lsb) % num_sets;
  uint64_t tag = (address >> set_lsb) / num_sets;
  for (uint32_t i = 0; i < num_ways; i++)
  {
    if (tags[set][i]->second != cs_invalid && tags[set][i]->first == tag)
    {
      cout << "  -- L1" << ((type == ct_cachel1d || type == ct_cachel1d_t1 || type == ct_cachel1d_t2) ? "D[" : "I[") << num << "] : " << tags[set][i]->second << endl; 
      break;
    }
  }
  /*list< pair< uint64_t, coherence_state_type > >::iterator set_iter;

  for (set_iter = tags[set].begin(); set_iter != tags[set].end(); ++set_iter)
  {
    if (set_iter->second != cs_invalid && set_iter->first == tag)
    {
      cout << "  -- L1" << ((type == ct_cachel1d) ? "D[" : "I[") << num << "] : " << set_iter->second << endl; 
      break;
    }
  }*/
}


uint32_t CacheL1::process_event(uint64_t curr_time)
{
  multimap<uint64_t, LocalQueueElement *>::iterator req_event_iter = req_event.begin();
  multimap<uint64_t, LocalQueueElement *>::iterator rep_event_iter = rep_event.begin();
  //list< pair< uint64_t, coherence_state_type > >::iterator set_iter;
  pair< uint64_t, coherence_state_type > * set_iter = NULL;

  LocalQueueElement * rep_lqe = NULL;
  LocalQueueElement * req_lqe = NULL;
  // event -> queue
  if (rep_q.empty() == false)
  {
    rep_lqe = rep_q.front();
    rep_q.pop();
  }
  else if (rep_event_iter != rep_event.end() && rep_event_iter->first == curr_time)
  {
    rep_lqe = rep_event_iter->second;
    ++rep_event_iter;
  }

  while (rep_event_iter != rep_event.end() && rep_event_iter->first == curr_time)
  {
    rep_q.push(rep_event_iter->second);
    ++rep_event_iter;
  }
  rep_event.erase(curr_time);

  while (req_event_iter != req_event.end() && req_event_iter->first == curr_time)
  {
    uint32_t bank = (req_event_iter->second->address >> set_lsb) % 1;//num_banks;
    req_qs[bank].push(req_event_iter->second);
    ++req_event_iter;
  }
  req_event.erase(curr_time);

  // reply events have higher priority than request events
  if (rep_lqe != NULL)
  {
    int  index;
    bool sent_to_l2 = false;
    bool addr_in_cache = false;
    for (index = 0; index < (1 << (l2_set_lsb - set_lsb)); index++)
    {
      // L1 -> LSU
      uint64_t address = ((rep_lqe->address>>l2_set_lsb)<<l2_set_lsb) +
        (rep_lqe->address + index*(1 << set_lsb))%(1 << l2_set_lsb);
      //uint64_t address = rep_lqe->address;
      uint64_t line_addr = (rep_lqe->address >> set_lsb) << set_lsb;
      uint32_t set = (address >> set_lsb) % num_sets;
      uint64_t tag = (address >> set_lsb) / num_sets;
      event_type etype = rep_lqe->type;

#ifdef DEBUG_CACHE
      display_event(curr_time, rep_lqe, "P");
#endif

      // search if there is a cache line that already has the tag
      /*for (set_iter = tags[set].begin(); set_iter != tags[set].end(); ++set_iter)
      {
        if (set_iter->second != cs_invalid && set_iter->first == tag)
        {
          break;
        }
      }*/
      uint32_t idx = 0;
      for ( ; idx < num_ways; idx++)
      {
        if (tags[set][idx]->second != cs_invalid && tags[set][idx]->first == tag)
        {
          set_iter = tags[set][idx];
          break;
        }
      }

      uint32_t mshr_idx = num_mshrs;
      switch (etype)
      {
        case et_nack:
          num_nack++;
        case et_rd_bypass:
          if (sent_to_l2 == true)
          {
            break;
          }
          sent_to_l2 = true;
          num_bypass++;
          rep_lqe->from.pop();
          /* Jiayi, deal with MSHRs begin */
#ifdef DEBUG_MSHR
          display(); cout << " MSHR erase line addr: " << hex << line_addr << " for reply [" << rep_lqe << "] " << dec; rep_lqe->display();
#endif
          assert(mshr_indices.find(line_addr) != mshr_indices.end());
          mshr_idx = mshr_indices[line_addr];
          assert(mshrs[mshr_idx]->valid);
          assert(mshrs[mshr_idx]->line_addr == line_addr);
          assert(mshrs[mshr_idx]->target == rep_lqe);
          for (list<LocalQueueElement *>::iterator iter = mshrs[mshr_idx]->target_list.begin();
              iter != mshrs[mshr_idx]->target_list.end(); iter++)
          {
            (*iter)->type = etype;
            add_event_to_lsu(curr_time, *iter);
          }
          mshrs[mshr_idx]->release();
          mshr_indices.erase(line_addr);
          /* MSHRs end */
          add_event_to_lsu(curr_time, rep_lqe);
          break;

        case et_evict:
          num_coherency_access++;
          if (set_iter != NULL)
          //if (set_iter != tags[set].end())
          {
            num_ev_coherency++;

            if (set_iter->second == cs_modified)
            {
              set_iter->second = cs_invalid;
              if (sent_to_l2 == false)
              {
                sent_to_l2 = true;
                rep_lqe->from.pop();
                rep_lqe->from.push(this);
                if (!geq->is_nuca || (geq->is_nuca && geq->which_l2(rep_lqe->address) == cachel2->num)) {
                  cachel2->add_rep_event(curr_time + l1_to_l2_t, rep_lqe);
                } else {
                  noc->add_rep_event(curr_time + l1_to_xbar_t, rep_lqe, this);  // Jiayi, if top() is l1 and from is l1, it is for l2 in noc
                }
              }
              break;
            }
            set_iter->second = cs_invalid;
          }
          if (index == (1 << (l2_set_lsb - set_lsb)) - 1 && sent_to_l2 == false) delete rep_lqe;
          break;

        case et_m_to_s:
        case et_m_to_m:
          num_coherency_access++;
          if (set_iter == NULL || set_iter->second != cs_modified)
          //if (set_iter == tags[set].end() || set_iter->second != cs_modified)
          {
            if (set_iter != NULL) assert(set_iter->second == cs_invalid); // Jiayi, TODO: delete this
            if (sent_to_l2 == true) break;
            sent_to_l2 = true;
            rep_lqe->from.pop();
            rep_lqe->from.push(this);
            if (!geq->is_nuca || (geq->is_nuca && geq->which_l2(rep_lqe->address) == cachel2->num)) {
              cachel2->add_rep_event(curr_time + l1_to_l2_t, rep_lqe);
            } else {
              noc->add_rep_event(curr_time + l1_to_xbar_t, rep_lqe, this);  // Jiayi, if top() is l1 and from is l1, it is for l2 in noc
            }
          }
          else
          {
            if (etype == et_m_to_m)
            {
              num_ev_coherency++;
              set_iter->second = cs_invalid;
            }
            else
            {
              set_iter->second = cs_shared;
            }
            if (sent_to_l2 == true) break;
            sent_to_l2 = true;
            rep_lqe->from.pop();
            rep_lqe->from.push(this);
            if (!geq->is_nuca || (geq->is_nuca && geq->which_l2(rep_lqe->address) == cachel2->num)) {
              cachel2->add_rep_event(curr_time + l1_to_l2_t, rep_lqe);
            } else {
              noc->add_rep_event(curr_time + l1_to_xbar_t, rep_lqe, this);  // Jiayi, if top() is l1 and from is l1, it is for l2 in noc
            }
          }
          break;

        case et_dir_rd:
          num_coherency_access++;
          //if (set_iter == tags[set].end())
          if (set_iter == NULL)
          {
            // oops -- the cache line is already evicted. do nothing.
            //delete rep_lqe;
          }
          else
          {
            if (sent_to_l2 == true) break;
            sent_to_l2 = true;
            if (set_iter->second != cs_modified)
            {
              show_state(address); rep_lqe->display();  geq->display();  ASSERTX(0);
            }
            num_ev_coherency++;
            set_iter->second = cs_exclusive;
            rep_lqe->type    = et_evict;
            rep_lqe->from.push(this);
            if (!geq->is_nuca || (geq->is_nuca && geq->which_l2(rep_lqe->address) == cachel2->num)) {
              cachel2->add_rep_event(curr_time + l1_to_l2_t, rep_lqe);
            } else {
              noc->add_rep_event(curr_time + l1_to_xbar_t, rep_lqe, this);// Jiayi, if top() is l1 and from is l1, it is for l2 in noc
            } // reply for dirty read?
          }
          if (sent_to_l2 == false && index == (1 << (l2_set_lsb - set_lsb)) - 1) delete rep_lqe;
          break;

        case et_read:
        case et_write:
          if (index != 0) break;
          rep_lqe->from.pop();
          if (set_iter == NULL)
          //if (set_iter == tags[set].end())
          {
            //set_iter = tags[set].begin();
            idx = 0;
            set_iter = tags[set][idx];
            if (set_iter->second != cs_invalid)
            {
              // evicted due to lack of $ capacity
              num_ev_capacity++;
              LocalQueueElement * lqe = new LocalQueueElement(this,
                  (set_iter->second == cs_modified) ? et_evict : et_evict_nd,
                  ((set_iter->first*num_sets + set) << set_lsb));
              lqe->th_id = rep_lqe->th_id;
              if (!geq->is_nuca || (geq->is_nuca && geq->which_l2(lqe->address) == cachel2->num)) {
                cachel2->add_rep_event(curr_time + l1_to_l2_t, lqe);
              } else {
                noc->add_rep_event(curr_time + l1_to_xbar_t, lqe, this);// Jiayi, if top() is l1 and from is l1, it is for l2 in noc
              }
            }
          }
          else
          {
            addr_in_cache = true;
          }

          set_iter->first  = tag;
          set_iter->second = (etype == et_read && (addr_in_cache == false || set_iter->second != cs_modified)) ? cs_exclusive : cs_modified;
          for (uint32_t i = idx; i < num_ways-1; i++)
          {
            tags[set][i] = tags[set][i+1];
          }
          tags[set][num_ways-1] = set_iter;
          //tags[set].push_back(*set_iter);
          //tags[set].erase(set_iter);
          /* Jiayi, deal with MSHRs begin */
#ifdef DEBUG_MSHR
          display(); cout << " MSHR erase line addr: " << hex << line_addr << " for reply [" << rep_lqe << "] " << dec; rep_lqe->display();
#endif
          assert(mshr_indices.find(line_addr) != mshr_indices.end());
          mshr_idx = mshr_indices[line_addr];
          assert(mshrs[mshr_idx]->valid);
          assert(mshrs[mshr_idx]->line_addr == line_addr);
          assert(mshrs[mshr_idx]->target == rep_lqe);
          for (list<LocalQueueElement *>::iterator iter = mshrs[mshr_idx]->target_list.begin();
              iter != mshrs[mshr_idx]->target_list.end(); iter++)
          {
            add_event_to_lsu(curr_time, *iter);
          }
          mshrs[mshr_idx]->release();
          mshr_indices.erase(line_addr);
          /* MSHRs end */
          add_event_to_lsu(curr_time, rep_lqe);
          break;

        case et_e_to_s:
        case et_s_to_s:
        default:
          show_state(address); rep_lqe->display(); geq->display(); ASSERTX(0);
          break;
      }
    }
  }
  else
  {
    bool any_request = false;

    for (uint32_t i = 0; i < num_banks; i++)
    {
      if (req_qs[/*i*/0].empty() == true)
      {
        continue;
      }
      any_request = true;

      req_lqe = req_qs[/*i*/0].front();
      req_qs[/*i*/0].pop();
      // process the first request event
      uint64_t address = req_lqe->address;
      uint64_t line_addr = (req_lqe->address>>l2_set_lsb)<<l2_set_lsb;
      uint32_t set = (address >> set_lsb) % num_sets;
      uint64_t tag = (address >> set_lsb) / num_sets;
      event_type etype = req_lqe->type;
      bool hit = always_hit;
      bool is_coherence_miss = false;
      //list< pair< uint64_t, coherence_state_type > > & curr_set = tags[set];

#ifdef DEBUG_CACHE
      display_event(curr_time, req_lqe, "Q");
#endif

      switch (etype)
      {
        case et_read:
          num_rd_access++;
          num_wr_access--;
        case et_write:
          num_wr_access++;

          //for (set_iter = curr_set.begin(); set_iter != curr_set.end() && hit == false; ++set_iter)
          for (uint32_t idx = 0; idx < num_ways && hit == false; idx++)
          {
            set_iter = tags[set][idx];
            if (set_iter->second == cs_invalid)
            {
              continue;
            }
            else if (set_iter->first == tag)
            {
              if (set_iter->second == cs_modified ||
                  (etype == et_read && (set_iter->second == cs_shared || set_iter->second == cs_exclusive)))
              {
                hit = true;
                for (uint32_t i = idx; i < num_ways-1; i++)
                {
                  tags[set][i] = tags[set][i+1];
                }
                tags[set][num_ways-1] = set_iter;
                //curr_set.push_back(*set_iter);
                //curr_set.erase(set_iter);
                break;
              }
              else if (etype == et_write && (set_iter->second == cs_shared || set_iter->second == cs_exclusive))
              {
                // on a write miss, invalidate the entry so that the following
                // cache accesses to the address experience misses as well
                num_upgrade_req++;
                is_coherence_miss = true;
                set_iter->second = cs_invalid;
                break;
              }
              else
              { // miss
                break;
              }
            }
          }
          break;
        default:
          display();  req_lqe->display();  geq->display();  ASSERTX(0);
      }

      if (hit == false)
      {
        /* Jiayi, deal with MSHRs begin */
        if (mshr_indices.find(line_addr) != mshr_indices.end())
        {
          uint32_t mshr_idx = mshr_indices[line_addr];
          if (req_lqe->type == et_read && mshrs[mshr_idx]->etype == et_read)
          {
#ifdef DEBUG_MSHR
            display(); cout << " MSHR line addr " << hex << line_addr << " matched and is read, add to target list for request [" << req_lqe << "]" << dec; req_lqe->display();
#endif
            mshrs[mshr_idx]->add(req_lqe);
          }
          else
          {
#ifdef DEBUG_MSHR
            display(); cout << " MSHR line addr " << hex << line_addr << " read/write conflict, nack request [" << req_lqe << "]" << dec; req_lqe->display();
#endif
            req_lqe->type = et_nack;
            add_event_to_lsu(curr_time, req_lqe);
          }
        }
        else if (mshr_indices.size() == num_mshrs)
        {
#ifdef DEBUG_MSHR
          display(); cout << " MSHR full, no entry for line "  << hex << line_addr << ", nack request [" << req_lqe << "]" << dec; req_lqe->display();
#endif
          req_lqe->type = et_nack;
          add_event_to_lsu(curr_time, req_lqe);
        }
        else
        {
#ifdef DEBUG_MSHR
          display(); cout << " MSHR insert line addr " << hex << line_addr << " for request [" << req_lqe << "] " << dec; req_lqe->display();
#endif
          uint32_t mshr_idx = num_mshrs;
          for (mshr_idx = 0; mshr_idx < num_mshrs; mshr_idx++)
          {
            if (mshrs[mshr_idx]->valid == false) break;
          }
          assert(mshr_idx < num_mshrs);
          mshr_indices.insert(make_pair(line_addr, mshr_idx));
          mshrs[mshr_idx]->allocate(line_addr, req_lqe, etype);
          req_lqe->from.push(this);
          if (!geq->is_nuca || (geq->is_nuca && geq->which_l2(req_lqe->address) == cachel2->num)) {
            cachel2->add_req_event(curr_time + l1_to_l2_t, req_lqe);
          } else {
            noc->add_req_event(curr_time + l1_to_xbar_t, req_lqe, this);// Jiayi, if top() is l1 and from is l1, it is for l2 in noc
          }
        }
        /* MSHRs end */

        if (is_coherence_miss == false)
        {
          (etype == et_write) ? num_wr_miss++ : num_rd_miss++;
        }
      }
      else
      {
        add_event_to_lsu(curr_time, req_lqe);
      }

      if (etype == et_read && use_prefetch == true)
      {
        // update the prefetch entries if
        //  1) current and (previous or next) $ line addresses are in the L1 $
        //  2) do not cause multiple prefetches to the same $ line (check existing entries)
        uint64_t address = ((req_lqe->address >> set_lsb) << set_lsb);
        uint64_t prev_addr = address - (1 << set_lsb);
        uint64_t next_addr = address + (1 << set_lsb);
        // check_prev first
        bool     next_addr_exist = false;
        bool     prev_addr_exist = false;
        for (uint32_t idx = 0; idx < num_pre_entries; idx++)
        {
          if (pres[idx]->addr != 0 && pres[idx]->addr == next_addr)
          {
            pres[idx]->hit  = true;
            next_addr_exist = true;
            break;
          }
        }
        if (next_addr_exist == false)
        {
          uint32_t set = (prev_addr >> set_lsb) % num_sets;
          uint64_t tag = (prev_addr >> set_lsb) / num_sets;
          for (uint32_t idx = 0; idx < num_ways; idx++)
          {
            set_iter = tags[set][idx];
            if (set_iter->second == cs_invalid)
            {
              continue;
            }
            else if (set_iter->first == tag)
            {
              prev_addr_exist = true;
              break;
            }
          }
          if (prev_addr_exist == true)
          {
            LocalQueueElement * lqe = new LocalQueueElement(this, et_read, next_addr);
            lqe->th_id = req_lqe->th_id;
            if (!geq->is_nuca || (geq->is_nuca && geq->which_l2(next_addr) == cachel2->num)) {
              cachel2->add_req_event(curr_time + l1_to_l2_t, lqe);
            } else {
              noc->add_req_event(curr_time + l1_to_xbar_t, lqe, this);// Jiayi, if top() is l1 and from is l1, it is for l2 in noc
            }
            // update the prefetch entry
            if (pres[oldest_pre_entry_idx]->addr != 0)
            {
              num_prefetch_requests++;
              num_prefetch_hits += (pres[oldest_pre_entry_idx]->hit == true) ? 1 : 0;
            }
            pres[oldest_pre_entry_idx]->hit  = false;
            pres[oldest_pre_entry_idx]->addr = next_addr;
            oldest_pre_entry_idx = (oldest_pre_entry_idx + 1) % num_pre_entries;
          }
        }
        for (uint32_t idx = 0; idx < num_pre_entries && prev_addr_exist == false; idx++)
        {
          if (pres[idx]->addr != 0 && pres[idx]->addr == prev_addr)
          {
            pres[idx]->hit  = true;
            prev_addr_exist = true;
            break;
          }
        }
        if (prev_addr_exist == false)
        {
          next_addr_exist = false;
          uint32_t set = (next_addr >> set_lsb) % num_sets;
          uint64_t tag = (next_addr >> set_lsb) / num_sets;
          for (uint32_t idx = 0; idx < num_ways; idx++)
          {
            set_iter = tags[set][idx];
            if (set_iter->second == cs_invalid)
            {
              continue;
            }
            else if (set_iter->first == tag)
            {
              next_addr_exist = true;
              break;
            }
          }
          if (next_addr_exist == true)
          {
            LocalQueueElement * lqe = new LocalQueueElement(this, et_read, next_addr);
            lqe->th_id = req_lqe->th_id;
            if (!geq->is_nuca || (geq->is_nuca && geq->which_l2(next_addr) == cachel2->num)) {
              cachel2->add_req_event(curr_time + l1_to_l2_t, lqe);
            } else {
              noc->add_req_event(curr_time + l1_to_xbar_t, lqe, this);// Jiayi, if top() is l1 and from is l1, it is for l2 in noc
            }
            // update the prefetch entry
            if (pres[oldest_pre_entry_idx]->addr != 0)
            {
              num_prefetch_requests++;
              num_prefetch_hits += (pres[oldest_pre_entry_idx]->hit == true) ? 1 : 0;
            }
            pres[oldest_pre_entry_idx]->hit  = false;
            pres[oldest_pre_entry_idx]->addr = prev_addr;
            oldest_pre_entry_idx = (oldest_pre_entry_idx + 1) % num_pre_entries;
          }
        }
      }
    }

    /*if (any_request == false)
    {
      geq->display();  ASSERTX(0);
    }*/
  }

  if (rep_q.empty() == false)
  {
    geq->add_event(curr_time + process_interval, this);
  }
  else
  {
    for (uint32_t i = 0; i < num_banks; i++)
    {
      if (req_qs[i].empty() == false)
      {
        geq->add_event(curr_time + process_interval, this);
        break;
      }
    }
  }

  return 0; 
}


void CacheL1::add_event_to_lsu(uint64_t curr_time, LocalQueueElement * lqe)
{
  if (type == ct_cachel1d || type == ct_cachel1d_t1 || type == ct_cachel1d_t2)
  {
    (lqe->from.top())->add_rep_event(curr_time + l1_to_lsu_t, lqe);
  }
  else
  {
    (lqe->from.top())->add_req_event(curr_time + l1_to_lsu_t, lqe);
  }
}


// in L2, num_sets is the number of sets of all L2 banks. set_lsb still sets the size of a cache line.
// bank and set numbers are specified like:
// [ MSB <-----------------> LSB ]
// [ ... SETS  BANKS  CACHE_LINE ]
CacheL2::CacheL2(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
 :Cache(type_, num_, mcsim_),
  l2_to_l1_t  (get_param_uint64("to_l1_t", 45)),
  l2_to_dir_t (get_param_uint64("to_dir_t", 90)),
  l2_to_xbar_t(get_param_uint64("to_xbar_t", 90)),
  num_flits_per_packet(get_param_uint64("num_flits_per_packet", 1))
{
  num_sets         = get_param_uint64("num_sets",  512);
  num_ways         = get_param_uint64("num_ways",  8);
  set_lsb          = get_param_uint64("set_lsb",   6);
  process_interval = get_param_uint64("process_interval", 20);
  num_banks_log2   = log2(num_banks);

  num_sets_per_subarray = get_param_uint64("num_sets_per_subarray", 1);
  always_hit            = (get_param_str("always_hit") == "true");
  display_life_time     = (get_param_str("display_life_time") == "true");

  num_destroyed_cache_lines = 0;
  cache_line_life_time      = 0;
  time_between_last_access_and_cache_destroy = 0;
  num_ev_from_l1      = 0;
  num_ev_from_l1_miss = 0;

  //tags   = vector< list< L2Entry > >(num_sets, list< L2Entry >(num_ways, L2Entry()));
  tags = new L2Entry ** [num_sets];
  for (uint32_t i = 0; i < num_sets; i++)
  {
    tags[i] = new L2Entry * [num_ways];
    for (uint32_t j = 0; j < num_ways; j++)
    {
      tags[i][j] = new L2Entry();
    }
  }

  num_mshrs = get_param_uint64("num_mshrs", 32);  // Jiayi, MSHR
  mshrs = new MSHREntry * [num_mshrs];
  for (uint32_t i = 0; i < num_mshrs; i++)
  {
    mshrs[i] = new MSHREntry();
  }
}


CacheL2::~CacheL2()
{
  if (num_rd_access > 0)
  {
    cout << "  -- L2$ [" << setw(3) << num << "] : RD (miss, access)=( "
      << setw(10) << num_rd_miss << ", " << setw(10) << num_rd_access << ")= " 
      << setw(6) << setiosflags(ios::fixed) << setprecision(2) << 100.00*num_rd_miss/num_rd_access << "%" << endl;
  }
  if (num_wr_access > 0)
  {
    cout << "  -- L2$ [" << setw(3) << num << "] : WR (miss, access)=( "
      << setw(10) << num_wr_miss << ", " << setw(10) << num_wr_access << ")= "
      << setw(6) << setiosflags(ios::fixed) << setprecision(2) << 100.00*num_wr_miss/num_wr_access << "%" << endl;
  }

  if (num_ev_coherency > 0 || num_ev_capacity > 0 || num_coherency_access > 0 || num_upgrade_req > 0)
  {
    cout << "  -- L2$ [" << setw(3) << num << "] : (ev_coherency, ev_capacity, coherency_access, up_req, bypass, nack)=( "
         << setw(10) << num_ev_coherency << ", " << setw(10) << num_ev_capacity << ", " 
         << setw(10) << num_coherency_access << ", " << setw(10) << num_upgrade_req << ", "
         << setw(10) << num_bypass << ", " << setw(10) << num_nack << ")" << endl;
  }
  if (num_ev_from_l1 > 0)
  {
    cout << "  -- L2$ [" << setw(3) << num << "] : EV_from_L1 (miss, access)=( "
         << setw(10) << num_ev_from_l1_miss << ", " << setw(10) << num_ev_from_l1 << ")= "
         << setiosflags(ios::fixed) << setprecision(2) << 100.0*num_ev_from_l1_miss/num_ev_from_l1 << "%, ";
  }
  if (num_rd_access > 0 || num_wr_access > 0)
  {
    uint32_t num_cache_lines    = 0;
    uint32_t num_i_cache_lines  = 0;
    uint32_t num_e_cache_lines  = 0;
    uint32_t num_s_cache_lines  = 0;
    uint32_t num_m_cache_lines  = 0;
    uint32_t num_tr_cache_lines = 0;
    int32_t  addr_offset_lsb = get_param_uint64("addr_offset_lsb", "", 48);

    map<uint64_t, uint64_t> dirty_cl_per_offset;

    for (uint32_t j = 0; j < num_sets; j++)
    {
      //for (list<CacheL2::L2Entry>::iterator iter = tags[j].begin(); iter != tags[j].end(); ++iter)
      for (uint32_t k = 0; k < num_ways; k++)
      {
        L2Entry * iter = tags[j][k];
        if (iter->type == cs_modified)
        {
          uint64_t addr   = ((iter->tag * num_sets) << set_lsb);
          uint64_t offset = addr >> addr_offset_lsb;

          if (dirty_cl_per_offset.find(offset) == dirty_cl_per_offset.end())
          {
            dirty_cl_per_offset[offset] = 1;
          }
          else
          {
            dirty_cl_per_offset[offset]++;
          }
        }
        switch (iter->type)
        {
          case cs_invalid:   num_i_cache_lines++;  break;
          case cs_exclusive: num_e_cache_lines++;  break;
          case cs_shared:    num_s_cache_lines++;  break;
          case cs_modified:  num_m_cache_lines++;  break;
          default:           num_tr_cache_lines++; break;
        }
      }
    }
    num_cache_lines = num_i_cache_lines + num_e_cache_lines +
                      num_s_cache_lines + num_m_cache_lines + num_tr_cache_lines;

    cout << " L2$ (i,e,s,m,tr) ratio=(" 
      << setiosflags(ios::fixed) << setw(4) << 1000 * num_i_cache_lines  / num_cache_lines << ", "
      << setiosflags(ios::fixed) << setw(4) << 1000 * num_e_cache_lines  / num_cache_lines << ", "
      << setiosflags(ios::fixed) << setw(4) << 1000 * num_s_cache_lines  / num_cache_lines << ", "
      << setiosflags(ios::fixed) << setw(4) << 1000 * num_m_cache_lines  / num_cache_lines << ", "
      << setiosflags(ios::fixed) << setw(4) << 1000 * num_tr_cache_lines / num_cache_lines << "), num_dirty_lines (pid:#) = ";

    for (map<uint64_t, uint64_t>::iterator m_iter = dirty_cl_per_offset.begin(); m_iter != dirty_cl_per_offset.end(); m_iter++)
    {
      cout << m_iter->first << " : " << m_iter->second << " , ";
    }
    cout << endl;
  }
  if (display_life_time == true && num_destroyed_cache_lines > 0)
  {
    cout << "  -- L2$ [" << setw(3) << num << "] : (cache_line_life_time, time_between_last_access_and_cache_destroy) = ("
      << setiosflags(ios::fixed) << setprecision(2)
      << 1.0 * cache_line_life_time / (process_interval * num_destroyed_cache_lines) << ", "
      << setiosflags(ios::fixed) << setprecision(2) 
      << 1.0 * time_between_last_access_and_cache_destroy / (process_interval * num_destroyed_cache_lines)
      << ") L2$ cycles" << endl;
  }
  // free the memory
  for (uint32_t i = 0; i < num_sets; i++)
  {
    for (uint32_t j = 0; j < num_ways; j++)
    {
      delete tags[i][j];
    }
    delete [] tags[i];
  }
  delete [] tags;
  for (uint32_t i = 0; i < num_mshrs; i++)
  {
    delete mshrs[i];
  }
  delete [] mshrs;
}


void CacheL2::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  if (event_time % process_interval != 0)
  {
    event_time += process_interval - event_time%process_interval;
  }
  geq->add_event(event_time, this);
  req_event.insert(pair<uint64_t, LocalQueueElement *>(event_time, local_event));
}


void CacheL2::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  if (event_time % process_interval != 0)
  {
    event_time += process_interval - event_time%process_interval;
  }
  geq->add_event(event_time, this);
  rep_event.insert(pair<uint64_t, LocalQueueElement *>(event_time, local_event));
}


void CacheL2::show_state(uint64_t address)
{
  uint32_t set = (address >> set_lsb) % num_sets;
  uint64_t tag = (address >> set_lsb) / num_sets;
  list< L2Entry >::iterator set_iter;

  //for (set_iter = tags[set].begin(); set_iter != tags[set].end(); ++set_iter)
  for (uint32_t k = 0; k < num_ways; k++)
  {
    L2Entry * set_iter = tags[set][k];
    if (set_iter->type != cs_invalid && set_iter->tag == tag)
    {
      cout << "  -- L2 [" << num << "] : " << set_iter->type
        << ", " << set_iter->type_l1l2; 
      for(std::set<Component *>::iterator iter = set_iter->sharedl1.begin();
          iter != set_iter->sharedl1.end(); ++iter)
      {
        cout << ", (" << (*iter)->type << ", " << (*iter)->num << ") ";
      }
      cout << endl;
      break;
    }
  }
}


uint32_t CacheL2::process_event(uint64_t curr_time)
{
  multimap<uint64_t, LocalQueueElement *>::iterator req_event_iter = req_event.begin();
  multimap<uint64_t, LocalQueueElement *>::iterator rep_event_iter = rep_event.begin();
  //list< L2Entry >::iterator set_iter;
  uint32_t idx = 0;
  L2Entry * set_iter = NULL;

  LocalQueueElement * rep_lqe = NULL;
  LocalQueueElement * req_lqe = NULL;
  // event -> queue
  if (rep_q.empty() == false)
  {
    rep_lqe = rep_q.front();
    rep_q.pop();
  }
  else if (rep_event_iter != rep_event.end() && rep_event_iter->first == curr_time)
  {
    rep_lqe = rep_event_iter->second;
    ++rep_event_iter;
  }

  while (rep_event_iter != rep_event.end() && rep_event_iter->first == curr_time)
  {
    rep_q.push(rep_event_iter->second);
    ++rep_event_iter;
  }
  rep_event.erase(curr_time);

  while (req_event_iter != req_event.end() && req_event_iter->first == curr_time)
  {
    uint32_t bank = (req_event_iter->second->address >> set_lsb) % num_banks;
    req_qs[bank].push(req_event_iter->second);
    ++req_event_iter;
  }
  req_event.erase(curr_time);


  if (rep_lqe != NULL)
  {
#ifdef DEBUG_CACHE
    display_event(curr_time, rep_lqe, "P");
#endif
    // reply events have a higher priority than request events
    uint64_t address = rep_lqe->address;
    uint64_t line_addr = (address >> set_lsb) << set_lsb;
    uint32_t set = (address >> set_lsb) % num_sets;
    uint64_t tag = (address >> set_lsb) / num_sets;
    event_type etype = rep_lqe->type;
    //test_tags(set);

    // look for an entry which already has tag
    //for (set_iter = tags[set].begin(); set_iter != tags[set].end(); ++set_iter)
    for (idx = 0; idx < num_ways; idx++)
    {
      set_iter = tags[set][idx];
      if (set_iter->type == cs_invalid)
      {
        continue;
      }
      else if (set_iter->tag == tag)
      {
        break;
      }
    }

    if (etype == et_write_nd)
    {
      rep_lqe->from.pop();
      if (idx == num_ways || set_iter->type != cs_tr_to_m)
      {
        rep_lqe->type = et_nack;
        if (!geq->is_nuca || (geq->is_nuca && (rep_lqe->from.top())->num == num)) {  // Jiayi, same tile
          (rep_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
        } else {
          crossbar->add_rep_event(curr_time + l2_to_xbar_t, rep_lqe, this); // this(l2) to top(l1)
        }
        LocalQueueElement * lqe = new LocalQueueElement(this, et_e_to_i, address);
        lqe->th_id = rep_lqe->th_id;
        add_event_to_LL(curr_time, lqe, false);
      }
      else
      {
        while (set_iter->sharedl1.empty() == false)
        {
          if ((*(set_iter->sharedl1.begin())) != rep_lqe->from.top())
          {
            LocalQueueElement * lqe = new LocalQueueElement(this, et_evict,
                ((set_iter->tag*num_sets + set) << set_lsb));
            lqe->th_id = rep_lqe->th_id;
            if (!geq->is_nuca || (geq->is_nuca && (*(set_iter->sharedl1.begin()))->num == num)) {  // Jiayi, same tile
              (*(set_iter->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
            } else {// Jiayi, TODO: buggy, from.top() is l2 and 'from' is destination
              crossbar->add_rep_event(curr_time + l2_to_xbar_t, lqe, *(set_iter->sharedl1.begin()));
            }
          }
          set_iter->sharedl1.erase(set_iter->sharedl1.begin());
        }
        set_iter->type      = cs_modified;
        set_iter->type_l1l2 = cs_modified;
        set_iter->tag       = tag;
        set_iter->sharedl1.insert(rep_lqe->from.top());
        set_iter->last_access_time = curr_time;
        for (uint32_t i = idx; i < num_ways-1; i++)
        {
          tags[set][i] = tags[set][i+1];
        }
        tags[set][num_ways-1] = set_iter;
        //tags[set].push_back(*set_iter);
        //tags[set].erase(set_iter);

        rep_lqe->type = et_write;
        if (!geq->is_nuca || (geq->is_nuca && (rep_lqe->from.top())->num == num)) {  // Jiayi, same tile
          (rep_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
        } else {
          crossbar->add_rep_event(curr_time + l2_to_xbar_t, rep_lqe, this);// Jiayi, if top() is l1 and from is l2, l2 to l1
        }

        LocalQueueElement * lqe = new LocalQueueElement(this, et_e_to_m, address);
        lqe->th_id = rep_lqe->th_id;
        add_event_to_LL(curr_time, lqe, false);
      }
      /* Jiayi, deal with MSHRs begin */
#ifdef DEBUG_MSHR
      display(); cout << " MSHR erase line addr: " << hex << line_addr << " for reply [" << rep_lqe << "] " << dec; rep_lqe->display();
#endif
      assert(mshr_indices.find(line_addr) != mshr_indices.end());
      uint32_t mshr_idx = mshr_indices[line_addr];
      assert(mshrs[mshr_idx]->valid);
      assert(mshrs[mshr_idx]->line_addr == line_addr);
      assert(mshrs[mshr_idx]->target == rep_lqe);
      assert(mshrs[mshr_idx]->target_list.empty());
      mshrs[mshr_idx]->release();
      mshr_indices.erase(line_addr);
      /* MSHRs end */
    }
    else if (etype == et_e_rd || etype == et_s_rd || etype == et_write)
    {
      bool bypass = false;
      bool shared = false;
      rep_lqe->from.pop();
      // read miss return traffic
      if (idx == num_ways)
      {
        set_iter = tags[set][0];  //set_iter = tags[set].begin();
        idx      = 0;
        uint64_t set_addr = ((set_iter->tag*num_sets + set) << set_lsb);

        if (set_iter->type == cs_tr_to_s || set_iter->type == cs_tr_to_m || 
            set_iter->type == cs_tr_to_e || set_iter->type == cs_tr_to_i)
        {
          // Jiayi, no space for replacement, nack it.
          // TODO: keep it in mshr, make space for it and drain it
          bypass = true; 
          if (rep_lqe->from.size() > 1)
          {
            rep_lqe->type = et_nack;
            if (!geq->is_nuca || (geq->is_nuca && (rep_lqe->from.top())->num == num)) {  // Jiayi, same tile
              (rep_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
            } else {
              crossbar->add_rep_event(curr_time + l2_to_xbar_t, rep_lqe, this);// Jiayi, if top() is l1 and from is l2, l2 to l1
            }
          }
          LocalQueueElement * lqe = new LocalQueueElement(this, et_evict, rep_lqe->address);
          lqe->th_id = rep_lqe->th_id;
          add_event_to_LL(curr_time, lqe, false);
          if (rep_lqe->from.size() == 1)
          {
            delete rep_lqe;
          }
        }
        else if (set_iter->type != cs_invalid)
        {
          num_ev_capacity++;
          num_destroyed_cache_lines++;
          cache_line_life_time += (curr_time - set_iter->first_access_time);
          time_between_last_access_and_cache_destroy += (curr_time - set_iter->last_access_time);
          set_iter->first_access_time = curr_time;
          set_iter->last_access_time  = curr_time;
          // capacity miss
          while (set_iter->sharedl1.empty() == false)
          {
            LocalQueueElement * lqe = new LocalQueueElement(this, et_evict, set_addr);
            lqe->th_id = rep_lqe->th_id;
            if (!geq->is_nuca || (geq->is_nuca && (*(set_iter->sharedl1.begin()))->num == num)) {  // Jiayi, same tile
              (*(set_iter->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
            } else {  // Jiayi, TODO: buggy, from.top() is l2 and 'from is l1' is destination
              crossbar->add_rep_event(curr_time + l2_to_l1_t, lqe, *(set_iter->sharedl1.begin()));
            }
            set_iter->sharedl1.erase(set_iter->sharedl1.begin());
          }
          // then send eviction event to Directory or Crossbar
          if (set_iter->type_l1l2 != cs_modified)
          {
            LocalQueueElement * lqe = new LocalQueueElement(this, et_evict, set_addr);
            lqe->th_id = rep_lqe->th_id;
            add_event_to_LL(curr_time, lqe, false, set_iter->type == cs_modified);
          }
        }
        else
        {
          set_iter->first_access_time = curr_time;
          set_iter->last_access_time  = curr_time;
        }
      }
      else
      {
        uint64_t set_addr = ((set_iter->tag*num_sets + set) << set_lsb);

        if (etype == et_write)
        {
          while (set_iter->sharedl1.empty() == false)
          {
            if ((*(set_iter->sharedl1.begin())) != rep_lqe->from.top())
            {
              LocalQueueElement * lqe = new LocalQueueElement(this, et_evict, set_addr);
              lqe->th_id = rep_lqe->th_id;
              if (!geq->is_nuca || (geq->is_nuca && (*(set_iter->sharedl1.begin()))->num == num)) {  // Jiayi, same tile
                (*(set_iter->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
              } else {  // Jiayi, TODO: buggy, from.top() is l2 and 'from is l1' is destination
                crossbar->add_rep_event(curr_time + l2_to_l1_t, lqe, *(set_iter->sharedl1.begin()));
              }
            }
            set_iter->sharedl1.erase(set_iter->sharedl1.begin());
          }
        }
        else if (etype == et_e_rd || etype == et_s_rd)
        {
          if (set_iter->type == cs_modified || set_iter->type == cs_tr_to_e)
          {
            bypass = true;  // this event happened earlier, don't change the state of cache
            if (rep_lqe->from.size() > 1)
            {
              rep_lqe->type = et_rd_bypass;
              if (!geq->is_nuca || (geq->is_nuca && (rep_lqe->from.top())->num == num)) {  // Jiayi, same tile
                (rep_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
              } else {
                crossbar->add_rep_event(curr_time + l2_to_xbar_t, rep_lqe, this);// Jiayi, if top() is l1 and from is l2, l2 to l1
              }
            }
            else
            {
              delete rep_lqe;
            }
          }
          else if (etype == et_s_rd)
          {
            shared = true;
          }
        }
      }

      /* Jiayi, deal with MSHRs begin */
#ifdef DEBUG_MSHR
      display(); cout << " MSHR erase line addr: " << hex << line_addr << " for reply [" << rep_lqe << "] " << dec; rep_lqe->display();
#endif
      assert(mshr_indices.find(line_addr) != mshr_indices.end());
      uint32_t mshr_idx = mshr_indices[line_addr];
      assert(mshrs[mshr_idx]->valid);
      assert(mshrs[mshr_idx]->line_addr == line_addr);
      assert(mshrs[mshr_idx]->target == rep_lqe);
      shared |= !mshrs[mshr_idx]->target_list.empty();

      if (idx == num_ways)
      {
        display();  rep_lqe->display();  geq->display();  ASSERTX(0);
      }
      else if (bypass == false)
      {
        set_iter->type      = (etype == et_e_rd) ? cs_exclusive : (etype == et_s_rd) ? cs_shared : cs_modified;
        if (rep_lqe->from.size() > 1)
        {
          set_iter->type_l1l2 = (etype == et_write) ? cs_modified : (shared == true) ? cs_shared : cs_exclusive;
          set_iter->tag       = tag;
          set_iter->sharedl1.insert(rep_lqe->from.top());
        }
        else
        {
          //XXX: Jiayi, what condition will be?
          set_iter->type_l1l2 = cs_invalid;
          set_iter->tag       = tag;
        }
        set_iter->last_access_time = curr_time;
        for (uint32_t i = idx; i < num_ways-1; i++)
        {
          tags[set][i] = tags[set][i+1];
        }
        tags[set][num_ways-1] = set_iter;
        //tags[set].push_back(*set_iter);
        //tags[set].erase(set_iter);

        rep_lqe->type = (etype == et_write) ? et_write : et_read;
        if (rep_lqe->from.size() > 1)
        {
          if (!geq->is_nuca || (geq->is_nuca && (rep_lqe->from.top())->num == num)) {  // Jiayi, same tile
            (rep_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
          } else {
            crossbar->add_rep_event(curr_time + l2_to_xbar_t, rep_lqe, this);// Jiayi, if top() is l1 and from is l2, l2 to l1 in noc
          }
        }
        else
        {
          delete rep_lqe;
        }
        for (list<LocalQueueElement *>::iterator iter = mshrs[mshr_idx]->target_list.begin();
            iter != mshrs[mshr_idx]->target_list.end(); iter++)
        {
          LocalQueueElement *target_lqe = *iter;
          assert(target_lqe->from.size() > 1);
          assert(target_lqe->type == et_read);//TODO: delete to reduce simulation time
          set_iter->sharedl1.insert(target_lqe->from.top()); // insert to sharer list
          if (!geq->is_nuca || (geq->is_nuca && (target_lqe->from.top())->num == num))
          {
            (target_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, target_lqe);
          }
          else
          {
            crossbar->add_rep_event(curr_time + l2_to_xbar_t, target_lqe, this);// if top() is l1 and from is l2, l2 to l1 in noc
          }
        }
      }
      else
      {
        num_bypass++;
        for (list<LocalQueueElement *>::iterator iter = mshrs[mshr_idx]->target_list.begin();
            iter != mshrs[mshr_idx]->target_list.end(); iter++)
        {
          LocalQueueElement *target_lqe = *iter;
          assert(target_lqe->from.size() > 1);
          assert(target_lqe->type == et_read);//TODO: delete to reduce simulation time
          target_lqe->type = rep_lqe->type;
          if (!geq->is_nuca || (geq->is_nuca && (target_lqe->from.top())->num == num))
          {
            (target_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, target_lqe);
          }
          else
          {
            crossbar->add_rep_event(curr_time + l2_to_xbar_t, target_lqe, this);// if top() is l1 and from is l2, l2 to l1 in noc
          }
        }
      }
      mshrs[mshr_idx]->release();
      mshr_indices.erase(line_addr);
      /* MSHRs end */
    }
    else if (etype == et_m_to_s || etype == et_m_to_m)  // back from L1, coherence, Jiayi
    {
      rep_lqe->from.pop();
      num_coherency_access++;

      if (idx != num_ways && set_iter->type == cs_tr_to_i && set_iter->pending != NULL)
      {
        num_ev_coherency++;
        switch (set_iter->type_l1l2)
        {
          case cs_tr_to_i:
            delete rep_lqe;
            break;
          case cs_tr_to_m:
            rep_lqe->type = et_nack;
            if (!geq->is_nuca || (geq->is_nuca && (rep_lqe->from.top())->num == num)) {  // Jiayi, same tile
              rep_lqe->from.top()->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
            } else {
              crossbar->add_rep_event(curr_time + l2_to_xbar_t, rep_lqe, this);// Jiayi, if top() is l1 and from is l2, l2 to l1 in noc
            }
            break;
          default: // set_iter->type_l1l2 == cs_tr_to_s
            rep_lqe->type = et_nack;
            if (!geq->is_nuca || (geq->is_nuca && (rep_lqe->from.top())->num == num)) {  // Jiayi, same tile
              rep_lqe->from.top()->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
            } else {
              crossbar->add_rep_event(curr_time + l2_to_xbar_t, rep_lqe, this);// Jiayi, if top() is l1 and from is l2, l2 to l1 in noc
            }
            set_iter->sharedl1.insert(rep_lqe->from.top());
            while (set_iter->sharedl1.empty() == false)
            {
              LocalQueueElement * lqe = new LocalQueueElement(this, et_evict,
                  ((set_iter->tag*num_sets + set) << set_lsb));
              lqe->th_id = rep_lqe->th_id;
              if (!geq->is_nuca || (geq->is_nuca && (*(set_iter->sharedl1.begin()))->num == num)) {  // Jiayi, same tile
                (*(set_iter->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
              } else {  // Jiayi, TODO: buggy, from.top() is l2 and 'from is l1' is destination
                crossbar->add_rep_event(curr_time + l2_to_l1_t, lqe, *(set_iter->sharedl1.begin()));
              }
              set_iter->sharedl1.erase(set_iter->sharedl1.begin());
            }
            break;
        }

        add_event_to_LL(curr_time, set_iter->pending, true, true);
        num_destroyed_cache_lines++;
        cache_line_life_time += (curr_time - set_iter->first_access_time);
        time_between_last_access_and_cache_destroy += (curr_time - set_iter->last_access_time);
        set_iter->pending = NULL;
        set_iter->type_l1l2 = cs_invalid;
        set_iter->type      = cs_invalid;
        for (uint32_t i = idx; i < num_ways-1; i++)
        {
          tags[set][i] = tags[set][i+1];
        }
        tags[set][num_ways-1] = set_iter;
        //tags[set].push_back(*set_iter);
        //tags[set].erase(set_iter);
      }
      else if (idx != num_ways && (set_iter->type_l1l2 == cs_tr_to_m || set_iter->type_l1l2 == cs_tr_to_s))
      {
        num_ev_coherency++;
        set_iter->last_access_time = curr_time;
        set_iter->type_l1l2 = (set_iter->type_l1l2 == cs_tr_to_s) ? cs_shared : 
          (set_iter->pending == NULL) ? cs_modified : cs_invalid;
        set_iter->sharedl1.insert(rep_lqe->from.top());
        rep_lqe->type = (etype == et_m_to_s) ? et_read : 
          (set_iter->pending == NULL) ? et_write : et_nack;
        if (!geq->is_nuca || (geq->is_nuca && (rep_lqe->from.top())->num == num)) {  // Jiayi, same tile
          rep_lqe->from.top()->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
        } else {
          crossbar->add_rep_event(curr_time + l2_to_xbar_t, rep_lqe, this);// Jiayi, if top() is l1 and from is l2, l2 to l1 in noc
        }
        if (set_iter->pending != NULL)
        {
          add_event_to_LL(curr_time, set_iter->pending, true, true);
          set_iter->pending = NULL;
          set_iter->type    = cs_shared;
        }
        for (uint32_t i = idx; i < num_ways-1; i++)
        {
          tags[set][i] = tags[set][i+1];
        }
        tags[set][num_ways-1] = set_iter;
        //tags[set].push_back(*set_iter);
        //tags[set].erase(set_iter);
      }
      else
      {
        show_state(rep_lqe->address);
        rep_lqe->from.top()->show_state(rep_lqe->address);
        rep_lqe->display();  geq->display();  ASSERTX(0);
      }
    }
    else if (etype == et_evict || etype == et_evict_nd)
    {
      num_ev_from_l1++;
      if (idx != num_ways)
      {
        set_iter->last_access_time = curr_time;
        if (set_iter->type == cs_tr_to_s)
        {
          num_coherency_access++;
          set_iter->type = cs_shared;
          add_event_to_LL(curr_time, set_iter->pending, true, true);
          set_iter->pending = NULL;
          set_iter->sharedl1.insert(rep_lqe->from.top());
        }
        else
        {
          // cache line is evicted from L1
          set_iter->sharedl1.erase(rep_lqe->from.top());
          if (set_iter->sharedl1.empty() == true && set_iter->type_l1l2 != cs_tr_to_s &&
              set_iter->type_l1l2 != cs_tr_to_m && set_iter->type_l1l2 != cs_tr_to_i)
          {
            set_iter->type_l1l2 = cs_invalid;
          }
          for (uint32_t i = idx; i < num_ways-1; i++)
          {
            tags[set][i] = tags[set][i+1];
          }
          tags[set][num_ways-1] = set_iter;
          //tags[set].push_back(*set_iter);
          //tags[set].erase(set_iter);
        }
        delete rep_lqe;
      }
      else
      {
        num_ev_from_l1_miss++;
        if (etype == et_evict && always_hit == false)
        {
          rep_lqe->from.push(this);
          add_event_to_LL(curr_time, rep_lqe, true, true);
        }
        else
        {
          delete rep_lqe;
        }
      }
    }
    else if (etype == et_dir_rd)
    {
      num_coherency_access++;
      if (idx == num_ways)
      {
        rep_lqe->type = et_dir_rd_nd;
        // cache line is already evicted -- return now
        add_event_to_LL(curr_time, rep_lqe, false);
      }
      else
      {
        if (set_iter->type != cs_modified)
        {
          cout << "type = " << set_iter->type << ", type_l1l2 = " << set_iter->type_l1l2 << endl;
          show_state(rep_lqe->address);
          rep_lqe->from.top()->show_state(rep_lqe->address);
          rep_lqe->display();  geq->display();  ASSERTX(0);
        }
        else if (set_iter->type_l1l2 == cs_invalid || 
            set_iter->type_l1l2 == cs_exclusive ||
            set_iter->type_l1l2 == cs_shared)
        {
          set_iter->type      = cs_shared;
          set_iter->last_access_time = curr_time;
          add_event_to_LL(curr_time, rep_lqe, true, true);
        }
        else if (set_iter->type_l1l2 == cs_modified)
        {
          if (set_iter->sharedl1.size() != 1)
          {
            cout << "sharedl1.size() = " << set_iter->sharedl1.size() << endl;
            display();  rep_lqe->display();  geq->display();  ASSERTX(0);
          }
          // special case: data is in L1
          set_iter->last_access_time = curr_time;
          set_iter->type_l1l2 = cs_exclusive;
          set_iter->type      = cs_tr_to_s;
          set_iter->pending   = rep_lqe;
          LocalQueueElement * lqe = new LocalQueueElement(this, et_dir_rd,
              ((set_iter->tag*num_sets + set) << set_lsb));
          lqe->th_id = rep_lqe->th_id;
          if (!geq->is_nuca || (geq->is_nuca && (*(set_iter->sharedl1.begin()))->num == num)) {  // Jiayi, same tile
            (*(set_iter->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
          } else {  // Jiayi, TODO: buggy, from.top() is l2 and 'from is l1' is destination
            crossbar->add_rep_event(curr_time + l2_to_l1_t, lqe, *(set_iter->sharedl1.begin()));
          }
          set_iter->sharedl1.erase(set_iter->sharedl1.begin());
        }
        else if (set_iter->type_l1l2 == cs_tr_to_m || set_iter->type_l1l2 == cs_tr_to_s)
        {
          if (set_iter->pending != NULL)
          {
            set_iter->pending->display();
            display();  rep_lqe->display();  geq->display();  ASSERTX(0);
          }
          set_iter->last_access_time = curr_time;
          set_iter->pending          = rep_lqe;
        }
        else
        {
          // DIR->L2->L1->L2->DIR traffic -- not implemented yet
          cout << "type_l1l2 = " << set_iter->type_l1l2 << endl;
          display();  rep_lqe->display();  geq->display();  ASSERTX(0);
        }
      }
    }
    else if (etype == et_nack)
    {
      num_nack++;
      num_bypass++;
      rep_lqe->from.pop();
      if (rep_lqe->from.size() > 1)
      {
        /* Jiayi, deal with MSHRs begin */
#ifdef DEBUG_MSHR
        display(); cout << " MSHR erase line addr: " << hex << line_addr << " for reply [" << rep_lqe << dec << "] at tick " << curr_time << " "; rep_lqe->display();
#endif
        assert(mshr_indices.find(line_addr) != mshr_indices.end());
        uint32_t mshr_idx = mshr_indices[line_addr];
        assert(mshrs[mshr_idx]->valid);
        assert(mshrs[mshr_idx]->line_addr == line_addr);
        assert(mshrs[mshr_idx]->target == rep_lqe);
        for (list<LocalQueueElement *>::iterator iter = mshrs[mshr_idx]->target_list.begin();
            iter != mshrs[mshr_idx]->target_list.end(); iter++)
        {
          LocalQueueElement *target_lqe = *iter;
          assert(target_lqe->from.size() > 1);
          assert(target_lqe->type == et_read);//TODO: delete to reduce simulation time
          target_lqe->type = et_nack;
          if (!geq->is_nuca || (geq->is_nuca && (target_lqe->from.top())->num == num))
          {
            (target_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, target_lqe);
          }
          else
          {
            crossbar->add_rep_event(curr_time + l2_to_xbar_t, target_lqe, this);// if top() is l1 and from is l2, l2 to l1 in noc
          }
        }
        mshrs[mshr_idx]->release();
        mshr_indices.erase(line_addr);
        /* MSHRs end */
        if (!geq->is_nuca || (geq->is_nuca && (rep_lqe->from.top())->num == num)) {  // Jiayi, same tile
          (rep_lqe->from.top())->add_rep_event(curr_time + l2_to_l1_t, rep_lqe);
        } else {
          crossbar->add_rep_event(curr_time + l2_to_xbar_t, rep_lqe, this);// Jiayi, if top() is l1 and from is l2, l2 to l1 in noc
        }
      }
      else
      {
        delete rep_lqe;
      }
    }
    else if (etype == et_e_to_s || etype == et_s_to_s)
    {
      num_coherency_access++;

      if (idx != num_ways)
      {
        if (set_iter->type != cs_exclusive && set_iter->type != cs_shared && set_iter->type != cs_tr_to_m)
        {
          cout << "address = 0x" << hex << address << dec << endl;
          cout << "[" << curr_time << "]  sharedl1.size() = " << set_iter->sharedl1.size() 
            << " " << set_iter->type << endl;  
          display();  rep_lqe->display();  geq->display();  ASSERTX(0);
        }
        set_iter->last_access_time = curr_time;
        set_iter->type = cs_shared;
        // return to directory
        add_event_to_LL(curr_time, rep_lqe, true, true);
      }
      else
      {
        // the cache line is evicted already so that we can't get data from this L2
        rep_lqe->type = (etype == et_e_to_s) ? et_e_to_s_nd : et_s_to_s_nd;
        // return to directory
        add_event_to_LL(curr_time, rep_lqe, false);
      }
    }
    else if (etype == et_invalidate || etype == et_invalidate_nd)
    {
      bool enter_intermediate_state = false;

      num_coherency_access++;
      if (idx != num_ways)
      {
        if (set_iter->type == cs_tr_to_s || //set_iter->type == cs_tr_to_m ||
            set_iter->type == cs_tr_to_e || set_iter->type == cs_tr_to_i)
        {
          show_state(rep_lqe->address);
          cout << "etype = " << etype << endl;
          display();  rep_lqe->display();  geq->display();  ASSERTX(0);
        }
        else if (set_iter->type == cs_modified && set_iter->type_l1l2 == cs_modified)
        {
          enter_intermediate_state = true;
          if (set_iter->sharedl1.size() != 1)
          {
            cout << "sharedl1.size() = " << set_iter->sharedl1.size() << endl;
            display();  rep_lqe->display();  geq->display();  ASSERTX(0);
          }
          // special case: data is in L1
          set_iter->last_access_time = curr_time;
          set_iter->type_l1l2 = cs_tr_to_i;
          set_iter->type      = cs_tr_to_i;
          set_iter->pending   = rep_lqe;
          LocalQueueElement * lqe = new LocalQueueElement(this, et_m_to_m,
              ((set_iter->tag*num_sets + set) << set_lsb));
          lqe->th_id = rep_lqe->th_id;
          if (!geq->is_nuca || (geq->is_nuca && (*(set_iter->sharedl1.begin()))->num == num)) {  // Jiayi, same tile
            (*(set_iter->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
          } else {  // Jiayi, TODO: buggy, from.top() is l2 and 'from is l1' is destination
            crossbar->add_rep_event(curr_time + l2_to_l1_t, lqe, *(set_iter->sharedl1.begin()));
          }
          set_iter->sharedl1.erase(set_iter->sharedl1.begin());
        }
        else if (set_iter->type == cs_modified &&
            (set_iter->type_l1l2 == cs_tr_to_m || set_iter->type_l1l2 == cs_tr_to_s))
        {
          enter_intermediate_state = true;
          if (set_iter->pending != NULL)
          {
            set_iter->pending->display();
            display();  rep_lqe->display();  geq->display();  ASSERTX(0);
          }
          set_iter->last_access_time = curr_time;
          set_iter->type      = cs_tr_to_i;
          set_iter->pending   = rep_lqe;
        }
        else
        {
          // evict the corresponding cache lines in L1 and L2 and return
          while (set_iter->sharedl1.empty() == false)
          {
            LocalQueueElement * lqe = new LocalQueueElement(this, et_evict,
                ((set_iter->tag*num_sets + set) << set_lsb));
            lqe->th_id = rep_lqe->th_id;
            if (!geq->is_nuca || (geq->is_nuca && (*(set_iter->sharedl1.begin()))->num == num)) {  // Jiayi, same tile
              (*(set_iter->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
            } else {  // Jiayi, TODO: buggy, from.top() is l2 and 'from is l1' is destination
              crossbar->add_rep_event(curr_time + l2_to_l1_t, lqe, *(set_iter->sharedl1.begin()));
            }
            set_iter->sharedl1.erase(set_iter->sharedl1.begin());
          }

          num_ev_coherency++;
          num_destroyed_cache_lines++;
          cache_line_life_time += (curr_time - set_iter->first_access_time);
          time_between_last_access_and_cache_destroy += (curr_time - set_iter->last_access_time);
          set_iter->type      = cs_invalid;
          set_iter->type_l1l2 = cs_invalid;
        }
      }
      else
      {
        rep_lqe->type = et_invalidate_nd;
      }

      if (enter_intermediate_state == false)
      {
        // return to directory
        add_event_to_LL(curr_time, rep_lqe, true, rep_lqe->type == et_invalidate);
      }
    }
    else if (etype == et_nop)
    {
      delete rep_lqe;
    }
    else
    {
      cout << "etype = " << etype << endl;
      display();  rep_lqe->display();  geq->display();  ASSERTX(0);
    }
  }
  else
  {
    bool any_request = false;

    for (uint32_t i = 0; i < num_banks; i++)
    {
      if (req_qs[i].empty() == true)
      {
        continue;
      }
      any_request = true;

      req_lqe = req_qs[i].front();
      req_qs[i].pop();

#ifdef DEBUG_CACHE
      display_event(curr_time, req_lqe, "Q");
#endif
      // process the first request event
      uint64_t address = req_lqe->address;
      uint64_t line_addr = (address >> set_lsb) << set_lsb;
      uint32_t set = (address >> set_lsb) % num_sets;
      uint64_t tag = (address >> set_lsb) / num_sets;
      event_type etype = req_lqe->type;
      bool is_coherence_miss = false;
      //test_tags(set);

      bool hit = always_hit;
      bool enter_intermediate_state = false;

      if (etype == et_read)
      {
        // see if cache hits
        num_rd_access++;

        //for (set_iter = tags[set].begin(); set_iter != tags[set].end(); ++set_iter)
        for (idx = 0; idx < num_ways; idx++)
        {
          set_iter = tags[set][idx];
          if (set_iter->type == cs_invalid || set_iter->type == cs_tr_to_e)
          {
            continue;
          }
          if (set_iter->tag == tag && req_lqe->from.size() == 1)
          {
            hit = true;
          }
          else if (set_iter->tag == tag)
          {
            if (set_iter->type_l1l2 == cs_invalid &&
                (set_iter->type == cs_exclusive || set_iter->type == cs_shared || set_iter->type == cs_modified))
            {
              // cache hit, and type_l1l2 will be cs_exclusive
              set_iter->type_l1l2 = cs_exclusive;
              assert(set_iter->sharedl1.empty()); // Jiayi
              set_iter->sharedl1.insert(req_lqe->from.top());
            }
            else if (set_iter->type_l1l2 == cs_exclusive &&
                (set_iter->type == cs_exclusive || set_iter->type == cs_shared || set_iter->type == cs_modified))
            {
              // cache hit, and type_l1l2 will be cs_exclusive or cs_shared
              set_iter->sharedl1.insert(req_lqe->from.top());
              if (set_iter->sharedl1.size() > 1)
              {
                set_iter->type_l1l2 = cs_shared;
              }
            }
            else if (set_iter->type_l1l2 == cs_shared &&
                (set_iter->type == cs_exclusive || set_iter->type == cs_shared || set_iter->type == cs_modified))
            {
              // cache hit, and type_l1l2 will be cs_shared
              set_iter->sharedl1.insert(req_lqe->from.top());
            }
            else if (set_iter->type_l1l2 == cs_modified && set_iter->type == cs_modified)
            {
              // cache hit, and type_l1l2 will be cs_shared, m_to_s event request will be delivered to L1
              if (set_iter->sharedl1.size() > 1)
              {
                cout << "[" << curr_time << "]  sharedl1.size() = " << set_iter->sharedl1.size() << endl;
                cout << "type = " << set_iter->type << ", type_l1l2 = " << set_iter->type_l1l2 << endl;
                display();  req_lqe->display();  geq->display();  ASSERTX(0);
              }

              if (set_iter->sharedl1.size() == 1)
              {
                if (*(set_iter->sharedl1.begin()) != req_lqe->from.top())
                {
                  enter_intermediate_state = true;
                  req_lqe->from.push(this);
                  req_lqe->type = et_m_to_s;
                  if (!geq->is_nuca || (geq->is_nuca && (*(set_iter->sharedl1.begin()))->num == num)) {  // Jiayi, 06/06/17
                    (*(set_iter->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, req_lqe);
                  } else {  // Jiayi, TODO: buggy, from.top() is l2 and 'from is l1' is destination
                    crossbar->add_rep_event(curr_time + l2_to_xbar_t, req_lqe, *(set_iter->sharedl1.begin()));// might want to use add_req in noc
                  }
                  set_iter->type_l1l2 = cs_tr_to_s;
                  set_iter->sharedl1.erase(set_iter->sharedl1.begin());
                }
              }
              else
              {
                set_iter->type_l1l2 = cs_shared;
                set_iter->sharedl1.insert(req_lqe->from.top());
              }
            }
            else if ((set_iter->type_l1l2 == cs_exclusive && set_iter->type == cs_tr_to_s) ||
                set_iter->type_l1l2 == cs_tr_to_s || set_iter->type_l1l2 == cs_tr_to_m ||
                set_iter->type_l1l2 == cs_tr_to_i || set_iter->type == cs_tr_to_m)
            {
              req_lqe->type = et_nack;
            }
            else
            {
              cout << "type = " << set_iter->type << ", type_l1l2 = " << set_iter->type_l1l2 << endl;
              display();  req_lqe->display();  geq->display();  ASSERTX(0);
            }

            set_iter->last_access_time = curr_time;
            hit = true;
            for (uint32_t i = idx; i < num_ways-1; i++)
            {
              tags[set][i] = tags[set][i+1];
            }
            tags[set][num_ways-1] = set_iter;
            //tags[set].push_back(*set_iter);
            //tags[set].erase(set_iter);
            break;
          }
        }
      }
      else if (etype == et_write)
      {
        num_wr_access++;

        //for (set_iter = tags[set].begin(); set_iter != tags[set].end(); ++set_iter)
        for (idx = 0; idx < num_ways; idx++)
        {
          set_iter = tags[set][idx];
          if (set_iter->type == cs_exclusive || set_iter->type == cs_shared)
          {
            if (set_iter->tag == tag)
            {
              if (set_iter->type == cs_exclusive && set_iter->sharedl1.size() == 1 &&
                  (*(set_iter->sharedl1.begin()) == req_lqe->from.top()))
              {
                set_iter->last_access_time = curr_time;
                set_iter->type = cs_tr_to_m;
              }
              else
              {
                while (set_iter->sharedl1.empty() == false)
                {
                  if ((*(set_iter->sharedl1.begin())) != req_lqe->from.top())
                  {
                    LocalQueueElement * lqe = new LocalQueueElement(this, et_evict,
                        ((set_iter->tag*num_sets + set) << set_lsb));
                    lqe->th_id = req_lqe->th_id;
                    if (!geq->is_nuca || (geq->is_nuca && (*(set_iter->sharedl1.begin()))->num == num)) { // Jiayi, in the same tile
                      (*(set_iter->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
                    } else {  // Jiayi, TODO: buggy, from.top() is l2 and 'from is l1' is destination
                      crossbar->add_rep_event(curr_time + l2_to_xbar_t, lqe, *(set_iter->sharedl1.begin()));
                    }
                  }
                  set_iter->sharedl1.erase(set_iter->sharedl1.begin());
                  set_iter->last_access_time = curr_time;
                  set_iter->type = cs_invalid;
                }
              }
              num_upgrade_req++;
              is_coherence_miss = true;
              break;
            }
          }
          else if (set_iter->type == cs_invalid || set_iter->type == cs_tr_to_e)
          {
            continue;
          }
          else if (set_iter->tag == tag)
          {
            if (set_iter->type == cs_modified && set_iter->type_l1l2 == cs_invalid)
            {
              // cache hit, and type_l1l2 will be cs_modified
            }
            else if (set_iter->type == cs_modified && set_iter->type_l1l2 == cs_modified)
            {
              // cache hit, and type_l1l2 will be cs_modified
              if (set_iter->sharedl1.size() != 1)
              {
                cout << "[" << curr_time << "]  sharedl1.size() = " << set_iter->sharedl1.size() << endl;
                cout << "type = " << set_iter->type << ", type_l1l2 = " << set_iter->type_l1l2 << endl;
                display();  req_lqe->display();  geq->display();  ASSERTX(0);
              }
              if (*(set_iter->sharedl1.begin()) != req_lqe->from.top())
              {
                enter_intermediate_state = true;
                req_lqe->from.push(this);
                req_lqe->type = et_m_to_m;
                if (!geq->is_nuca || (geq->is_nuca && (*(set_iter->sharedl1.begin()))->num == num)) { // Jiayi, in the same tile
                  (*(set_iter->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, req_lqe);
                } else {  // Jiayi, TODO: buggy, from.top() is l2 and 'from is l1' is destination
                  crossbar->add_rep_event(curr_time + l2_to_xbar_t, req_lqe, *(set_iter->sharedl1.begin()));
                }
                set_iter->type_l1l2 = cs_tr_to_m;
                set_iter->sharedl1.erase(set_iter->sharedl1.begin());
                set_iter->last_access_time = curr_time;
                hit = true;
                for (uint32_t i = idx; i < num_ways-1; i++)
                {
                  tags[set][i] = tags[set][i+1];
                }
                tags[set][num_ways-1] = set_iter;
                //tags[set].push_back(*set_iter);
                //tags[set].erase(set_iter);
                break;
              }
            }
            else if (set_iter->type == cs_modified && set_iter->type_l1l2 == cs_exclusive)
            {
              // cache hit, and type_l1l2 will be cs_modified
              if (set_iter->sharedl1.size() > 1)
              {
                cout << "[" << curr_time << "]  sharedl1.size() = " << set_iter->sharedl1.size() << endl;
                cout << "type = " << set_iter->type << ", type_l1l2 = " << set_iter->type_l1l2 << endl;
                display();  req_lqe->display();  geq->display();  ASSERTX(0);
              }

              if (set_iter->sharedl1.empty() == false && (*(set_iter->sharedl1.begin())) != req_lqe->from.top())
              {
                LocalQueueElement * lqe = new LocalQueueElement(this, et_evict,
                    ((set_iter->tag*num_sets + set) << set_lsb));
                lqe->th_id = req_lqe->th_id;
                if (!geq->is_nuca || (geq->is_nuca && (*(set_iter->sharedl1.begin()))->num == num)) { // Jiayi, in the same tile
                  (*(set_iter->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
                } else {  // Jiayi, TODO: buggy, from.top() is l2 and 'from is l1' is destination
                  crossbar->add_rep_event(curr_time + l2_to_xbar_t, lqe, *(set_iter->sharedl1.begin()));
                }
                set_iter->sharedl1.erase(set_iter->sharedl1.begin());
              }
            }
            else if (set_iter->type == cs_modified && set_iter->type_l1l2 == cs_shared)
            {
              // cache hit, and type_l1l2 will be cs_modified
              while (set_iter->sharedl1.empty() == false)
              {
                if ((*(set_iter->sharedl1.begin())) != req_lqe->from.top())
                {
                  LocalQueueElement * lqe = new LocalQueueElement(this, et_evict,
                      ((set_iter->tag*num_sets + set) << set_lsb));
                  lqe->th_id = req_lqe->th_id;
                  if (!geq->is_nuca || (geq->is_nuca && (*(set_iter->sharedl1.begin()))->num == num)) { // Jiayi, in the same tile
                    (*(set_iter->sharedl1.begin()))->add_rep_event(curr_time + l2_to_l1_t, lqe);
                  } else {  // Jiayi, TODO: buggy, from.top() is l2 and 'from is l1' is destination
                    crossbar->add_rep_event(curr_time + l2_to_xbar_t, lqe, *(set_iter->sharedl1.begin()));
                  }
                }
                set_iter->sharedl1.erase(set_iter->sharedl1.begin());
              }
            }
            else if ((set_iter->type_l1l2 == cs_exclusive && set_iter->type == cs_tr_to_s) ||
                set_iter->type_l1l2 == cs_tr_to_s || set_iter->type_l1l2 == cs_tr_to_m ||
                set_iter->type_l1l2 == cs_tr_to_i || set_iter->type == cs_tr_to_m)
            {
              set_iter->last_access_time = curr_time;
              req_lqe->type = et_nack;
              hit = true;
              for (uint32_t i = idx; i < num_ways-1; i++)
              {
                tags[set][i] = tags[set][i+1];
              }
              tags[set][num_ways-1] = set_iter;
              //tags[set].push_back(*set_iter);
              //tags[set].erase(set_iter);
              break;
            }
            else
            {
              cout << "type = " << set_iter->type << ", type_l1l2 = " << set_iter->type_l1l2 << endl;
              display();  req_lqe->display();  geq->display();  ASSERTX(0);
            }

            set_iter->last_access_time = curr_time;
            set_iter->type_l1l2 = cs_modified;
            set_iter->sharedl1.insert(req_lqe->from.top());
            hit = true;
            for (uint32_t i = idx; i < num_ways-1; i++)
            {
              tags[set][i] = tags[set][i+1];
            }
            tags[set][num_ways-1] = set_iter;
            //tags[set].push_back(*set_iter);
            //tags[set].erase(set_iter);
            break;
          }
        }
      }
      else
      {
        cout << "etype = " << etype << endl;
        req_lqe->display();  geq->display();  ASSERTX(0);
      }

      if (enter_intermediate_state == false) // real miss, not coherence miss
      {
        if (hit == false)
        {
          // Jiayi, MSHRs
          if (mshr_indices.find(line_addr) != mshr_indices.end())
          {
            uint32_t mshr_idx = mshr_indices[line_addr];
            if (req_lqe->type == et_read && mshrs[mshr_idx]->etype == et_read)
            {
#ifdef DEBUG_MSHR
              display(); cout << " MSHR line addr " << hex << line_addr << " matched and is read, add to target list for request [" << req_lqe << "]" << dec; req_lqe->display();
#endif
              mshrs[mshr_idx]->add(req_lqe);
            }
            else
            {
#ifdef DEBUG_MSHR
              display(); cout << " MSHR line addr " << hex << line_addr << " read/write conflict, nack request [" << req_lqe << "]" << dec; req_lqe->display();
#endif
              req_lqe->type = et_nack;
              if (req_lqe->from.size() > 1)
              {
                if (geq->is_nuca && req_lqe->from.top()->num == num) {
                  req_lqe->from.top()->add_rep_event(curr_time + l2_to_l1_t, req_lqe);
                } else {
                  crossbar->add_rep_event(curr_time + l2_to_xbar_t, req_lqe, this);// to l1 (from.top()) and from is L2, l2 to l1
                }
              }
              else
              {
                display();  req_lqe->display();  geq->display();  ASSERTX(0);
              }
            }
          }
          else if (mshr_indices.size() == num_mshrs)
          {
#ifdef DEBUG_MSHR
            display(); cout << " MSHR full, no entry for line " << hex << line_addr << ", nack request [" << req_lqe << dec << "] at time " << curr_time << " "; req_lqe->display();
#endif
            req_lqe->type = et_nack;
            if (req_lqe->from.size() > 1)
            {
              if (geq->is_nuca && req_lqe->from.top()->num == num) {
                req_lqe->from.top()->add_rep_event(curr_time + l2_to_l1_t, req_lqe);
              } else {
                crossbar->add_rep_event(curr_time + l2_to_xbar_t, req_lqe, this);// to l1 (from.top()) and from is L2, l2 to l1
              }
            }
            else
            {
              display();  req_lqe->display();  geq->display();  ASSERTX(0);
            }
          }
          else
          {
#ifdef DEBUG_MSHR
            display(); cout << " MSHR insert line addr " << hex << line_addr << " for request [" << req_lqe << dec << "] "; req_lqe->display();
#endif
            uint32_t mshr_idx = num_mshrs;
            for (mshr_idx = 0; mshr_idx < num_mshrs; mshr_idx++)
            {
              if (mshrs[mshr_idx]->valid == false) break;
            }
            assert(mshr_idx < num_mshrs);
            mshr_indices.insert(make_pair(line_addr, mshr_idx));
            mshrs[mshr_idx]->allocate(line_addr, req_lqe, etype);
            req_lqe->from.push(this);
            if (directory && ( (geq->is_nuca && geq->which_mc(address) == directory->num) ||
                  (geq->is_asymmetric == false && geq->which_mc(address) == directory->num) ))
            {
              directory->add_req_event(curr_time + l2_to_dir_t, req_lqe);
            }
            else
            {
              crossbar->add_req_event(curr_time + l2_to_xbar_t, req_lqe, this);// Jiayi, from.top() and from is L2, to dir
            }
          }

          if (is_coherence_miss == false)
          {
            (etype == et_write) ? num_wr_miss++ : num_rd_miss++;
          }
        }
        else if (req_lqe->from.size() > 1)
        {
          if (!geq->is_nuca || (geq->is_nuca && req_lqe->from.top()->num == num)) {
            req_lqe->from.top()->add_rep_event(curr_time + l2_to_l1_t, req_lqe);
          } else {
            crossbar->add_rep_event(curr_time + l2_to_xbar_t, req_lqe, this);// to l1 (from.top()) and from is L2, l2 to l1
          }
        }
      }
    }

    if (any_request == false)
    {
      req_event_iter->second->display();  geq->display();  ASSERTX(0);
    }
  }

  if (rep_q.empty() == false)
  {
    geq->add_event(curr_time + process_interval, this);
  }
  else
  {
    for (uint32_t i = 0; i < num_banks; i++)
    {
      if (req_qs[i].empty() == false)
      {
        geq->add_event(curr_time + process_interval, this);
        break;
      }
    }
  }

  return 0;
}


void CacheL2::add_event_to_LL(
    uint64_t curr_time,
    LocalQueueElement * lqe,
    bool check_top,
    bool is_data)
{
  /*if (geq->is_asymmetric == false && ((check_top == true  && lqe->from.top() == directory) ||
        (check_top == false && geq->which_mc(lqe->address) == directory->num)))*/
  if ((geq->is_nuca || geq->is_asymmetric == false) && ((check_top == true && lqe->from.top() == directory) ||
      (check_top == false && directory && geq->which_mc(lqe->address) == directory->num)))  // Jiayi, 06/07/17
  {
    directory->add_rep_event(curr_time + l2_to_dir_t, lqe);
  }
  else
  {
    if (is_data)
    {
      crossbar->add_rep_event(curr_time+l2_to_xbar_t, lqe, num_flits_per_packet, this);
    }
    else
    {
      crossbar->add_rep_event(curr_time+l2_to_xbar_t, lqe, this);
    }
  }
}


void CacheL2::test_tags(uint32_t set)
{
  std::set<uint64_t> tag_set;
  for (uint32_t k = 0; k < num_ways; k++)
  {
    L2Entry * iter = tags[set][k];

    if (iter->type != cs_invalid)
    {
      if (tag_set.find(iter->tag) != tag_set.end())
      {
        for (uint32_t kk = 0; kk < num_ways; kk++)
        {
          cout << tags[set][kk]->type << tags[set][kk]->tag << ", ";
        }
        cout << endl;
        ASSERTX(0);
      }
      tag_set.insert(iter->tag);
    }
  }
}
