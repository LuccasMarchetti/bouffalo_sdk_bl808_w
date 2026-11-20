#include "bflb_mtimer.h"
#include "board.h"
#include "bl808_pm.h"

/*
 * HBN0 woken by a falling edge on an AON pin (GPIO9-15, 40, 41). GPIO13 here:
 * pull it to ground to wake the chip. The RTC wakes it anyway after 10 s.
 */
int main(void)
{
    board_init();

    printf("boot: %s, HBN_IRQ_STAT 0x%08x\r\n", pm_hbn_woke_up() ? "woke up from HBN" : "power on or reset",
           (unsigned int)BL_RD_REG(HBN_BASE, HBN_IRQ_STAT));
    pm_hbn_aon_pin_wakeup(1 << HBN_INT_GPIO13, HBN_GPIO_INT_TRIGGER_ASYNC_FALLING_EDGE);
    printf("enter hbn0: pull GPIO13 low to wake up\r\n");
    bflb_mtimer_delay_ms(5);
    pm_hbn_mode_enter(PM_HBN_LEVEL_0, 10 * 32768);

    while (1) {
    }
}
