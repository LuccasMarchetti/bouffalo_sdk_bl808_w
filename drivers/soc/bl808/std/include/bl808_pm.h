#ifndef __BL808_PM_H__
#define __BL808_PM_H__

#include "bl808_common.h"
#include "bl808_hbn.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Low power modes of the BL808, run from the M0 (see LowPower in the
 * reference manual):
 *  - PDS1: MM domain (D0) off;  PDS2: USB off;  PDS3: MM and USB off.
 *    The M0 keeps its state and returns from pm_pds_mode_enter().
 *  - PDS7: also the CORE_MISC domain (M0 and GLB) off: the chip boots again.
 *  - HBN0: everything but the AON domains off, 4 KB HBN RAM kept (the boot
 *    ROM writes its first word on the way back); HBN1: HBN RAM off too. The
 *    chip boots again; HBN_RSV0 reads PM_HBN_WAKEUP_FLAG.
 * The crystal and the PLLs stay on during PDS: turning them off (xtalOff,
 * *pllOff) hung the chip on the M1s Dock, so it is not offered here.
 */
enum pm_pds_sleep_level {
    PM_PDS_LEVEL_1 = 1,
    PM_PDS_LEVEL_2 = 2,
    PM_PDS_LEVEL_3 = 3,
    PM_PDS_LEVEL_7 = 7,
};

enum pm_hbn_sleep_level {
    PM_HBN_LEVEL_0,
    PM_HBN_LEVEL_1,
};

/* PDS wakeup sources, positions in PDS_INT (cr_pds_wakeup_src_en) */
#define PM_PDS_WAKEUP_BY_PDS_TIMER (10U) /* PDS sleep timer */
#define PM_PDS_WAKEUP_BY_HBN_IRQ   (11U) /* HBN interrupts: AON pins, RTC, ACOMP, BOD */
#define PM_PDS_WAKEUP_BY_GLB_GPIO  (12U) /* GPIO interrupt of the GLB (bflb_gpio_int_*) */
#define PM_PDS_WAKEUP_BY_PDS_GPIO  (13U) /* PDS GPIO interrupt (did not trigger on the M1s Dock) */
#define PM_PDS_WAKEUP_BY_IRRX      (14U)

/* "WHBN": HBN_RSV0 after a wakeup from HBN (the boot ROM rewrites "EHBN") */
#define PM_HBN_WAKEUP_FLAG 0x4e424857UL

void pm_pds_mask_all_wakeup_src(void);
BL_Err_Type pm_pds_wakeup_src_en(uint32_t src);
/* event bits of the last return from pm_pds_mode_enter() (PDS_INT[31:21]), bit n = source 10 + n */
uint32_t pm_pds_get_wakeup_event(void);
/* sleep_time in 32 kHz cycles (0: no timer); returns for PDS1/2/3 */
void pm_pds_mode_enter(enum pm_pds_sleep_level pds_level, uint32_t sleep_time);
/* AON pin wakeup: pins is a mask of HBN_INT_GPIO9..HBN_INT_GPIO41 bits (9 bits) */
void pm_hbn_aon_pin_wakeup(uint16_t pins, HBN_GPIO_INT_Trigger_Type trig);
/* sleep_time in 32 kHz cycles (0: no RTC); does not return */
void pm_hbn_mode_enter(enum pm_hbn_sleep_level hbn_level, uint64_t sleep_time);
/* the chip woke up from HBN (reads HBN_RSV0) */
int pm_hbn_woke_up(void);

#ifdef __cplusplus
}
#endif

#endif
