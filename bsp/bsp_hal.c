#include "bsp_hal.h"
#include "bsp_pins.h"
#include "FreeRTOS.h"
#include "task.h"

I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim4;

/* Cho ST HAL dung tick cua FreeRTOS. KHONG goi HAL_Init() */
uint32_t HAL_GetTick(void)
{
    return (uint32_t)xTaskGetTickCount();
}

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    (void)TickPriority;
    return HAL_OK;
}

void HAL_Delay(uint32_t Delay)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(Delay));
    } else {
        volatile uint32_t n = Delay * (SystemCoreClock / 8000U);
        while (n--) {}
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *h)
{
    if (h->Instance == I2C1) {
        GPIO_InitTypeDef g = {0};
        __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
        g.Mode  = GPIO_MODE_AF_OD;
        g.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &g);
        __HAL_RCC_I2C1_CLK_ENABLE();
    }
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *h)
{
    if (h->Instance == TIM4) {
        GPIO_InitTypeDef g = {0};
        __HAL_RCC_TIM4_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        g.Pin   = SERVO_PIN;
        g.Mode  = GPIO_MODE_AF_PP;
        g.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(SERVO_PORT, &g);
    }
}

int bsp_hal_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 3 nut bam PA2, PA3, PA4: input pull-up (nhan = 0) */
    g.Pin   = BTN_NEXT_Pin | BTN_UP_Pin | BTN_EN_Pin;
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);

    /* I2C1 400 kHz cho OLED */
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 400000U;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0U;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0U;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) { return -1; }

    /* TIM4 CH3 PWM 50 Hz, 1 tick = 1 us */
    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = (SystemCoreClock / 1000000U) - 1U;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = 19999U;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim4) != HAL_OK) { return -2; }

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 1500U;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim4, &oc, TIM_CHANNEL_3) != HAL_OK) { return -3; }

    return 0;
}
