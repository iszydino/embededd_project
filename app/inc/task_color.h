#ifndef TASK_COLOR_H
#define TASK_COLOR_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

typedef enum {
    COLOR_NONE = 0,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_WHITE,
    COLOR_UNKNOWN
} ColorId_t;

typedef struct {
    ColorId_t id;
    uint32_t  r, g, b;      /* tan so tho, Hz (mau cuoi) */
    uint8_t   nr, ng, nb;   /* sau tru nen, thang 0..255 */
} ColorData_t;

BaseType_t task_color_create(void);

/* Goi tu task khac de yeu cau do (che do COLOR_TRIGGER_MODE = 1) */
void task_color_trigger(void);

/* Goi tu ISR (nut bam / IR) */
void task_color_trigger_fromisr(BaseType_t *pxWoken);

#endif
