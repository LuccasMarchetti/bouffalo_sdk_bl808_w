/*
 * Vector unit (XuanTie C906, RVV 0.7.1) under FreeRTOS on the BL808 D0.
 *
 * Three tasks of the same priority keep their own values in v0-v31 and
 * busy-wait, so the tick preempts them in the middle. Each round checks that
 * the registers still hold the task's values. Any error means the context
 * switch did not save the vector state (see portable/GCC/RISC-V/c906/portVector.h).
 *
 * Expected output after about 10 s: "VECTOR TEST PASS".
 */
#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <string.h>
#include "board.h"

#if !defined(__riscv_vector)
#error "Build for the D0 (CPU_ID=d0): the vector extension is needed"
#endif

#define NUM_TASKS   3
#define VREG_BYTES  16                  /* C906: VLEN = 128 */
#define GROUP_BYTES (8 * VREG_BYTES)
#define FILE_BYTES  (4 * GROUP_BYTES)   /* v0-v31 */
#define TEST_TICKS  pdMS_TO_TICKS(10000)

static volatile uint32_t rounds[NUM_TASKS];
static volatile uint32_t errors[NUM_TASKS];

#if defined(__riscv_xtheadvector)
#define VSETVLI "th.vsetvli"
#define VLE     "th.vle.v"
#define VSE     "th.vse.v"
#else
#define VSETVLI "vsetvli"
#define VLE     "vle.v"
#define VSE     "vse.v"
#endif

static void vector_load(const uint8_t *buf)
{
    unsigned long avl = GROUP_BYTES;

    __asm__ volatile(VSETVLI " zero, %[avl], e8, m8\n"
                     VLE " v0, (%[p0])\n"
                     VLE " v8, (%[p1])\n"
                     VLE " v16, (%[p2])\n"
                     VLE " v24, (%[p3])\n"
                     :
                     : [avl] "r"(avl), [p0] "r"(buf), [p1] "r"(buf + GROUP_BYTES),
                       [p2] "r"(buf + 2 * GROUP_BYTES), [p3] "r"(buf + 3 * GROUP_BYTES)
                     : "memory");
}

static void vector_store(uint8_t *buf)
{
    unsigned long avl = GROUP_BYTES;

    __asm__ volatile(VSETVLI " zero, %[avl], e8, m8\n"
                     VSE " v0, (%[p0])\n"
                     VSE " v8, (%[p1])\n"
                     VSE " v16, (%[p2])\n"
                     VSE " v24, (%[p3])\n"
                     :
                     : [avl] "r"(avl), [p0] "r"(buf), [p1] "r"(buf + GROUP_BYTES),
                       [p2] "r"(buf + 2 * GROUP_BYTES), [p3] "r"(buf + 3 * GROUP_BYTES)
                     : "memory");
}

static void vector_task(void *arg)
{
    const int id = (int)(uintptr_t)arg;
    static uint8_t want[NUM_TASKS][FILE_BYTES];
    static uint8_t got[NUM_TASKS][FILE_BYTES];

    for (int i = 0; i < FILE_BYTES; i++) {
        want[id][i] = (uint8_t)(0x40 * (id + 1) + i);
    }

    for (;;) {
        vector_load(want[id]);
        /* Long enough for the tick to switch tasks in the middle */
        for (volatile uint32_t spin = 0; spin < 20000; spin++) {
        }
        vector_store(got[id]);
        if (memcmp(want[id], got[id], FILE_BYTES) != 0) {
            errors[id]++;
        }
        rounds[id]++;
    }
}

static void monitor_task(void *arg)
{
    TickType_t start = xTaskGetTickCount();
    uint32_t total_errors;

    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        total_errors = 0;
        for (int i = 0; i < NUM_TASKS; i++) {
            printf("task %d: %lu rounds, %lu errors\r\n", i, (unsigned long)rounds[i],
                   (unsigned long)errors[i]);
            total_errors += errors[i];
        }
        if ((xTaskGetTickCount() - start) >= TEST_TICKS) {
            printf(total_errors == 0 ? "VECTOR TEST PASS\r\n" : "VECTOR TEST FAIL\r\n");
            vTaskSuspend(NULL);
        }
    }
}

int main(void)
{
    board_init();
    printf("FreeRTOS vector context test on the D0\r\n");

    for (int i = 0; i < NUM_TASKS; i++) {
        xTaskCreate(vector_task, "vec", 512, (void *)(uintptr_t)i, 2, NULL);
    }
    xTaskCreate(monitor_task, "mon", 512, NULL, 3, NULL);

    vTaskStartScheduler();

    while (1) {
    }
}
