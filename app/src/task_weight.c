#include "FreeRTOS.h"
#include "task.h"
#include "task_weight.h"
#include "app_cfg.h"
#include "app_ipc.h"
#include "hal_hx711.h"

static TaskHandle_t xWeightTaskHandle = NULL;

static int32_t fbuf[WEIGHT_AVG_N];
static uint8_t fidx = 0;

static int32_t gOffset = 0;
static int32_t gShownG10 = 0;
static uint8_t gFilled = 0;

static void filter_fill(int32_t value)
{
    for (uint8_t i = 0; i < WEIGHT_AVG_N; i++) {
        fbuf[i] = value;
    }
    fidx = 0;
    gFilled = WEIGHT_AVG_N;
}

static void filter_push(int32_t value)
{
    fbuf[fidx] = value;
    fidx = (uint8_t)((fidx + 1U) % WEIGHT_AVG_N);
    if (gFilled < WEIGHT_AVG_N) gFilled++;
}

static int32_t filter_get(int32_t *range)
{
    int32_t s[WEIGHT_AVG_N];

    for (uint8_t i = 0; i < gFilled; i++) {
        s[i] = fbuf[i];
    }

    for (uint8_t i = 1; i < gFilled; i++) {
        int32_t key = s[i];
        int j = (int)i - 1;
        while (j >= 0 && s[j] > key) {
            s[j + 1] = s[j];
            j--;
        }
        s[j + 1] = key;
    }

    if (gFilled == 0) {
        if (range) *range = 0;
        return 0;
    }

    uint8_t trim = (gFilled > 2U * WEIGHT_TRIM) ? WEIGHT_TRIM : 0U;
    int64_t sum = 0;
    for (uint8_t i = trim; i < (uint8_t)(gFilled - trim); i++) {
        sum += s[i];
    }

    if (range) {
        *range = s[gFilled - trim - 1U] - s[trim];
    }

    return (int32_t)(sum / (gFilled - 2U * trim));
}

static void do_tare(void)
{
    int64_t sum = 0;
    uint8_t good = 0;

    filter_fill(0);

    for (uint8_t i = 0; i < WEIGHT_TARE_N * 2U && good < WEIGHT_TARE_N; i++) {
        int32_t raw;
        if (hal_hx711_read(&raw, 200U) && !hal_hx711_raw_is_bad(raw)) {
            sum += raw;
            good++;
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }

    if (good > 0U) {
        gOffset = (int32_t)(sum / good);
        filter_fill(gOffset);
        gShownG10 = 0;
    }
}

static void prvWeightTask(void *pvParameters)
{
    (void)pvParameters;

    TickType_t lastWake = xTaskGetTickCount();

    /* Power-up tare: scale must be unloaded. */
    do_tare();

    for (;;) {
        if ((xEventGroupWaitBits(
                xEvt,
                EVT_TARE,
                pdTRUE,
                pdFALSE,
                0) & EVT_TARE) != 0U) {
            do_tare();
        }

        int32_t raw;
        WeightData_t d = {0};

        if (hal_hx711_read(&raw, 200U)) {
            if (!hal_hx711_raw_is_bad(raw)) {
                int32_t cur = filter_get(NULL);
                int32_t diff = raw - cur;
                if (diff < 0) diff = -diff;

                float stepRaw = WEIGHT_STEP_G * HX711_SCALE;

                if ((float)diff >= stepRaw) {
                    static uint8_t bigCount = 0;
                    if (++bigCount >= 2U) {
                        filter_fill(raw);
                        bigCount = 0;
                    } else {
                        filter_push(raw);
                    }
                } else {
                    filter_push(raw);
                }

                int32_t range = 0;
                int32_t filt = filter_get(&range);
                uint8_t stable = ((float)range < (WEIGHT_STABLE_G * HX711_SCALE));

                int32_t net = filt - gOffset;
                float f = ((float)net * 10.0f) / HX711_SCALE;
                int32_t g10 = (int32_t)(f >= 0.0f ? f + 0.5f : f - 0.5f);

                int32_t delta = g10 - gShownG10;
                if (delta < 0) delta = -delta;
                if (delta >= WEIGHT_DEADBAND_G10) {
                    gShownG10 = g10;
                }

                if (gShownG10 > -WEIGHT_ZERO_BAND_G10 &&
                    gShownG10 < WEIGHT_ZERO_BAND_G10) {
                    gShownG10 = 0;
                }

                d.g10 = gShownG10;
                d.stable = stable;
                d.raw = filt;
                xQueueOverwrite(xWeightQueue, &d);
            }
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(WEIGHT_PERIOD_MS));
    }
}

BaseType_t task_weight_create(void)
{
    return xTaskCreate(
        prvWeightTask,
        "WEIGHT",
        TASK_WEIGHT_STACK,
        NULL,
        TASK_WEIGHT_PRIO,
        &xWeightTaskHandle
    );
}
