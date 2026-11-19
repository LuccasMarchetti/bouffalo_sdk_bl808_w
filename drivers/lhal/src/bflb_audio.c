#include "bflb_audio.h"
#include "bflb_mtimer.h"
#include "hardware/audio_reg.h"

/* GLB: clock gates, software resets and the audio clock dividers */
#define AUDIO_GLB_BASE        (0x20000000)
#define AUDIO_GLB_AUDIO_CFG0  (AUDIO_GLB_BASE + 0x340)
#define AUDIO_GLB_SWRST_CFG0  (AUDIO_GLB_BASE + 0x540)
#define AUDIO_GLB_SWRST_CFG1  (AUDIO_GLB_BASE + 0x544)
#define AUDIO_GLB_CGEN_CFG1   (AUDIO_GLB_BASE + 0x584)
#define AUDIO_GLB_CGEN_CFG2   (AUDIO_GLB_BASE + 0x588)
#define AUDIO_GLB_CODEC_BIT   (1 << 21) /* cgen_cfg2, swrst_cfg0 (GLB_AHB_MCU_SW_AUDIO = 21) */
#define AUDIO_GLB_AUPDM_BIT   (1 << 28) /* cgen_cfg1, swrst_cfg1 (GLB_AHB_MCU_SW_AUPDM = 60) */
/* audio_cfg0: PDM, ADC and DAC clock enables, automatic dividers */
#define AUDIO_GLB_CFG0_PDM_EN (1 << 7)
#define AUDIO_GLB_CFG0_ADC_EN (1 << 15)
#define AUDIO_GLB_CFG0_DAC_EN (1 << 23)
#define AUDIO_GLB_CFG0_AUTO   (1U << 31)

#define AUDIO_IS_CODEC(dev) ((dev)->idx == 0)

static void audio_glb_reset(uint32_t reg, uint32_t bit)
{
    uint32_t regval = getreg32(reg);

    putreg32(regval & ~bit, reg);
    putreg32(regval | bit, reg);
    putreg32(regval & ~bit, reg);
}

static void audio_codec_power_on(uint32_t reg_base)
{
    uint32_t regval;

    /* Same sequence as the BL606P codec driver (Audio_Poweron_init):
     * band gap trimmed for a 2.5 V LDO, ADC2 op-amp bias at maximum */
    regval = getreg32(reg_base + AUDIO_CODEC_POWER2_OFFSET);
    regval &= ~((0xf << AUDIO_HPBG_ADJ_COARSE_SHIFT) | (0xf << AUDIO_HPBG_ADJ_FINE_SHIFT));
    regval |= (0x8 << AUDIO_HPBG_ADJ_COARSE_SHIFT) | (0x8 << AUDIO_HPBG_ADJ_FINE_SHIFT);
    putreg32(regval, reg_base + AUDIO_CODEC_POWER2_OFFSET);

    regval = getreg32(reg_base + AUDIO_CODEC_ADC0_OFFSET);
    regval |= AUDIO_ADC_ICTRL_ADC2_MASK;
    putreg32(regval, reg_base + AUDIO_CODEC_ADC0_OFFSET);

    regval = getreg32(reg_base + AUDIO_CODEC_POWER1_OFFSET);
    regval &= ~AUDIO_CODEC_IBIAS_MASK;
    regval |= AUDIO_CODEC_VRDA_EN | (5 << AUDIO_CODEC_IBIAS_SHIFT);
    putreg32(regval, reg_base + AUDIO_CODEC_POWER1_OFFSET);

    regval = getreg32(reg_base + AUDIO_CODEC_POWER2_OFFSET);
    regval |= AUDIO_LDO25_PU | AUDIO_HPBG_PU;
    putreg32(regval, reg_base + AUDIO_CODEC_POWER2_OFFSET);

    bflb_mtimer_delay_ms(10);
}

static uint32_t audio_fifo_ctrl(const struct bflb_audio_config_s *config, uint8_t threshold, uint32_t level_mask)
{
    uint32_t regval = 0;

    if (config->resolution == AUDIO_RES_20BIT) {
        regval |= AUDIO_FIFO_DATA_RES;
    }
    regval |= ((uint32_t)config->data_mode << AUDIO_FIFO_DATA_MODE_SHIFT) & AUDIO_FIFO_DATA_MODE_MASK;
    /* drq_cnt = 0: the DMA request uses the same threshold as the interrupt */
    regval |= ((uint32_t)threshold << AUDIO_FIFO_TRG_LEVEL_SHIFT) & level_mask;
    return regval;
}

void bflb_audio_init(struct bflb_device_s *dev, const struct bflb_audio_config_s *config)
{
    uint32_t reg_base = dev->reg_base;
    uint32_t regval;

    if (AUDIO_IS_CODEC(dev)) {
        putreg32(getreg32(AUDIO_GLB_CGEN_CFG2) | AUDIO_GLB_CODEC_BIT, AUDIO_GLB_CGEN_CFG2);
        audio_glb_reset(AUDIO_GLB_SWRST_CFG0, AUDIO_GLB_CODEC_BIT);
    } else {
        putreg32(getreg32(AUDIO_GLB_CGEN_CFG1) | AUDIO_GLB_AUPDM_BIT, AUDIO_GLB_CGEN_CFG1);
        audio_glb_reset(AUDIO_GLB_SWRST_CFG1, AUDIO_GLB_AUPDM_BIT);
    }

    /* The source is the AUPLL; the dividers follow adc_rate/dac_rate */
    regval = getreg32(AUDIO_GLB_AUDIO_CFG0);
    regval |= AUDIO_GLB_CFG0_AUTO | AUDIO_GLB_CFG0_ADC_EN | AUDIO_GLB_CFG0_DAC_EN | AUDIO_GLB_CFG0_PDM_EN;
    putreg32(regval, AUDIO_GLB_AUDIO_CFG0);

    if (AUDIO_IS_CODEC(dev)) {
        audio_codec_power_on(reg_base);
    }

    regval = getreg32(reg_base + AUDIO_TOP_OFFSET);
    regval &= ~(AUDIO_ADC_RATE_MASK | AUDIO_DAC_RATE_MASK);
    regval |= ((uint32_t)config->adc_rate << AUDIO_ADC_RATE_SHIFT) & AUDIO_ADC_RATE_MASK;
    regval |= ((uint32_t)config->dac_rate << AUDIO_DAC_RATE_SHIFT) & AUDIO_DAC_RATE_MASK;
    regval |= AUDIO_CKG_EN;
    putreg32(regval, reg_base + AUDIO_TOP_OFFSET);

    putreg32(audio_fifo_ctrl(config, config->rx_threshold, AUDIO_RX_FIFO_TRG_LEVEL_MASK) | AUDIO_FIFO_FLUSH,
             reg_base + AUDIO_RX_FIFO_CTRL_OFFSET);
    putreg32(audio_fifo_ctrl(config, config->rx_threshold, AUDIO_RX_FIFO_TRG_LEVEL_MASK),
             reg_base + AUDIO_RX_FIFO_CTRL_OFFSET);
    if (AUDIO_IS_CODEC(dev)) {
        putreg32(audio_fifo_ctrl(config, config->tx_threshold, AUDIO_TX_FIFO_TRG_LEVEL_MASK) | AUDIO_FIFO_FLUSH,
                 reg_base + AUDIO_TX_FIFO_CTRL_OFFSET);
        putreg32(audio_fifo_ctrl(config, config->tx_threshold, AUDIO_TX_FIFO_TRG_LEVEL_MASK),
                 reg_base + AUDIO_TX_FIFO_CTRL_OFFSET);
    }

    /* FIFO interfaces on, every ADC/DAC/AEC off until configured */
    regval = getreg32(reg_base + AUDIO_ITF_OFFSET);
    regval &= ~(0x7f << AUDIO_ADC_EN_SHIFT);
    regval |= AUDIO_ADC_ITF_EN;
    if (AUDIO_IS_CODEC(dev)) {
        regval |= AUDIO_DAC_ITF_EN;
    }
    putreg32(regval, reg_base + AUDIO_ITF_OFFSET);

    if (AUDIO_IS_CODEC(dev)) {
        /* Besides the per channel mute there is a mute for all ADC and one
         * for both DAC (the BL606P driver always clears them), and the
         * "hardware" ADC volume at 0x400 */
        regval = getreg32(reg_base + AUDIO_ADC_S0_MISC_OFFSET);
        putreg32(regval & ~AUDIO_VOL_ALL_MUTE, reg_base + AUDIO_ADC_S0_MISC_OFFSET);
        regval = getreg32(reg_base + AUDIO_DAC_S0_MISC_OFFSET);
        putreg32(regval & ~AUDIO_VOL_ALL_MUTE, reg_base + AUDIO_DAC_S0_MISC_OFFSET);
        regval = getreg32(reg_base + AUDIO_ADC_H_OFFSET);
        regval &= ~(AUDIO_VOL_MUTE | AUDIO_VOL_MASK);
        putreg32(regval | AUDIO_VOL_UPDATE, reg_base + AUDIO_ADC_H_OFFSET);
    }
    for (uint8_t i = 0; i < 3; i++) {
        bflb_audio_adc_set_volume(dev, i, 0);
        bflb_audio_adc_mute(dev, i, false);
    }
    if (AUDIO_IS_CODEC(dev)) {
        for (uint8_t i = 0; i < 2; i++) {
            bflb_audio_dac_set_volume(dev, i, 0);
            bflb_audio_dac_mute(dev, i, false);
        }
    }
}

void bflb_audio_deinit(struct bflb_device_s *dev)
{
    uint32_t reg_base = dev->reg_base;

    bflb_audio_rx_stop(dev);
    putreg32(0, reg_base + AUDIO_ITF_OFFSET);
    putreg32(getreg32(reg_base + AUDIO_TOP_OFFSET) & ~AUDIO_CKG_EN, reg_base + AUDIO_TOP_OFFSET);
    if (AUDIO_IS_CODEC(dev)) {
        bflb_audio_tx_stop(dev);
        putreg32(0, reg_base + AUDIO_CODEC_DAC_OFFSET);
        putreg32(getreg32(AUDIO_GLB_CGEN_CFG2) & ~AUDIO_GLB_CODEC_BIT, AUDIO_GLB_CGEN_CFG2);
    } else {
        putreg32(getreg32(AUDIO_GLB_CGEN_CFG1) & ~AUDIO_GLB_AUPDM_BIT, AUDIO_GLB_CGEN_CFG1);
    }
}

int bflb_audio_adc_config(struct bflb_device_s *dev, uint8_t adc, uint8_t src, uint8_t pdm_sel, uint8_t pga_db)
{
    uint32_t reg_base = dev->reg_base;
    uint32_t regval;

    if (adc > 2 || pdm_sel > AUDIO_PDM_2_R) {
        return -EINVAL;
    }
    if (src == AUDIO_ADC_SRC_ANALOG) {
        if (!AUDIO_IS_CODEC(dev)) {
            return -ENOTSUP;
        }
        if (pga_db != 0 && (pga_db < 6 || pga_db > 42 || pga_db % 3 != 0)) {
            return -EINVAL;
        }
    } else if (src != AUDIO_ADC_SRC_PDM) {
        return -EINVAL;
    }

    regval = getreg32(reg_base + AUDIO_DAC_0_OFFSET);
    regval &= ~(1 << AUDIO_ADC_SRC_SHIFT(adc));
    if (src == AUDIO_ADC_SRC_PDM) {
        regval |= (1 << AUDIO_ADC_SRC_SHIFT(adc));
    }
    putreg32(regval, reg_base + AUDIO_DAC_0_OFFSET);

    if (src == AUDIO_ADC_SRC_PDM) {
        regval = getreg32(reg_base + AUDIO_PDM_0_OFFSET);
        regval &= ~(0x7 << AUDIO_ADC_PDM_SEL_SHIFT(adc));
        regval |= ((uint32_t)pdm_sel << AUDIO_ADC_PDM_SEL_SHIFT(adc));
        regval |= (1 << (AUDIO_PDM_EN_SHIFT + pdm_sel / 2));
        putreg32(regval, reg_base + AUDIO_PDM_0_OFFSET);
    } else {
        /* analog: microphone bias, modulator and PGA */
        regval = getreg32(reg_base + AUDIO_CODEC_POWER1_OFFSET);
        regval |= AUDIO_MICBIAS_PU | AUDIO_MICBIAS_CHOP_EN;
        putreg32(regval, reg_base + AUDIO_CODEC_POWER1_OFFSET);

        regval = getreg32(reg_base + AUDIO_CODEC_ADC0_OFFSET + 4 * adc);
        regval &= ~AUDIO_ADC_PGA_GAIN_MASK;
        regval |= ((uint32_t)(pga_db / 3) << AUDIO_ADC_PGA_GAIN_SHIFT) & AUDIO_ADC_PGA_GAIN_MASK;
        regval |= AUDIO_ADC_EN_SDM | AUDIO_ADC_EN_PGA;
        putreg32(regval, reg_base + AUDIO_CODEC_ADC0_OFFSET + 4 * adc);
    }

    return bflb_audio_set_rx_source(dev, adc, AUDIO_RX_SRC_ADC(adc));
}

int bflb_audio_adc_enable(struct bflb_device_s *dev, uint8_t adc_mask, bool enable)
{
    uint32_t regval;

    if (adc_mask & ~0x7) {
        return -EINVAL;
    }
    regval = getreg32(dev->reg_base + AUDIO_ITF_OFFSET);
    if (enable) {
        regval |= ((uint32_t)adc_mask << AUDIO_ADC_EN_SHIFT);
    } else {
        regval &= ~((uint32_t)adc_mask << AUDIO_ADC_EN_SHIFT);
    }
    putreg32(regval, dev->reg_base + AUDIO_ITF_OFFSET);
    return 0;
}

int bflb_audio_adc_set_hpf(struct bflb_device_s *dev, uint8_t adc, bool k1_en, uint8_t k1, bool k2_en, uint8_t k2)
{
    uint32_t regval;
    uint32_t field = 0;

    if (adc > 2 || k1 > 15 || k2 > 15) {
        return -EINVAL;
    }
    field |= k1 & AUDIO_ADC_HPF_K1_MASK;
    field |= (k1_en ? AUDIO_ADC_HPF_K1_EN : 0);
    field |= ((uint32_t)k2 << AUDIO_ADC_HPF_K2_SHIFT);
    field |= (k2_en ? AUDIO_ADC_HPF_K2_EN : 0);

    regval = getreg32(dev->reg_base + AUDIO_ADC_1_OFFSET);
    regval &= ~(0x3ff << AUDIO_ADC_HPF_SHIFT(adc));
    regval |= field << AUDIO_ADC_HPF_SHIFT(adc);
    putreg32(regval, dev->reg_base + AUDIO_ADC_1_OFFSET);
    return 0;
}

static int audio_set_volume(uint32_t reg, int16_t half_db)
{
    uint32_t regval;

    if (half_db < -255 || half_db > 255) {
        return -EINVAL;
    }
    regval = getreg32(reg);
    regval &= ~AUDIO_VOL_MASK;
    regval |= (((uint32_t)half_db & 0x1ff) << AUDIO_VOL_SHIFT);
    regval |= AUDIO_VOL_UPDATE;
    putreg32(regval, reg);
    return 0;
}

int bflb_audio_adc_set_volume(struct bflb_device_s *dev, uint8_t adc, int16_t half_db)
{
    if (adc > 2) {
        return -EINVAL;
    }
    if (!AUDIO_IS_CODEC(dev)) {
        /* aupdm: volume in bits 8:0, no update bit */
        if (half_db < -255 || half_db > 255) {
            return -EINVAL;
        }
        putreg32((uint32_t)half_db & 0x1ff, dev->reg_base + AUPDM_ADC_S0_OFFSET + 4 * adc);
        return 0;
    }
    return audio_set_volume(dev->reg_base + AUDIO_ADC_S0_OFFSET + 8 * adc, half_db);
}

int bflb_audio_adc_mute(struct bflb_device_s *dev, uint8_t adc, bool mute)
{
    uint32_t reg = dev->reg_base + AUDIO_ADC_S0_OFFSET + 8 * adc;
    uint32_t regval;

    if (adc > 2) {
        return -EINVAL;
    }
    if (!AUDIO_IS_CODEC(dev)) {
        return mute ? -ENOTSUP : 0;
    }
    regval = getreg32(reg);
    regval = mute ? (regval | AUDIO_VOL_MUTE) : (regval & ~AUDIO_VOL_MUTE);
    putreg32(regval, reg);
    return 0;
}

int bflb_audio_set_rx_source(struct bflb_device_s *dev, uint8_t rx, uint8_t src)
{
    uint32_t regval;

    if (rx > 4 || src > 4 || (!AUDIO_IS_CODEC(dev) && src > 2)) {
        return -EINVAL;
    }
    regval = getreg32(dev->reg_base + AUDIO_ITF_OFFSET);
    regval &= ~(0x7 << AUDIO_RX_SEL_SHIFT(rx));
    regval |= ((uint32_t)src << AUDIO_RX_SEL_SHIFT(rx));
    putreg32(regval, dev->reg_base + AUDIO_ITF_OFFSET);
    return 0;
}

int bflb_audio_dac_enable(struct bflb_device_s *dev, uint8_t dac_mask, bool enable)
{
    uint32_t reg_base = dev->reg_base;
    uint32_t regval;
    uint32_t ana = 0;

    if (!AUDIO_IS_CODEC(dev)) {
        return -ENOTSUP;
    }
    if (dac_mask & ~0x3) {
        return -EINVAL;
    }

    /* tx n feeds DAC n */
    regval = getreg32(reg_base + AUDIO_ITF_OFFSET);
    regval &= ~((1 << AUDIO_TX_SEL_SHIFT(0)) | (1 << AUDIO_TX_SEL_SHIFT(1)));
    regval |= (1 << AUDIO_TX_SEL_SHIFT(1));
    if (enable) {
        regval |= ((uint32_t)dac_mask << AUDIO_DAC_EN_SHIFT);
    } else {
        regval &= ~((uint32_t)dac_mask << AUDIO_DAC_EN_SHIFT);
    }
    putreg32(regval, reg_base + AUDIO_ITF_OFFSET);

    /* Analog: DAC 0 = left, DAC 1 = right (LINEOUT, gain -3 dB as the BL606P) */
    ana = AUDIO_DAC_SDAC_LINEOUT | AUDIO_DAC_LINEOUT_MODE | AUDIO_DAC_EN_DAC | AUDIO_DAC_EN_LINEOUT | 1;
    regval = getreg32(reg_base + AUDIO_CODEC_DAC_OFFSET);
    for (uint8_t i = 0; i < 2; i++) {
        uint32_t shift = (i == 0) ? AUDIO_DACL_SHIFT : AUDIO_DACR_SHIFT;

        if (!(dac_mask & (1 << i))) {
            continue;
        }
        regval &= ~((ana | AUDIO_DAC_LINEOUT_GAIN_MASK) << shift);
        if (enable) {
            regval |= ana << shift;
        }
    }
    putreg32(regval, reg_base + AUDIO_CODEC_DAC_OFFSET);

    if (enable) {
        regval = getreg32(reg_base + AUDIO_CODEC_DAC_RAMP_OFFSET);
        regval &= ~AUDIO_DAC_LO_VREF_RAMP_DCL_EN;
        regval |= AUDIO_DAC_PU_VREF_RAMP;
        putreg32(regval, reg_base + AUDIO_CODEC_DAC_RAMP_OFFSET);
    }
    return 0;
}

int bflb_audio_dac_set_volume(struct bflb_device_s *dev, uint8_t dac, int16_t half_db)
{
    if (!AUDIO_IS_CODEC(dev)) {
        return -ENOTSUP;
    }
    if (dac > 1) {
        return -EINVAL;
    }
    return audio_set_volume(dev->reg_base + AUDIO_DAC_S0_OFFSET + 8 * dac, half_db);
}

int bflb_audio_dac_mute(struct bflb_device_s *dev, uint8_t dac, bool mute)
{
    uint32_t reg = dev->reg_base + AUDIO_DAC_S0_OFFSET + 8 * dac;
    uint32_t regval;

    if (!AUDIO_IS_CODEC(dev)) {
        return -ENOTSUP;
    }
    if (dac > 1) {
        return -EINVAL;
    }
    regval = getreg32(reg);
    regval = mute ? (regval | AUDIO_VOL_MUTE) : (regval & ~AUDIO_VOL_MUTE);
    putreg32(regval, reg);
    return 0;
}

int bflb_audio_volume_mode(struct bflb_device_s *dev, bool dac, uint8_t ch, uint8_t mode, uint8_t ramp_rate,
                           bool soft_mute)
{
    uint32_t reg;
    uint32_t regval;

    if (!AUDIO_IS_CODEC(dev)) {
        return -ENOTSUP;
    }
    if (ch > (dac ? 1 : 2) || mode > AUDIO_VOLUME_RAMP_ZERO || ramp_rate > 15) {
        return -EINVAL;
    }
    reg = dev->reg_base + (dac ? AUDIO_DAC_S0_OFFSET : AUDIO_ADC_S0_OFFSET) + 8 * ch;
    regval = getreg32(reg);
    regval &= ~(AUDIO_VOL_CTRL_MODE_MASK | AUDIO_VOL_RMP_RATE_MASK | AUDIO_VOL_MUTE_SOFT);
    regval |= ((uint32_t)mode << AUDIO_VOL_CTRL_MODE_SHIFT) | ((uint32_t)ramp_rate << AUDIO_VOL_RMP_RATE_SHIFT);
    regval |= soft_mute ? AUDIO_VOL_MUTE_SOFT : 0;
    putreg32(regval, reg);
    return 0;
}

int bflb_audio_aec_enable(struct bflb_device_s *dev, uint8_t aec_mask, bool enable)
{
    uint32_t regval;

    if (!AUDIO_IS_CODEC(dev)) {
        return -ENOTSUP;
    }
    if (aec_mask & ~0x3) {
        return -EINVAL;
    }
    regval = getreg32(dev->reg_base + AUDIO_ITF_OFFSET);
    if (enable) {
        regval |= ((uint32_t)aec_mask << AUDIO_AEC_EN_SHIFT);
    } else {
        regval &= ~((uint32_t)aec_mask << AUDIO_AEC_EN_SHIFT);
    }
    putreg32(regval, dev->reg_base + AUDIO_ITF_OFFSET);
    return 0;
}

int bflb_audio_test_sine(struct bflb_device_s *dev, bool enable, uint8_t step, uint8_t adc_mask)
{
    uint32_t reg_base = dev->reg_base;
    uint32_t regval;

    if (step > 7 || (adc_mask & ~0x7)) {
        return -EINVAL;
    }
    regval = getreg32(reg_base + AUDIO_DBG_2_OFFSET);
    for (uint8_t i = 0; i < 3; i++) {
        regval &= ~AUDIO_ADC_IN_TEST_SEL(i);
        if (enable && (adc_mask & (1 << i))) {
            regval |= AUDIO_ADC_IN_TEST_SEL(i);
        }
    }
    putreg32(regval, reg_base + AUDIO_DBG_2_OFFSET);

    regval = getreg32(reg_base + AUDIO_DBG_0_OFFSET);
    regval &= ~(AUDIO_SIN_EN | AUDIO_SIN_STEP_MASK);
    if (enable) {
        regval |= AUDIO_SIN_EN | ((uint32_t)step << AUDIO_SIN_STEP_SHIFT);
    }
    putreg32(regval, reg_base + AUDIO_DBG_0_OFFSET);
    return 0;
}

void bflb_audio_rx_start(struct bflb_device_s *dev, uint8_t ch_mask)
{
    uint32_t regval = getreg32(dev->reg_base + AUDIO_RX_FIFO_CTRL_OFFSET);

    regval &= ~AUDIO_RX_FIFO_CH_EN_MASK;
    regval |= ((uint32_t)ch_mask << AUDIO_FIFO_CH_EN_SHIFT) & AUDIO_RX_FIFO_CH_EN_MASK;
    putreg32(regval, dev->reg_base + AUDIO_RX_FIFO_CTRL_OFFSET);
}

void bflb_audio_rx_stop(struct bflb_device_s *dev)
{
    uint32_t regval = getreg32(dev->reg_base + AUDIO_RX_FIFO_CTRL_OFFSET);

    putreg32(regval & ~AUDIO_RX_FIFO_CH_EN_MASK, dev->reg_base + AUDIO_RX_FIFO_CTRL_OFFSET);
}

void bflb_audio_rx_flush(struct bflb_device_s *dev)
{
    uint32_t regval = getreg32(dev->reg_base + AUDIO_RX_FIFO_CTRL_OFFSET);

    putreg32(regval | AUDIO_FIFO_FLUSH, dev->reg_base + AUDIO_RX_FIFO_CTRL_OFFSET);
    putreg32(regval & ~AUDIO_FIFO_FLUSH, dev->reg_base + AUDIO_RX_FIFO_CTRL_OFFSET);
}

uint32_t bflb_audio_rx_count(struct bflb_device_s *dev)
{
    return (getreg32(dev->reg_base + AUDIO_RX_FIFO_STATUS_OFFSET) & AUDIO_RX_FIFO_CNT_MASK) >> AUDIO_FIFO_CNT_SHIFT;
}

uint32_t bflb_audio_rx_read(struct bflb_device_s *dev)
{
    return getreg32(dev->reg_base + AUDIO_RX_FIFO_DATA_OFFSET);
}

uint32_t bflb_audio_rx_read_buf(struct bflb_device_s *dev, uint32_t *buf, uint32_t len)
{
    uint32_t cnt = bflb_audio_rx_count(dev);
    uint32_t n = 0;

    while (n < len) {
        if (cnt == 0) {
            cnt = bflb_audio_rx_count(dev);
            if (cnt == 0) {
                break;
            }
        }
        buf[n++] = getreg32(dev->reg_base + AUDIO_RX_FIFO_DATA_OFFSET);
        cnt--;
    }
    return n;
}

void bflb_audio_tx_start(struct bflb_device_s *dev, uint8_t ch_mask)
{
    uint32_t regval;

    if (!AUDIO_IS_CODEC(dev)) {
        return;
    }
    regval = getreg32(dev->reg_base + AUDIO_TX_FIFO_CTRL_OFFSET);
    regval &= ~AUDIO_TX_FIFO_CH_EN_MASK;
    regval |= ((uint32_t)ch_mask << AUDIO_FIFO_CH_EN_SHIFT) & AUDIO_TX_FIFO_CH_EN_MASK;
    putreg32(regval, dev->reg_base + AUDIO_TX_FIFO_CTRL_OFFSET);
}

void bflb_audio_tx_stop(struct bflb_device_s *dev)
{
    uint32_t regval;

    if (!AUDIO_IS_CODEC(dev)) {
        return;
    }
    regval = getreg32(dev->reg_base + AUDIO_TX_FIFO_CTRL_OFFSET);
    putreg32(regval & ~AUDIO_TX_FIFO_CH_EN_MASK, dev->reg_base + AUDIO_TX_FIFO_CTRL_OFFSET);
}

void bflb_audio_tx_flush(struct bflb_device_s *dev)
{
    uint32_t regval;

    if (!AUDIO_IS_CODEC(dev)) {
        return;
    }
    regval = getreg32(dev->reg_base + AUDIO_TX_FIFO_CTRL_OFFSET);
    putreg32(regval | AUDIO_FIFO_FLUSH, dev->reg_base + AUDIO_TX_FIFO_CTRL_OFFSET);
    putreg32(regval & ~AUDIO_FIFO_FLUSH, dev->reg_base + AUDIO_TX_FIFO_CTRL_OFFSET);
}

uint32_t bflb_audio_tx_space(struct bflb_device_s *dev)
{
    if (!AUDIO_IS_CODEC(dev)) {
        return 0;
    }
    return (getreg32(dev->reg_base + AUDIO_TX_FIFO_STATUS_OFFSET) & AUDIO_TX_FIFO_CNT_MASK) >> AUDIO_FIFO_CNT_SHIFT;
}

void bflb_audio_tx_write(struct bflb_device_s *dev, uint32_t data)
{
    putreg32(data, dev->reg_base + AUDIO_TX_FIFO_DATA_OFFSET);
}

void bflb_audio_link_rxdma(struct bflb_device_s *dev, bool enable)
{
    uint32_t regval = getreg32(dev->reg_base + AUDIO_RX_FIFO_CTRL_OFFSET);

    regval = enable ? (regval | AUDIO_FIFO_DRQ_EN) : (regval & ~AUDIO_FIFO_DRQ_EN);
    putreg32(regval, dev->reg_base + AUDIO_RX_FIFO_CTRL_OFFSET);
}

void bflb_audio_link_txdma(struct bflb_device_s *dev, bool enable)
{
    uint32_t regval;

    if (!AUDIO_IS_CODEC(dev)) {
        return;
    }
    regval = getreg32(dev->reg_base + AUDIO_TX_FIFO_CTRL_OFFSET);
    regval = enable ? (regval | AUDIO_FIFO_DRQ_EN) : (regval & ~AUDIO_FIFO_DRQ_EN);
    putreg32(regval, dev->reg_base + AUDIO_TX_FIFO_CTRL_OFFSET);
}

uint32_t bflb_audio_rx_fifo_addr(struct bflb_device_s *dev)
{
    return dev->reg_base + AUDIO_RX_FIFO_DATA_OFFSET;
}

uint32_t bflb_audio_tx_fifo_addr(struct bflb_device_s *dev)
{
    return dev->reg_base + AUDIO_TX_FIFO_DATA_OFFSET;
}

static void audio_fifo_int_mask(uint32_t reg, uint32_t over, uint32_t under, uint32_t avail, uint32_t int_mask,
                                bool mask)
{
    uint32_t bits = 0;
    uint32_t regval;

    bits |= (int_mask & over) ? AUDIO_FIFO_O_INT_EN : 0;
    bits |= (int_mask & under) ? AUDIO_FIFO_U_INT_EN : 0;
    bits |= (int_mask & avail) ? AUDIO_FIFO_A_INT_EN : 0;
    if (bits == 0) {
        return;
    }
    regval = getreg32(reg);
    regval = mask ? (regval & ~bits) : (regval | bits);
    putreg32(regval, reg);
}

void bflb_audio_int_mask(struct bflb_device_s *dev, uint32_t int_mask, bool mask)
{
    uint32_t reg_base = dev->reg_base;

    audio_fifo_int_mask(reg_base + AUDIO_RX_FIFO_CTRL_OFFSET, AUDIO_INT_RX_OVER, AUDIO_INT_RX_UNDER,
                        AUDIO_INT_RX_AVAIL, int_mask, mask);
    if (AUDIO_IS_CODEC(dev)) {
        audio_fifo_int_mask(reg_base + AUDIO_TX_FIFO_CTRL_OFFSET, AUDIO_INT_TX_OVER, AUDIO_INT_TX_UNDER,
                            AUDIO_INT_TX_AVAIL, int_mask, mask);
    }
}

uint32_t bflb_audio_get_intstatus(struct bflb_device_s *dev)
{
    uint32_t reg_base = dev->reg_base;
    uint32_t sts = 0;
    uint32_t regval;

    regval = getreg32(reg_base + AUDIO_RX_FIFO_STATUS_OFFSET);
    sts |= (regval & AUDIO_FIFO_O_INT) ? AUDIO_INT_RX_OVER : 0;
    sts |= (regval & AUDIO_FIFO_U_INT) ? AUDIO_INT_RX_UNDER : 0;
    sts |= (regval & AUDIO_FIFO_A_INT) ? AUDIO_INT_RX_AVAIL : 0;
    if (AUDIO_IS_CODEC(dev)) {
        regval = getreg32(reg_base + AUDIO_TX_FIFO_STATUS_OFFSET);
        sts |= (regval & AUDIO_FIFO_O_INT) ? AUDIO_INT_TX_OVER : 0;
        sts |= (regval & AUDIO_FIFO_U_INT) ? AUDIO_INT_TX_UNDER : 0;
        sts |= (regval & AUDIO_FIFO_A_INT) ? AUDIO_INT_TX_AVAIL : 0;
        regval = getreg32(reg_base + AUDIO_STATUS_OFFSET);
        sts |= (regval & AUDIO_STATUS_VAD_INT) ? AUDIO_INT_VAD : 0;
        sts |= (regval & AUDIO_STATUS_VOL_INTS) ? AUDIO_INT_VOLUME : 0;
    }
    return sts;
}

void bflb_audio_int_clear(struct bflb_device_s *dev, uint32_t int_clear)
{
    uint32_t regval;

    if ((int_clear & AUDIO_INT_VAD) && AUDIO_IS_CODEC(dev)) {
        regval = getreg32(dev->reg_base + AUDIO_STATUS_OFFSET);
        putreg32(regval | AUDIO_STATUS_VAD_INT_CLR, dev->reg_base + AUDIO_STATUS_OFFSET);
        putreg32(regval & ~AUDIO_STATUS_VAD_INT_CLR, dev->reg_base + AUDIO_STATUS_OFFSET);
    }
    if ((int_clear & AUDIO_INT_VOLUME) && AUDIO_IS_CODEC(dev)) {
        static const uint16_t vol_regs[] = { AUDIO_ADC_H_OFFSET, AUDIO_ADC_S0_OFFSET, AUDIO_ADC_S0_OFFSET + 8,
                                             AUDIO_ADC_S0_OFFSET + 16, AUDIO_DAC_S0_OFFSET, AUDIO_DAC_S0_OFFSET + 8 };
        /* the update bits stay set until software drops them (BL606P driver) */
        for (unsigned i = 0; i < sizeof(vol_regs) / sizeof(vol_regs[0]); i++) {
            regval = getreg32(dev->reg_base + vol_regs[i]);
            putreg32(regval & ~AUDIO_VOL_UPDATE, dev->reg_base + vol_regs[i]);
        }
        regval = getreg32(dev->reg_base + AUDIO_STATUS_OFFSET);
        putreg32(regval | AUDIO_STATUS_VOL_CLRS, dev->reg_base + AUDIO_STATUS_OFFSET);
        putreg32(regval & ~AUDIO_STATUS_VOL_CLRS, dev->reg_base + AUDIO_STATUS_OFFSET);
    }
}
