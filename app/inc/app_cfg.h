#ifndef APP_CFG_H
#define APP_CFG_H

#include "FreeRTOS.h"
#include "task.h"

/* Load Cell / HX711 */
#define TASK_WEIGHT_PRIO       (tskIDLE_PRIORITY + 2)
#define TASK_WEIGHT_STACK      256

#define HX711_SCALE            105.0f   /* raw counts per gram; MUST calibrate */
#define WEIGHT_PERIOD_MS       100
#define WEIGHT_TARE_N          20

/* Filter: 10 samples, discard 2 highest + 2 lowest. */
#define WEIGHT_AVG_N           10
#define WEIGHT_TRIM            2

/* Weight behavior copied from the supplied standalone code. */
#define WEIGHT_STEP_G          20.0f
#define WEIGHT_STABLE_G        0.5f
#define WEIGHT_DEADBAND_G10    2       /* 0.2 g */
#define WEIGHT_ZERO_BAND_G10   5       /* 0.5 g */

#endif
