#ifndef HAL_HX711_H
#define HAL_HX711_H

#include <stdint.h>
#include <stdbool.h>

void hal_hx711_init(void);

/**
 * @brief Read one 24-bit sample from HX711 channel B, gain 32.
 * @param raw Destination for signed raw value.
 * @param timeout_ms Maximum wait time for DOUT to go low.
 * @return true on success, false on timeout.
 */
bool hal_hx711_read(int32_t *raw, uint32_t timeout_ms);

bool hal_hx711_raw_is_bad(int32_t raw);

#endif
