#ifndef APP_SORT_H
#define APP_SORT_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "task_color.h"

#define MAX_COLORS      3
#define SLOT_REJECT     0xFF
#define PRESET_COUNT    4

#define TASK_SERVO_OLED_STACK  512
#define TASK_SERVO_OLED_PRIO   (tskIDLE_PRIORITY + 2)

typedef enum {
    STATE_IDLE = 0,
    STATE_WEIGHING,
    STATE_COLOR,
    STATE_SORTING,
    STATE_DONE
} SysState_t;

typedef struct {
    uint8_t slot;
    float   weight_g;
} SortCmd_t;

typedef struct {
    SysState_t state;
    float      weight_g;
    uint8_t    slot;
} DisplayMsg_t;

typedef struct {
    const char *name;
    ColorId_t   id;
} ColorPreset_t;

typedef struct {
    uint8_t enabled;
    uint8_t preset;
    float   w_min, w_max;
} ColorSlot_t;

extern const ColorPreset_t g_presets[PRESET_COUNT];
extern ColorSlot_t         g_slot[MAX_COLORS];
extern volatile uint16_t   g_sortCount[MAX_COLORS + 1];

extern QueueHandle_t       xSortQueue;
extern QueueHandle_t       xDisplayQueue;
extern SemaphoreHandle_t   xCfgMutex;

void       App_SharedInit(void);
uint8_t    Sort_Decide(ColorId_t color, float weight_g);
void       Display_Post(SysState_t st, float weight_g, uint8_t slot);
void       ServoOledTask(void *arg);
BaseType_t task_servo_oled_create(void);

#endif
