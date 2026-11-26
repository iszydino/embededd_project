#ifndef HAL_HX711_H
#define HAL_HX711_H

#include <stdint.h>
#include <stdbool.h>

/* So xung SCK them sau 24 bit: 1 = kenh A gain128, 2 = kenh B gain32, 3 = kenh A gain64.
 * Mac dinh 2 nhu code goc. Doi o day neu load cell noi kenh A. */
#ifndef HX711_GAIN_PULSES
#define HX711_GAIN_PULSES 2U
#endif

void hal_hx711_init(void);

/* Doc 1 mau 24 bit. Tra ve true neu OK, false neu timeout cho DOUT xuong thap. */
bool hal_hx711_read(int32_t *raw, uint32_t timeout_ms);

bool hal_hx711_raw_is_bad(int32_t raw);

#endif
