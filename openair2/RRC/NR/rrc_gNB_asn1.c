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

#include <stdbool.h>
#include "seq_arr.h"
#include "NR_DRB-ToAddMod.h"
#include "NR_RadioBearerConfig.h"
#include "common/utils/oai_asn1.h"
#include "common/utils/LOG/log.h"
#include "openair2/SDAP/nr_sdap/nr_sdap_configuration.h"
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_configuration.h"
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_asn1_utils.h"

void get_sdap_config_ie(NR_DRB_ToAddMod_t *drb_ToAddMod,
                        const int pdusession_id,
                        seq_arr_t *qos,
                        const nr_sdap_configuration_t *sdap_config)
{
  DevAssert(drb_ToAddMod);
  // cn-association
  asn1cCalloc(drb_ToAddMod->cnAssociation, cn_association);
  // SDAP
  cn_association->present = NR_DRB_ToAddMod__cnAssociation_PR_sdap_Config;
  asn1cCalloc(cn_association->choice.sdap_Config, sc);
  sc->defaultDRB = true;
  sc->pdu_Session = pdusession_id;
  sc->sdap_HeaderDL = sdap_config->header_dl_absent ? NR_SDAP_Config__sdap_HeaderDL_absent : NR_SDAP_Config__sdap_HeaderDL_present;
  sc->sdap_HeaderUL = sdap_config->header_ul_absent ? NR_SDAP_Config__sdap_HeaderUL_absent : NR_SDAP_Config__sdap_HeaderUL_present;
  // QoS
  asn1cCalloc(sc->mappedQoS_FlowsToAdd, mappedQoS_FlowsToAdd);
  FOR_EACH_SEQ_ARR(pdusession_level_qos_parameter_t *, q, qos)
  {
    NR_QFI_t *qfi = calloc_or_fail(1, sizeof(*qfi));
    *qfi = q->qfi;
    LOG_D(NR_RRC, "Adding QFI %ld to PDU Session %d\n", *qfi, pdusession_id);
    asn1cSeqAdd(&mappedQoS_FlowsToAdd->list, qfi);
  }
  DevAssert(cn_association->present != NR_DRB_ToAddMod__cnAssociation_PR_NOTHING);
}

static NR_PDCP_Config_t *get_default_PDCP_config(const bool drb_integrity,
                                                 const bool drb_ciphering,
                                                 const nr_pdcp_configuration_t *pdcp)
{
  NR_PDCP_Config_t *out = calloc_or_fail(1, sizeof(*out));
  asn1cCallocOne(out->t_Reordering, encode_t_reordering(pdcp->drb.t_reordering));
  if (!drb_ciphering) {
    asn1cCalloc(out->ext1, ext1);
    asn1cCallocOne(ext1->cipheringDisabled, NR_PDCP_Config__ext1__cipheringDisabled_true);
  }
  asn1cCalloc(out->drb, drb);
  asn1cCallocOne(drb->discardTimer, encode_discard_timer(pdcp->drb.discard_timer));
  asn1cCallocOne(drb->pdcp_SN_SizeUL, encode_sn_size_ul(pdcp->drb.sn_size));
  asn1cCallocOne(drb->pdcp_SN_SizeDL, encode_sn_size_dl(pdcp->drb.sn_size));
  drb->headerCompression.present = NR_PDCP_Config__drb__headerCompression_PR_notUsed;
  drb->headerCompression.choice.notUsed = 0;
  if (drb_integrity) {
    asn1cCallocOne(drb->integrityProtection, NR_PDCP_Config__drb__integrityProtection_enabled);
  }
  return out;
}

void get_pdcp_config_ie(NR_DRB_ToAddMod_t *drb_ToAddMod,
                        const bool do_drb_integrity,
                        const bool do_drb_ciphering,
                        const bool reestablish,
                        const nr_pdcp_configuration_t *pdcp_config)
{
  DevAssert(drb_ToAddMod);
  drb_ToAddMod->pdcp_Config = get_default_PDCP_config(do_drb_integrity, do_drb_ciphering, pdcp_config);
  if (reestablish) {
    asn1cCallocOne(drb_ToAddMod->reestablishPDCP, NR_DRB_ToAddMod__reestablishPDCP_true);
  }
}