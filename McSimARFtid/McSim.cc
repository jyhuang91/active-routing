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

#include "McSim.h"
#include "PTSCache.h"
#include "PTSComponent.h"
#include "PTSCore.h"
#include "PTSO3Core.h"
#include "PTSTLB.h"
#include "PTSXbar.h"
#include "PTSDirectory.h"
#include "PTSRBoL.h"
#include "PTSMemoryController.h"
#include "PTSHMCController.h"//pritam added
#include <iomanip>
#include <fstream>
#include <assert.h>
#include <string>
#include <sstream>

extern "C" {
#include "xed-category-enum.h"
}

using namespace PinPthread;


ostream& operator<<(ostream & output, component_type ct)
{
  switch (ct)
  {
    case ct_core:      output << "ct_core"; break;
    case ct_lsu:       output << "ct_lsu"; break;
    case ct_o3core:    output << "ct_o3core"; break;
    case ct_o3core_t1:    output << "ct_o3core_t1"; break;
    case ct_o3core_t2:    output << "ct_o3core_t2"; break;
    case ct_cachel1d:  output << "ct_l1d$"; break;
    case ct_cachel1d_t1:  output << "ct_l1d$_t1"; break;
    case ct_cachel1d_t2:  output << "ct_l1d$_t2"; break;
    case ct_cachel1i:  output << "ct_l1i$"; break;
    case ct_cachel1i_t1:  output << "ct_l1i$_t1"; break;
    case ct_cachel1i_t2:  output << "ct_l1i$_t2"; break;
    case ct_cachel2:   output << "ct_l2$"; break;
    case ct_cachel2_t1:output << "ct_l2$_t1"; break;
    case ct_cachel2_t2:output << "ct_l2$_t2"; break;
    case ct_directory: output << "ct_dir"; break;
    case ct_crossbar:  output << "ct_xbar"; break;
    case ct_rbol:      output << "ct_rbol"; break;
    case ct_memory_controller: output << "ct_mc"; break;
    case ct_hmc_controller: output << "ct_hmc"; break;//pritam added
    case ct_tlbl1d:    output << "ct_tlbl1d"; break;
    case ct_tlbl1i:    output << "ct_tlbl1i"; break;
    case ct_tlbl2:     output << "ct_tlbl2"; break;
    case ct_mesh:      output << "ct_mesh"; break;
    case ct_ring:      output << "ct_ring"; break;
    default: output << ct; break;
  }
  return output;
}



ostream & operator << (ostream & output, event_type et)
{
  switch (et)
  {
    case et_read:       output << "et_rd"; break;
    case et_write:      output << "et_wr"; break;
    case et_write_nd:   output << "et_wrnd"; break;
    case et_evict:      output << "et_ev"; break;
    case et_evict_nd:   output << "et_evnd"; break;
    case et_dir_evict:  output << "et_dir_ev"; break;
    case et_e_to_m:     output << "et_e_to_m"; break;
    case et_e_to_i:     output << "et_e_to_i"; break;
    case et_e_to_s:     output << "et_es"; break;
    case et_e_to_s_nd:  output << "et_esnd"; break;
    case et_m_to_s:     output << "et_ms"; break;
    case et_m_to_m:     output << "et_mm"; break;
    case et_m_to_i:     output << "et_mi"; break;
    case et_s_to_s:     output << "et_ss"; break;
    case et_s_to_s_nd:  output << "et_ssnd"; break;
    case et_m_to_e:     output << "et_me"; break;
    case et_dir_rd:     output << "et_dr"; break;
    case et_dir_rd_nd:  output << "et_drnd"; break;
    case et_e_rd:       output << "et_er"; break;
    case et_s_rd:       output << "et_sr"; break;
    case et_nack:       output << "et_na"; break;
    case et_invalidate: output << "et_in"; break;
    case et_invalidate_nd: output << "et_innd"; break;
    case et_s_rd_wr:    output << "et_rw"; break;
    case et_rd_bypass:  output << "et_rb"; break;
    case et_tlb_rd:     output << "et_tr"; break;
    case et_i_to_e:     output << "et_ie"; break;
    case et_rd_dir_info_req: output << "et_rdiq"; break;
    case et_rd_dir_info_rep: output << "et_rdip"; break;
    case et_nop:        output << "et_nop"; break;
    case et_hmc_update_add:   output << "et_hmc_update_add"; break;
    case et_hmc_update_mult:  output << "et_hmc_update_mult"; break;
    case et_hmc_gather:       output << "et_hmc_gather"; break;
    case et_hmc_gather_rep:   output << "et_hmc_gather_rep"; break;
    default: break;
  }
  return output;
}



ostream & operator << (ostream & output, coherence_state_type cs)
{
  switch (cs)
  {
    case cs_invalid:   output << "cs_invalid"; break;
    case cs_shared:    output << "cs_shared"; break;
    case cs_exclusive: output << "cs_exclusive"; break;
    case cs_modified:  output << "cs_modified"; break;
    case cs_tr_to_i:   output << "cs_tr_to_i"; break;
    case cs_tr_to_s:   output << "cs_tr_to_s"; break;
    case cs_tr_to_m:   output << "cs_tr_to_m"; break;
    case cs_tr_to_e:   output << "cs_tr_to_e"; break;
    case cs_m_to_s:    output << "cs_m_to_s"; break;
    default: break;
  }
  return output;
}



ostream & operator << (ostream & output, ins_type it)
{
  switch (it)
  {
    case mem_rd:      output << "mem_rd"; break;
    case mem_2nd_rd:  output << "mem_2nd_rd"; break;
    case mem_wr:      output << "mem_wr"; break;
    case no_mem:      output << "no_mem"; break;
    case ins_branch_taken:     output << "ins_branch_taken"; break;
    case ins_branch_not_taken: output << "ins_branch_not_taken"; break;
    case ins_lock:    output << "ins_lock"; break;
    case ins_unlock:  output << "ins_unlock"; break;
    case ins_barrier: output << "ins_barrier"; break;
    case ins_x87:     output << "ins_x87"; break;
    case ins_notify:  output << "ins_notify"; break;
    case ins_waitfor: output << "ins_waitfor"; break;
    case ins_invalid: output << "ins_invalid"; break;
    case ins_update_add: output << "ins_update_add"; break;
    case ins_update_mult: output << "ins_update_mult"; break;
    case ins_gather: output << "ins_gather"; break;
    default: break;
  }
  return output;
}



McSim::McSim(PthreadTimingSimulator * pts_)
 :pts(pts_),
  skip_all_instrs(pts_->get_param_str("pts.skip_all_instrs") == "true" ? true : false),
  simulate_only_data_caches(pts_->get_param_str("pts.simulate_only_data_caches") == "true" ? true : false),
  show_l2_stat_per_interval(pts_->get_param_str("pts.show_l2_stat_per_interval") == "true" ? true : false),
  is_race_free_application(pts_->get_param_str("pts.is_race_free_application") == "false" ? false : true),
  max_acc_queue_size(pts_->get_param_uint64("pts.max_acc_queue_size", 1000)),
  hmc_topology(pts_->get_param_str("pts.hmc_top") == "DFLY" ? DFLY : MESH),
  core_frequency(pts->get_param_uint64("pts.core_frequency", 2)),
  cores(), hthreads(), l1ds(), l1is(), l2s(), dirs(), rbols(), /*mcs(),*/ hmcs(),  tlbl1ds(), tlbl1is(), comps(),
  num_fetched_instrs(0), num_instrs_printed_last_time(0),
  num_destroyed_cache_lines_last_time(0), cache_line_life_time_last_time(0),
  time_between_last_access_and_cache_destroy_last_time(0)
{
  global_q      = new GlobalEventQueue(this);
  num_hthreads  = pts->get_param_uint64("pts.num_hthreads", max_hthreads);
  use_o3core    = pts->get_param_str("pts.use_o3core") == "true" ? true : false;
  use_rbol      = pts->get_param_str("pts.use_rbol") == "true" ? true : false;
  is_asymmetric = pts->get_param_str("is_asymmetric") == "true" ? true : false;
  is_nuca       = pts->get_param_str("pts.is_nuca") == "true" ? true: false;  // Jiayi, 06/05/17

  double cpu_clk = 1.0 / (double) core_frequency; // unit in ns
  hmc_net = Network::New(4, hmc_topology, cpu_clk);

  uint32_t num_threads_per_l1_cache   = pts->get_param_uint64("pts.num_hthreads_per_l1$", 4);
  assert(use_o3core == false || num_threads_per_l1_cache == 1);
  uint32_t num_l1_caches_per_l2_cache = pts->get_param_uint64("pts.num_l1$_per_l2$", 2);
  uint32_t num_mcs                    = pts->get_param_uint64("pts.num_mcs", 2);
  print_interval                      = pts->get_param_uint64("pts.print_interval", 1000000);
  string   noc_type(pts->get_param_str("pts.noc_type"));

  // for stats
  if (use_o3core)
  {
    lsu_process_interval = pts->get_param_uint64("pts.o3core.process_interval", 10);
  }
  else
  {
    lsu_process_interval = pts->get_param_uint64("pts.lsu.process_interval",    10);
  }
  curr_time_last          = 0;
  num_fetched_instrs_last = 0;
  num_mem_acc_last        = 0;
  num_used_pages_last     = 0;
  num_l1_acc_last         = 0;
  num_l1_miss_last        = 0;
  num_l2_acc_last         = 0;
  num_l2_miss_last        = 0;
  num_dependency_distance_last = 0;

  ngather = 0;
  ngather_recieved = 0;
  num_ld_last = 0;
  num_st_last = 0;
  num_update_ins_last     = 0;
  num_gather_ins_last     = 0;
  num_update_acc_last     = 0;
  num_gather_acc_last     = 0;
  num_update_acc_sent_last = 0;
  num_back_invalidate_last = 0;

  mem_ld_acc_lat_last       = 0.0;
  mem_st_acc_lat_last       = 0.0;
  update_req_lat_last       = 0.0;
  update_stall_lat_last     = 0.0;
  update_roundtrip_lat_last = 0.0;
  gather_roundtrip_lat_last = 0.0;

  if (noc_type != "mesh" && noc_type != "ring" &&
      num_mcs * num_l1_caches_per_l2_cache * num_threads_per_l1_cache > num_hthreads)
  {
    cout << "the number of memory controller must not be larger than the number of L2 caches" << endl;
    exit(1);
  }

  if (is_nuca == true && noc_type == "mesh")
  {
    for (uint32_t i = 0; i < num_hthreads; i++)
    {
      if (use_o3core == true)
      {
        o3cores.push_back(new O3Core(ct_o3core, i, this));
        is_migrate_ready.push_back(false);
      }
      else
      {
        hthreads.push_back(new Hthread(ct_lsu, i, this));
      }
    }

    // instantiate L1 caches
    for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache; i++)
    {
      if (use_o3core == false)
      {
        cores.push_back(new Core(ct_core, i, this));
      }
      l1is.push_back(new CacheL1(ct_cachel1i, i, this));
      l1ds.push_back(new CacheL1(ct_cachel1d, i, this));
      tlbl1ds.push_back(new TLBL1(ct_tlbl1d, i, this));
      tlbl1is.push_back(new TLBL1(ct_tlbl1i, i, this));
    }
    if (num_hthreads % num_threads_per_l1_cache != 0)
    {
      if (use_o3core == false)
      {
        cores.push_back(cores[0]);
      }
      l1is.push_back(l1is[0]);
      l1ds.push_back(l1ds[0]);
      tlbl1ds.push_back(tlbl1ds[0]);
      tlbl1is.push_back(tlbl1is[0]);
    }

    // connect hthreads and l1s
    for (uint32_t i = 0; i < num_hthreads; i++)
    {
      if (use_o3core == true)
      {
        o3cores[i]->cachel1i = (l1is[i]);
        l1is[i]->lsus.push_back(o3cores[i]);
        o3cores[i]->cachel1d = (l1ds[i]);
        l1ds[i]->lsus.push_back(o3cores[i]);
        o3cores[i]->tlbl1d   = (tlbl1ds[i]);
        tlbl1ds[i]->lsus.push_back(o3cores[i]);
        o3cores[i]->tlbl1i   = (tlbl1is[i]);
        tlbl1is[i]->lsus.push_back(o3cores[i]);
      }
      else
      {
        hthreads[i]->core     = (cores[i/num_threads_per_l1_cache]);
        cores[i/num_threads_per_l1_cache]->hthreads.push_back(hthreads[i]);
        cores[i/num_threads_per_l1_cache]->is_active.push_back(false);
        hthreads[i]->cachel1i = (l1is[i/num_threads_per_l1_cache]);
        l1is[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
        hthreads[i]->cachel1d = (l1ds[i/num_threads_per_l1_cache]);
        l1ds[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
        hthreads[i]->tlbl1d   = (tlbl1ds[i/num_threads_per_l1_cache]);
        tlbl1ds[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
        hthreads[i]->tlbl1i   = (tlbl1is[i/num_threads_per_l1_cache]);
        tlbl1is[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
      }
    }

    assert(noc_type == "mesh");
    noc = new Mesh2D(ct_mesh, 0, this);

    // connect cores with noc
    if (use_o3core == true)
    {
      for (uint32_t i = 0; i < num_hthreads; i++)
      {
        o3cores[i]->noc = noc;
        noc->o3core.push_back(o3cores[i]);
      }
    }

    // instantiate L2 caches and directories
    for (uint32_t i = 0; i < num_hthreads; i++)
    {
      l2s.push_back(new CacheL2(ct_cachel2, i, this));
      l2s[i]->directory = NULL;
      l2s[i]->crossbar = noc;
      noc->cachel2.push_back(l2s[i]);

      l1is[i]->cachel2 = l2s[i];
      l1ds[i]->cachel2 = l2s[i];
    }

    // connect l1s and network
    for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache; i++) {
      l1is[i]->noc = noc;
      l1ds[i]->noc = noc;
      noc->cachel1i.push_back(l1is[i]);
      noc->cachel1d.push_back(l1ds[i]);
    }

    for (uint32_t i = 0; i < num_mcs; i++) {
      //mcs.push_back(new MemoryController(ct_memory_controller, i, this));
      hmcs.push_back(new PTSHMCController(ct_hmc_controller, i, this, hmc_net));
      dirs.push_back(new Directory(ct_directory, i, this));
      //dirs[i]->memorycontroller = (mcs[i]);
      //mcs[i]->directory = (dirs[i]);
      dirs[i]->hmccontroller = (hmcs[i]);
      hmcs[i]->directory = (dirs[i]);

      noc->directory.push_back(dirs[i]);
      noc->hmccontroller.push_back(hmcs[i]);
      dirs[i]->cachel2  = NULL;
      dirs[i]->crossbar = noc;
      hmcs[i]->noc = noc;
    }

    noc->connect_l2_directory();
  }
  else if (is_nuca == true)
  {
    for (uint32_t i = 0; i < num_hthreads; i++)
    {
      if (use_o3core == true)
      {
        o3cores.push_back(new O3Core(ct_o3core, i, this));
        is_migrate_ready.push_back(false);
      }
      else
      {
        hthreads.push_back(new Hthread(ct_lsu, i, this));
      }
    }

    // instantiate L1 caches
    for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache; i++)
    {
      if (use_o3core == false)
      {
        cores.push_back(new Core(ct_core, i, this));
      }
      l1is.push_back(new CacheL1(ct_cachel1i, i, this));
      l1ds.push_back(new CacheL1(ct_cachel1d, i, this));
      tlbl1ds.push_back(new TLBL1(ct_tlbl1d, i, this));
      tlbl1is.push_back(new TLBL1(ct_tlbl1i, i, this));
    }
    if (num_hthreads % num_threads_per_l1_cache != 0)
    {
      if (use_o3core == false)
      {
        cores.push_back(cores[0]);
      }
      l1is.push_back(l1is[0]);
      l1ds.push_back(l1ds[0]);
      tlbl1ds.push_back(tlbl1ds[0]);
      tlbl1is.push_back(tlbl1is[0]);
    }

    // connect hthreads and l1s
    for (uint32_t i = 0; i < num_hthreads; i++)
    {
      if (use_o3core == true)
      {
        o3cores[i]->cachel1i = (l1is[i]);
        l1is[i]->lsus.push_back(o3cores[i]);
        o3cores[i]->cachel1d = (l1ds[i]);
        l1ds[i]->lsus.push_back(o3cores[i]);
        o3cores[i]->tlbl1d   = (tlbl1ds[i]);
        tlbl1ds[i]->lsus.push_back(o3cores[i]);
        o3cores[i]->tlbl1i   = (tlbl1is[i]);
        tlbl1is[i]->lsus.push_back(o3cores[i]);
      }
      else
      {
        hthreads[i]->core     = (cores[i/num_threads_per_l1_cache]);
        cores[i/num_threads_per_l1_cache]->hthreads.push_back(hthreads[i]);
        cores[i/num_threads_per_l1_cache]->is_active.push_back(false);
        hthreads[i]->cachel1i = (l1is[i/num_threads_per_l1_cache]);
        l1is[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
        hthreads[i]->cachel1d = (l1ds[i/num_threads_per_l1_cache]);
        l1ds[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
        hthreads[i]->tlbl1d   = (tlbl1ds[i/num_threads_per_l1_cache]);
        tlbl1ds[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
        hthreads[i]->tlbl1i   = (tlbl1is[i/num_threads_per_l1_cache]);
        tlbl1is[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
      }
    }

    assert(noc_type == "xbar");
    noc = new Crossbar(ct_crossbar, 0, this, num_hthreads / num_threads_per_l1_cache / num_l1_caches_per_l2_cache);

    // connect cores with noc
    if (use_o3core == true)
    {
      for (uint32_t i = 0; i < num_hthreads; i++)
      {
        o3cores[i]->noc = noc;
        noc->o3core.push_back(o3cores[i]);
      }
    }

    // instantiate L2 caches and directories
    for (uint32_t i = 0; i < num_hthreads; i++)
    {
      l2s.push_back(new CacheL2(ct_cachel2, i, this));
      l2s[i]->directory = NULL;
      l2s[i]->crossbar = noc;
      noc->cachel2.push_back(l2s[i]);

      l1is[i]->cachel2 = l2s[i];
      l1ds[i]->cachel2 = l2s[i];
    }

    // connect l1s and network
    for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache; i++) {
      l1is[i]->noc = noc;
      l1ds[i]->noc = noc;
      noc->cachel1i.push_back(l1is[i]);
      noc->cachel1d.push_back(l1ds[i]);
    }

    for (uint32_t i = 0; i < num_mcs; i++) {
      //mcs.push_back(new MemoryController(ct_memory_controller, i, this));
      hmcs.push_back(new PTSHMCController(ct_hmc_controller, i, this, hmc_net));
      dirs.push_back(new Directory(ct_directory, i, this));
      //dirs[i]->memorycontroller = (mcs[i]);
      dirs[i]->hmccontroller = (hmcs[i]);
      //mcs[i]->directory = (dirs[i]);
      hmcs[i]->directory = (dirs[i]);

      noc->directory.push_back(dirs[i]);
      noc->hmccontroller.push_back(hmcs[i]);
      dirs[i]->cachel2  = NULL;
      dirs[i]->crossbar = noc;
      hmcs[i]->noc = noc;
    }

    noc->connect_l2_directory();
  }
  else if (is_asymmetric == false)
  {
    for (uint32_t i = 0; i < num_hthreads; i++)
    {
      if (use_o3core == true)
      {
        o3cores.push_back(new O3Core(ct_o3core, i, this));
        is_migrate_ready.push_back(false);
      }
      else
      {
        hthreads.push_back(new Hthread(ct_lsu, i, this));
      }
    }

    // instantiate L1 caches
    for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache; i++)
    {
      if (use_o3core == false)
      {
        cores.push_back(new Core(ct_core, i, this));
      }
      l1is.push_back(new CacheL1(ct_cachel1i, i, this));
      l1ds.push_back(new CacheL1(ct_cachel1d, i, this));
      tlbl1ds.push_back(new TLBL1(ct_tlbl1d, i, this));
      tlbl1is.push_back(new TLBL1(ct_tlbl1i, i, this));
    }
    if (num_hthreads % num_threads_per_l1_cache != 0)
    {
      if (use_o3core == false)
      {
        cores.push_back(cores[0]);
      }
      l1is.push_back(l1is[0]);
      l1ds.push_back(l1ds[0]);
      tlbl1ds.push_back(tlbl1ds[0]);
      tlbl1is.push_back(tlbl1is[0]);
    }

    // connect hthreads and l1s
    for (uint32_t i = 0; i < num_hthreads; i++)
    {
      if (use_o3core == true)
      {
        o3cores[i]->cachel1i = (l1is[i]);
        l1is[i]->lsus.push_back(o3cores[i]);
        o3cores[i]->cachel1d = (l1ds[i]);
        l1ds[i]->lsus.push_back(o3cores[i]);
        o3cores[i]->tlbl1d   = (tlbl1ds[i]);
        tlbl1ds[i]->lsus.push_back(o3cores[i]);
        o3cores[i]->tlbl1i   = (tlbl1is[i]);
        tlbl1is[i]->lsus.push_back(o3cores[i]);
      }
      else
      {
        hthreads[i]->core     = (cores[i/num_threads_per_l1_cache]);
        cores[i/num_threads_per_l1_cache]->hthreads.push_back(hthreads[i]);
        cores[i/num_threads_per_l1_cache]->is_active.push_back(false);
        hthreads[i]->cachel1i = (l1is[i/num_threads_per_l1_cache]);
        l1is[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
        hthreads[i]->cachel1d = (l1ds[i/num_threads_per_l1_cache]);
        l1ds[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
        hthreads[i]->tlbl1d   = (tlbl1ds[i/num_threads_per_l1_cache]);
        tlbl1ds[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
        hthreads[i]->tlbl1i   = (tlbl1is[i/num_threads_per_l1_cache]);
        tlbl1is[i/num_threads_per_l1_cache]->lsus.push_back(hthreads[i]);
      }
    }

    // instantiate L2 caches
    for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache / num_l1_caches_per_l2_cache; i++)
    {
      l2s.push_back(new CacheL2(ct_cachel2, i, this));
    }

    // connect l1s and l2s
    for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache; i++)
    {
      l1is[i]->cachel2 = l2s[i/num_l1_caches_per_l2_cache];
      l1ds[i]->cachel2 = l2s[i/num_l1_caches_per_l2_cache];
      l2s[i/num_l1_caches_per_l2_cache]->cachel1i.push_back(l1is[i]);
      l2s[i/num_l1_caches_per_l2_cache]->cachel1d.push_back(l1ds[i]);
    }

    if (noc_type == "mesh" || noc_type == "ring")
    {
      if (noc_type == "mesh")
      {
        noc = new Mesh2D(ct_mesh, 0, this);
      }
      else
      {
        noc = new Ring(ct_ring, 0, this);
      }
      Directory * dummy_dir = new Directory(ct_directory, num_mcs, this);

      for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache / num_l1_caches_per_l2_cache; i++)
      {
        l2s[i]->directory = dummy_dir;
        l2s[i]->crossbar  = noc;
        noc->cachel2.push_back(l2s[i]);
      }

      for (uint32_t i = 0; i < num_mcs; i++)
      {
        //mcs.push_back(new MemoryController(ct_memory_controller, i, this));
        hmcs.push_back(new PTSHMCController(ct_hmc_controller, i, this, hmc_net));
        dirs.push_back(new Directory(ct_directory, i, this));
        if (use_rbol == true)
        {
          rbols.push_back(new RBoL(ct_rbol, i, this));
          rbols[i]->directory        = dirs[i];
          //rbols[i]->mc = mcs[i];
          rbols[i]->hmc = hmcs[i];

          //dirs[i]->memorycontroller  = rbols[i];
          //mcs[i]->directory          = rbols[i];
          dirs[i]->hmccontroller  = rbols[i];
          hmcs[i]->directory          = rbols[i];
        }
        else
        {
          //dirs[i]->memorycontroller = (mcs[i]);
          //mcs[i]->directory = (dirs[i]);
          dirs[i]->hmccontroller = (hmcs[i]);
          hmcs[i]->directory = (dirs[i]);
        }

        noc->directory.push_back(dirs[i]);
        dirs[i]->cachel2  = NULL;
        dirs[i]->crossbar = noc;
      }
    }
    else
    {
      noc = new Crossbar(ct_crossbar, 0, this, num_hthreads / num_threads_per_l1_cache / num_l1_caches_per_l2_cache);

      // instantiate directories
      // currently it is assumed that (# of MCs) == (# of L2$s) == (# of directories)
      for (uint32_t i = 0; i < num_hthreads / num_threads_per_l1_cache / num_l1_caches_per_l2_cache; i++)
      {
        //mcs.push_back(new MemoryController(ct_memory_controller, i, this));
        hmcs.push_back(new PTSHMCController(ct_hmc_controller, i, this,hmc_net));
        dirs.push_back(new Directory(ct_directory, i, this));
        if (use_rbol == true)
        {
          rbols.push_back(new RBoL(ct_rbol, i, this));
          rbols[i]->directory        = dirs[i];
          //rbols[i]->mc = mcs[i];
          rbols[i]->hmc = hmcs[i];

          //dirs[i]->memorycontroller  = rbols[i];
          //mcs[i]->directory          = rbols[i];
          dirs[i]->hmccontroller  = rbols[i];
          hmcs[i]->directory          = rbols[i];
        }
        else
        {
          //dirs[i]->memorycontroller = (mcs[i]);
          //mcs[i]->directory = (dirs[i]);
          dirs[i]->hmccontroller = (hmcs[i]);
          hmcs[i]->directory = (dirs[i]);
        }

        l2s[i]->directory = (dirs[i]);
        l2s[i]->crossbar  = noc;
        noc->directory.push_back(dirs[i]);
        noc->cachel2.push_back(l2s[i]);
        dirs[i]->cachel2  = (l2s[i]);
        dirs[i]->crossbar = noc;
      }
    }
  }
  else
  {
    Component ** noc_nodes;

    uint32_t num_noc_nodes = pts->get_param_uint64("noc.num_node", 1);
    noc_nodes              = new Component * [num_noc_nodes];
    uint32_t num_l2_caches = 0;
    uint32_t num_l1_caches = 0;
    uint32_t num_dirs      = 0;
    uint32_t num_cores     = 0;

    noc = new Crossbar(ct_crossbar, 0, this, num_noc_nodes);

    for (uint32_t i = 0; i < num_noc_nodes; i++)
    {
      stringstream num_to_str;
      num_to_str << i;
      string node_type = pts->get_param_str(string("noc.node.")+num_to_str.str());

      if (node_type == "l2$.t1" || node_type == "l2$.t2")
      {
        component_type l2_type = ct_cachel2_t1;

        if (node_type == "l2$.t2")
        {
          l2_type = ct_cachel2_t2;
        }
        CacheL2 * l2  = new CacheL2(l2_type, num_l2_caches++, this);
        l2s.push_back(l2);
        noc_nodes[i]  = l2;
        noc->cachel2.push_back(l2);
        l2->directory = NULL;
        l2->crossbar  = noc;

        uint32_t num_l1_per_l2 = pts->get_param_uint64(node_type+string(".num_l1$"), 1);

        for (uint32_t j = 0; j < num_l1_per_l2; j++)
        {
          stringstream num_to_str;
          num_to_str << j;
          string l1_type = pts->get_param_str(node_type+string(".l1$.")+num_to_str.str());

          component_type l1i_type = ct_cachel1i_t1;
          component_type l1d_type = ct_cachel1d_t1;
          if (l1_type == "t2")
          {
            l1i_type = ct_cachel1i_t2;
            l1d_type = ct_cachel1d_t2;
          }
          CacheL1 * l1i = new CacheL1(l1i_type, num_l1_caches, this);
          CacheL1 * l1d = new CacheL1(l1d_type, num_l1_caches, this);
          TLBL1   * l1itlb = new TLBL1(ct_tlbl1i, num_l1_caches, this);
          TLBL1   * l1dtlb = new TLBL1(ct_tlbl1d, num_l1_caches++, this);
          l1is.push_back(l1i);
          l1ds.push_back(l1d);
          tlbl1is.push_back(l1itlb);
          tlbl1ds.push_back(l1dtlb);

          l2->cachel1i.push_back(l1i);
          l2->cachel1d.push_back(l1d);
          l1i->cachel2 = l2;
          l1d->cachel2 = l2;

          string o3_type = pts->get_param_str(string("l1i$.")+l1_type+string(".core.0"));
          O3Core * o3core;

          if (o3_type == "o3.t1")
          {
            o3core = new O3Core(ct_o3core_t1, num_cores++, this);
          }
          else
          {
            o3core = new O3Core(ct_o3core_t2, num_cores++, this);
          }
          o3cores.push_back(o3core);
          is_migrate_ready.push_back(false);

          o3core->cachel1i = l1i;
          o3core->cachel1d = l1d;
          o3core->tlbl1i   = l1itlb;
          o3core->tlbl1d   = l1dtlb;
          l1i->lsus.push_back(o3core);
          l1d->lsus.push_back(o3core);
          l1itlb->lsus.push_back(o3core);
          l1dtlb->lsus.push_back(o3core);
        }
      }
      else
      {
        //MemoryController * mc  = new MemoryController(ct_memory_controller, num_dirs, this);
        PTSHMCController * hmc  = new PTSHMCController(ct_hmc_controller, num_dirs, this,hmc_net);
        Directory * dir = new Directory(ct_directory, num_dirs++, this);
        //mcs.push_back(mc);
        hmcs.push_back(hmc);
        dirs.push_back(dir);

        //mc->directory = dir;
        //dir->memorycontroller = mc;
        hmc->directory = dir;
        dir->hmccontroller = hmc;
        dir->cachel2  = NULL;
        dir->crossbar = noc;

        noc_nodes[i]  = dir;
        noc->directory.push_back(dir);
      }
    }

    assert(num_mcs == num_dirs);
  }
  cout << right << setfill(' ');
}


McSim::~McSim()
{
  cout << right << setfill(' ');
  uint64_t ipc1000 = (global_q->curr_time == 0 ) ? 0 : (1000 * num_fetched_instrs * lsu_process_interval / global_q->curr_time);
  cout << "  -- total number of fetched instructions : " << num_fetched_instrs
    << " (IPC = " << setw(3) << ipc1000/1000 << "." << setfill('0') << setw(3) << ipc1000%1000 << ")" << endl;
  cout << "  -- total number of ticks: " << global_q->curr_time << " , cycles: " << global_q->curr_time / lsu_process_interval << endl;

  uint64_t num_mem_acc = 0;
  uint64_t num_update_acc = 0;
  uint64_t num_gather_acc = 0;
  double update_req_lat = 0.0;
  double update_stall_lat = 0.0;
  for (uint32_t i = 0; i < hmcs.size(); i++)
  {
    num_mem_acc += hmcs[i]->num_reqs;
    num_update_acc += hmcs[i]->num_update;
    num_gather_acc += hmcs[i]->num_gather;
    update_req_lat += hmcs[i]->total_update_req_time;
    update_stall_lat += hmcs[i]->total_update_stall_time;
  }
  update_req_lat = update_req_lat / num_update_acc / hmcs[0]->process_interval;
  update_stall_lat = update_stall_lat / num_update_acc / hmcs[0]->process_interval;

  uint64_t num_update_ins = 0;
  uint64_t num_gather_ins = 0;
  double update_roundtrip_lat = 0.0;
  double gather_roundtrip_lat = 0.0;
  for (uint32_t i = 0; i < o3cores.size(); i++)
  {
    num_update_ins += o3cores[i]->num_update_ins;
    num_gather_ins += o3cores[i]->num_gather_ins;
    update_roundtrip_lat += o3cores[i]->total_update_roundtrip_time;
    gather_roundtrip_lat += o3cores[i]->total_gather_roundtrip_time;
  }
  assert(num_update_acc == num_update_ins && num_gather_acc == num_gather_ins);
  update_roundtrip_lat = update_roundtrip_lat / lsu_process_interval / num_update_ins;
  gather_roundtrip_lat = gather_roundtrip_lat / lsu_process_interval / num_gather_ins;

  uint64_t num_back_inv = 0;
  for (uint32_t i = 0; i < dirs.size(); i++)
  {
    num_back_inv += dirs[i]->num_back_invalidate;
  }

  cout << "  -- total number of mem accs : " <<  num_mem_acc << endl;
  cout << "  -- total number of updates : " << num_update_acc << endl;
  cout << "  -- total number of gathers : " << num_gather_acc << endl;
  cout << "  -- total ngather recieved : " << ngather_recieved << endl;
  cout << "  -- total ngather to o3cores : " << ngather << endl;
  cout << "  -- total number of back invalidations : " << num_back_inv << endl;
  cout << "  -- average update request latency : " << update_req_lat << endl;
  cout << "  -- average update stalls in hmc controllers : " << update_stall_lat << endl;
  cout << "  -- average update roundtrip latency : " << update_roundtrip_lat << endl;
  cout << "  -- average gather roundtrip latency : " << gather_roundtrip_lat << endl;

  cout << right << setfill(' ');
  for (vector<Hthread *>::iterator iter = hthreads.begin(); iter != hthreads.end(); ++iter)
  {
    delete (*iter);
  }
  for (vector<O3Core *>::iterator iter = o3cores.begin(); iter != o3cores.end(); ++iter)
  {
    delete (*iter);
  }
  for (vector<CacheL2 *>::iterator iter = l2s.begin(); iter != l2s.end(); ++iter)
  {
    delete (*iter);
  }
  for (vector<Directory *>::iterator iter = dirs.begin(); iter != dirs.end(); ++iter)
  {
    delete (*iter);
  }
  for (vector<MemoryController *>::iterator iter = mcs.begin(); iter != mcs.end(); ++iter)
  {
    delete (*iter);
  }
  for (vector<PTSHMCController *>::iterator iter = hmcs.begin(); iter != hmcs.end(); ++iter)//pritam added
  {
    delete (*iter);
  }
  delete noc;
  for (vector<CacheL1 *>::iterator iter = l1is.begin(); iter != l1is.end(); ++iter)
  {
    delete (*iter);
  }
  for (vector<CacheL1 *>::iterator iter = l1ds.begin(); iter != l1ds.end(); ++iter)
  {
    delete (*iter);
  }
  for (vector<TLBL1 *>::iterator iter = tlbl1is.begin(); iter != tlbl1is.end(); ++iter)
  {
    delete (*iter);
  }
  for (vector<TLBL1 *>::iterator iter = tlbl1ds.begin(); iter != tlbl1ds.end(); ++iter)
  {
    delete (*iter);
  }

  delete hmc_net;
  delete global_q;
}


void McSim::show_state(uint64_t addr)
{
  for (list<Component *>::iterator iter = comps.begin(); iter != comps.end(); ++iter)
  {
    (*iter)->show_state(addr);
  }
}


pair<uint32_t, uint64_t> McSim::resume_simulation(bool must_switch)
{
  pair<uint32_t, uint64_t> ret_val;  // <thread_id, time>

  if (/*must_switch == true &&*/
      global_q->event_queue.begin() == global_q->event_queue.end())
  {
    bool any_resumable_thread = false;
    if (use_o3core == true)
    {
      for (uint32_t i = 0; i < o3cores.size(); i++)
      {
        O3Core * o3core = o3cores[i];
        if (o3core->active == true)
        {
          ret_val.first  = o3core->num;
          ret_val.second = global_q->curr_time;
          i = o3cores.size();
          any_resumable_thread = true;
          break;
        }
      }
    }
    else
    {
      for (uint32_t i = 0; i < cores.size(); i++)
      {
        Core * core = cores[i];
        for (uint32_t j = 0; j < core->hthreads.size(); j++)
        {
          Hthread * hthread = core->hthreads[j];
          if (/*hthread->mem_acc.empty() == true && core->is_active[j] == true &&*/
              hthread != NULL && hthread->active == true)
          {
            ret_val.first  = hthread->num;
            ret_val.second = global_q->curr_time;
            //core->is_active[j] = false;
            i = cores.size();
            any_resumable_thread = true;
            break;
          }
        }
      }
    }

    if (any_resumable_thread == false)
    {
      cout << global_q->curr_time << endl;
      for (uint32_t i = 0; i < cores.size(); i++)
      {
        Core * core = cores[i];
        for (uint32_t j = 0; j < core->hthreads.size(); j++)
        {
          Hthread * hthread = core->hthreads[j];
          if (hthread == NULL)
          {
            continue;
          }
          if (use_o3core == false)
          {
            cout << hthread->mem_acc.empty() << ", ";
          }
          cout << core->is_active[j] << ", ";
          cout << hthread->active << ": ";
          cout << hthread->resume_time << ", " << hthread->latest_bmp_time << endl;
        }
      }

      //cout << "  -- event queue can not be empty : cycle = " << curr_time << endl;
      //ASSERTX(0);
    }
    else
    {
      return ret_val;
    }
  }

  ret_val.first  = global_q->process_event();
  ret_val.second = global_q->curr_time;

  if (num_fetched_instrs / print_interval != num_instrs_printed_last_time)
  {
    //cout.setf(left); cout.setfill(' ');
    num_instrs_printed_last_time = num_fetched_instrs / print_interval;
    cout << "  -- [" << dec << setw(12) << global_q->curr_time << "]: " 
      << setw(10) << num_fetched_instrs << " instrs so far,";

    if (global_q->curr_time > curr_time_last)
    {
      uint64_t ipc1000 = 1000 * (num_fetched_instrs - num_fetched_instrs_last) * lsu_process_interval /
        (global_q->curr_time - curr_time_last);
      cout << " IPC= " << setw(3) << ipc1000/1000 << "." << setfill('0') << setw(3) << ipc1000%1000
        << setfill(' ') << ", ";
    }

    uint64_t num_l1_acc  = 0;
    uint64_t num_l1_miss = 0;
    for (unsigned int i = 0; i < l1ds.size(); i++)
    {
      num_l1_acc += l1ds[i]->num_rd_access + l1ds[i]->num_wr_access + l1is[i]->num_rd_access + l1is[i]->num_wr_access - l1ds[i]->num_nack - l1is[i]->num_nack;
      num_l1_miss += l1ds[i]->num_rd_miss + l1ds[i]->num_wr_miss + l1is[i]->num_rd_miss + l1is[i]->num_wr_miss - l1ds[i]->num_nack - l1is[i]->num_nack;
    }

    uint64_t num_l2_acc  = 0;
    uint64_t num_l2_miss = 0;
    for (unsigned int i = 0; i < l2s.size(); i++)
    {
      num_l2_acc  += l2s[i]->num_rd_access + l2s[i]->num_wr_access - l2s[i]->num_nack;
      num_l2_miss += l2s[i]->num_rd_miss + l2s[i]->num_wr_miss - l2s[i]->num_nack;
    }

    cout << "L1 (acc, miss)=( " << setw(7) << num_l1_acc - num_l1_acc_last << ", "
      << setw(6) << num_l1_miss - num_l1_miss_last << "), ";
    cout << "L2 (acc, miss)=( " << setw(6) << num_l2_acc - num_l2_acc_last << ", "
      << setw(6) << num_l2_miss - num_l2_miss_last << "), ";
    num_l1_acc_last = num_l1_acc;
    num_l1_miss_last = num_l1_miss;
    num_l2_acc_last = num_l2_acc;
    num_l2_miss_last = num_l2_miss;

    uint64_t num_mem_acc = 0;
    uint64_t num_used_pages = 0;

    uint64_t num_update_acc = 0;
    uint64_t num_gather_acc = 0;
    uint64_t num_update_acc_sent = 0;

    double update_req_lat   = 0.0;
    double update_stall_lat = 0.0;

    for (unsigned int i = 0; i < hmcs.size(); i++)
    {
      num_used_pages += hmcs[i]->os_page_acc_dist_curr.size();
      num_mem_acc    += hmcs[i]->num_reqs;
      num_update_acc += hmcs[i]->num_update;
      num_gather_acc += hmcs[i]->num_gather;
      num_update_acc_sent += hmcs[i]->num_update_sent;
      update_req_lat += hmcs[i]->total_update_req_time / hmcs[i]->process_interval;
      update_stall_lat += hmcs[i]->total_update_stall_time / hmcs[i]->process_interval;
    }
    for (unsigned int i = 0; i < mcs.size(); i++)
    {
      num_used_pages += mcs[i]->os_page_acc_dist_curr.size();
      num_mem_acc    += mcs[i]->num_reqs;
    }

    cout << setw(6) << num_mem_acc - num_mem_acc_last << " mem accs, ( ";
    cout << setw(4) << num_used_pages << ", ";

    num_used_pages = 0;
    for (unsigned int i = 0; i < hmcs.size(); i++)
    {
      hmcs[i]->update_acc_dist();
      num_used_pages += hmcs[i]->os_page_acc_dist.size();
    }
    for (unsigned int i = 0; i < mcs.size(); i++)
    {
      mcs[i]->update_acc_dist();
      num_used_pages += mcs[i]->os_page_acc_dist.size();
    }

    cout << setw(4) << num_used_pages - num_used_pages_last << ") touched pages (this time, 1stly), ";
    cout << setw(6) << num_update_acc - num_update_acc_last << " update accs, ";
    cout << setw(6) << num_gather_acc - num_gather_acc_last << " gather accs, ";
    num_mem_acc_last    = num_mem_acc;
    num_used_pages_last = num_used_pages;

    if (o3cores.size() > 0)
    {
      uint64_t total_dependency_distance = 0;
      for (unsigned int i = 0; i < o3cores.size(); i++)
      {
        total_dependency_distance += o3cores[i]->total_dependency_distance;
      }

      if (num_fetched_instrs > num_fetched_instrs_last)
      {
        uint64_t dd1000 = 1000 * (total_dependency_distance - num_dependency_distance_last)/(num_fetched_instrs - num_fetched_instrs_last);
        cout << " avg_dd= " << setw(2) << dd1000/1000 << "." << setfill('0') << setw(3) << dd1000%1000
             << setfill(' ') << ", ";
      }
      num_dependency_distance_last = total_dependency_distance;
    }

    uint64_t num_back_invalidate = 0;
    for (uint32_t i = 0; i < dirs.size(); i++)
    {
      num_back_invalidate += dirs[i]->num_back_invalidate;
    }
    cout << setw(6) << num_back_invalidate - num_back_invalidate_last << " back-inv, ";
    num_back_invalidate_last = num_back_invalidate;

    double avg_update_req_lat   = 0.0;
    double avg_update_stall_lat = 0.0;
    if (num_update_acc == num_update_acc_last)
    {
      assert(update_req_lat == update_req_lat_last);
    }
    else
    {
      avg_update_req_lat = (update_req_lat - update_req_lat_last) / (num_update_acc - num_update_acc_last);
      num_update_acc_last = num_update_acc;
      update_req_lat_last = update_req_lat;
    }
    if (num_update_acc_sent == num_update_acc_sent_last)
    {
      assert(update_stall_lat == update_stall_lat_last);
    }
    else
    {
      avg_update_stall_lat = (update_stall_lat - update_stall_lat_last) / (num_update_acc_sent - num_update_acc_sent_last);
      update_stall_lat_last = update_stall_lat;
      num_update_acc_sent_last = num_update_acc_sent;
    }
    cout << setw(6) << avg_update_req_lat << " update-noc-lat, ";
    cout << setw(6) << avg_update_stall_lat << " update-stall-lat, ";

    if (o3cores.size() > 0)
    {
      uint64_t num_update_ins = 0;
      uint64_t num_gather_ins = 0;
      double   update_roundtrip_lat = 0.0;
      double   gather_roundtrip_lat = 0.0;
      for (uint32_t i = 0; i < o3cores.size(); i++)
      {
        num_update_ins += o3cores[i]->num_update_ins;
        num_gather_ins += o3cores[i]->num_gather_ins;
        update_roundtrip_lat += o3cores[i]->total_update_roundtrip_time / lsu_process_interval;
        gather_roundtrip_lat += o3cores[i]->total_gather_roundtrip_time / lsu_process_interval;
      }

      double avg_update_roundtrip_lat = 0.0;
      double avg_gather_roundtrip_lat = 0.0;
      if (num_update_ins == num_update_ins_last)
      {
        assert(update_roundtrip_lat == update_roundtrip_lat_last);
      }
      else
      {
        avg_update_roundtrip_lat = (update_roundtrip_lat - update_roundtrip_lat_last) / (num_update_ins - num_update_ins_last);
        num_update_ins_last = num_update_ins;
        update_roundtrip_lat_last = update_roundtrip_lat;
      }
      if (num_gather_ins == num_gather_ins_last)
      {
        if (gather_roundtrip_lat != gather_roundtrip_lat_last)
        {
          cerr << "num_gather_ins: " << num_gather_ins << ", num_gather_ins_last: " << num_gather_ins_last
            << ", gather_roundtrip_lat: " << gather_roundtrip_lat << ", gather_roundtrip_lat_last: " << gather_roundtrip_lat_last << endl;
        }
        assert(gather_roundtrip_lat == gather_roundtrip_lat_last);
      }
      else
      {
        avg_gather_roundtrip_lat = (gather_roundtrip_lat - gather_roundtrip_lat_last) / (num_gather_ins - num_gather_ins_last);
        num_gather_ins_last = num_gather_ins;
        gather_roundtrip_lat_last = gather_roundtrip_lat;
      }
      cout << setw(6) << avg_update_roundtrip_lat << " update-roundtrip-lat, ";
      cout << setw(6) << avg_gather_roundtrip_lat << " gather-roundtrip-lat, ";
    }

    uint64_t num_ld = 0;
    uint64_t num_st = 0;
    double mem_ld_acc_lat = 0.0;
    double mem_st_acc_lat = 0.0;
    double avg_ld_acc_lat = 0.0;
    double avg_st_acc_lat = 0.0;
    for (uint32_t i = 0; i < o3cores.size(); i++)
    {
      num_ld += o3cores[i]->num_rd;
      num_st += o3cores[i]->num_wr;
      mem_ld_acc_lat += o3cores[i]->total_mem_rd_time / lsu_process_interval;
      mem_st_acc_lat += o3cores[i]->total_mem_wr_time / lsu_process_interval;
    }
    if (num_ld == num_ld_last)
    {
      assert(mem_ld_acc_lat == mem_ld_acc_lat_last);
    }
    else
    {
      avg_ld_acc_lat = (mem_ld_acc_lat - mem_ld_acc_lat_last) / (num_ld - num_ld_last);
      mem_ld_acc_lat_last = mem_ld_acc_lat;
      num_ld_last = num_ld;
    }
    if (num_st == num_st_last)
    {
      assert(mem_st_acc_lat == mem_st_acc_lat_last);
    }
    else
    {
      avg_st_acc_lat = (mem_st_acc_lat - mem_st_acc_lat_last) / (num_st - num_st_last);
      mem_st_acc_lat_last = mem_st_acc_lat;
      num_st_last = num_st;
    }
    cout << setw(6) << avg_ld_acc_lat << " load-amat, ";
    cout << setw(6) << avg_st_acc_lat << " store-amat, ";

    cout << setw(4) << noc->req_noc_latency << " req_noc_lat, ";

    num_fetched_instrs_last = num_fetched_instrs;
    curr_time_last = global_q->curr_time;

    if (show_l2_stat_per_interval == true)
    {
      show_l2_cache_summary();
    }
    else
    {
      cout << endl;
    }
  }
  return ret_val;
}


uint32_t McSim::add_instruction(
    uint32_t hthreadid_,
    uint64_t curr_time_,
    uint64_t waddr,
    UINT32   wlen,
    uint64_t raddr,
    uint64_t raddr2,
    UINT32   rlen,
    uint64_t ip, 
    uint32_t category,
    bool     isbranch,
    bool     isbranchtaken,
    bool     islock,
    bool     isunlock,
    bool     isbarrier,
    uint32_t rr0, uint32_t rr1, uint32_t rr2, uint32_t rr3,
    uint32_t rw0, uint32_t rw1, uint32_t rw2, uint32_t rw3
    )
{
  // push a new event to the event queue
  uint32_t num_available_slot = 0;
  num_fetched_instrs++;

  if (use_o3core == true)
  {
    O3Core * o3core = o3cores[hthreadid_];

    if (o3core->o3queue_size == 0)
    {
      if (o3core->resume_time <= curr_time_)
      {
        global_q->add_event(curr_time_, o3core);
        o3core->is_active = true;
      }
    }
    ins_type type = (category == 128) ? ins_update_add :
      (category == 130)               ? ins_update_mult :
      (category == 129)               ? ins_gather :
      (islock == true && isunlock == true && isbarrier == false) ? ins_notify :
      (islock == true && isunlock == true && isbarrier == true) ? ins_waitfor :
      (isbranch && isbranchtaken)        ? ins_branch_taken :
      (isbranch && !isbranchtaken)       ? ins_branch_not_taken :
      (category == XED_CATEGORY_X87_ALU || category == XED_CATEGORY_SSE) ? ins_x87 :  // treat an SSE op as an X87 op
      (islock == true)                   ? ins_lock :
      (isunlock == true)                 ? ins_unlock : 
      (isbarrier == true)                ? ins_barrier : no_mem;

    if (type == ins_gather) ngather_recieved++;  // Jiayi, debug

    o3core->num_instrs++;
    o3core->num_call_ops += (category == XED_CATEGORY_CALL) ? 1 : 0;
    if (o3core->o3queue_size >= o3core->o3queue_max_size)
    {
      return 0;
      cout << " *_* " << curr_time_;
      o3core->displayO3Queue();
      o3core->displayO3ROB();
      global_q->display();
      ASSERTX(0);
    }
    if (type == ins_gather) ngather++;  // Jiayi, debug
    O3Queue & o3q_entry  = o3core->o3queue[(o3core->o3queue_max_size + o3core->o3queue_head + o3core->o3queue_size)%o3core->o3queue_max_size];
    o3q_entry.state      = o3iqs_not_in_queue;
    o3q_entry.ready_time = curr_time_;
    o3q_entry.waddr      = waddr;
    o3q_entry.wlen       = wlen;
    o3q_entry.raddr      = raddr;
    o3q_entry.raddr2     = raddr2;
    o3q_entry.rlen       = rlen;
    o3q_entry.ip         = ip;
    o3q_entry.type       = type;
    o3q_entry.rr0        = rr0;
    o3q_entry.rr1        = rr1;
    o3q_entry.rr2        = rr2;
    o3q_entry.rr3        = rr3;
    o3q_entry.rw0        = rw0;
    o3q_entry.rw1        = rw1;
    o3q_entry.rw2        = rw2;
    o3q_entry.rw3        = rw3;

    o3core->o3queue_size++;

    if ((raddr != 0 && !is_race_free_application && !o3core->is_private(raddr)) ||
        (raddr != 0 && !is_race_free_application && !o3core->is_private(raddr2)) || 
        (waddr != 0 && !is_race_free_application && !o3core->is_private(waddr)))
    {
      num_available_slot = 0;
    }

    //if (ip != 0)
    //{
    //  cout << curr_time_ << ", " << hex << ip << dec << ", " << category;
    //  cout << ", " << xed_category_enum_t2str((xed_category_enum_t) category) << endl;
    //}

    //if (o3core->o3queue_size + 4 >= o3core->o3queue_max_size) resume = true;
    num_available_slot = ((o3core->o3queue_size + 4 > o3core->o3queue_max_size) ? 0 :
        (o3core->o3queue_max_size - (o3core->o3queue_size + 4)));
  }
  else
  {
    Hthread * hthread = hthreads[hthreadid_];

    if (hthread->mem_acc.empty() == true)
    {
      if (hthread->resume_time <= curr_time_)
      {
        global_q->add_event(curr_time_, hthread->core);
        if (hthread->num == hthreads.size() - 1)
        {
          hthread->core->is_active[hthread->core->hthreads.size() - 1] = true;
        }
        else
        {
          hthread->core->is_active[hthread->num%hthread->core->hthreads.size()] = true;
        }
      }
    }
    ins_type type = (isbranch && isbranchtaken)  ? ins_branch_taken :
      (isbranch && !isbranchtaken)       ? ins_branch_not_taken :
      (category == XED_CATEGORY_X87_ALU || category == XED_CATEGORY_SSE) ? ins_x87 :  // treat an SSE op as an X87 op
      (islock == true)                   ? ins_lock :
      (isunlock == true)                 ? ins_unlock : 
      (isbarrier == true)                ? ins_barrier : no_mem;

    hthread->num_call_ops += (category == XED_CATEGORY_CALL) ? 1 : 0;
    hthread->tlb_rd    = false;
    if (simulate_only_data_caches == false)
    {
      hthread->mem_acc.push(std::pair<ins_type, uint64_t>(type, ip));
    }

    //if (ip != 0)
    //{
    //  cout << curr_time_ << ", " << hex << ip << dec << ", " << category;
    //  cout << ", " << xed_category_enum_t2str((xed_category_enum_t) category) << endl;
    //}

    if (raddr)
    {
      if (rlen % sizeof(uint64_t) != 0)
      {
        rlen = rlen - (rlen % sizeof(uint64_t)) + sizeof(uint64_t);
      }

      while (rlen != 0)
      {
        if (!is_race_free_application && !hthread->is_private(raddr)) num_available_slot = 0;
        hthread->mem_acc.push(std::pair<ins_type, uint64_t>(mem_rd, raddr));
        raddr += sizeof(uint64_t);
        if (raddr2)
        {
          if (!is_race_free_application && !hthread->is_private(raddr2)) num_available_slot = 0; 
          hthread->mem_acc.push(std::pair<ins_type, uint64_t>(mem_rd, raddr2));
          raddr2 += sizeof(uint64_t);
        }
        rlen -= sizeof(uint64_t);
      }
    }

    if (waddr)
    {
      if (wlen % sizeof(uint64_t) != 0)
      {
        wlen = wlen - (wlen % sizeof(uint64_t)) + sizeof(uint64_t);
      }

      while (wlen != 0)
      {
        if (!is_race_free_application && !hthread->is_private(waddr)) num_available_slot = 0;
        hthread->mem_acc.push(std::pair<ins_type, uint64_t>(mem_wr, waddr));
        waddr += sizeof(uint64_t);
        wlen  -= sizeof(uint64_t);
      }
    }

    num_available_slot = ((max_acc_queue_size <= hthread->mem_acc.size()) ? 0 : 
        (max_acc_queue_size - hthread->mem_acc.size()));

  }

  return num_available_slot;
}


void McSim::set_stack_n_size(
    int32_t pth_id,
    ADDRINT stack,
    ADDRINT stacksize)
{
  if (use_o3core == true)
  {
    o3cores[pth_id]->stack     = stack;
    o3cores[pth_id]->stacksize = stacksize;
  }
  else
  {
    hthreads[pth_id]->stack     = stack;
    hthreads[pth_id]->stacksize = stacksize;
  }
}


void McSim::set_active(int32_t pth_id, bool is_active)
{
  if (use_o3core == true)
  {
    o3cores[pth_id]->active    = is_active;
  }
  else
  {
    hthreads[pth_id]->active   = is_active;
  }
}


void McSim::show_l2_cache_summary()
{
  uint32_t num_cache_lines    = 0;
  uint32_t num_i_cache_lines  = 0;
  uint32_t num_e_cache_lines  = 0;
  uint32_t num_s_cache_lines  = 0;
  uint32_t num_m_cache_lines  = 0;
  uint32_t num_tr_cache_lines = 0;

  uint64_t num_destroyed_cache_lines = 0;
  uint64_t cache_line_life_time = 0;
  uint64_t time_between_last_access_and_cache_destroy = 0;

  for (uint32_t i = 0; i < l2s.size(); i++)
  {
    num_destroyed_cache_lines += l2s[i]->num_destroyed_cache_lines;
    cache_line_life_time      += l2s[i]->cache_line_life_time;
    time_between_last_access_and_cache_destroy += l2s[i]->time_between_last_access_and_cache_destroy;

    for (uint32_t j = 0; j < l2s[i]->num_sets; j++)
    {
      for (uint32_t k = 0; k < l2s[i]->num_ways; k++)
      {
        CacheL2::L2Entry * iter = l2s[i]->tags[j][k];
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
  }

  num_cache_lines = num_i_cache_lines + num_e_cache_lines +
    num_s_cache_lines + num_m_cache_lines + num_tr_cache_lines;

  cout << " L2$ (i,e,s,m,tr) ratio=(" 
    << setiosflags(ios::fixed) << setw(4) << 1000 * num_i_cache_lines  / num_cache_lines << ", "
    << setiosflags(ios::fixed) << setw(4) << 1000 * num_e_cache_lines  / num_cache_lines << ", "
    << setiosflags(ios::fixed) << setw(4) << 1000 * num_s_cache_lines  / num_cache_lines << ", "
    << setiosflags(ios::fixed) << setw(4) << 1000 * num_m_cache_lines  / num_cache_lines << ", "
    << setiosflags(ios::fixed) << setw(4) << 1000 * num_tr_cache_lines / num_cache_lines << "), "
    << endl;
  /*  << "avg L2$ line life = ";

  if (num_destroyed_cache_lines == num_destroyed_cache_lines_last_time)
  {
    cout << "NaN, avg time bet last acc to L2$ destroy = NaN" << endl;
  }
  else
  {
    cout << setiosflags(ios::fixed) 
      << (cache_line_life_time - cache_line_life_time_last_time) /
      ((num_destroyed_cache_lines - num_destroyed_cache_lines_last_time) * l2s[0]->process_interval) << ", "
      << "avg time bet last acc to $ destroy = "
      << setiosflags(ios::fixed)
      << (time_between_last_access_and_cache_destroy - time_between_last_access_and_cache_destroy_last_time) /
      ((num_destroyed_cache_lines - num_destroyed_cache_lines_last_time) * l2s[0]->process_interval) 
      << " L2$ cycles" << endl;
  }*/
}


void McSim::update_os_page_req_dist(uint64_t addr)
{
  //if (mcs[0]->display_os_page_usage == true)
  if (hmcs[0]->display_os_page_usage == true)
  {
    uint64_t page_num = addr / (1 << global_q->page_sz_base_bit);
    map<uint64_t, uint64_t>::iterator p_iter = os_page_req_dist.find(page_num);

    if (p_iter != os_page_req_dist.end())
    {
      (p_iter->second)++;
    }
    else
    {
      os_page_req_dist.insert(pair<uint64_t, uint64_t>(page_num, 1));
    }
  }
}
