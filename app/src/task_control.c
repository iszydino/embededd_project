#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "task_control.h"
#include "task_color.h"
#include "app_ipc.h"
#include "app_sort.h"

#ifndef APP_USE_WEIGHT
#define APP_USE_WEIGHT 0
#endif
#if APP_USE_WEIGHT
#include "task_weigh.h"
#endif

#define CONTROL_STACK_WORDS   256
#define CONTROL_PRIO          (tskIDLE_PRIORITY + 3)

#define PROBE_PERIOD_MS       300U    /* chu ky do mau o trang thai cho */
#define PROBE_CONFIRM         2U      /* so lan lien tiep cung mot mau */
#define COLOR_TIMEOUT_MS      3000U
#define WEIGH_WAIT_MS         9000U   /* lon hon thoi gian toi da cua task_weigh */
#define SORT_TIMEOUT_MS       6000U

/* Bien debug doc bang OpenOCD */
volatile uint32_t g_ctl_state, g_ctl_slot, g_ctl_color;
volatile uint32_t g_heap_free, g_heap_min;
volatile int32_t  g_ctl_w10, g_ctl_delta, g_ctl_base;

static void prvDebug(void)
{
    g_heap_free = (uint32_t)xPortGetFreeHeapSize();
    g_heap_min  = (uint32_t)xPortGetMinimumEverFreeHeapSize();
}

static void prvSetState(SysState_t st, int32_t w10, uint8_t slot)
{
    g_ctl_state = (uint32_t)st;
    g_ctl_w10   = w10;
    g_ctl_slot  = slot;
    Display_Post(st, (float)w10 / 10.0f, slot);
}

static ColorData_t prvMeasureColor(void)
{
    ColorData_t cd = {0};
    cd.id = COLOR_UNKNOWN;

    (void)xQueueReset(xColorQueue);      /* bo ket qua cu */
    task_color_trigger();
    if (xQueueReceive(xColorQueue, &cd, pdMS_TO_TICKS(COLOR_TIMEOUT_MS)) != pdPASS) {
        cd.id = COLOR_UNKNOWN;
    }
    g_ctl_color = (uint32_t)cd.id;
    return cd;
}

static void prvSort(const ColorData_t *cd, int32_t w10)
{
    float     w = (float)w10 / 10.0f;
    SortCmd_t cmd;

    cmd.slot     = Sort_Decide(cd->id, w);
    cmd.weight_g = w;

    prvSetState(STATE_SORTING, w10, cmd.slot);
    (void)xEventGroupClearBits(xEvt, EVT_SORT_DONE);
    if (xQueueSend(xSortQueue, &cmd, pdMS_TO_TICKS(100)) == pdPASS) {
        (void)xEventGroupWaitBits(xEvt, EVT_SORT_DONE, pdTRUE, pdFALSE,
                                  pdMS_TO_TICKS(SORT_TIMEOUT_MS));
    }
    prvSetState(STATE_DONE, w10, cmd.slot);
}

static void prvControlTask(void *pvParameters)
{
    (void)pvParameters;

    ColorData_t cd;
    ColorId_t   last_id = COLOR_NONE;
    uint32_t    pcnt    = 0U;
    SysState_t  st      = STATE_IDLE;
    TickType_t  t_probe = 0;

    prvSetState(STATE_IDLE, 0, SLOT_REJECT);

    for (;;) {
        prvDebug();
        vTaskDelay(pdMS_TO_TICKS(100));
        TickType_t now = xTaskGetTickCount();
        if ((now - t_probe) < pdMS_TO_TICKS(PROBE_PERIOD_MS)) { continue; }
        t_probe = now;

        cd = prvMeasureColor();

        if (st == STATE_IDLE) {
            if (cd.id == COLOR_NONE) { pcnt = 0U; last_id = COLOR_NONE; continue; }
            if (cd.id == last_id) { pcnt++; } else { pcnt = 1U; last_id = cd.id; }
            if (pcnt < PROBE_CONFIRM) { continue; }

            /* --- 1. Mau da xac nhan --- */
            pcnt = 0U; last_id = COLOR_NONE;
            int32_t w10 = 0;
            prvSetState(STATE_COLOR, 0, Sort_Decide(cd.id, 0.0f));

            /* --- 2. Can: task_weigh tu hien man hinh rieng (mau + khoi luong) --- */
#if APP_USE_WEIGHT
            if (!task_weigh_measure(cd.id, &w10, pdMS_TO_TICKS(WEIGH_WAIT_MS))) {
                w10 = 0;
            }
#endif
            /* --- 3. Servo --- */
            prvSort(&cd, w10);
            st = STATE_DONE;
            pcnt = 0U;
            t_probe = xTaskGetTickCount();
        } else {
            /* --- 4. Cho lay vat ra: mau ve "khong co vat" --- */
            if (cd.id == COLOR_NONE) {
                if (++pcnt >= PROBE_CONFIRM) {
                    st = STATE_IDLE;
                    pcnt = 0U; last_id = COLOR_NONE;
                    prvSetState(STATE_IDLE, 0, SLOT_REJECT);
                }
            } else {
                pcnt = 0U;
            }
        }
    }
}

BaseType_t task_control_create(void)
{
    return xTaskCreate(prvControlTask, "CONTROL", CONTROL_STACK_WORDS,
                       NULL, CONTROL_PRIO, NULL);
}
