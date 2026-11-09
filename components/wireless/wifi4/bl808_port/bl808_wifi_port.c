/*
 * BL808 (M0) platform glue for the wifi4 MAC firmware blob.
 *
 * Adapted from Zephyr's BL808 Wi-Fi support (soc/bflb/bl808/wifi/
 * bflb_wifi_platform.c and soc/bflb/bl808/bflb_rf.c, Apache-2.0,
 * Copyright The Zephyr Project Contributors), which runs the same blob
 * (libbl606p_firmware_prebuild.a, pinevoice_smartspeaker_sdk@305dd9a) on the
 * Sipeed M1s Dock.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "bflb_core.h"
#include "bflb_irq.h"
#include "bl808.h"
#include "bl808_aon.h"
#include "hardware/glb_reg.h"
#include "rfparam_adapter.h"

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

/* Per-rate TX power targets in dBm (Zephyr bl808.dtsi defaults). */
static int8_t bl808_pwr_11b[4] = { 17, 17, 17, 17 };
static int8_t bl808_pwr_11g[8] = { 15, 15, 15, 15, 15, 15, 15, 15 };
static int8_t bl808_pwr_11n[8] = { 14, 14, 14, 14, 14, 14, 14, 14 };

extern void trpc_update_power_11b(int8_t *pwr);
extern void trpc_update_power_11g(int8_t *pwr);
extern void trpc_update_power_11n(int8_t *pwr);

/* Wi-Fi shared RAM bounds, from bl808_flash_m0.ld */
extern uint8_t __wifi_ram_start[];
extern uint8_t __wifi_ram_end[];

/* struct wifi_bt_coex_ctx, normally provided by the BT controller blob.
 * A zeroed context leaves the Wi-Fi side in charge of the RF. Weak so the
 * BLE controller's definition wins when it is linked. */
#define BL808_COEX_CTX_WORDS 7U
__attribute__((weak)) uint32_t coex_timing_control_ctx[BL808_COEX_CTX_WORDS];

/* Timing and logging hooks the BL808 blobs import (the BL602 ones do not). */
#include "FreeRTOS.h"
#include "task.h"

uint32_t bl_os_clock_gettime_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* The blob traces every scanned channel through bl_printf; drop it unless
 * debugging, as Zephyr does (the output latency also disturbs RF timing). */
int bl_printf(const char *fmt, ...)
{
    (void)fmt;
    return 0;
}

/* Coexistence debug GPIOs, not wired on the supported boards. */
void bl_gpio_enable_output(uint8_t pin, uint8_t pullup, uint8_t pulldown)
{
    (void)pin;
    (void)pullup;
    (void)pulldown;
}

void bl_gpio_output_set(uint8_t pin, uint8_t value)
{
    (void)pin;
    (void)value;
}

static void bl808_wifi_clock_enable(void)
{
    uint32_t v;

    v = getreg32(GLB_BASE + GLB_CGEN_CFG0_OFFSET);
    putreg32(v | BL808_GLB_CGEN_WIFI_PHY_MSK, GLB_BASE + GLB_CGEN_CFG0_OFFSET);

    v = getreg32(GLB_BASE + GLB_CGEN_CFG2_OFFSET);
    putreg32(v | GLB_CGEN_S2_WIFI_MSK, GLB_BASE + GLB_CGEN_CFG2_OFFSET);

    v = getreg32(GLB_BASE + BL808_GLB_WIFI_CFG0_OFFSET);
    v = (v & ~BL808_GLB_WIFI_MAC_CORE_DIV_MSK) | BL808_GLB_WIFI_MAC_CORE_DIV_40M;
    putreg32(v, GLB_BASE + BL808_GLB_WIFI_CFG0_OFFSET);
}

int rfparam_init(uint32_t base_addr, void *rf_para, uint32_t len)
{
    (void)base_addr;
    (void)rf_para;
    (void)len;

    /* The blob's NOLOAD .fw.SHRAM / .fw.SHRAMIPC objects are not zeroed by
     * the startup code. */
    memset(__wifi_ram_start, 0, (size_t)(__wifi_ram_end - __wifi_ram_start));

#if CONFIG_BL808_WIFI_XTAL_CAPCODE != 0
    AON_Set_Xtal_CapCode(CONFIG_BL808_WIFI_XTAL_CAPCODE, CONFIG_BL808_WIFI_XTAL_CAPCODE);
#endif

    bl808_wifi_clock_enable();

    trpc_update_power_11b(bl808_pwr_11b);
    trpc_update_power_11g(bl808_pwr_11g);
    trpc_update_power_11n(bl808_pwr_11n);

    return 0;
}

/* RF calibration, called by the Wi-Fi (and BLE) blobs. The first one, at
 * bring-up, must not be preempted or the SoC hangs. Periodic recalibrations
 * while connected keep interrupts enabled, or the MAC stalls. */
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

/*
 * Blob bug workarounds, offsets taken from Zephyr.
 *
 * The station is marked valid in sta_mgmt_register() before me_init_rate()
 * gives it a rate-control block, so a TX confirmation in that window reaches
 * rc_update_counters() with a NULL block and trips a fatal assert. Skip the
 * uninitialised stations.
 */
#define RC_STA_INFO_STRIDE  368U
#define RC_STA_INFO_VALID   39U
#define RC_STA_INFO_STATS   324U
#define BL808_STA_INFO_COUNT 5U

extern uint8_t sta_info_tab[];
extern void __real_rc_update_counters(uint32_t sta_idx, uint32_t a, uint32_t b);
extern void __real_sta_mgmt_unregister(uint32_t sta_idx);

void __wrap_rc_update_counters(uint32_t sta_idx, uint32_t a, uint32_t b)
{
    const uint8_t *sta;

    if (sta_idx >= BL808_STA_INFO_COUNT) {
        return;
    }

    sta = &sta_info_tab[sta_idx * RC_STA_INFO_STRIDE];
    if ((sta[RC_STA_INFO_VALID] != 0xffU) &&
        (*(const uint32_t *)&sta[RC_STA_INFO_STATS] == 0U)) {
        return;
    }

    __real_rc_update_counters(sta_idx, a, b);
}

/* Removing an interface deletes its station again after the disconnect
 * already did; the second call indexes vif_info_tab with 0xff. Deleting a
 * station that is already gone is a no-op. */
void __wrap_sta_mgmt_unregister(uint32_t sta_idx)
{
    if (sta_idx >= BL808_STA_INFO_COUNT) {
        return;
    }

    if (sta_info_tab[(sta_idx * RC_STA_INFO_STRIDE) + RC_STA_INFO_VALID] == 0xffU) {
        return;
    }

    __real_sta_mgmt_unregister(sta_idx);
}
