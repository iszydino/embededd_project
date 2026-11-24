#include <stddef.h>
#include "hal_tcs3200.h"
#include "stm32f1xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* OUT = PA0 (TIM2_ETR). S0..S3 = PB12..PB15 */
#define TCS_CTRL_PORT        GPIOB
#define TCS_S0_PIN           12U
#define TCS_S1_PIN           13U
#define TCS_S2_PIN           14U
#define TCS_S3_PIN           15U

/* PHAI >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (chinh theo FreeRTOSConfig.h cua nhom) */
#define TCS_IRQ_PRIO         6U
#define TCS_SETTLE_MS        5U
#define TCS_MUTEX_TIMEOUT_MS 500U
#define TCS_WAIT_MARGIN_MS   10U

static SemaphoreHandle_t s_mutex = NULL;
static SemaphoreHandle_t s_done  = NULL;
static uint8_t s_inited = 0;

static void gpio_out_pp_2mhz(GPIO_TypeDef *port, uint32_t pin)
{
    volatile uint32_t *cr = (pin < 8U) ? &port->CRL : &port->CRH;
    uint32_t shift = (pin & 7U) * 4U;
    uint32_t v = *cr;
    v &= ~(0xFUL << shift);
    v |=  (0x2UL << shift);
    *cr = v;
}

static void pin_write(uint32_t pin, uint32_t level)
{
    TCS_CTRL_PORT->BSRR = level ? (1UL << pin) : (1UL << (pin + 16U));
}

static void set_filter(tcs_filter_t f)
{
    switch (f) {
    case TCS_FILTER_RED:   pin_write(TCS_S2_PIN, 0); pin_write(TCS_S3_PIN, 0); break;
    case TCS_FILTER_BLUE:  pin_write(TCS_S2_PIN, 0); pin_write(TCS_S3_PIN, 1); break;
    case TCS_FILTER_CLEAR: pin_write(TCS_S2_PIN, 1); pin_write(TCS_S3_PIN, 0); break;
    case TCS_FILTER_GREEN: pin_write(TCS_S2_PIN, 1); pin_write(TCS_S3_PIN, 1); break;
    default: break;
    }
}

hal_tcs_status_t hal_tcs3200_set_scale(tcs_scale_t s)
{
    switch (s) {
    case TCS_SCALE_POWER_DOWN: pin_write(TCS_S0_PIN, 0); pin_write(TCS_S1_PIN, 0); break;
    case TCS_SCALE_2:          pin_write(TCS_S0_PIN, 0); pin_write(TCS_S1_PIN, 1); break;
    case TCS_SCALE_20:         pin_write(TCS_S0_PIN, 1); pin_write(TCS_S1_PIN, 0); break;
    case TCS_SCALE_100:        pin_write(TCS_S0_PIN, 1); pin_write(TCS_S1_PIN, 1); break;
    default: return HAL_TCS_ERR_PARAM;
    }
    return HAL_TCS_OK;
}

hal_tcs_status_t hal_tcs3200_init(void)
{
    if (s_inited) { return HAL_TCS_OK; }

    s_mutex = xSemaphoreCreateMutex();
    s_done  = xSemaphoreCreateBinary();
    if ((s_mutex == NULL) || (s_done == NULL)) { return HAL_TCS_ERR_INIT; }

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN;
    (void)RCC->APB1ENR;

    /* PA0: input floating */
    GPIOA->CRL = (GPIOA->CRL & ~0xFUL) | 0x4UL;

    gpio_out_pp_2mhz(TCS_CTRL_PORT, TCS_S0_PIN);
    gpio_out_pp_2mhz(TCS_CTRL_PORT, TCS_S1_PIN);
    gpio_out_pp_2mhz(TCS_CTRL_PORT, TCS_S2_PIN);
    gpio_out_pp_2mhz(TCS_CTRL_PORT, TCS_S3_PIN);
    (void)hal_tcs3200_set_scale(TCS_SCALE_20);
    set_filter(TCS_FILTER_RED);

    /* TIM2: dem xung ngoai ETR (external clock mode 2) */
    TIM2->CR1  = 0;
    TIM2->PSC  = 0;
    TIM2->ARR  = 0xFFFF;
    TIM2->SMCR = TIM_SMCR_ECE;
    TIM2->EGR  = TIM_EGR_UG;
    TIM2->SR   = 0;

    /* TIM3: cua so do, tick 0.1 ms, one-pulse, ngat update */
    TIM3->CR1  = TIM_CR1_OPM;
    TIM3->PSC  = (uint16_t)((SystemCoreClock / 10000UL) - 1UL);
    TIM3->ARR  = 0xFFFF;
    TIM3->EGR  = TIM_EGR_UG;
    TIM3->SR   = 0;
    TIM3->DIER = TIM_DIER_UIE;
    NVIC_SetPriority(TIM3_IRQn, TCS_IRQ_PRIO);
    NVIC_EnableIRQ(TIM3_IRQn);

    s_inited = 1;
    return HAL_TCS_OK;
}

void TIM3_IRQHandler(void)
{
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR = ~TIM_SR_UIF;
        TIM2->CR1 &= ~TIM_CR1_CEN;

        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(s_done, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

hal_tcs_status_t hal_tcs3200_read_freq(tcs_filter_t filter,
                                       uint16_t window_ms,
                                       uint32_t *freq_hz)
{
    if (!s_inited)                         { return HAL_TCS_ERR_INIT; }
    if ((freq_hz == NULL) || (window_ms == 0U) || (window_ms > 1000U) ||
        ((uint32_t)filter > (uint32_t)TCS_FILTER_CLEAR)) { return HAL_TCS_ERR_PARAM; }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(TCS_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return HAL_TCS_ERR_BUSY;
    }

    hal_tcs_status_t st = HAL_TCS_OK;

    set_filter(filter);
    vTaskDelay(pdMS_TO_TICKS(TCS_SETTLE_MS));
    (void)xSemaphoreTake(s_done, 0);

    TIM2->CNT = 0;
    TIM2->SR  = 0;
    TIM3->CNT = 0;
    TIM3->ARR = (uint16_t)((uint32_t)window_ms * 10UL - 1UL);
    TIM3->SR  = 0;

    TIM2->CR1 |= TIM_CR1_CEN;
    TIM3->CR1 |= TIM_CR1_CEN;

    if (xSemaphoreTake(s_done, pdMS_TO_TICKS(window_ms + TCS_WAIT_MARGIN_MS)) != pdTRUE) {
        TIM3->CR1 &= ~TIM_CR1_CEN;
        TIM2->CR1 &= ~TIM_CR1_CEN;
        st = HAL_TCS_ERR_TIMEOUT;
    } else if (TIM2->SR & TIM_SR_UIF) {
        st = HAL_TCS_ERR_OVERFLOW;
    } else {
        *freq_hz = ((uint32_t)TIM2->CNT * 1000UL) / window_ms;
    }

    (void)xSemaphoreGive(s_mutex);
    return st;
}

hal_tcs_status_t hal_tcs3200_read_rgb(uint16_t window_ms, tcs_rgb_t *out)
{
    hal_tcs_status_t st;
    if (out == NULL) { return HAL_TCS_ERR_PARAM; }

    if ((st = hal_tcs3200_read_freq(TCS_FILTER_RED,   window_ms, &out->r)) != HAL_TCS_OK) { return st; }
    if ((st = hal_tcs3200_read_freq(TCS_FILTER_GREEN, window_ms, &out->g)) != HAL_TCS_OK) { return st; }
    return   hal_tcs3200_read_freq(TCS_FILTER_BLUE,  window_ms, &out->b);
}
