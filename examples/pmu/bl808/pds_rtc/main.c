#include "bflb_mtimer.h"
#include "board.h"
#include "bl808_pm.h"

/*
 * PDS1/2/3: the M0 sleeps 1 s and returns from pm_pds_mode_enter(); PDS7: the
 * M0 is powered off and the chip boots again after 1 s.
 */
int main(void)
{
    static const enum pm_pds_sleep_level levels[] = { PM_PDS_LEVEL_1, PM_PDS_LEVEL_2, PM_PDS_LEVEL_3 };

    board_init();
    pm_pds_mask_all_wakeup_src();

    for (int i = 0; i < 3; i++) {
        uint64_t t0 = bflb_mtimer_get_time_us();

        printf("enter pds%d for 1 s\r\n", levels[i]);
        bflb_mtimer_delay_ms(5);
        pm_pds_mode_enter(levels[i], 32768);
        printf("back from pds%d after %u us, wakeup event 0x%03x\r\n", levels[i],
               (unsigned int)(bflb_mtimer_get_time_us() - t0), (unsigned int)pm_pds_get_wakeup_event());
    }

    printf("enter pds7 for 1 s: the chip boots again\r\n");
    bflb_mtimer_delay_ms(5);
    pm_pds_mode_enter(PM_PDS_LEVEL_7, 32768);

    while (1) {
    }
}
