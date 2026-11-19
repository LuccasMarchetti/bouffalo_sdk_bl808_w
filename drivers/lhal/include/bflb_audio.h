#ifndef _BFLB_AUDIO_H
#define _BFLB_AUDIO_H

#include "bflb_core.h"

/**
 * BL808 / BL606P audio: one driver for two blocks with the same digital
 * front end.
 *   - "audio" (idx 0): codec with 3 ADC (analog microphone or PDM), 2 DAC
 *     (LINEOUT), 2 AEC (copy of the DAC data into the RX FIFO), RX and TX FIFO;
 *   - "aupdm" (idx 1): 3 ADC with PDM input only and a RX FIFO.
 * The FIFO is 32 bits wide; the channels enabled in the FIFO are interleaved
 * in channel order.
 */

/* Sampling rate (ADC: up to 96K; DAC: up to 192K). From the AUPLL at
 * 442.368 MHz (48 kHz family) with GLB_AUDIO_CFG0 auto divider. */
#define AUDIO_RATE_8K   0
#define AUDIO_RATE_16K  1
#define AUDIO_RATE_24K  2
#define AUDIO_RATE_32K  3
#define AUDIO_RATE_48K  4
#define AUDIO_RATE_96K  5
#define AUDIO_RATE_192K 6

/* FIFO sample resolution */
#define AUDIO_RES_16BIT 0
#define AUDIO_RES_20BIT 1

/* Position of the sample MSB in the 32 bit FIFO word (sign extended) */
#define AUDIO_DATA_MSB_31 0
#define AUDIO_DATA_MSB_23 1
#define AUDIO_DATA_MSB_19 2
#define AUDIO_DATA_MSB_15 3

/* ADC input */
#define AUDIO_ADC_SRC_ANALOG 0 /* codec only */
#define AUDIO_ADC_SRC_PDM    1

/* PDM line and edge for an ADC in PDM mode */
#define AUDIO_PDM_0_L 0
#define AUDIO_PDM_0_R 1
#define AUDIO_PDM_1_L 2
#define AUDIO_PDM_1_R 3
#define AUDIO_PDM_2_L 4
#define AUDIO_PDM_2_R 5

/* RX FIFO channels (rx0..rx4) and their sources */
#define AUDIO_RX_CH(n)      (1 << (n))
#define AUDIO_RX_SRC_ADC(n) (n)      /* ADC 0..2 */
#define AUDIO_RX_SRC_AEC(n) (3 + (n)) /* AEC 0..1, codec only: DAC data */
/* TX FIFO channels (tx0, tx1) */
#define AUDIO_TX_CH(n)      (1 << (n))

/* Interrupts (bflb_audio_int_mask/get_intstatus/int_clear) */
#define AUDIO_INT_RX_OVER  (1 << 0)
#define AUDIO_INT_RX_UNDER (1 << 1)
#define AUDIO_INT_RX_AVAIL (1 << 2) /* level: RX count above the threshold */
#define AUDIO_INT_TX_OVER  (1 << 3)
#define AUDIO_INT_TX_UNDER (1 << 4)
#define AUDIO_INT_TX_AVAIL (1 << 5) /* level: TX free count above the threshold */
#define AUDIO_INT_VAD      (1 << 6)
#define AUDIO_INT_VOLUME   (1 << 7) /* a volume/mute ramp finished; no enable bit: clear it in the ISR */

/* How a new volume takes effect (codec only) */
#define AUDIO_VOLUME_FORCE      0 /* at once */
#define AUDIO_VOLUME_RAMP       1 /* ramp, rate 2^ramp_rate samples per 0.5 dB step (reset: 6) */
#define AUDIO_VOLUME_RAMP_ZERO  2 /* ramp, steps at zero crossings (reset mode) */

struct bflb_audio_config_s {
    uint8_t adc_rate;     /* AUDIO_RATE_* */
    uint8_t dac_rate;     /* AUDIO_RATE_*, codec only */
    uint8_t resolution;   /* AUDIO_RES_* (RX and TX) */
    uint8_t data_mode;    /* AUDIO_DATA_MSB_* (RX and TX) */
    uint8_t rx_threshold; /* 0..63 (aupdm) / 0..127 (codec) */
    uint8_t tx_threshold; /* 0..63, codec only */
};

#ifdef __cplusplus
extern "C" {
#endif

void bflb_audio_init(struct bflb_device_s *dev, const struct bflb_audio_config_s *config);
void bflb_audio_deinit(struct bflb_device_s *dev);

/* ADC n: source, PDM line (PDM source) and analog PGA gain in dB (analog
 * source: 0 or 6..42 in 3 dB steps). Also routes rx n to ADC n. */
int bflb_audio_adc_config(struct bflb_device_s *dev, uint8_t adc, uint8_t src, uint8_t pdm_sel, uint8_t pga_db);
int bflb_audio_adc_enable(struct bflb_device_s *dev, uint8_t adc_mask, bool enable);
int bflb_audio_adc_set_hpf(struct bflb_device_s *dev, uint8_t adc, bool k1_en, uint8_t k1, bool k2_en, uint8_t k2);
/* Digital volume in 0.5 dB steps, -255..+255 (0 = 0 dB); mute */
int bflb_audio_adc_set_volume(struct bflb_device_s *dev, uint8_t adc, int16_t half_db);
int bflb_audio_adc_mute(struct bflb_device_s *dev, uint8_t adc, bool mute);
int bflb_audio_set_rx_source(struct bflb_device_s *dev, uint8_t rx, uint8_t src);

/* Codec only: DAC digital path, analog LINEOUT (gain 0..7, as the BL606P
 * driver: 1 = -3 dB) and volume */
int bflb_audio_dac_enable(struct bflb_device_s *dev, uint8_t dac_mask, bool enable);
int bflb_audio_dac_set_volume(struct bflb_device_s *dev, uint8_t dac, int16_t half_db);
int bflb_audio_dac_mute(struct bflb_device_s *dev, uint8_t dac, bool mute);
/* Volume update mode of ADC n (dac = false) or DAC n (dac = true); soft_mute
 * ramps the mute as well. Measured with the reset values (RAMP_ZERO, rate 6):
 * -12 dB settles in ~150 ms at 16 kHz. */
int bflb_audio_volume_mode(struct bflb_device_s *dev, bool dac, uint8_t ch, uint8_t mode, uint8_t ramp_rate,
                           bool soft_mute);
int bflb_audio_aec_enable(struct bflb_device_s *dev, uint8_t aec_mask, bool enable);

/* Internal sine generator into the ADC filters (test, no microphone):
 * step 0..7 sets the frequency. adc_mask selects the ADC inputs. */
int bflb_audio_test_sine(struct bflb_device_s *dev, bool enable, uint8_t step, uint8_t adc_mask);

/* FIFO */
void bflb_audio_rx_start(struct bflb_device_s *dev, uint8_t ch_mask);
void bflb_audio_rx_stop(struct bflb_device_s *dev);
void bflb_audio_rx_flush(struct bflb_device_s *dev);
uint32_t bflb_audio_rx_count(struct bflb_device_s *dev);
uint32_t bflb_audio_rx_read(struct bflb_device_s *dev);
/* Reads up to len words that are already in the FIFO; returns how many */
uint32_t bflb_audio_rx_read_buf(struct bflb_device_s *dev, uint32_t *buf, uint32_t len);
void bflb_audio_tx_start(struct bflb_device_s *dev, uint8_t ch_mask);
void bflb_audio_tx_stop(struct bflb_device_s *dev);
void bflb_audio_tx_flush(struct bflb_device_s *dev);
uint32_t bflb_audio_tx_space(struct bflb_device_s *dev);
void bflb_audio_tx_write(struct bflb_device_s *dev, uint32_t data);

/* DMA request on the FIFO threshold (DMA_REQUEST_AUDIO_RX/TX, DMA_REQUEST_AUPDM_RX) */
void bflb_audio_link_rxdma(struct bflb_device_s *dev, bool enable);
void bflb_audio_link_txdma(struct bflb_device_s *dev, bool enable);
/* FIFO data register for the DMA */
uint32_t bflb_audio_rx_fifo_addr(struct bflb_device_s *dev);
uint32_t bflb_audio_tx_fifo_addr(struct bflb_device_s *dev);

/* mask = true disables the interrupt sources in int_mask */
void bflb_audio_int_mask(struct bflb_device_s *dev, uint32_t int_mask, bool mask);
uint32_t bflb_audio_get_intstatus(struct bflb_device_s *dev);
/* Clears VAD and VOLUME (also drops the volume update bits); the FIFO flags
 * follow the FIFO state (over/under: flush) */
void bflb_audio_int_clear(struct bflb_device_s *dev, uint32_t int_clear);

#ifdef __cplusplus
}
#endif

#endif
