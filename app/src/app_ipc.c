#include "app_ipc.h"

QueueHandle_t xWeightQueue;
EventGroupHandle_t xEvt;

void app_ipc_init(void)
{
    xWeightQueue = xQueueCreate(1, sizeof(WeightData_t));
    xEvt = xEventGroupCreate();

    configASSERT(xWeightQueue != NULL);
    configASSERT(xEvt != NULL);
}
