#include "bflb_mtimer.h"
#include "board.h"
#include "bl808_pm.h"

/* HBN0 for 1 s, woken by the RTC; the chip boots again and repeats */
int main(void)
{
    board_init();

    printf("boot: %s\r\n", pm_hbn_woke_up() ? "woke up from HBN" : "power on or reset");
    printf("enter hbn0 for 1 s\r\n");
    bflb_mtimer_delay_ms(5);
    pm_hbn_mode_enter(PM_HBN_LEVEL_0, 32768);

    while (1) {
    }
}
