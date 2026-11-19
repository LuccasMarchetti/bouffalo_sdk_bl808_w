/**
 * BL808 / BL606P audio blocks:
 *   - audio codec (AUDIO_BASE, 0x20055000): 3 ADC (analog or PDM), 2 DAC,
 *     2 AEC (echo reference), RX and TX FIFO, volume, VAD, analog front end;
 *   - aupdm (AUPDM_BASE, 0x2000ac00): the same digital front end with PDM
 *     input only, 3 ADC and a RX FIFO.
 *
 * Offsets and fields from the BL606P register file
 * (components/multimedia/xcodec/src/bl606p/drv/msp_audio_reg.h) and the
 * PDM chapter of the BL808 reference manual (audpdm_* registers).
 */
#ifndef __HARDWARE_AUDIO_REG_H__
#define __HARDWARE_AUDIO_REG_H__

/* Common to both blocks */
#define AUDIO_TOP_OFFSET            (0x00)
#define AUDIO_ITF_OFFSET            (0x04)
#define AUDIO_ADC_0_OFFSET          (0x08)
#define AUDIO_ADC_1_OFFSET          (0x0C)
#define AUDIO_DAC_0_OFFSET          (0x10)
#define AUDIO_DAC_1_OFFSET          (0x14)
#define AUDIO_AEC_0_OFFSET          (0x18)
#define AUDIO_PDM_0_OFFSET          (0x1C)
#define AUDIO_DBG_0_OFFSET          (0x24)
#define AUDIO_DBG_1_OFFSET          (0x28)
#define AUDIO_DBG_2_OFFSET          (0x2C)
#define AUDIO_DBG_3_OFFSET          (0x30)
#define AUDIO_DBG_4_OFFSET          (0x34)
#define AUDIO_RX_FIFO_CTRL_OFFSET   (0x80)
#define AUDIO_RX_FIFO_STATUS_OFFSET (0x84)
#define AUDIO_RX_FIFO_DATA_OFFSET   (0x88)
/* Codec only */
#define AUDIO_TX_FIFO_CTRL_OFFSET   (0x8C)
#define AUDIO_TX_FIFO_STATUS_OFFSET (0x90)
#define AUDIO_TX_FIFO_DATA_OFFSET   (0x94)
#define AUDIO_ADC_H_OFFSET          (0x400) /* "hardware" ADC volume (AGC path) */
#define AUDIO_ADC_S0_OFFSET         (0x408) /* S1 at +0x08, S2 at +0x10 */
#define AUDIO_ADC_S0_MISC_OFFSET    (0x40C)
#define AUDIO_STATUS_OFFSET         (0x430)
#define AUDIO_DAC_S0_OFFSET         (0x434) /* S1 at +0x08 */
#define AUDIO_DAC_S0_MISC_OFFSET    (0x438)
#define AUDIO_VAD_CTRL_0_OFFSET     (0x600)
#define AUDIO_CODEC_POWER1_OFFSET   (0x700)
#define AUDIO_CODEC_POWER2_OFFSET   (0x704)
#define AUDIO_CODEC_DAC_OFFSET      (0x708)
#define AUDIO_CODEC_DAC_RAMP_OFFSET (0x70C)
#define AUDIO_CODEC_ADC0_OFFSET     (0x710) /* ADC1 at +0x04, ADC2 at +0x08 */
/* aupdm only: plain 9 bit volume, no ramp or update bit */
#define AUPDM_ADC_S0_OFFSET         (0x38) /* S1 at +0x04, S2 at +0x08 */

/* 0x00 top */
#define AUDIO_CKG_EN                (1 << 0U)
#define AUDIO_DAC_ITF_INV_SEL       (1 << 1U)
#define AUDIO_ADC_ITF_INV_SEL       (1 << 2U)
#define AUDIO_PDM_ITF_INV_SEL       (1 << 3U)
#define AUDIO_DAC_RATE_SHIFT        (24U)
#define AUDIO_DAC_RATE_MASK         (0x7 << AUDIO_DAC_RATE_SHIFT)
#define AUDIO_ADC_RATE_SHIFT        (28U)
#define AUDIO_ADC_RATE_MASK         (0x7 << AUDIO_ADC_RATE_SHIFT)

/* 0x04 itf */
#define AUDIO_ADC_EN_SHIFT          (0U) /* adc_0_en .. adc_2_en */
#define AUDIO_DAC_EN_SHIFT          (3U) /* dac_0_en, dac_1_en */
#define AUDIO_AEC_EN_SHIFT          (5U) /* aec_0_en, aec_1_en */
#define AUDIO_RX_SEL_SHIFT(n)       (13U + 3U * (n)) /* rx0..rx4 source, 3 bits */
#define AUDIO_TX_SEL_SHIFT(n)       (28U + (n))      /* tx0, tx1 source, 1 bit */
#define AUDIO_ADC_ITF_EN            (1 << 30U)
#define AUDIO_DAC_ITF_EN            (1 << 31U)

/* 0x0C adc_1: high pass filter, 10 bits per ADC */
#define AUDIO_ADC_HPF_SHIFT(n)      (10U * (n))
#define AUDIO_ADC_HPF_K1_MASK       (0xf)
#define AUDIO_ADC_HPF_K1_EN         (1 << 4U)
#define AUDIO_ADC_HPF_K2_SHIFT      (5U)
#define AUDIO_ADC_HPF_K2_EN         (1 << 9U)

/* 0x10 dac_0: ADC source (analog/PDM), PDM levels, mixer 0 */
#define AUDIO_ADC_PDM_H_SHIFT       (0U)
#define AUDIO_ADC_PDM_L_SHIFT       (6U)
#define AUDIO_ADC_SRC_SHIFT(n)      (12U + 3U * (n)) /* 1 = PDM */
#define AUDIO_MIX_0_SEL_SHIFT       (21U)
#define AUDIO_MIX_0_MODE_SHIFT      (23U)

/* 0x1C pdm_0 */
#define AUDIO_PDM_EN_SHIFT          (0U) /* pdm_0_en .. pdm_2_en */
#define AUDIO_ADC_PDM_SEL_SHIFT(n)  (3U + 3U * (n))

/* 0x24 dbg_0 */
#define AUDIO_SIN_EN                (1 << 0U)
#define AUDIO_SIN_STEP_SHIFT        (1U)
#define AUDIO_SIN_STEP_MASK         (0x7 << AUDIO_SIN_STEP_SHIFT)
/* 0x2C dbg_2 */
#define AUDIO_ADC_IN_TEST_SEL(n)    (1 << (23U + (n)))
/* 0x30 dbg_3 */
#define AUDIO_DAC_IN_TEST_SEL(n)    (1 << (22U + (n)))

/* 0x80 / 0x8C fifo ctrl */
#define AUDIO_FIFO_FLUSH            (1 << 0U)
#define AUDIO_FIFO_O_INT_EN         (1 << 1U)
#define AUDIO_FIFO_U_INT_EN         (1 << 2U)
#define AUDIO_FIFO_A_INT_EN         (1 << 3U)
#define AUDIO_FIFO_DRQ_EN           (1 << 4U)
#define AUDIO_FIFO_DATA_RES         (1 << 5U) /* 1 = 20 bit */
#define AUDIO_FIFO_CH_EN_SHIFT      (8U)
#define AUDIO_RX_FIFO_CH_EN_MASK    (0x1f << AUDIO_FIFO_CH_EN_SHIFT)
#define AUDIO_TX_FIFO_CH_EN_MASK    (0x3 << AUDIO_FIFO_CH_EN_SHIFT)
#define AUDIO_FIFO_DRQ_CNT_SHIFT    (14U)
#define AUDIO_FIFO_DRQ_CNT_MASK     (0x3 << AUDIO_FIFO_DRQ_CNT_SHIFT)
#define AUDIO_FIFO_TRG_LEVEL_SHIFT  (16U)
#define AUDIO_RX_FIFO_TRG_LEVEL_MASK (0x7f << AUDIO_FIFO_TRG_LEVEL_SHIFT)
#define AUDIO_TX_FIFO_TRG_LEVEL_MASK (0x3f << AUDIO_FIFO_TRG_LEVEL_SHIFT)
#define AUDIO_FIFO_DATA_MODE_SHIFT  (24U)
#define AUDIO_FIFO_DATA_MODE_MASK   (0x3 << AUDIO_FIFO_DATA_MODE_SHIFT)

/* 0x84 / 0x90 fifo status */
#define AUDIO_FIFO_O_INT            (1 << 1U)
#define AUDIO_FIFO_U_INT            (1 << 2U)
#define AUDIO_FIFO_A_INT            (1 << 4U)
#define AUDIO_FIFO_CNT_SHIFT        (16U)
#define AUDIO_RX_FIFO_CNT_MASK      (0x7f << AUDIO_FIFO_CNT_SHIFT)
#define AUDIO_TX_FIFO_CNT_MASK      (0x3f << AUDIO_FIFO_CNT_SHIFT)

/* 0x408 + 8n / 0x434 + 8n volume (codec) */
#define AUDIO_VOL_RMP_RATE_SHIFT    (2U)
#define AUDIO_VOL_RMP_RATE_MASK     (0xf << AUDIO_VOL_RMP_RATE_SHIFT)
#define AUDIO_VOL_CTRL_MODE_SHIFT   (10U)
#define AUDIO_VOL_CTRL_MODE_MASK    (0x3 << AUDIO_VOL_CTRL_MODE_SHIFT)
#define AUDIO_VOL_MUTE_SOFT         (1 << 30U)
#define AUDIO_VOL_UPDATE            (1 << 12U)
#define AUDIO_VOL_SHIFT             (13U)
#define AUDIO_VOL_MASK              (0x1ff << AUDIO_VOL_SHIFT)
#define AUDIO_VOL_MUTE              (1 << 31U)
/* 0x40C / 0x438 misc */
#define AUDIO_VOL_ALL_UPDATE        (1 << 0U)
#define AUDIO_VOL_ALL_MUTE          (1 << 1U) /* adc_s012_mute / dac_s01_mute */

/* 0x430 status: volume done (adc_s0..s2, dac_s0..s1) with a clear bit next to it */
#define AUDIO_STATUS_VOL_INTS       ((1 << 6U) | (1 << 8U) | (1 << 10U) | (1 << 16U) | (1 << 18U))
#define AUDIO_STATUS_VOL_CLRS       (AUDIO_STATUS_VOL_INTS << 1)
#define AUDIO_STATUS_VAD_INT        (1 << 25U)
#define AUDIO_STATUS_VAD_INT_CLR    (1 << 26U)

/* 0x600 vad_ctrl_0 */
#define AUDIO_VAD_EN                (1 << 0U)

/* 0x700 codec_power1 */
#define AUDIO_MICBIAS_PU            (1 << 0U)
#define AUDIO_MICBIAS_CHOP_EN       (1 << 1U)
#define AUDIO_MICBIAS_VSEL_SHIFT    (4U)
#define AUDIO_MICBIAS_VSEL_MASK     (0x3 << AUDIO_MICBIAS_VSEL_SHIFT)
#define AUDIO_CODEC_VRDA_EN         (1 << 12U)
#define AUDIO_CODEC_IBIAS_SHIFT     (16U)
#define AUDIO_CODEC_IBIAS_MASK      (0x7 << AUDIO_CODEC_IBIAS_SHIFT)

/* 0x704 codec_power2 */
#define AUDIO_HPBG_PU               (1 << 4U)
#define AUDIO_HPBG_ADJ_FINE_SHIFT   (8U)
#define AUDIO_HPBG_ADJ_COARSE_SHIFT (12U)
#define AUDIO_LDO25_PU              (1 << 22U)

/* 0x708 codec_dac: right in bits 15:0, left in bits 31:16 */
#define AUDIO_DACR_SHIFT            (0U)
#define AUDIO_DACL_SHIFT            (16U)
#define AUDIO_DAC_LINEOUT_GAIN_MASK (0x7)
#define AUDIO_DAC_EN_LINEOUT        (1 << 3U)
#define AUDIO_DAC_EN_DAC            (1 << 4U)
#define AUDIO_DAC_LINEOUT_MODE      (1 << 6U)
#define AUDIO_DAC_SDAC_LINEOUT      (1 << 10U)

/* 0x70C codec_dac_ramp */
#define AUDIO_DAC_LO_VREF_RAMP_DCL_EN (1 << 0U)
#define AUDIO_DAC_PU_VREF_RAMP        (1 << 2U)

/* 0x710 + 4n codec_adcN */
#define AUDIO_ADC_PGA_GAIN_SHIFT    (8U)
#define AUDIO_ADC_PGA_GAIN_MASK     (0xf << AUDIO_ADC_PGA_GAIN_SHIFT)
#define AUDIO_ADC_ICTRL_ADC2_SHIFT  (14U)
#define AUDIO_ADC_ICTRL_ADC2_MASK   (0x3 << AUDIO_ADC_ICTRL_ADC2_SHIFT)
#define AUDIO_ADC_EN_SDM            (1 << 20U)
#define AUDIO_ADC_EN_PGA            (1 << 21U)

#endif /* __HARDWARE_AUDIO_REG_H__ */
