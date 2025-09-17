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
#include "openair2/SDAP/nr_sdap/nr_sdap_configuration.h"
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_configuration.h"

void get_sdap_config_ie(NR_DRB_ToAddMod_t *drb_ToAddMod,
                        const int pdusession_id,
                        seq_arr_t *qos,
                        const nr_sdap_configuration_t *sdap_config);

void get_pdcp_config_ie(NR_DRB_ToAddMod_t *drb_ToAddMod,
                        const bool do_drb_integrity,
                        const bool do_drb_ciphering,
                        const bool reestablish,
                        const nr_pdcp_configuration_t *pdcp_config);
