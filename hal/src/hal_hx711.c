#include "hal_hx711.h"
#include "bsp_pins.h"
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

#define HX711_GAIN_PULSES 2U

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks) {}
}

void hal_hx711_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    g.Pin = HX711_SCK_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(HX711_SCK_PORT, &g);

    g.Pin = HX711_DT_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(HX711_DT_PORT, &g);

    HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_RESET);
    dwt_init();
}

bool hal_hx711_read(int32_t *raw, uint32_t timeout_ms)
{
    if (raw == NULL) return false;

    uint32_t t0 = HAL_GetTick();

    while (HAL_GPIO_ReadPin(HX711_DT_PORT, HX711_DT_PIN) == GPIO_PIN_SET) {
        if ((HAL_GetTick() - t0) >= timeout_ms) return false;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    uint32_t data = 0;

    taskENTER_CRITICAL();

    for (uint8_t i = 0; i < 24; i++) {
        HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_SET);
        delay_us(1);

        data <<= 1;
        if (HAL_GPIO_ReadPin(HX711_DT_PORT, HX711_DT_PIN) == GPIO_PIN_SET) {
            data |= 1U;
        }

        HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_RESET);
        delay_us(1);
    }

    /* 2 pulses -> Channel B, gain 32 for the next conversion. */
    for (uint8_t i = 0; i < HX711_GAIN_PULSES; i++) {
        HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_SET);
        delay_us(1);
        HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_RESET);
        delay_us(1);
    }

    taskEXIT_CRITICAL();

    if (data & 0x800000UL) {
        data |= 0xFF000000UL;
    }

    *raw = (int32_t)data;
    return true;
}

bool hal_hx711_raw_is_bad(int32_t raw)
{
    return (raw >= 0x7FFFFF) || (raw <= -0x800000);
}
