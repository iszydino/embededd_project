#ifndef TASK_WEIGH_H
#define TASK_WEIGH_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "task_color.h"

BaseType_t task_weigh_create(void);

/* Goi tu task_control sau khi da biet mau. Chan toi khi can xong (hoac het gio).
 * g10 = khoi luong, don vi 0.1 g. Tra ve true neu do duoc. */
bool task_weigh_measure(ColorId_t color, int32_t *g10, TickType_t timeout);

#endif
