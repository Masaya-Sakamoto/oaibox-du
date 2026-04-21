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

#ifndef __BEAM_ANTENNA_CONTROL_H__
#define __BEAM_ANTENNA_CONTROL_H__

#include <stdint.h>

typedef struct {
  uint16_t rnti;
  int16_t old_beam_index;
  int16_t new_beam_index;
  int frame;
  int slot;
} beam_switch_event_t;

typedef void (*beam_switch_notify_fn)(const beam_switch_event_t *event);

typedef struct {
  beam_switch_notify_fn on_beam_switch;
  void *priv;
} antenna_control_if_t;

antenna_control_if_t *create_default_antenna_ctrl(void);
void destroy_antenna_ctrl(antenna_control_if_t *ctrl);

#endif /* __BEAM_ANTENNA_CONTROL_H__ */
