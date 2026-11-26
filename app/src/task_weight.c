#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "app_cfg.h"
#include "app_ipc.h"
#include "hal_hx711.h"
#include "task_weight.h"

#define HX_TIMEOUT_MS  300U
#ifndef WEIGHT_WARMUP_MS
#define WEIGHT_WARMUP_MS 5000U
#endif

/* Bien debug doc bang OpenOCD */
volatile int32_t  g_w_raw, g_w_g10;
volatile uint32_t g_w_stable, g_w_fail;
volatile int32_t  g_w_off, g_w_spread;

static int32_t  s_win[WEIGHT_AVG_N];
static uint32_t s_cnt, s_idx;
static int32_t  s_offset;

static int32_t prvAbs(int32_t v) { return (v < 0) ? -v : v; }

static bool prvReadValid(int32_t *raw)
{
    if (!hal_hx711_read(raw, HX_TIMEOUT_MS)) { return false; }
    return !hal_hx711_raw_is_bad(*raw);
}

/* Trung binh cat bo: sap xep, bo WEIGHT_TRIM mau thap nhat va cao nhat.
 * spread = chenh lech con lai, dung de xet on dinh. */
static int32_t prvTrimmed(const int32_t *w, int32_t *spread)
{
    int32_t t[WEIGHT_AVG_N];
    for (uint32_t i = 0; i < WEIGHT_AVG_N; i++) { t[i] = w[i]; }
    for (uint32_t i = 1; i < WEIGHT_AVG_N; i++) {
        int32_t  v = t[i];
        uint32_t j = i;
        while ((j > 0U) && (t[j - 1U] > v)) { t[j] = t[j - 1U]; j--; }
        t[j] = v;
    }
    int32_t sum = 0;
    for (uint32_t i = WEIGHT_TRIM; i < (WEIGHT_AVG_N - WEIGHT_TRIM); i++) { sum += t[i]; }
    *spread = t[WEIGHT_AVG_N - WEIGHT_TRIM - 1U] - t[WEIGHT_TRIM];
    return sum / (int32_t)(WEIGHT_AVG_N - 2U * WEIGHT_TRIM);
}

/* Dat 0 g: trung binh WEIGHT_TARE_N mau luc can trong */
static void prvTare(void)
{
    int64_t  sum = 0;
    uint32_t got = 0U, tries = 0U;
    int32_t  raw;

    (void)prvReadValid(&raw);          /* bo 2 mau dau: kenh/gain co the chua kip doi */
    (void)prvReadValid(&raw);
    while ((got < WEIGHT_TARE_N) && (tries < (3U * WEIGHT_TARE_N))) {
        tries++;
        if (prvReadValid(&raw)) { sum += raw; got++; } else { g_w_fail++; }
    }
    if (got > 0U) { s_offset = (int32_t)(sum / (int64_t)got); }
}

static void prvWeightTask(void *pvParameters)
{
    (void)pvParameters;
    WeightData_t out  = { 0, 0U, 0 };
    int32_t      last = 0;
    float        sc   = HX711_SCALE;
    if (sc < 0.0f) { sc = -sc; }

    vTaskDelay(pdMS_TO_TICKS(WEIGHT_WARMUP_MS));
    prvTare();
    g_w_off = s_offset;

    for (;;) {
        if ((xEventGroupGetBits(xEvt) & EVT_TARE) != 0U) {
            (void)xEventGroupClearBits(xEvt, EVT_TARE);
            prvTare();
            s_cnt = 0U; s_idx = 0U; last = 0;
        }

        int32_t raw;
        if (!prvReadValid(&raw)) {
            g_w_fail++;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        g_w_raw = raw;

        s_win[s_idx] = raw;
        s_idx = (s_idx + 1U) % WEIGHT_AVG_N;
        if (s_cnt < WEIGHT_AVG_N) { s_cnt++; }
        if (s_cnt < WEIGHT_AVG_N) { continue; }

        int32_t spread;
        int32_t mean = prvTrimmed(s_win, &spread);
        g_w_spread = spread;

        float   gf  = ((float)(mean - s_offset) * 10.0f) / HX711_SCALE;
        int32_t g10 = (int32_t)((gf >= 0.0f) ? (gf + 0.5f) : (gf - 0.5f));
        if (prvAbs(g10) < WEIGHT_ZERO_BAND_G10)      { g10 = 0; }
        else if (prvAbs(g10 - last) < WEIGHT_DEADBAND_G10) { g10 = last; }
        last = g10;

        float spread_g10 = ((float)spread * 10.0f) / sc;

        out.g10    = g10;
        out.raw    = mean;
        out.stable = (spread_g10 <= (WEIGHT_STABLE_G * 10.0f)) ? 1U : 0U;
        (void)xQueueOverwrite(xWeightQueue, &out);

        g_w_g10    = g10;
        g_w_stable = out.stable;
        vTaskDelay(1);       /* HX711 10 mau/giay nen viec doc da tu giu nhip */
    }
}

BaseType_t task_weight_create(void)
{
    return xTaskCreate(prvWeightTask, "WEIGHT", TASK_WEIGHT_STACK,
                       NULL, TASK_WEIGHT_PRIO, NULL);
}
