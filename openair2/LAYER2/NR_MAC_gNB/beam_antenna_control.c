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
 */

#include "beam_antenna_control.h"

#include "common/utils/LOG/log.h"

#include <stdlib.h>

static void default_beam_switch_notify(const beam_switch_event_t *event)
{
  LOG_I(NR_MAC,
        "[AntennaCtrl] Beam switch: RNTI=0x%04x beam %d -> %d at (%d.%d)\n",
        event->rnti,
        event->old_beam_index,
        event->new_beam_index,
        event->frame,
        event->slot);
}

antenna_control_if_t *create_default_antenna_ctrl(void)
{
  antenna_control_if_t *ctrl = calloc(1, sizeof(*ctrl));
  if (ctrl == NULL)
    return NULL;

  ctrl->on_beam_switch = default_beam_switch_notify;
  ctrl->priv = NULL;
  return ctrl;
}

void destroy_antenna_ctrl(antenna_control_if_t *ctrl)
{
  free(ctrl);
}
