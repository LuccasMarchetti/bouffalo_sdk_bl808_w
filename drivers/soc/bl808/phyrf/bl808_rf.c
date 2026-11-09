/*
 * BL808 (M0) RF bring-up shared by Wi-Fi and BLE, on top of the BL606P/BL808
 * PHY/RF blob (libbl606p_phyrf.a).
 *
 * Provides rfparam_init(), the entry point the SDK's Wi-Fi and BLE examples
 * already call on other chips, so they run unchanged on BL808.
 *
 * Adapted from Zephyr's BL808 support (soc/bflb/bl808/bflb_rf.c,
 * soc/bflb/bl808/wifi/bflb_wifi_platform.c and soc.c, Apache-2.0,
 * Copyright The Zephyr Project Contributors).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bflb_core.h"
#include "bflb_irq.h"
#include "bl808.h"
#include "bl808_aon.h"
#include "hardware/glb_reg.h"
#include "rfparam_adapter.h"
#include "wl_api.h"

/* BL808 boards in this SDK run from a 40 MHz crystal (board.c). */
#define BL808_XTAL_FREQ_HZ 40000000U

/* MAC core clock = WIFIPLL 80 MHz / (div + 1) = 40 MHz */
#define BL808_GLB_WIFI_CFG0_OFFSET      0x3B0U
#define BL808_GLB_WIFI_MAC_CORE_DIV_MSK 0xFU
#define BL808_GLB_WIFI_MAC_CORE_DIV_40M 1U
/* CGEN_CFG0 bit 7: Wi-Fi PHY clock (shared RF block, BLE depends on it) */
#define BL808_GLB_CGEN_WIFI_PHY_MSK     (1U << 7)

/* Crystal load capacitor code. 0 keeps whatever the boot ROM/eFuse set.
 * Zephyr uses 50 on the Sipeed M1s Dock (within 1 ppm, vs ~57 ppm off). */
#ifndef CONFIG_BL808_WIFI_XTAL_CAPCODE
#define CONFIG_BL808_WIFI_XTAL_CAPCODE 0
#endif

/* Which radios are linked into this image. */
extern void wifi_main(void *param) __attribute__((weak));
extern void btble_controller_init(uint8_t task_priority) __attribute__((weak));

/* Wi-Fi TX power targets in dBm (Zephyr bl808.dtsi defaults). */
static int8_t bl808_pwr_11b[4] = { 17, 17, 17, 17 };
static int8_t bl808_pwr_11g[8] = { 15, 15, 15, 15, 15, 15, 15, 15 };
static int8_t bl808_pwr_11n[8] = { 14, 14, 14, 14, 14, 14, 14, 14 };

extern void trpc_update_power_11b(int8_t *pwr);
extern void trpc_update_power_11g(int8_t *pwr);
extern void trpc_update_power_11n(int8_t *pwr);

/* Wi-Fi shared RAM bounds, from bl808_flash_m0.ld */
extern uint8_t __wifi_ram_start[];
extern uint8_t __wifi_ram_end[];

/* Layout of the object libbl606p_phyrf's wl_cfg_get() hands back. */
_Static_assert(sizeof(struct wl_cfg_t) == 124U, "wl_cfg_t does not match libbl606p_phyrf");
_Static_assert(offsetof(struct wl_cfg_t, param) == 4U, "wl_cfg_t.param offset");

static void bl808_wifi_mac_clock_enable(void)
{
    uint32_t v;

    v = getreg32(GLB_BASE + GLB_CGEN_CFG2_OFFSET);
    putreg32(v | GLB_CGEN_S2_WIFI_MSK, GLB_BASE + GLB_CGEN_CFG2_OFFSET);

    v = getreg32(GLB_BASE + BL808_GLB_WIFI_CFG0_OFFSET);
    v = (v & ~BL808_GLB_WIFI_MAC_CORE_DIV_MSK) | BL808_GLB_WIFI_MAC_CORE_DIV_40M;
    putreg32(v, GLB_BASE + BL808_GLB_WIFI_CFG0_OFFSET);
}

static int bl808_ble_rf_init(bool with_wifi)
{
    /* The blob ignores rmem and returns its own static object. */
    struct wl_cfg_t *cfg = wl_cfg_get(NULL);

    if (cfg == NULL) {
        return -1;
    }

    cfg->mode = with_wifi ? WL_API_MODE_ALL : WL_API_MODE_BZ;
    cfg->en_param_load = 0;
    /* btble_controller_init() runs the full calibration itself. */
    cfg->en_full_cal = 0;
    cfg->en_capcode_set = 0;
    cfg->param_load = NULL;
    cfg->capcode_set = NULL;
    cfg->capcode_get = NULL;
    cfg->param.xtalfreq_hz = BL808_XTAL_FREQ_HZ;

    return (wl_init() == 0) ? 0 : -1;
}

int rfparam_init(uint32_t base_addr, void *rf_para, uint32_t len)
{
    bool with_wifi = (wifi_main != NULL);
    bool with_ble = (btble_controller_init != NULL);
    uint32_t v;

    (void)base_addr;
    (void)rf_para;
    (void)len;

#if CONFIG_BL808_WIFI_XTAL_CAPCODE != 0
    AON_Set_Xtal_CapCode(CONFIG_BL808_WIFI_XTAL_CAPCODE, CONFIG_BL808_WIFI_XTAL_CAPCODE);
#endif

    /* PHY clock: shared by Wi-Fi and BLE. */
    v = getreg32(GLB_BASE + GLB_CGEN_CFG0_OFFSET);
    putreg32(v | BL808_GLB_CGEN_WIFI_PHY_MSK, GLB_BASE + GLB_CGEN_CFG0_OFFSET);

    if (with_wifi) {
        /* The blob's NOLOAD .fw.SHRAM / .fw.SHRAMIPC objects are not zeroed
         * by the startup code. */
        memset(__wifi_ram_start, 0, (size_t)(__wifi_ram_end - __wifi_ram_start));
        bl808_wifi_mac_clock_enable();
        trpc_update_power_11b(bl808_pwr_11b);
        trpc_update_power_11g(bl808_pwr_11g);
        trpc_update_power_11n(bl808_pwr_11n);
    }

    if (with_ble && bl808_ble_rf_init(with_wifi) != 0) {
        return -1;
    }

    return 0;
}

/* RF calibration, called by the Wi-Fi and BLE blobs. The first one, at
 * bring-up, must not be preempted or the SoC hangs. Periodic recalibrations
 * while Wi-Fi is connected keep interrupts enabled, or the MAC stalls. */
extern int __real_rfc_init(uint32_t xtal);

int __wrap_rfc_init(uint32_t xtal)
{
    static bool booted;
    uintptr_t flags = 0;
    int r;

    if (!booted) {
        flags = bflb_irq_save();
    }
    r = __real_rfc_init(xtal);
    if (!booted) {
        bflb_irq_restore(flags);
        booted = true;
    }
    return r;
}
