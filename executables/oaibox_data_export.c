//
// Created by user on 14/10/21.
// Modified for file output
//
#ifndef OAIBOX_DATA_EXPORT
#define OAIBOX_DATA_EXPORT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h> // usleep()

#include <softmodem-common.h>
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "openair2/RRC/NR/rrc_gNB_UE_context.h"
#include "PHY/defs_gNB.h"

// ソケット設定の代わりにファイルパスを定義
#define OUTPUT_FILE_PREFIX "/tmp/oaibox_stats"
#define UPDATE_INTERVAL 1000 // Milliseconds
#define BUFFER_SIZE 65535

// 引数を sockfd から FILE* に変更
void func(FILE *fp)
{
  uint64_t timestamp = 0;

  // ループ条件: グローバルな終了フラグ(oai_exit)のみチェック
  while (!oai_exit) {
    // Get useconds until the next run and usleep for that amount of time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t current_timestamp = tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
    int useconds = (int)(timestamp + (UPDATE_INTERVAL * 1000) - current_timestamp);
    if (useconds < 0 || useconds > (UPDATE_INTERVAL * 1000)) {
      useconds = 0;
    }
    usleep(useconds);
    // Update timestamp to the current timestamp
    timestamp = current_timestamp + useconds;

    long physCellId = RC.nrmac[0]->common_channels[0].ServingCellConfigCommon->physCellId
                          ? *RC.nrmac[0]->common_channels[0].ServingCellConfigCommon->physCellId
                          : 0;

    uint32_t buffer_len = 0;
    char buffer[BUFFER_SIZE] = {0};
    snprintf(buffer,
             BUFFER_SIZE,
             "{\"id\": %ld, "
             "\"frame\": %d, "
             "\"slot\": %d, "
             "\"pci\": %ld, "
             "\"dlCarrierFreq\": %lu, "
             "\"ulCarrierFreq\": %lu, "
             "\"ues\": [",
             RC.nrrrc[0]->nr_cellid,
             RC.nrmac[0]->frame,
             RC.nrmac[0]->slot,
             physCellId,
             RC.gNB[0]->frame_parms.dl_CarrierFreq,
             RC.gNB[0]->frame_parms.ul_CarrierFreq);

    int ue_counter = 0;

    UE_iterator (RC.nrmac[0]->UE_info.list, UE) {
      if (ue_counter > 0) {
        buffer_len = strlen(buffer);
        snprintf(&buffer[buffer_len], BUFFER_SIZE - buffer_len, ", ");
      }
      ue_counter++;

      uint32_t ue_id = 0;
      NR_mac_stats_t *mac_stats = &UE->mac_stats;
      long rsrp = mac_stats->num_rsrp_meas > 0 ? mac_stats->cumul_rsrp / mac_stats->num_rsrp_meas : 0;
      double rsrq = NAN;
      double sinr = NAN;
      double ho_elapsed_ms = 0.0;
      float pucch_snr = (float)UE->UE_sched_ctrl.pucch_snrx10 / 10.0;
      float pusch_snr = (float)UE->UE_sched_ctrl.pusch_snrx10 / 10.0;
      float rssi = (float)UE->UE_sched_ctrl.raw_rssi / 10.0;
      uint8_t cqi = (UE->UE_sched_ctrl.pucch_snrx10 + 640) / 5.0 / 10.0;
      if (cqi > 15) {
        cqi = 15;
      }

      // RRC Measurements values
      struct rrc_gNB_ue_context_s *ue_context_p = rrc_gNB_get_ue_context_by_rnti_any_du(RC.nrrrc[0], UE->rnti);
      if (ue_context_p) {
        ue_id = ue_context_p->ue_context.amf_ue_ngap_id;

        if (ue_context_p->ue_context.measResults) {
          if (ue_context_p->ue_context.measResults->measResultServingMOList.list.count > 1)
            LOG_W(RRC,
                  "Received %d MeasResultServMO, but handling only 1!\n",
                  ue_context_p->ue_context.measResults->measResultServingMOList.list.count);

          NR_MeasResultServMO_t *measresultservmo = ue_context_p->ue_context.measResults->measResultServingMOList.list.array[0];
          NR_MeasResultNR_t *measresultnr = &measresultservmo->measResultServingCell;
          NR_MeasQuantityResults_t *mqr = measresultnr->measResult.cellResults.resultsSSB_Cell;

          if (mqr != NULL) {
            rsrp = *mqr->rsrp - 156;
            rsrq = (float)(*mqr->rsrq - 87) / 2.0f;
            sinr = (float)(*mqr->sinr - 46) / 2.0f;
          }
        }
        ho_elapsed_ms = ue_context_p->ue_context.ho_elapsed_ms;
      }

      buffer_len = strlen(buffer);
      snprintf(&buffer[buffer_len],
               BUFFER_SIZE - buffer_len,
               "{"
               "\"ueId\": \"%d\", "
               "\"rnti\": \"%04x\", "
               "\"inSync\": %d, "
               "\"dlBytes\": %" PRIu64
               ", "
               "\"dlMcs\": %d, "
               "\"dlBler\": %f, "
               "\"ulBytes\": %" PRIu64
               ", "
               "\"ulMcs\": %d, "
               "\"ulBler\": %f, "
               "\"ri\": %d, "
               "\"pmi\": \"(%d,%d)\", "
               "\"phr\": %d, "
               "\"pcmax\": %d, ",
               ue_id,
               UE->rnti,
               !UE->UE_sched_ctrl.ul_failure,
               UE->mac_stats.dl.total_bytes, // sum byte values from all LCIDs
               UE->UE_sched_ctrl.dl_bler_stats.mcs,
               UE->UE_sched_ctrl.dl_bler_stats.bler,
               UE->mac_stats.ul.total_bytes,
               UE->UE_sched_ctrl.ul_bler_stats.mcs,
               UE->UE_sched_ctrl.ul_bler_stats.bler,
               UE->UE_sched_ctrl.CSI_report.cri_ri_li_pmi_cqi_report.ri + 1,
               UE->UE_sched_ctrl.CSI_report.cri_ri_li_pmi_cqi_report.pmi_x1,
               UE->UE_sched_ctrl.CSI_report.cri_ri_li_pmi_cqi_report.pmi_x2,
               UE->UE_sched_ctrl.ph,
               UE->UE_sched_ctrl.pcmax);

      if (UE->UE_sched_ctrl.CSI_report.cri_ri_li_pmi_cqi_report.wb_cqi_1tb != 0) {
        cqi = UE->UE_sched_ctrl.CSI_report.cri_ri_li_pmi_cqi_report.wb_cqi_1tb;
      }
      if (!isnan(rsrq)) {
        buffer_len = strlen(buffer);
        snprintf(&buffer[buffer_len], BUFFER_SIZE - buffer_len, "\"rsrq\": %.1f, ", rsrq);
      }
      if (!isnan(sinr)) {
        buffer_len = strlen(buffer);
        snprintf(&buffer[buffer_len], BUFFER_SIZE - buffer_len, "\"sinr\": %.1f, ", sinr);
      }
      if (ho_elapsed_ms > 0.0) {
        buffer_len = strlen(buffer);
        snprintf(&buffer[buffer_len], BUFFER_SIZE - buffer_len, "\"hoElapsedMs\": %.1f, ", ho_elapsed_ms);
      }

      buffer_len = strlen(buffer);
      snprintf(&buffer[buffer_len],
               BUFFER_SIZE - buffer_len,
               "\"rsrp\": %ld, "
               "\"rssi\": %.1f, "
               "\"cqi\": %d, "
               "\"pucchSnr\": %.1f, "
               "\"puschSnr\": %.1f",
               rsrp,
               rssi,
               cqi,
               pucch_snr,
               pusch_snr);

      // I/Q symbols
      if (IS_SOFTMODEM_EXPORT_MOD_SYMBOLS_ENABLED) {
        // 注: オリジナルのロジックは長いのでそのまま保持します
        buffer_len = strlen(buffer);
        snprintf(&buffer[buffer_len], BUFFER_SIZE - buffer_len, ", \"modSymbols\": [");
        NR_gNB_PUSCH *pusch = &RC.gNB[0]->pusch_vars[0];
        int buffer_length = ceil_mod(pusch->rb_size * NR_NB_SC_PER_RB, 16);
        int k_factor = max((pusch->num_symbols * pusch->rb_size * NR_NB_SC_PER_RB) / 1000, 1);
        for (int symbol = pusch->start_symbol; symbol < pusch->start_symbol + pusch->num_symbols; symbol++) {
          c16_t *pusch_dataF = (c16_t *)&pusch->rxdataF_comp[0][symbol * buffer_length];
          int nr_re = pusch->ul_valid_re_per_slot[symbol];
          int m1 = symbol == pusch->start_symbol + pusch->num_symbols - 1 ? 1 : 0;
          for (int k = 0; k < nr_re - m1; k += k_factor) {
            buffer_len = strlen(buffer);
            snprintf(&buffer[buffer_len], BUFFER_SIZE - buffer_len, "[%d, %d], ", pusch_dataF[k].r, pusch_dataF[k].i);
          }
          if (m1 == 1) {
            buffer_len = strlen(buffer);
            snprintf(&buffer[buffer_len], BUFFER_SIZE - buffer_len, "[%d, %d]", pusch_dataF[nr_re - 1].r, pusch_dataF[nr_re - 1].i);
          }
        }
        buffer_len = strlen(buffer);
        snprintf(&buffer[buffer_len], BUFFER_SIZE - buffer_len, "]");
      }
      buffer_len = strlen(buffer);
      snprintf(&buffer[buffer_len], BUFFER_SIZE - buffer_len, "}");
    }
    buffer_len = strlen(buffer);
    snprintf(&buffer[buffer_len], BUFFER_SIZE - buffer_len, "],");

    if (RC.gNB[0]->segments_count > 0 && RC.gNB[0]->ldpc_iterations_count > 0) {
      buffer_len = strlen(buffer);
      snprintf(&buffer[buffer_len],
               BUFFER_SIZE - buffer_len,
               " \"avgLdpcIterations\": %.1f,",
               RC.gNB[0]->ldpc_iterations_count / (float)RC.gNB[0]->segments_count);
      RC.gNB[0]->segments_count = 0;
      RC.gNB[0]->ldpc_iterations_count = 0;
    }

    buffer_len = strlen(buffer);
    snprintf(&buffer[buffer_len], BUFFER_SIZE - buffer_len, " \"timestamp\": %ju}\n", timestamp / 1000);

    // ソケット送信 (write) を ファイル書き込み (fprintf) に変更
    buffer_len = strlen(buffer);
    if (buffer_len + 1 < BUFFER_SIZE) {
      if (fprintf(fp, "%s", buffer) < 0) {
          printf("OAIBOX: Error writing to file\n");
      }
      // 重要: バッファリングせず即時ディスクに書き込む
      fflush(fp);
    } else {
      printf("OAIBOX: Error preparing buffer, buffer too small\n");
    }

  } // End while(!oai_exit)
}

void *oaibox_data_export_func()
{
  // SIGPIPE処理はファイル書き込みでは不要なため削除可能ですが、
  // 他のスレッドへの影響を避けるため残しても無害です。
  // sigaction(SIGPIPE, &(struct sigaction){{SIG_IGN}}, NULL);

  char filename[256];
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  // ファイル名を生成 (例: /tmp/oaibox_stats_20231027_153000.json)
  // %Y=年, %m=月, %d=日, %H=時, %M=分, %S=秒
  if (strftime(filename, sizeof(filename), OUTPUT_FILE_PREFIX "_%Y%m%d_%H%M%S.json", t) == 0) {
      // 万が一バッファ不足等で生成失敗した場合はデフォルト名を使用
      sprintf(filename, "%s_unknown.json", OUTPUT_FILE_PREFIX);
  }

  printf("OAIBOX: Opening file for data export: %s\n", filename);

  // ファイルを書き込みモード("w")でオープン。
  // 既存の内容を消したくない場合は "a" (append) に変更してください。
  FILE *fp = fopen(filename, "w");
  if (fp == NULL) {
    perror("OAIBOX: Failed to open output file");
    return NULL;
  }

  // データ収集・書き込みループへ
  func(fp);

  // ループを抜けたらクローズ
  fclose(fp);
  printf("OAIBOX: Data export finished.\n");

  return NULL;
}

#endif