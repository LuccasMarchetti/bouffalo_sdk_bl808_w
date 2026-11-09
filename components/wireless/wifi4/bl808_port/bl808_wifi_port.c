/*
 * BL808 (M0) platform glue for the wifi4 MAC firmware blob.
 *
 * RF bring-up (clocks, Wi-Fi RAM, TX power, rfparam_init) lives in
 * drivers/soc/bl808/phyrf/bl808_rf.c, shared with BLE.
 *
 * Adapted from Zephyr's BL808 Wi-Fi support (soc/bflb/bl808/wifi/
 * bflb_wifi_platform.c, Apache-2.0,
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
