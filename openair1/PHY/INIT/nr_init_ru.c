/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PHY/phy_extern.h"
#include "assertions.h"
#include <math.h>
#include "openair1/PHY/defs_RU.h"
#include "openair1/PHY/defs_nr_common.h"
#include "openair1/PHY/defs_gNB.h"
#include "SIMULATION/TOOLS/sim.h"
#include "common/config/config_userapi.h"

#define CIR_CONF_SECTION "cir_conf"

static void init_cir_variables(void* param)
{
  RU_t* ru = (RU_t*)param;
  int nb_tx = ru->nb_tx;
  int nb_rx = ru->nb_rx;
  double path_loss_dB = 0.0;
  double amp_gain_dB = 0.0;
  double noise_power_dB = 0.0;
  int channel_length = 1;

  paramdef_t cir_params[] = {
    {"path_loss_dB",   "Max path gain in CIR dataset [dB] (read-only reference)", 0, .dblptr = &path_loss_dB,   .defdblval = 0.0,    TYPE_DOUBLE, 0},
    {"amp_gain_dB",    "Amplitude gain offset [dB] to compensate path loss",      0, .dblptr = &amp_gain_dB,   .defdblval = 0.0,    TYPE_DOUBLE, 0},
    {"noise_power_dB", "Noise level [dB]",                                        0, .dblptr = &noise_power_dB, .defdblval = -200.0, TYPE_DOUBLE, 0},
    {"channel_length", "Number of delay taps N for CIR convolution",              0, .iptr   = &channel_length, .defintval = 1,      TYPE_INT,    0},
  };
  config_get(config_get_if(), cir_params, sizeofArray(cir_params), CIR_CONF_SECTION);
  LOG_I(NR_PHY,
        "CIR config: path_loss_dB=%.2f, amp_gain_dB=%.2f, noise_power_dB=%.2f, channel_length=%d\n",
        path_loss_dB,
        amp_gain_dB,
        noise_power_dB,
        channel_length);

  // Write params
  pthread_mutex_lock(&ru->proc.mutex_mimo);
  ru->pathLossLinear = pow(10, (path_loss_dB + amp_gain_dB) / 20.0);
  ru->noise_per_sample = pow(10, noise_power_dB / 20.0) * 256; // The useful signal is scaled on the order of 256.
  ru->channel_length = channel_length;
  ru->cir_was_received = false;
  pthread_mutex_unlock(&ru->proc.mutex_mimo);

  // CIR data
  ru->cirMIMO_simulmatrix = malloc16_clear(nb_tx * nb_rx * channel_length * sizeof(cf_t));
  pthread_mutex_lock(&ru->proc.mutex_mimo);
  int l = 0;
  for (int a_tx = 0; a_tx < nb_tx; a_tx++) {
    for (int a_rx = 0; a_rx < nb_rx; a_rx++) {
      ru->cirMIMO_simulmatrix[a_rx * channel_length * nb_tx + l * nb_rx + a_tx].r = 1.0f / ru->pathLossLinear;
      ru->cirMIMO_simulmatrix[a_rx * channel_length * nb_tx + l * nb_rx + a_tx].i = 0.0f;
    }
  }
  pthread_mutex_unlock(&ru->proc.mutex_mimo);

  // Delay index list
  ru->delayindexlist = malloc16_clear(channel_length * sizeof(int));
  pthread_mutex_lock(&ru->proc.mutex_mimo);
  for (int l = 0; l < channel_length; l++) {
    ru->delayindexlist[l] = l;
  }
  pthread_mutex_unlock(&ru->proc.mutex_mimo);
}

static void init_noise(void* param)
{
  RU_t* ru = (RU_t*)param;
  NR_DL_FRAME_PARMS* fp = ru->nr_frame_parms;
  int samples_per_slot = get_samples_per_slot(0, fp) + ru->sf_extension;

  randominit();

  // init noise_array
  ru->noise_array = malloc16_clear(ru->nb_tx * sizeof(cf_t));
  for (int a = 0; a < ru->nb_tx; a++)
    ru->noise_array[a] = malloc16_clear(samples_per_slot * sizeof(cf_t));

  // set noise -> noise_array
  for (int a = 0; a < ru->nb_tx; a++) {
    for (int i = 0; i < samples_per_slot; i++) {
      pthread_mutex_lock(&ru->proc.mutex_noise);
      ru->noise_array[a][i].r = (float)gaussZiggurat(0.0, 1.0);
      ru->noise_array[a][i].i = (float)gaussZiggurat(0.0, 1.0);
      pthread_mutex_unlock(&ru->proc.mutex_noise);
    }
  }
}

void nr_phy_init_RU(RU_t *ru)
{
  NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;

  LOG_D(PHY, "Initializing RU signal buffers (if_south %s) nb_tx %d, nb_rx %d\n", ru_if_types[ru->if_south], ru->nb_tx, ru->nb_rx);

  // copy configuration from gNB[0] in to RU, assume that all gNB instances sharing RU use the same configuration
  // (at least the parts that are needed by the RU, numerology and PRACH)

  int nb_tx_streams = ru->nb_tx;
  int nb_rx_streams = ru->nb_rx;
  LOG_I(NR_PHY, "nb_tx_streams %d, nb_rx_streams %d\n", nb_tx_streams, nb_rx_streams);
  const unsigned int num_symbols = fp->symbols_per_slot * fp->slots_per_frame;
  ru->common.beam_id = malloc16_clear(num_symbols * sizeof(*ru->common.beam_id));
  for (int i = 0; i < num_symbols; i++) {
    ru->common.beam_id[i] = malloc16_clear(nb_tx_streams * sizeof(**ru->common.beam_id));
  }

  if ((nb_tx_streams > fp->nb_antennas_tx) || (nb_rx_streams > fp->nb_antennas_rx))
    LOG_W(NR_PHY, "There could be unused baseband ports because of fewer logical ports.\n");

  if (ru->if_south <= REMOTE_IF5) { // this means REMOTE_IF5 or LOCAL_RF, so allocate memory for time-domain signals 
    // Time-domain signals
    ru->common.txdata = (int32_t**)malloc16(nb_tx_streams * sizeof(int32_t*));
    ru->common.rxdata = (int32_t**)malloc16(nb_rx_streams * sizeof(int32_t*));

    for (int i = 0; i < nb_tx_streams; i++) {
      // Allocate 10 subframes of I/Q TX signal data (time) if not
      ru->common.txdata[i] = (int32_t*)malloc16_clear((ru->sf_extension + fp->samples_per_frame) * sizeof(int32_t));
      LOG_D(PHY,
            "[INIT] common.txdata[%d] = %p (%lu bytes,sf_extension %d)\n",
            i,
            ru->common.txdata[i],
            (ru->sf_extension + fp->samples_per_frame) * sizeof(int32_t),
            ru->sf_extension);
      ru->common.txdata[i] = &ru->common.txdata[i][ru->sf_extension];

      LOG_D(PHY, "[INIT] common.txdata[%d] = %p \n", i, ru->common.txdata[i]);
    }
    for (int i = 0; i < nb_rx_streams; i++) {
      ru->common.rxdata[i] = (int32_t*)malloc16_clear(fp->samples_per_frame * sizeof(int32_t));
    }
  } // IF5 or local RF
  else {
    ru->common.txdata = (int32_t**)NULL;
    ru->common.rxdata = (int32_t**)NULL;
  }
  if (ru->function != NGFI_RRU_IF5) { // we need to do RX/TX RU processing
    LOG_D(PHY, "nb_tx %d\n", ru->nb_tx);
    ru->common.rxdata_7_5kHz = (int32_t**)malloc16(ru->nb_rx*sizeof(int32_t*) );
    for (int i = 0; i < ru->nb_rx; i++) {
      ru->common.rxdata_7_5kHz[i] = (int32_t*)malloc16_clear( 2*fp->samples_per_subframe*2*sizeof(int32_t) );
      LOG_D(PHY, "rxdata_7_5kHz[%d] %p for RU %d\n", i, ru->common.rxdata_7_5kHz[i], ru->idx);
    }

    // allocate precoding input buffers (TX)
    ru->common.txdataF = (int32_t **)malloc16(ru->nb_tx*sizeof(int32_t*));
    // [hna] samples_per_frame without CP
    for(int i = 0; i < ru->nb_tx; ++i)
      ru->common.txdataF[i] = (int32_t *)malloc16_clear(fp->samples_per_slot_wCP * sizeof(int32_t));

    // allocate IFFT input buffers (TX)
    ru->common.txdataF_BF = (int32_t **)malloc16(nb_tx_streams * sizeof(int32_t*));
    LOG_D(PHY, "[INIT] common.txdata_BF= %p (%lu bytes)\n", ru->common.txdataF_BF, nb_tx_streams * sizeof(int32_t *));
    for (int i = 0; i < nb_tx_streams; i++) {
      ru->common.txdataF_BF[i] =
          (int32_t *)malloc16_clear(fp->samples_per_slot_wCP * sizeof(int32_t));
      LOG_D(PHY, "txdataF_BF[%d] %p for RU %d\n", i, ru->common.txdataF_BF[i], ru->idx);
    }
    // allocate FFT output buffers (RX)
    ru->common.rxdataF = (int32_t**)malloc16(nb_rx_streams * sizeof(int32_t*));
    for (int i = 0; i < nb_rx_streams; i++) {
      // allocate 4 slots of I/Q signal data (frequency)
      int size = RU_RX_SLOT_DEPTH * fp->symbols_per_slot * fp->ofdm_symbol_size;
      ru->common.rxdataF[i] = (int32_t*)malloc16_clear(sizeof(**ru->common.rxdataF) * size);
      LOG_D(PHY, "rxdataF[%d] %p for RU %d\n", i, ru->common.rxdataF[i], ru->idx);
    }

    AssertFatal(ru->num_gNB <= NUMBER_OF_gNB_MAX, "gNB instances %d > %d\n", ru->num_gNB,NUMBER_OF_gNB_MAX);

    LOG_D(PHY, "[INIT] %s() ru->num_gNB:%d \n", __FUNCTION__, ru->num_gNB);
  } // !=IF5

  // Init CIR variables
  init_cir_variables(ru); // init: convolution matrix, channel_length, pathLossLinear, noise_per_sample
  init_noise(ru); // init: noise array
  ru->common.buffboundary = 0;
  ru->common.circular_buff_size = fp->samples_per_frame;
  ru->common.circular_buff = malloc16_clear(ru->nb_rx * sizeof(cf_t));
  for (int a = 0; a < ru->nb_rx; a++)
    ru->common.circular_buff[a] = malloc16_clear(ru->common.circular_buff_size * sizeof(cf_t));
  int samples_per_slot = fp->samples_per_slot0 + ru->sf_extension;
  ru->common.noise_array = malloc16_clear(ru->nb_rx * samples_per_slot * sizeof(cf_t));
  ru->common.simul_input =
      malloc16_clear(ru->nb_tx * ru->channel_length * samples_per_slot * sizeof(cf_t)); // allocate MIMO temporary store fields
}

void nr_phy_free_RU(RU_t *ru)
{
  LOG_D(PHY, "Freeing RU signal buffers (if_south %s) nb_tx %d\n", ru_if_types[ru->if_south], ru->nb_tx);
  int nb_tx_streams = ru->nb_tx;
  int nb_rx_streams = ru->nb_rx;

  if (ru->if_south <= REMOTE_IF5) { // this means REMOTE_IF5 or LOCAL_RF, so free memory for time-domain signals
    // Hack: undo what is done at allocation
    for (int i = 0; i < nb_tx_streams; i++) {
      int32_t *p = &ru->common.txdata[i][-ru->sf_extension];
      free_and_zero(p);
    }
    free_and_zero(ru->common.txdata);

    for (int i = 0; i < nb_rx_streams; i++)
      free_and_zero(ru->common.rxdata[i]);
    free_and_zero(ru->common.rxdata);
  } // else: IF5 or local RF -> nothing to free()

  if (ru->function != NGFI_RRU_IF5) { // we need to do RX/TX RU processing
    for (int i = 0; i < ru->nb_rx; i++)
      free_and_zero(ru->common.rxdata_7_5kHz[i]);
    free_and_zero(ru->common.rxdata_7_5kHz);

    // free beamforming input buffers (TX)
    for (int i = 0; i < ru->nb_tx; i++)
      free_and_zero(ru->common.txdataF[i]);
    free_and_zero(ru->common.txdataF);

    // free IFFT input buffers (TX)
    for (int i = 0; i < nb_tx_streams; i++)
      free_and_zero(ru->common.txdataF_BF[i]);
    free_and_zero(ru->common.txdataF_BF);

    // free FFT output buffers (RX)
    for (int i = 0; i < nb_rx_streams; i++)
      free_and_zero(ru->common.rxdataF[i]);
    free_and_zero(ru->common.rxdataF);

    NR_DL_FRAME_PARMS *fp = ru->nr_frame_parms;
    for (int i = 0; i < fp->symbols_per_slot * fp->slots_per_frame; ++i)
      free_and_zero(ru->common.beam_id[i]);
    free_and_zero(ru->common.beam_id);
  }

  PHY_VARS_gNB *gNB0 = ru->gNB_list[0];
  gNB0->num_RU--;
  DevAssert(gNB0->num_RU >= 0);

  // Free CIR variables
  free(ru->common.simul_input);
  free(ru->delayindexlist);
  free(ru->cirMIMO_simulmatrix);
  for (int a = 0; a < ru->nb_tx; a++)
    free(ru->noise_array[a]);
  free(ru->noise_array);
  for (int a = 0; a < ru->nb_rx; a++)
    free(ru->common.circular_buff[a]);
  free(ru->common.circular_buff);
  free(ru->common.noise_array);
}
