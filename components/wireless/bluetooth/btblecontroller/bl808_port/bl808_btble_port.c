/*
 * BL808 (M0) glue for the BL606P/BL808 BT/BLE controller blob
 * (libbl606p_btblecontroller_prebuild.a, pinevoice_smartspeaker_sdk@305dd9a).
 *
 * The blob was built for an AliOS (aos_*) environment and an older HAL. This
 * file provides those imports on FreeRTOS and the SDK's LHAL, and maps the
 * SDK's BLE host API names onto the blob's.
 *
 * Adapted from Zephyr (modules/hal_bouffalolab/shim/aos_shim.c and
 * soc/bflb/bl808/ble/btblecontroller_port_hw.c, Apache-2.0, Copyright The
 * Zephyr Project Contributors), where the same blob runs on the M1s Dock.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "bflb_irq.h"

/* ---------------------------------------------------------------------- */
/* Names the SDK BLE host uses that the BL808 blob spells differently     */
/* ---------------------------------------------------------------------- */

extern void ble_controller_deinit(void);
extern uint16_t bt_onchiphci_hanlde_rx_acl(void *param, uint8_t *host_buf_data);

void btble_controller_deinit(void)
{
    ble_controller_deinit();
}

uint16_t bt_onchiphci_handle_rx_acl(void *param, uint8_t *host_buf_data)
{
    return bt_onchiphci_hanlde_rx_acl(param, host_buf_data);
}

/* ---------------------------------------------------------------------- */
/* Legacy HAL: interrupts and eFuse MAC                                    */
/* ---------------------------------------------------------------------- */

void bl_irq_register(int irqnum, void *handler)
{
    /* The blob's handlers take no arguments; extra ones are ignored. */
    bflb_irq_attach(irqnum, (irq_callback)handler, NULL);
}

void bl_irq_enable(unsigned int source)
{
    bflb_irq_enable(source);
}

void bl_irq_disable(unsigned int source)
{
    bflb_irq_disable(source);
}

void bl_irq_pending_clear(unsigned int source)
{
    bflb_irq_clear_pending(source);
}

extern uint8_t bflb_efuse_is_mac_address_slot_empty(uint8_t slot, uint8_t reload);
extern int bflb_efuse_read_mac_address_opt(uint8_t slot, uint8_t mac[6], uint8_t reload);

/* Same slot order as the Wi-Fi port: most recent eFuse MAC slot first. */
int EF_Ctrl_Read_MAC_Address(uint8_t mac[6])
{
    for (int slot = 2; slot >= 0; slot--) {
        if (!bflb_efuse_is_mac_address_slot_empty(slot, 1) &&
            bflb_efuse_read_mac_address_opt(slot, mac, 1) == 0) {
            return 0;
        }
    }
    return -1;
}

/* ---------------------------------------------------------------------- */
/* AliOS (aos_*) API on FreeRTOS                                           */
/* ---------------------------------------------------------------------- */

#define AOS_WAIT_FOREVER 0xFFFFFFFFU

void *aos_malloc(unsigned int size)
{
    return pvPortMalloc(size);
}

void aos_free(void *mem)
{
    vPortFree(mem);
}

/*
 * The controller treats the object behind a queue handle as its own aos
 * queue struct: on deinit it reads the message buffer pointer at offset 16
 * and aos_free()s it before calling aos_queue_free(). Keep that layout.
 * The controller also passes the address of its handle variable, not the
 * handle itself, to send/recv/free.
 */
struct aos_queue_wrapper {
    uint32_t rsvd[4];
    void *buf;
    QueueHandle_t q;
    uint32_t msg_size;
};

_Static_assert(__builtin_offsetof(struct aos_queue_wrapper, buf) == 16,
               "controller expects the queue buffer pointer at offset 16");

/* max_msg is the size of one message; size is the whole buffer. */
int aos_queue_new(void *queue_handle, void *buf, unsigned int size, int max_msg)
{
    struct aos_queue_wrapper *w;
    unsigned int count;

    if (max_msg <= 0) {
        return -1;
    }
    count = size / (unsigned int)max_msg;
    if (count == 0) {
        return -1;
    }

    w = pvPortMalloc(sizeof(*w));
    if (w == NULL) {
        return -1;
    }
    memset(w, 0, sizeof(*w));
    w->buf = buf;
    w->msg_size = (uint32_t)max_msg;
    w->q = xQueueCreate(count, (UBaseType_t)max_msg);
    if (w->q == NULL) {
        vPortFree(w);
        return -1;
    }

    *(struct aos_queue_wrapper **)queue_handle = w;
    return 0;
}

void aos_queue_free(void *queue_handle)
{
    struct aos_queue_wrapper **hp = (struct aos_queue_wrapper **)queue_handle;

    if (hp == NULL || *hp == NULL) {
        return;
    }
    vQueueDelete((*hp)->q);
    vPortFree(*hp);
    *hp = NULL;
}

int aos_queue_send(void *queue_handle, void *msg, unsigned int size)
{
    struct aos_queue_wrapper *w = *(struct aos_queue_wrapper **)queue_handle;
    BaseType_t woken = pdFALSE;
    BaseType_t ok;

    (void)size;

    if (xPortIsInsideInterrupt()) {
        ok = xQueueSendFromISR(w->q, msg, &woken);
        portYIELD_FROM_ISR(woken);
    } else {
        ok = xQueueSend(w->q, msg, 0);
    }
    return (ok == pdTRUE) ? 0 : -1;
}

int aos_queue_recv(void *queue_handle, unsigned int ms, void *msg, unsigned int *size)
{
    struct aos_queue_wrapper *w = *(struct aos_queue_wrapper **)queue_handle;
    TickType_t ticks;

    if (ms == AOS_WAIT_FOREVER) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(ms);
    }

    if (xQueueReceive(w->q, msg, ticks) != pdTRUE) {
        return -1;
    }
    if (size != NULL) {
        *size = w->msg_size;
    }
    return 0;
}

/*
 * prio: the controller passes back the priority given to
 * btble_controller_init() (the SDK examples use configMAX_PRIORITIES - 1).
 * Clamp it to the valid FreeRTOS range.
 */
int aos_task_new_ext(void *task, const char *name, void (*fn)(void *arg), void *arg,
                     int stack_size, int prio)
{
    TaskHandle_t h = NULL;

    if (prio < 0) {
        prio = 0;
    } else if (prio >= configMAX_PRIORITIES) {
        prio = configMAX_PRIORITIES - 1;
    }

    if (xTaskCreate(fn, name, (configSTACK_DEPTH_TYPE)(stack_size / sizeof(StackType_t)),
                    arg, (UBaseType_t)prio, &h) != pdPASS) {
        return -1;
    }
    if (task != NULL) {
        *(void **)task = h;
    }
    return 0;
}

void krhino_task_dyn_del(void *task)
{
    vTaskDelete((TaskHandle_t)task);
}
