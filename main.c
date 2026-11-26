#include "stm32f1xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_hal.h"
#include "hal_tcs3200.h"
#include "hal_hx711.h"
#include "app_ipc.h"
#include "app_sort.h"
#include "task_color.h"
#include "task_control.h"

#ifndef APP_USE_WEIGHT
#define APP_USE_WEIGHT 0
#endif
#if APP_USE_WEIGHT
#include "task_weight.h"
#include "task_weigh.h"
#endif

volatile const char *g_ovf_name;

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

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    g_ovf_name = pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

int main(void)
{
    clock_72mhz();
    NVIC_SetPriorityGrouping(3U);

    if (bsp_hal_init() != 0) { for (;;) {} }
    (void)hal_tcs3200_init();
#if APP_USE_WEIGHT
    hal_hx711_init();
#endif
    app_ipc_init();

    if (task_color_create() != pdPASS)       { for (;;) {} }
    if (task_servo_oled_create() != pdPASS)  { for (;;) {} }
    if (task_control_create() != pdPASS)     { for (;;) {} }
#if APP_USE_WEIGHT
    if (task_weight_create() != pdPASS)      { for (;;) {} }
    if (task_weigh_create() != pdPASS)       { for (;;) {} }
#endif

    vTaskStartScheduler();
    for (;;) {}
}
