#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "task_color.h"
#include "task_color_cfg.h"
#include "app_ipc.h"
#include "hal_tcs3200.h"

#ifndef TASK_COLOR_STACK_WORDS
#define TASK_COLOR_STACK_WORDS  (configMINIMAL_STACK_SIZE + 128)
#endif
#ifndef TASK_COLOR_PRIO
#define TASK_COLOR_PRIO         (tskIDLE_PRIORITY + 2)
#endif

static TaskHandle_t xColorTaskHandle = NULL;

/* Tru nen, chuan hoa ve 0..255 */
static uint32_t prvNorm(uint32_t f, uint32_t dark, uint32_t white)
{
    uint32_t span = (white > dark) ? (white - dark) : 1U;
    uint32_t v = (f > dark) ? (f - dark) : 0U;
    v = (v * 255UL) / span;
    return (v > 255U) ? 255U : v;
}

static ColorId_t prvClassify(uint32_t r, uint32_t g, uint32_t b)
{
    uint32_t mx = r, mn = r;
    if (g > mx) { mx = g; }
    if (b > mx) { mx = b; }
    if (g < mn) { mn = g; }
    if (b < mn) { mn = b; }

    if (mx < COLOR_MIN_LEVEL)                         { return COLOR_NONE; }
    if ((mn * 10U) >= (mx * COLOR_WHITE_RATIO))       { return COLOR_WHITE; }
    if ((r * 10U >= g * COLOR_DOMINANCE_X10) && (r * 10U >= b * COLOR_DOMINANCE_X10)) { return COLOR_RED; }
    if ((g * 10U >= r * COLOR_DOMINANCE_X10) && (g * 10U >= b * COLOR_DOMINANCE_X10)) { return COLOR_GREEN; }
    if ((b * 10U >= r * COLOR_DOMINANCE_X10) && (b * 10U >= g * COLOR_DOMINANCE_X10)) { return COLOR_BLUE; }
    return COLOR_UNKNOWN;
}

/* Loc da so cho 3 mau: it nhat 2 mau trung nhau, neu khong thi UNKNOWN */
static ColorId_t prvMajority(const ColorId_t s[COLOR_SAMPLES])
{
    if ((s[0] == s[1]) || (s[0] == s[2])) { return s[0]; }
    if (s[1] == s[2])                     { return s[1]; }
    return COLOR_UNKNOWN;
}

/* Do 1 mau; tra pdTRUE neu doc HAL thanh cong */
static BaseType_t prvSample(ColorId_t *id, ColorData_t *d)
{
    tcs_rgb_t f;
    if (hal_tcs3200_read_rgb(COLOR_WINDOW_MS, &f) != HAL_TCS_OK) { return pdFALSE; }

    d->r = f.r; d->g = f.g; d->b = f.b;
    uint32_t nr = prvNorm(f.r, CAL_DARK_R, CAL_WHITE_R);
    uint32_t ng = prvNorm(f.g, CAL_DARK_G, CAL_WHITE_G);
    uint32_t nb = prvNorm(f.b, CAL_DARK_B, CAL_WHITE_B);
    d->nr = (uint8_t)nr; d->ng = (uint8_t)ng; d->nb = (uint8_t)nb;
    *id = prvClassify(nr, ng, nb);
    return pdTRUE;
}

static void prvColorTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
#if COLOR_TRIGGER_MODE
        /* cho trigger, co timeout de khong block vinh vien */
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0U) { continue; }
        vTaskDelay(pdMS_TO_TICKS(COLOR_SETTLE_MS));
#endif
        ColorId_t s[COLOR_SAMPLES];
        ColorData_t d;
        BaseType_t ok = pdTRUE;

        for (uint32_t i = 0; (i < COLOR_SAMPLES) && (ok == pdTRUE); i++) {
            ok = prvSample(&s[i], &d);
        }

        if (ok == pdTRUE) {
            d.id = prvMajority(s);
            if (xColorQueue != NULL) {
                (void)xQueueSend(xColorQueue, &d, 0);
            }
        }

#if !COLOR_TRIGGER_MODE
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(COLOR_PERIOD_MS));
#else
        (void)xLastWakeTime;
#endif
    }
}

BaseType_t task_color_create(void)
{
    return xTaskCreate(prvColorTask, "COLOR_TASK", TASK_COLOR_STACK_WORDS,
                       NULL, TASK_COLOR_PRIO, &xColorTaskHandle);
}

void task_color_trigger(void)
{
    if (xColorTaskHandle != NULL) { (void)xTaskNotifyGive(xColorTaskHandle); }
}

void task_color_trigger_fromisr(BaseType_t *pxWoken)
{
    if (xColorTaskHandle != NULL) { vTaskNotifyGiveFromISR(xColorTaskHandle, pxWoken); }
}
