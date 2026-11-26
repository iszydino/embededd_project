#ifndef BSP_PINS_H
#define BSP_PINS_H

#include "stm32f1xx_hal.h"

/* HX711 (giu nguyen cua ban Load cell) */
#define HX711_DT_PORT        GPIOB
#define HX711_DT_PIN         GPIO_PIN_0
#define HX711_SCK_PORT       GPIOB
#define HX711_SCK_PIN        GPIO_PIN_1

/* Servo: TIM4_CH3 = PB8 */
#define SERVO_PORT           GPIOB
#define SERVO_PIN            GPIO_PIN_8

/* 3 nut bam, noi chan xuong GND, pull-up noi */
#define BTN_NEXT_GPIO_Port   GPIOA
#define BTN_NEXT_Pin         GPIO_PIN_2
#define BTN_UP_GPIO_Port     GPIOA
#define BTN_UP_Pin           GPIO_PIN_3
#define BTN_EN_GPIO_Port     GPIOA
#define BTN_EN_Pin           GPIO_PIN_4

/* OLED SSD1306: I2C1 = PB6 (SCL), PB7 (SDA) */
/* TCS3200: OUT = PA0 (TIM2_ETR), S0..S3 = PB12..PB15, TIM2 + TIM3 */

#endif
