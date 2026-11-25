#include "regs.h"
#include "color.h"

#define WINDOW_MS   50U
#define SETTLE_MS   5U

static uint32_t g_sysclk = 8000000UL;       /* se thanh 72 MHz neu HSE/PLL ok */

/* Bien de xem bang gdb */
volatile uint32_t g_r, g_g, g_b, g_c;
volatile uint32_t g_status;
volatile uint32_t g_class, g_rn, g_gn, g_bn;                 /* 0 = ok, 1 = overflow */

/* ---------------- Clock 72 MHz (HSE 8 MHz x 9) ---------------- */
static void clock_init(void)
{
    uint32_t t = 0;

    FLASH->ACR = 0x12U;                     /* prefetch on, 2 wait states */
    RCC->CR |= (1UL << 16);                 /* HSEON */
    while (!(RCC->CR & (1UL << 17))) {      /* HSERDY */
        if (++t > 200000UL) { return; }     /* khong co thach anh: giu HSI 8 MHz */
    }

    RCC->CFGR = (1UL << 16) | (7UL << 18) | (4UL << 8);  /* PLLSRC=HSE, x9, APB1 /2 */
    RCC->CR |= (1UL << 24);                 /* PLLON */
    t = 0;
    while (!(RCC->CR & (1UL << 25))) {      /* PLLRDY */
        if (++t > 200000UL) { RCC->CR &= ~(1UL << 24); return; }
    }
    RCC->CFGR |= 2UL;                       /* SW = PLL */
    while ((RCC->CFGR & 0xCUL) != 0x8UL) {}
    g_sysclk = 72000000UL;
}

/* ---------------- UART1 PA9 TX, 115200 8N1 ---------------- */
static void uart_init(void)
{
    RCC->APB2ENR |= (1UL << 14);            /* USART1EN */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFUL << 4)) | (0xBUL << 4);  /* PA9: AF push-pull 50MHz */
    USART1->BRR = g_sysclk / 115200UL;
    USART1->CR1 = (1UL << 13) | (1UL << 3); /* UE | TE */
}

static void uart_putc(char c)
{
    while (!(USART1->SR & 0x80UL)) {}       /* TXE */
    USART1->DR = (uint32_t)c;
}

static void uart_puts(const char *s)
{
    while (*s) { uart_putc(*s++); }
}

static void uart_putu(uint32_t v)
{
    char b[11];
    int i = 0;
    if (v == 0U) { uart_putc('0'); return; }
    while (v) { b[i++] = (char)('0' + (v % 10U)); v /= 10U; }
    while (i) { uart_putc(b[--i]); }
}

/* ---------------- Dieu khien chan S0..S3 (PB12..PB15) ---------------- */
static void pb_write(uint32_t pin, uint32_t level)
{
    GPIOB->BSRR = level ? (1UL << pin) : (1UL << (pin + 16U));
}

/* 0=R, 1=G, 2=B, 3=Clear */
static void set_filter(uint32_t f)
{
    switch (f) {
    case 0: pb_write(14, 0); pb_write(15, 0); break;   /* Red   */
    case 1: pb_write(14, 1); pb_write(15, 1); break;   /* Green */
    case 2: pb_write(14, 0); pb_write(15, 1); break;   /* Blue  */
    default: pb_write(14, 1); pb_write(15, 0); break;  /* Clear */
    }
}

/* ---------------- TIM3: cua so thoi gian, tick = 0.1 ms ---------------- */
static void gate_start(uint32_t ticks)
{
    TIM3->CR1 = (1UL << 3);                 /* OPM */
    TIM3->PSC = (g_sysclk / 10000UL) - 1UL;
    TIM3->ARR = ticks - 1UL;
    TIM3->EGR = 1UL;                        /* UG: nap PSC/ARR */
    TIM3->SR  = 0;
    TIM3->CNT = 0;
    TIM3->CR1 = (1UL << 3) | 1UL;           /* OPM | CEN */
}

static void gate_wait(void)
{
    while (!(TIM3->SR & 1UL)) {}            /* UIF */
}

static void delay_ms(uint32_t ms)
{
    gate_start(ms * 10UL);
    gate_wait();
}

/* ---------------- Do tan so (Hz) 1 kenh ---------------- */
static uint32_t measure_hz(uint32_t filter)
{
    uint32_t cnt;

    set_filter(filter);
    delay_ms(SETTLE_MS);

    TIM2->CR1 = 0;
    TIM2->CNT = 0;
    TIM2->SR  = 0;

    TIM2->CR1 = 1UL;                        /* CEN: bat dau dem */
    gate_start(WINDOW_MS * 10UL);
    gate_wait();
    TIM2->CR1 = 0;                          /* dung dem */

    g_status = (TIM2->SR & 1UL) ? 1U : 0U;  /* UIF cua TIM2 = tran 16 bit */
    cnt = TIM2->CNT;
    return (cnt * 1000UL) / WINDOW_MS;
}

int main(void)
{
    clock_init();

    /* Clock ngoai vi: AFIO, GPIOA, GPIOB, GPIOC */
    RCC->APB2ENR |= (1UL << 0) | (1UL << 2) | (1UL << 3) | (1UL << 4);
    RCC->APB1ENR |= (1UL << 0) | (1UL << 1);   /* TIM2, TIM3 */
    (void)RCC->APB1ENR;

    /* PA0: input floating (OUT cua TCS3200 = TIM2_ETR) */
    GPIOA->CRL = (GPIOA->CRL & ~0xFUL) | 0x4UL;

    /* PB12..PB15: output push-pull 2 MHz (S0..S3) */
    GPIOB->CRH = (GPIOB->CRH & 0x0000FFFFUL) | 0x22220000UL;

    /* PC13: LED onboard */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFUL << 20)) | (0x2UL << 20);

    uart_init();

    /* Scale 20%: S0=1, S1=0 */
    pb_write(12, 1);
    pb_write(13, 0);

    /* TIM2: external clock mode 2 (ETR = PA0), rising edge, khong chia, khong loc */
    TIM2->CR1  = 0;
    TIM2->PSC  = 0;
    TIM2->ARR  = 0xFFFFUL;
    TIM2->SMCR = (1UL << 14);               /* ECE */
    TIM2->EGR  = 1UL;
    TIM2->SR   = 0;

    uart_puts("\r\nTCS3200 test, sysclk = ");
    uart_putu(g_sysclk);
    uart_puts(" Hz\r\n");

    for (;;) {
        g_r = measure_hz(0);
        g_g = measure_hz(1);
        g_b = measure_hz(2);
        g_c = measure_hz(3);
        g_class = (uint32_t)color_classify(g_r, g_g, g_b, (uint32_t *)&g_rn, (uint32_t *)&g_gn, (uint32_t *)&g_bn);

        uart_puts("R=");  uart_putu(g_r);
        uart_puts(" G=");  uart_putu(g_g);
        uart_puts(" B=");  uart_putu(g_b);
        uart_puts(" C=");  uart_putu(g_c);
        uart_puts(" -> "); uart_puts(color_name((color_id_t)g_class));
        uart_puts(g_status ? " [OVERFLOW]\r\n" : "\r\n");

        GPIOC->ODR ^= (1UL << 13);          /* nhay LED */
        delay_ms(200);
    }
}
