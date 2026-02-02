/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "ran_func_mac.h"
#include <assert.h>

static
const int mod_id = 0;

bool read_mac_sm(void* data)
{
  assert(data != NULL);

  mac_ind_data_t* mac = (mac_ind_data_t*)data;
  //fill_mac_ind_data(mac);

  mac->msg.tstamp = time_now_us();

  NR_UEs_t *UE_info = &RC.nrmac[mod_id]->UE_info;
  size_t num_ues = 0;
  UE_iterator(UE_info->connected_ue_list, ue) {
    if (ue)
      num_ues += 1;
  }

  mac->msg.len_ue_stats = num_ues;
  if(mac->msg.len_ue_stats > 0){
    mac->msg.ue_stats = calloc(mac->msg.len_ue_stats, sizeof(mac_ue_stats_impl_t));
    assert(mac->msg.ue_stats != NULL && "Memory exhausted" );
  }

  size_t i = 0; //TODO
  UE_iterator(UE_info->connected_ue_list, UE) {
    const NR_UE_sched_ctrl_t* sched_ctrl = &UE->UE_sched_ctrl;
    mac_ue_stats_impl_t* rd = &mac->msg.ue_stats[i];

    rd->in_sync = !sched_ctrl->ul_failure;
    rd->nr_cellid = RC.nrrrc[0]->nr_cellid;
    rd->frame = RC.nrmac[mod_id]->frame;
    rd->slot = 0; // previously had slot info, but the gNB runs multiple slots
                  // in parallel, so this has no real meaning

    rd->dl_aggr_tbs = UE->mac_stats.dl.total_bytes;
    rd->ul_aggr_tbs = UE->mac_stats.ul.total_bytes;

    if (is_dl_slot(rd->slot, &RC.nrmac[mod_id]->frame_structure)) {
      rd->dl_curr_tbs = UE->mac_stats.dl.current_bytes;
      rd->dl_sched_rb = UE->mac_stats.dl.current_rbs;
    }
    if (is_ul_slot(rd->slot, &RC.nrmac[mod_id]->frame_structure)) {
      rd->ul_curr_tbs = UE->mac_stats.ul.current_bytes;
      rd->ul_sched_rb = UE->mac_stats.ul.current_rbs;
    }

    rd->rnti = UE->rnti;
    rd->dl_aggr_prb = UE->mac_stats.dl.total_rbs;
    rd->ul_aggr_prb = UE->mac_stats.ul.total_rbs;
    rd->dl_aggr_retx_prb = UE->mac_stats.dl.total_rbs_retx;
    rd->ul_aggr_retx_prb = UE->mac_stats.ul.total_rbs_retx;

    rd->dl_aggr_bytes_sdus = UE->mac_stats.dl.lc_bytes[3];
    rd->ul_aggr_bytes_sdus = UE->mac_stats.ul.lc_bytes[3];

    rd->dl_aggr_sdus = UE->mac_stats.dl.num_mac_sdu;
    rd->ul_aggr_sdus = UE->mac_stats.ul.num_mac_sdu;

    rd->pusch_snr = (float) sched_ctrl->pusch_snrx10 / 10; //: float = -64;
    rd->pucch_snr = (float) sched_ctrl->pucch_snrx10 / 10; //: float = -64;

    rd->wb_cqi = sched_ctrl->CSI_report.cri_ri_li_pmi_cqi_report.wb_cqi_1tb;
    rd->dl_mcs1 = sched_ctrl->dl_bler_stats.mcs;
    rd->dl_bler = sched_ctrl->dl_bler_stats.bler;
    rd->ul_mcs1 = sched_ctrl->ul_bler_stats.mcs;
    rd->ul_bler = sched_ctrl->ul_bler_stats.bler;
    rd->dl_mcs2 = 0;
    rd->ul_mcs2 = 0;
    rd->phr = sched_ctrl->ph;

    rd->pcmax = sched_ctrl->pcmax;
    rd->pmi_cqi_ri = sched_ctrl->CSI_report.cri_ri_li_pmi_cqi_report.ri + 1;
    rd->pmi_cqi_X1 = sched_ctrl->CSI_report.cri_ri_li_pmi_cqi_report.pmi_x1;
    rd->pmi_cqi_X2 = sched_ctrl->CSI_report.cri_ri_li_pmi_cqi_report.pmi_x2;
    rd->raw_rssi = -128.0 + (float)UE->UE_sched_ctrl.raw_rssi * 0.1;
    uint8_t cqi = (UE->UE_sched_ctrl.pucch_snrx10 + 640) / 5.0 / 10.0;
    if (cqi > 15) {
      cqi = 15;
    }
    rd->cqi = cqi;
    if (UE->UE_sched_ctrl.CSI_report.cri_ri_li_pmi_cqi_report.wb_cqi_1tb != 0) {
      rd->cqi = UE->UE_sched_ctrl.CSI_report.cri_ri_li_pmi_cqi_report.wb_cqi_1tb;
    }
    rd->rsrp = UE->mac_stats.num_rsrp_meas > 0 ? UE->mac_stats.cumul_rsrp / UE->mac_stats.num_rsrp_meas : 0;
    rd->dl_qm = nr_get_Qm_dl(UE->UE_sched_ctrl.dl_bler_stats.mcs, UE->current_DL_BWP.mcsTableIdx);
    rd->ul_qm = nr_get_Qm_ul(UE->UE_sched_ctrl.ul_bler_stats.mcs, UE->current_UL_BWP.mcs_table);

    const uint32_t bufferSize = sched_ctrl->estimated_ul_buffer - sched_ctrl->sched_ul_bytes;
    rd->bsr = bufferSize;

    const size_t numDLHarq = 4;
    rd->dl_num_harq = numDLHarq;
    for (uint8_t j = 0; j < numDLHarq; ++j)
      rd->dl_harq[j] = UE->mac_stats.dl.rounds[j];
    rd->dl_harq[numDLHarq] = UE->mac_stats.dl.errors;

    const size_t numUlHarq = 4;
    rd->ul_num_harq = numUlHarq;
    for (uint8_t j = 0; j < numUlHarq; ++j)
      rd->ul_harq[j] = UE->mac_stats.ul.rounds[j];
    rd->ul_harq[numUlHarq] = UE->mac_stats.ul.errors;

    ++i;
  }

  return num_ues > 0;
}

void read_mac_setup_sm(void* data)
{
  assert(data != NULL);

  //mac_ctrl_req_data_t* mac_ctrl_req = (mac_ctrl_req_data_t*)data;

  //sm_ag_if_ans_t ans = {.type = CTRL_OUTCOME_SM_AG_IF_ANS_V0};
  //ans.subs_out.type = APERIODIC_SUBSCRIPTION_FLRC;

  return;
}

sm_ag_if_ans_t write_ctrl_mac_sm(void const* data)
{
  assert(data != NULL);

  mac_ctrl_req_data_t* mac_ctrl_req = (mac_ctrl_req_data_t*)data;

  // SST 0 not defined in standard
  if (mac_ctrl_req->msg.sst > 0) {
    gNB_MAC_INST* mac = RC.nrmac[mod_id];

    uint8_t sst = mac_ctrl_req->msg.sst;
    uint32_t sd = mac_ctrl_req->msg.sd;
    bool dl_create = true;
    bool ul_create = true;

    for (size_t i = 0; i < seq_arr_size(&mac->nssai_config_dl); i++) {
      nssai_config_t* c = seq_arr_at(&mac->nssai_config_dl, i);
      if (c->sst == sst && c->sd == sd) {
        if (mac_ctrl_req->msg.dl_num_prbs == 0) {
          LOG_A(NR_MAC, "Deleting DL SST %d SD %d\n", c->sst, c->sd);
          seq_arr_erase(&mac->nssai_config_dl, c);
        } else {
          c->prb_start = mac_ctrl_req->msg.dl_start_prb;
          c->num_prbs = mac_ctrl_req->msg.dl_num_prbs;
          LOG_A(NR_MAC, "Updating DL SST %d SD %d PRB Start %d num PRBs %d\n", c->sst, c->sd, c->prb_start, c->num_prbs);
        }
        dl_create = false;
        break;
      }
    }
    for (size_t i = 0; i < seq_arr_size(&mac->nssai_config_ul); i++) {
      nssai_config_t* c = seq_arr_at(&mac->nssai_config_ul, i);
      if (c->sst == sst && c->sd == sd) {
        if (mac_ctrl_req->msg.ul_num_prbs == 0) {
          LOG_A(NR_MAC, "Deleting UL SST %d SD %d\n", c->sst, c->sd);
          seq_arr_erase(&mac->nssai_config_ul, c);
        } else {
          c->prb_start = mac_ctrl_req->msg.ul_start_prb;
          c->num_prbs = mac_ctrl_req->msg.ul_num_prbs;
          LOG_A(NR_MAC, "Updating UL SST %d SD %d PRB Start %d num PRBs %d\n", c->sst, c->sd, c->prb_start, c->num_prbs);
        }
        ul_create = false;
        break;
      }
    }
    if (dl_create && mac_ctrl_req->msg.dl_num_prbs > 0) {
      nssai_config_t c;
      c.sst = sst;
      c.sd = sd;
      c.prb_start = mac_ctrl_req->msg.dl_start_prb;
      c.num_prbs = mac_ctrl_req->msg.dl_num_prbs;
      seq_arr_push_back(&mac->nssai_config_dl, &c, sizeof(nssai_config_t));
      LOG_A(NR_MAC, "Creating DL SST %d SD %d PRB Start %d num PRBs %d\n", c.sst, c.sd, c.prb_start, c.num_prbs);
    }

    if (ul_create && mac_ctrl_req->msg.ul_num_prbs > 0) {
      nssai_config_t c;
      c.sst = sst;
      c.sd = sd;
      c.prb_start = mac_ctrl_req->msg.ul_start_prb;
      c.num_prbs = mac_ctrl_req->msg.ul_num_prbs;
      seq_arr_push_back(&mac->nssai_config_ul, &c, sizeof(nssai_config_t));
      LOG_A(NR_MAC, "Creating UL SST %d SD %d PRB Start %d num PRBs %d\n", c.sst, c.sd, c.prb_start, c.num_prbs);
    }
  }

  // BeamID: from 1 to 64
  if (mac_ctrl_req->msg.rx_beam_id > 0 && mac_ctrl_req->msg.tx_beam_id > 0) {
    RU_t* ru = RC.ru[0];
    ru->rfdevice.beam_switching(mac_ctrl_req->msg.rx_beam_id, mac_ctrl_req->msg.tx_beam_id);
  }

  // BWP switching
  if (mac_ctrl_req->msg.ue_id > 0) {
    LOG_I(NR_MAC,
          "MAC SM triggering BWP switching for UE ID %d with dl_bwp_id %d and ul_bwp_id %d\n",
          mac_ctrl_req->msg.ue_id,
          mac_ctrl_req->msg.dl_bwp_id,
          mac_ctrl_req->msg.ul_bwp_id);
    nr_bwp_switching(mod_id, mac_ctrl_req->msg.ue_id, mac_ctrl_req->msg.dl_bwp_id);
  }

  sm_ag_if_ans_t ans = {.type = CTRL_OUTCOME_SM_AG_IF_ANS_V0};
  ans.subs_out.type = APERIODIC_SUBSCRIPTION_FLRC;

  return ans;
}
