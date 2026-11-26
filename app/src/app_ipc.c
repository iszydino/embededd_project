#include "app_ipc.h"
#include "app_sort.h"

QueueHandle_t      xWeightQueue = NULL;
QueueHandle_t      xColorQueue  = NULL;
EventGroupHandle_t xEvt         = NULL;

void app_ipc_init(void)
{
    xWeightQueue = xQueueCreate(1, sizeof(WeightData_t));
    xColorQueue  = xQueueCreate(2, sizeof(ColorData_t));
    xEvt         = xEventGroupCreate();

    App_SharedInit();

    configASSERT(xWeightQueue != NULL);
    configASSERT(xColorQueue != NULL);
    configASSERT(xEvt != NULL);
}
