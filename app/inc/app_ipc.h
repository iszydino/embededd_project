#ifndef APP_IPC_H
#define APP_IPC_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"

typedef struct {
    int32_t g10;      /* 0.1 gram */
    uint8_t stable;
    int32_t raw;
} WeightData_t;

#define EVT_TARE (1u << 0)

extern QueueHandle_t xWeightQueue;
extern EventGroupHandle_t xEvt;

void app_ipc_init(void);

#endif
