#include "app_ipc.h"

QueueHandle_t xColorQueue = NULL;

void app_ipc_init(void)
{
    xColorQueue = xQueueCreate(4, sizeof(ColorData_t));
}
