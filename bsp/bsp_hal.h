#ifndef BSP_HAL_H
#define BSP_HAL_H

#include "stm32f1xx_hal.h"

extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim4;

/* Goi 1 lan trong main(), sau khi set clock 72 MHz, truoc vTaskStartScheduler().
 * Khoi tao: GPIO nut bam, I2C1 (OLED), TIM4 PWM (servo). Tra ve 0 neu OK. */
int bsp_hal_init(void);

#endif
