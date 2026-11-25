#ifndef REGS_H
#define REGS_H
#include <stdint.h>

#define __IO volatile

typedef struct { __IO uint32_t CR, CFGR, CIR, APB2RSTR, APB1RSTR, AHBENR,
                               APB2ENR, APB1ENR, BDCR, CSR; } RCC_t;
typedef struct { __IO uint32_t ACR; } FLASH_t;
typedef struct { __IO uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR; } GPIO_t;
typedef struct { __IO uint32_t CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2,
                               CCER, CNT, PSC, ARR, RSVD0, CCR1, CCR2, CCR3,
                               CCR4, RSVD1, DCR, DMAR; } TIM_t;
typedef struct { __IO uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR; } USART_t;

#define RCC     ((RCC_t   *)0x40021000UL)
#define FLASH   ((FLASH_t *)0x40022000UL)
#define GPIOA   ((GPIO_t  *)0x40010800UL)
#define GPIOB   ((GPIO_t  *)0x40010C00UL)
#define GPIOC   ((GPIO_t  *)0x40011000UL)
#define TIM2    ((TIM_t   *)0x40000000UL)
#define TIM3    ((TIM_t   *)0x40000400UL)
#define USART1  ((USART_t *)0x40013800UL)

#endif
