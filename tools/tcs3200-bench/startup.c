#include <stdint.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

__attribute__((section(".isr_vector"), used))
void (*const g_vectors[16])(void) = {
    (void (*)(void))&_estack,  /* 0: initial SP */
    Reset_Handler,             /* 1: Reset */
    Default_Handler,           /* 2: NMI */
    Default_Handler,           /* 3: HardFault */
    Default_Handler,           /* 4: MemManage */
    Default_Handler,           /* 5: BusFault */
    Default_Handler,           /* 6: UsageFault */
    0, 0, 0, 0,                /* 7-10: reserved */
    Default_Handler,           /* 11: SVC */
    Default_Handler,           /* 12: DebugMon */
    0,                         /* 13: reserved */
    Default_Handler,           /* 14: PendSV */
    Default_Handler            /* 15: SysTick */
};

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) { *dst++ = *src++; }
    for (dst = &_sbss; dst < &_ebss; dst++) { *dst = 0; }
    (void)main();
    for (;;) {}
}

void Default_Handler(void)
{
    for (;;) {}
}
