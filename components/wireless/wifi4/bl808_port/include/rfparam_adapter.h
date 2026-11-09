/*
 * BL808 replacement for drivers/rfparam's rfparam_adapter.h, so the generic
 * Wi-Fi examples can call rfparam_init() unchanged on BL808 (M0).
 */
#ifndef BL808_RFPARAM_ADAPTER_H
#define BL808_RFPARAM_ADAPTER_H

#include <stdint.h>

/* Prepare clocks, Wi-Fi shared RAM and TX power before wifi_task_create().
 * Arguments are accepted for API compatibility and ignored. Returns 0. */
int rfparam_init(uint32_t base_addr, void *rf_para, uint32_t len);

#endif
