#ifndef APP_IPC_H
#define APP_IPC_H

#include "FreeRTOS.h"
#include "queue.h"
#include "task_color.h"

extern QueueHandle_t xColorQueue;   /* ColorData_t */

void app_ipc_init(void);

#endif
