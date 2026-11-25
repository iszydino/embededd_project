#include "stm32f1xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "hal_tcs3200.h"
#include "app_ipc.h"
#include "task_color.h"

volatile uint32_t g_r, g_g, g_b, g_status, g_count, g_class;
volatile uint32_t g_nr, g_ng, g_nb;

static void clock_72mhz(void)
{
    uint32_t t = 0;
    FLASH->ACR = 0x12U;
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) { if (++t > 200000UL) { return; } }
    RCC->CFGR = RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9 | RCC_CFGR_PPRE1_DIV2;
    RCC->CR |= RCC_CR_PLLON;
    t = 0;
    while (!(RCC->CR & RCC_CR_PLLRDY)) { if (++t > 200000UL) { RCC->CR &= ~RCC_CR_PLLON; return; } }
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}
    SystemCoreClock = 72000000UL;
}

/* Task gia lap task_display: nhan ket qua mau */
static void prvConsumer(void *p)
{
    (void)p;
    ColorData_t d;
    for (;;) {
        if (xQueueReceive(xColorQueue, &d, pdMS_TO_TICKS(1000)) == pdPASS) {
            g_r = d.r; g_g = d.g; g_b = d.b;
            g_nr = d.nr; g_ng = d.ng; g_nb = d.nb;
            g_class = (uint32_t)d.id;
            g_status = 0U;
            g_count++;
        } else {
            g_status = 1U;
        }
    }
}

void vApplicationStackOverflowHook(TaskHandle_t t, char *n) { (void)t; (void)n; for (;;) {} }

int main(void)
{
    clock_72mhz();
    NVIC_SetPriorityGrouping(3U);
    (void)hal_tcs3200_init();
    app_ipc_init();
    (void)task_color_create();
    xTaskCreate(prvConsumer, "CONSUMER", 128, NULL, tskIDLE_PRIORITY + 1, NULL);
    vTaskStartScheduler();
    for (;;) {}
}
