#ifndef APP_IPC_H
#define APP_IPC_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"
#include "task_color.h"

typedef struct {
    int32_t g10;      /* 0.1 gram */
    uint8_t stable;
    int32_t raw;
} WeightData_t;

#define EVT_TARE       (1u << 0)
#define EVT_SORT_DONE  (1u << 1)

extern QueueHandle_t      xWeightQueue;
extern QueueHandle_t      xColorQueue;
extern EventGroupHandle_t xEvt;

void app_ipc_init(void);

#endif
