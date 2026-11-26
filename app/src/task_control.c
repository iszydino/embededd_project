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
#include "app_cfg.h"
#endif

#define CONTROL_STACK_WORDS   256
#define CONTROL_PRIO          (tskIDLE_PRIORITY + 3)

/* Phat hien vat bang CHENH LECH raw so voi nen, khong phu thuoc HX711_SCALE */
#define PRESENT_RAW           300     /* >= nay: co vat */
#define RELEASE_RAW           150     /* <  nay: da lay vat ra */
#define PRESENT_CNT           3U      /* so mau lien tiep xac nhan co vat */
#define RELEASE_CNT           5U      /* so mau lien tiep xac nhan het vat */
#define SETTLE_MS             1200U   /* cho vat dat yen truoc khi do mau */

#define COLOR_TIMEOUT_MS      3000U
#define SORT_TIMEOUT_MS       6000U
#define TEST_PERIOD_MS        3000U   /* che do khong can */

#define PROBE_PERIOD_MS       300U    /* chu ky do mau de phat hien vat (khong can len can) */
#define PROBE_CONFIRM         2U      /* so lan do lien tiep de xac nhan */

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

#if APP_USE_WEIGHT
static int32_t prvToG10(int32_t delta)
{
    float g = ((float)delta * 10.0f) / HX711_SCALE;
    return (int32_t)((g >= 0.0f) ? (g + 0.5f) : (g - 0.5f));
}

static void prvRunWeight(void)
{
    WeightData_t w;
    ColorData_t  cd;
    SysState_t   st = STATE_IDLE;
    int32_t      base = 0, w10 = 0, shown = 0;
    uint32_t     cnt = 0U, pcnt = 0U;
    bool         have_base = false;
    TickType_t   t_mark = 0, t_probe = 0;
    ColorId_t    last_id = COLOR_NONE;

    prvSetState(STATE_IDLE, 0, SLOT_REJECT);

    for (;;) {
        prvDebug();

        int32_t d = 0, ad = 0, g10 = 0;
        bool    wv = false;
        if (xQueueReceive(xWeightQueue, &w, pdMS_TO_TICKS(100)) == pdPASS) {
            if (!have_base) { base = w.raw; have_base = true; }
            d   = w.raw - base;
            ad  = (d < 0) ? -d : d;
            g10 = prvToG10(d);
            wv  = true;
            g_ctl_delta = d;
            g_ctl_base  = base;
        }

        TickType_t now = xTaskGetTickCount();
        bool probe_due = ((now - t_probe) >= pdMS_TO_TICKS(PROBE_PERIOD_MS));

        switch (st) {
        case STATE_IDLE:
            if (wv) {
                if (ad >= PRESENT_RAW) {
                    if (++cnt >= PRESENT_CNT) {
                        cnt = 0U;
                        shown = g10;
                        t_mark = now;
                        st = STATE_WEIGHING;
                        prvSetState(STATE_WEIGHING, g10, SLOT_REJECT);
                    }
                } else {
                    cnt = 0U;
                    base += d / 64;          /* nen troi theo khi can trong */
                }
            }
            if ((st == STATE_IDLE) && probe_due) {
                t_probe = now;
                cd = prvMeasureColor();
                if (cd.id != COLOR_NONE) {
                    if (cd.id == last_id) { pcnt++; } else { pcnt = 1U; last_id = cd.id; }
                    if (pcnt >= PROBE_CONFIRM) {
                        w10 = wv ? g10 : 0;
                        pcnt = 0U; last_id = COLOR_NONE; cnt = 0U;
                        prvSetState(STATE_COLOR, w10, SLOT_REJECT);
                        prvSort(&cd, w10);
                        st = STATE_DONE;
                        t_probe = xTaskGetTickCount();
                    }
                } else {
                    pcnt = 0U; last_id = COLOR_NONE;
                }
            }
            break;

        case STATE_WEIGHING:
            if (wv && (ad < RELEASE_RAW)) {  /* chi la nhieu, hoac vat bi lay ra */
                st = STATE_IDLE;
                cnt = 0U;
                prvSetState(STATE_IDLE, 0, SLOT_REJECT);
                break;
            }
            if (wv && (((g10 - shown) >= 3) || ((shown - g10) >= 3))) {
                shown = g10;
                prvSetState(STATE_WEIGHING, shown, SLOT_REJECT);
            }
            if ((now - t_mark) >= pdMS_TO_TICKS(SETTLE_MS)) {
                w10 = wv ? g10 : shown;
                st  = STATE_COLOR;
            }
            break;

        case STATE_DONE:                     /* cho lay vat ra: can ve nen VA mau ve 'khong co vat' */
            if (wv) {
                if (ad < RELEASE_RAW) { cnt++; } else { cnt = 0U; }
            }
            if (probe_due) {
                t_probe = now;
                cd = prvMeasureColor();
                if (cd.id == COLOR_NONE) { pcnt++; } else { pcnt = 0U; }
            }
            if ((pcnt >= PROBE_CONFIRM) && ((!have_base) || (cnt >= RELEASE_CNT))) {
                st = STATE_IDLE;
                cnt = 0U; pcnt = 0U; last_id = COLOR_NONE;
                prvSetState(STATE_IDLE, 0, SLOT_REJECT);
            }
            break;

        default:
            break;
        }

        if (st == STATE_COLOR) {             /* duong phat hien bang can */
            prvSetState(STATE_COLOR, w10, SLOT_REJECT);
            cd = prvMeasureColor();
            prvSort(&cd, w10);
            st  = STATE_DONE;
            cnt = 0U; pcnt = 0U;
            t_probe = xTaskGetTickCount();
        }
    }
}
#else
static void prvRunNoWeight(void)
{
    ColorData_t cd;

    prvSetState(STATE_IDLE, 0, SLOT_REJECT);
    for (;;) {
        prvDebug();
        vTaskDelay(pdMS_TO_TICKS(TEST_PERIOD_MS));

        prvSetState(STATE_COLOR, 0, SLOT_REJECT);
        cd = prvMeasureColor();
        if (cd.id != COLOR_NONE) {       /* khong co vat thi khong gat servo */
            prvSort(&cd, 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        prvSetState(STATE_IDLE, 0, SLOT_REJECT);
    }
}
#endif

static void prvControlTask(void *pvParameters)
{
    (void)pvParameters;
#if APP_USE_WEIGHT
    prvRunWeight();
#else
    prvRunNoWeight();
#endif
}

BaseType_t task_control_create(void)
{
    return xTaskCreate(prvControlTask, "CONTROL", CONTROL_STACK_WORDS,
                       NULL, CONTROL_PRIO, NULL);
}
