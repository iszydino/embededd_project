#ifndef HAL_TCS3200_H
#define HAL_TCS3200_H

#include <stdint.h>

typedef enum {
    TCS_FILTER_RED = 0,
    TCS_FILTER_GREEN,
    TCS_FILTER_BLUE,
    TCS_FILTER_CLEAR
} tcs_filter_t;

typedef enum {
    TCS_SCALE_POWER_DOWN = 0,
    TCS_SCALE_2,
    TCS_SCALE_20,
    TCS_SCALE_100
} tcs_scale_t;

typedef enum {
    HAL_TCS_OK = 0,
    HAL_TCS_ERR_PARAM,
    HAL_TCS_ERR_INIT,
    HAL_TCS_ERR_BUSY,
    HAL_TCS_ERR_TIMEOUT,
    HAL_TCS_ERR_OVERFLOW
} hal_tcs_status_t;

typedef struct {
    uint32_t r;   /* Hz */
    uint32_t g;
    uint32_t b;
} tcs_rgb_t;

/* Goi 1 lan truoc vTaskStartScheduler() */
hal_tcs_status_t hal_tcs3200_init(void);
hal_tcs_status_t hal_tcs3200_set_scale(tcs_scale_t scale);

/* Do tan so 1 kenh. window_ms: 1..1000. Goi tu Task */
hal_tcs_status_t hal_tcs3200_read_freq(tcs_filter_t filter,
                                       uint16_t window_ms,
                                       uint32_t *freq_hz);

/* Do lan luot R, G, B */
hal_tcs_status_t hal_tcs3200_read_rgb(uint16_t window_ms, tcs_rgb_t *out);

#endif /* HAL_TCS3200_H */
