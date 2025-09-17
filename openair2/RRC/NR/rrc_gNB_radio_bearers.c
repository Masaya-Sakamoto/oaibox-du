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

#include "rrc_gNB_radio_bearers.h"
#include <stddef.h>
#include "E1AP_RLC-Mode.h"
#include "RRC/NR/nr_rrc_defs.h"
#include "T.h"
#include "asn_internal.h"
#include "assertions.h"
#include "common/platform_constants.h"
#include "common/utils/T/T.h"
#include "ngap_messages_types.h"
#include "oai_asn1.h"
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_asn1_utils.h"
#include "common/utils/alg/find.h"

static bool eq_pdu_session_id(const void *vval, const void *vit)
{
  const int id = *(const int *)vval;
  const rrc_pdu_session_param_t *elem = vit;
  return elem->param.pdusession_id == id;
}

/** @brief Free pdusession_t */
void free_pdusession(void *ptr)
{
  const rrc_pdu_session_param_t *elem = ptr;
  free_byte_array(elem->param.nas_pdu);
}

/** @brief Retrieves PDU Session for the input ID
 *  @return pointer to the found PDU Session, NULL if not found */
rrc_pdu_session_param_t *find_pduSession(seq_arr_t *seq, int id)
{
  DevAssert(seq);
  elm_arr_t elm = find_if(seq, &id, eq_pdu_session_id);
  if (elm.found)
    return (rrc_pdu_session_param_t *)elm.it;
  return NULL;
}

/** @brief Adds a new PDU Session to the list
 *  @return pointer to the new PDU Session */
rrc_pdu_session_param_t *add_pduSession(seq_arr_t *sessions_ptr, const pdusession_t *in)
{
  DevAssert(sessions_ptr);
  DevAssert(in);

  if (seq_arr_size(sessions_ptr) == NGAP_MAX_PDU_SESSION) {
    LOG_W(NR_RRC, "Reached maximum number of PDU Session = %ld\n", seq_arr_size(sessions_ptr));
    return NULL;
  }

  rrc_pdu_session_param_t new = {.param = *in, .status = PDU_SESSION_STATUS_NEW, .xid = -1};
  seq_arr_push_back(sessions_ptr, &new, sizeof(rrc_pdu_session_param_t));
  rrc_pdu_session_param_t *added = find_pduSession(sessions_ptr, in->pdusession_id);
  DevAssert(added);
  LOG_I(NR_RRC, "Added PDU Session %d, (total nb of sessions = %ld)\n", in->pdusession_id, seq_arr_size(sessions_ptr));

  return added;
}

/** @brief Removes a PDU Session from the list by ID
 *  @return true if successfully removed, false if not found */
bool rm_pduSession(seq_arr_t *sessions, int pdusession_id)
{
  DevAssert(sessions);

  rrc_pdu_session_param_t *session = find_pduSession(sessions, pdusession_id);

  if (session) {
    LOG_I(NR_RRC, "Removing PDU Session %d from RRC setup list\n", pdusession_id);
    seq_arr_erase(sessions, session);
    return true;
  }

  LOG_W(NR_RRC, "PDU Session %d not found to remove\n", pdusession_id);
  return false;
}

/** @brief Add drb_t item in the UE context list for @param pdusession_id */
drb_t *nr_rrc_add_drb(seq_arr_t *drb_ptr, int pdusession_id, nr_pdcp_configuration_t *pdcp)
{
  DevAssert(drb_ptr);

  // Get next available DRB ID
  int drb_id = 0;
  for (drb_id = 1; drb_id < MAX_DRBS_PER_UE + 1; drb_id++) {
    if (get_drb(drb_ptr, drb_id) == NULL)
      break;
  }

  if (!drb_id) {
    LOG_E(NR_RRC, "Cannot set up new DRB for pdusession_id=%d - reached maximum capacity\n", pdusession_id);
    return NULL;
  }

  // Add item to the list
  drb_t in = {.drb_id = drb_id, .pdusession_id = pdusession_id, .pdcp_config = *pdcp};
  seq_arr_push_back(drb_ptr, &in, sizeof(drb_t));
  drb_t *out = get_drb(drb_ptr, drb_id);
  DevAssert(out);
  LOG_I(NR_RRC, "Added DRB %d to established list (PDU Session ID=%d, total DRBs = %ld)\n", out->drb_id, pdusession_id, seq_arr_size(drb_ptr));
  return out;
}

static bool eq_drb_id(const void *vval, const void *vit)
{
  const int id = *(const int *)vval;
  const drb_t *elem = (const drb_t *)vit;
  return elem->drb_id == id;
}

/** @brief Retrieves DRB for the input ID
 *  @return pointer to the found DRB, NULL if not found */
drb_t *get_drb(seq_arr_t *seq, int id)
{
  DevAssert(id > 0 && id <= MAX_DRBS_PER_UE);
  elm_arr_t elm = find_if(seq, &id, eq_drb_id);
  if (elm.found)
    return (drb_t *)elm.it;
  return NULL;
}

/** @brief Free pdusession_t */
void free_drb(void *ptr)
{
  // do nothing
}

rrc_pdu_session_param_t *find_pduSession_from_drbId(gNB_RRC_UE_t *ue, int drb_id)
{
  const drb_t *drb = get_drb(&ue->drbs, drb_id);
  if (!drb) {
    LOG_E(NR_RRC, "UE %d: DRB %d not found\n", ue->rrc_ue_id, drb_id);
    return NULL;
  }
  int id = drb->pdusession_id;
  return find_pduSession(&ue->pduSessions, id);
}

static bool eq_drb_pdu_session_id(const void *vval, const void *vit)
{
  const int *id = (const int *)vval;
  const drb_t *elem = (const drb_t *)vit;
  return elem->pdusession_id == *id;
}

/** @brief Finds the first DRB with the given PDU session ID.
 *  @return Pointer to matching drb_t or NULL if not found. */
static drb_t *get_drb_by_pdusession_id(seq_arr_t *seq, int pdusession_id)
{
  elm_arr_t elm = find_if(seq, &pdusession_id, eq_drb_pdu_session_id);
  if (elm.found)
    return (drb_t *)elm.it;
  return NULL;
}

/** @brief Removes all DRBs associated with a given PDU session ID */
bool rm_drbs(seq_arr_t *drbs, int pdusession_id)
{
  DevAssert(drbs);

  drb_t *drb;
  while ((drb = get_drb_by_pdusession_id(drbs, pdusession_id)) != NULL) {
    LOG_I(NR_RRC, "Removing DRB ID %d associated with PDU Session ID %d\n", drb->drb_id, pdusession_id);
    seq_arr_erase(drbs, drb);
    return true;
  }

  LOG_W(NR_RRC, "DRB ID %d (PDU Session ID %d) not found to remove\n", drb->drb_id, pdusession_id);
  return true;
}

bearer_context_pdcp_config_t set_bearer_context_pdcp_config(const nr_pdcp_configuration_t pdcp,
                                                            bool um_on_default_drb,
                                                            const nr_redcap_ue_cap_t *redcap_cap)
{
  bearer_context_pdcp_config_t out = {0};
  if (redcap_cap && redcap_cap->support_of_redcap_r17 && !redcap_cap->pdcp_drb_long_sn_redcap_r17) {
    LOG_I(NR_RRC, "UE is RedCap without long PDCP SN support: overriding PDCP SN size to 12\n");
    out.pDCP_SN_Size_DL = NR_PDCP_Config__drb__pdcp_SN_SizeDL_len12bits;
    out.pDCP_SN_Size_UL = NR_PDCP_Config__drb__pdcp_SN_SizeUL_len12bits;
  } else {
    out.pDCP_SN_Size_DL = encode_sn_size_dl(pdcp.drb.sn_size);
    out.pDCP_SN_Size_UL = encode_sn_size_ul(pdcp.drb.sn_size);
  }
  out.discardTimer = encode_discard_timer(pdcp.drb.discard_timer);
  out.reorderingTimer = encode_t_reordering(pdcp.drb.t_reordering);
  out.rLC_Mode = um_on_default_drb ? E1AP_RLC_Mode_rlc_um_bidirectional : E1AP_RLC_Mode_rlc_am;
  out.pDCP_Reestablishment = false;
  return out;
}

static bool eq_qfi(const void *vval, const void *vit)
{
  const int id = *(const int *)vval;
  const nr_rrc_qos_t *elem = (const nr_rrc_qos_t *)vit;
  return elem->qos.qfi == id;
}

/** @brief Retrieves mapped QoS in UE context for the input @param qfi
 *  @return pointer to the found QoS, NULL if not found */
nr_rrc_qos_t *find_qos(seq_arr_t *seq, int qfi)
{
  DevAssert(seq);
  elm_arr_t elm = find_if(seq, &qfi, eq_qfi);
  if (elm.found)
    return (nr_rrc_qos_t *)elm.it;
  return NULL;
}

/** @brief Adds a new QoS @param in to UE context the list
 *  @note A UE can have up to 64 QoS flows which can be multiplexed
 *  into one or more DRBs. The QFI is the unique ID of the mapping.
 *  @return pointer to the newly added QoS */
nr_rrc_qos_t *add_qos(seq_arr_t *qos, const pdusession_level_qos_parameter_t *in)
{
  DevAssert(qos);
  DevAssert(in);

  if (seq_arr_size(qos) == MAX_QOS_FLOWS) {
    LOG_W(NR_RRC, "Reached maximum number of QoS flows = %ld\n", seq_arr_size(qos));
    return NULL;
  }

  nr_rrc_qos_t item = { .qos = *in };
  seq_arr_push_back(qos, &item, sizeof(nr_rrc_qos_t));

  // Double check successful add
  nr_rrc_qos_t *added = find_qos(qos, in->qfi);
  DevAssert(added);
  LOG_I(NR_RRC, "Added QoS flow with qfi=%d, total number of QoS flows = %ld\n", in->qfi, seq_arr_size(qos));

  // Only one QoS flow is supported
  AssertFatal(seq_arr_size(qos) == 1, "only 1 Qos flow supported\n");

  return added;
}
