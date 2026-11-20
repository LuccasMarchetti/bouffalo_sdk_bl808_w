#include "bflb_mtimer.h"
#include "bflb_acomp.h"
#include "bflb_gpio.h"
#include "board.h"
#include "bl808_pm.h"

/*
 * HBN0 woken by ACOMP0: GPIO17 (ADC channel 0) against 1.25 V, falling edge.
 * On the M1s Dock GPIO17 is the RX of the second BL702 port: sending a byte
 * to it at a low baud rate wakes the chip. The RTC wakes it anyway after 10 s.
 */
int main(void)
{
    struct bflb_acomp_config_s cfg = {
        .mux_en = 1,
        .pos_chan_sel = AON_ACOMP_CHAN_ADC0,
        .neg_chan_sel = AON_ACOMP_CHAN_VREF_1P25V,
        .vio_sel = 0,
        .scaling_factor = AON_ACOMP_SCALING_FACTOR_1,
        .bias_prog = AON_ACOMP_BIAS_POWER_MODE1,
        .hysteresis_pos_volt = AON_ACOMP_HYSTERESIS_VOLT_NONE,
        .hysteresis_neg_volt = AON_ACOMP_HYSTERESIS_VOLT_NONE,
    };
    struct bflb_device_s *gpio;

    board_init();
    printf("boot: %s, HBN_IRQ_STAT 0x%08x\r\n", pm_hbn_woke_up() ? "woke up from HBN" : "power on or reset",
           (unsigned int)BL_RD_REG(HBN_BASE, HBN_IRQ_STAT));

    gpio = bflb_device_get_by_name("gpio");
    bflb_gpio_init(gpio, GPIO_PIN_17, GPIO_ANALOG | GPIO_SMT_EN | GPIO_DRV_0);
    bflb_acomp_init(AON_ACOMP0_ID, &cfg);
    bflb_acomp_enable(AON_ACOMP0_ID);
    bflb_mtimer_delay_ms(2);
    HBN_Clear_IRQ(HBN_INT_ACOMP0);
    HBN_Enable_AComp0_IRQ(HBN_ACOMP_INT_EDGE_NEGEDGE);

    printf("enter hbn0: a falling edge on GPIO17 wakes up\r\n");
    bflb_mtimer_delay_ms(5);
    pm_hbn_mode_enter(PM_HBN_LEVEL_0, 10 * 32768);

    while (1) {
    }
}
