/*
 * Low power for the BL808 (M0): PDS 1/2/3/7 and HBN 0/1.
 *
 * Built from the bl808_pds/bl808_hbn drivers and the LowPower chapter of the
 * reference manual, and tested on a Sipeed M1s Dock: PDS1/2/3 and PDS7 by the
 * PDS timer, HBN0/1 by the RTC, HBN0 and PDS by an AON pin (GPIO15) and by
 * ACOMP0, PDS by the GLB GPIO interrupt.
 */
#include <string.h>
#include "bl808_pm.h"
#include "bl808_pds.h"
#include "bl808_glb.h"
#include "bflb_irq.h"
#include "hardware/pds_reg.h"
#include "hardware/hbn_reg.h"

void pm_pds_mask_all_wakeup_src(void)
{
    uint32_t v = BL_RD_REG(PDS_BASE, PDS_INT);

    v &= PDS_CR_PDS_WAKEUP_SRC_EN_UMSK;
    BL_WR_REG(PDS_BASE, PDS_INT, v);
}

BL_Err_Type pm_pds_wakeup_src_en(uint32_t src)
{
    uint32_t v;

    if (src < PDS_CR_PDS_WAKEUP_SRC_EN_POS ||
        src >= PDS_CR_PDS_WAKEUP_SRC_EN_POS + PDS_CR_PDS_WAKEUP_SRC_EN_LEN) {
        return INVALID;
    }
    v = BL_RD_REG(PDS_BASE, PDS_INT);
    v |= (1U << src);
    BL_WR_REG(PDS_BASE, PDS_INT, v);
    return SUCCESS;
}

static uint32_t pm_pds_last_event;

uint32_t pm_pds_get_wakeup_event(void)
{
    return pm_pds_last_event;
}

void ATTR_TCM_SECTION pm_pds_mode_enter(enum pm_pds_sleep_level pds_level, uint32_t sleep_time)
{
    PDS_DEFAULT_LV_CFG_Type c;
    uint32_t v;

    if (sleep_time != 0 && sleep_time <= PDS_WARMUP_LATENCY_CNT) {
        return;
    }

    memset(&c, 0, sizeof(c));
    c.pdsCtl.pdsStart = 1;
    c.pdsCtl.clkOff = 1;
    c.pdsCtl.memStby = 1;
    c.pdsCtl.isolation = 1;
    c.pdsCtl.pdsPwrOff = 1;
    c.pdsCtl.pdsCtlRfSel = 3;
    if (pds_level == PM_PDS_LEVEL_1 || pds_level == PM_PDS_LEVEL_3 || pds_level == PM_PDS_LEVEL_7) {
        c.pdsCtl4.dspPwrOff = 1;
        c.pdsCtl4.dspRst = 1;
        c.pdsCtl4.dspMemStby = 1;
        c.pdsCtl4.dspGateClk = 1;
        c.pdsCtl3.DspIsoEn = 1;
    }
    if (pds_level != PM_PDS_LEVEL_1) {
        c.pdsCtl4.usbPwrOff = 1;
        c.pdsCtl4.usbRst = 1;
        c.pdsCtl4.usbMemStby = 1;
        c.pdsCtl4.usbGateClk = 1;
        c.pdsCtl3.UsbIsoEn = 1;
    }
    if (pds_level == PM_PDS_LEVEL_7) {
        c.pdsCtl4.MiscPwrOff = 1;
        c.pdsCtl4.MiscRst = 1;
        c.pdsCtl4.MiscMemStby = 1;
        c.pdsCtl4.MiscGateClk = 1;
        c.pdsCtl3.MiscIsoEn = 1;
    }
    /* only the WFI of the M0 starts the PDS (the D0 and the LP may never sleep) */
    c.pdsCtl5.DspWfiMask = 1;
    c.pdsCtl5.LpWfiMask = 1;
    if (BL_RD_REG(PDS_BASE, PDS_GPIO_I_SET) != 0) {
        c.pdsCtl.ctrlGpioIePuPd = 1;
    }

    __asm volatile("csrc mstatus, 8");

    v = BL_RD_REG(PDS_BASE, PDS_INT);
    if (sleep_time) {
        v |= (1U << PM_PDS_WAKEUP_BY_PDS_TIMER);
    }
    v &= ~PDS_CR_PDS_WAKE_INT_MASK_MSK;
    BL_WR_REG(PDS_BASE, PDS_INT, v);
    PDS_IntClear();
    /* the PDS wakes the M0 out of WFI through its interrupt line (MIE stays off) */
    bflb_irq_clear_pending(PDS_WAKEUP_IRQn);
    bflb_irq_enable(PDS_WAKEUP_IRQn);

    PDS_Force_Config(&c.pdsCtl2, &c.pdsCtl3, &c.pdsCtl5);
    PDS_Enable(&c.pdsCtl, &c.pdsCtl4, sleep_time);
    __WFI();

    pm_pds_last_event = BL_GET_REG_BITS_VAL(BL_RD_REG(PDS_BASE, PDS_INT), PDS_RO_PDS_WAKEUP_EVENT);
    bflb_irq_disable(PDS_WAKEUP_IRQn);
    if (sleep_time) {
        v = BL_RD_REG(PDS_BASE, PDS_INT);
        v &= ~(1U << PM_PDS_WAKEUP_BY_PDS_TIMER);
        BL_WR_REG(PDS_BASE, PDS_INT, v);
    }
    PDS_IntClear();
    bflb_irq_clear_pending(PDS_WAKEUP_IRQn);
    __asm volatile("csrs mstatus, 8");
}

void pm_hbn_aon_pin_wakeup(uint16_t pins, HBN_GPIO_INT_Trigger_Type trig)
{
    HBN_AON_PAD_CFG_Type pc = { .ctrlEn = 1, .inputEn = 1, .outputEn = 0, .pullCfg = HBN_GPIO_PAD_PULL_NONE };
    uint32_t v;

    for (int i = 0; i <= HBN_AON_PAD_GPIO41; i++) {
        if (pins & (1 << i)) {
            HBN_Aon_Pad_Cfg(ENABLE, (HBN_AON_PAD_Type)i, &pc);
        }
    }
    /* the pin wakeup only works with the hardware pull control off */
    HBN_Hw_Pu_Pd_Cfg(DISABLE);
    HBN_GPIO_INT_Enable(trig);
    v = BL_RD_REG(HBN_BASE, HBN_IRQ_MODE);
    v = BL_SET_REG_BITS_VAL(v, HBN_PIN_WAKEUP_MASK, (~pins) & 0x1ff);
    BL_WR_REG(HBN_BASE, HBN_IRQ_MODE, v);
    if (pins) {
        pm_pds_wakeup_src_en(PM_PDS_WAKEUP_BY_HBN_IRQ);
    }
}

int pm_hbn_woke_up(void)
{
    return BL_RD_REG(HBN_BASE, HBN_RSV0) == PM_HBN_WAKEUP_FLAG;
}

static void ATTR_TCM_SECTION pm_hbn_enter(enum pm_hbn_sleep_level hbn_level)
{
    uint32_t v;

    GLB_Set_MCU_System_CLK(GLB_MCU_SYS_CLK_RC32M);

    /* the flash is not powered down: the boot ROM finds it as it was */
    v = BL_RD_REG(HBN_BASE, HBN_SRAM);
    v = BL_SET_REG_BIT(v, HBN_RETRAM_RET);
    v = BL_CLR_REG_BIT(v, HBN_RETRAM_SLP);
    BL_WR_REG(HBN_BASE, HBN_SRAM, v);

    v = BL_RD_REG(HBN_BASE, HBN_GLB);
    v = BL_SET_REG_BITS_VAL(v, HBN_ROOT_CLK_SEL, 0);
    BL_WR_REG(HBN_BASE, HBN_GLB, v);

    BL_WR_REG(HBN_BASE, HBN_RSV0, HBN_STATUS_ENTER_FLAG);

    v = BL_RD_REG(HBN_BASE, HBN_CTL);
    if (hbn_level == PM_HBN_LEVEL_1) {
        v = BL_SET_REG_BIT(v, HBN_PWRDN_HBN_CORE);
    } else {
        v = BL_CLR_REG_BIT(v, HBN_PWRDN_HBN_CORE);
    }
    v = BL_CLR_REG_BIT(v, HBN_PWR_ON_OPTION);
    BL_WR_REG(HBN_BASE, HBN_CTL, v);

    /* a pending HBN interrupt wakes the chip at once */
    BL_WR_REG(HBN_BASE, HBN_IRQ_CLR, 0xffffffff);
    BL_WR_REG(HBN_BASE, HBN_IRQ_CLR, 0);

    v = BL_SET_REG_BIT(v, HBN_MODE);
    BL_WR_REG(HBN_BASE, HBN_CTL, v);

    while (1) {
        arch_delay_ms(100);
        GLB_SW_POR_Reset();
    }
}

void pm_hbn_mode_enter(enum pm_hbn_sleep_level hbn_level, uint64_t sleep_time)
{
    uint32_t lo, hi;
    uint64_t t;

    __asm volatile("csrc mstatus, 8");

    if (sleep_time) {
        HBN_Clear_RTC_Counter();
        HBN_Enable_RTC_Counter();
        HBN_Get_RTC_Timer_Val(&lo, &hi);
        t = (((uint64_t)hi << 32) | lo) + sleep_time;
        HBN_Set_RTC_Timer(HBN_RTC_INT_DELAY_0T, (uint32_t)t, (uint32_t)(t >> 32), HBN_RTC_COMP_BIT0_39);
    } else {
        HBN_Clear_RTC_INT();
        HBN_Set_RTC_Timer(HBN_RTC_INT_DELAY_0T, 0xffffffff, 0xff, HBN_RTC_COMP_BIT0_39);
    }
    pm_hbn_enter(hbn_level);
}
