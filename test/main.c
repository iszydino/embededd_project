#include "stm32f1xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "hal_tcs3200.h"

volatile uint32_t g_r, g_g, g_b, g_status, g_count;

/* HSE 8MHz x9 = 72MHz, APB1 /2, flash 2 wait states */
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

static void prvTest(void *p)
{
    (void)p;
    for (;;) {
        tcs_rgb_t f;
        g_status = (uint32_t)hal_tcs3200_read_rgb(50, &f);
        if (g_status == 0U) { g_r = f.r; g_g = f.g; g_b = f.b; }
        g_count++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vApplicationStackOverflowHook(TaskHandle_t t, char *n) { (void)t; (void)n; for (;;) {} }

int main(void)
{
    clock_72mhz();
    NVIC_SetPriorityGrouping(3U);   /* group 4: 4 bit preempt, bat buoc voi FreeRTOS */
    (void)hal_tcs3200_init();
    xTaskCreate(prvTest, "TCS_TEST", 256, NULL, tskIDLE_PRIORITY + 1, NULL);
    vTaskStartScheduler();
    for (;;) {}
}
