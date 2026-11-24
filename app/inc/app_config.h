#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define MAX_COLORS      3          /* Toi da 3 mau phan loai */
#define SLOT_REJECT     0xFF       /* Khong khop -> khay loai */
#define PRESET_COUNT    8

typedef enum {
    STATE_IDLE = 0,     /* Cho san pham       */
    STATE_WEIGHING,     /* Dang can           */
    STATE_COLOR,        /* Dang nhan dien mau */
    STATE_SORTING,      /* Dang phan loai     */
    STATE_DONE          /* Hoan thanh         */
} SysState_t;

/* Sensor task -> ServoOled task */
typedef struct {
    uint8_t slot;       /* 0..2 hoac SLOT_REJECT */
    float   weight_g;
} SortCmd_t;

/* Sensor task -> ServoOled task (cap nhat man hinh) */
typedef struct {
    SysState_t state;
    float      weight_g;
    uint8_t    slot;
} DisplayMsg_t;

typedef struct {
    const char *name;
    int16_t h_min, h_max;       /* h_min > h_max: dai qua 0 do */
    uint8_t s_min, s_max;
    uint8_t v_min, v_max;
} ColorPreset_t;

typedef struct {
    uint8_t enabled;
    uint8_t preset;
    float   w_min, w_max;       /* w_max = 0: bo qua dieu kien khoi luong */
} ColorSlot_t;

extern const ColorPreset_t g_presets[PRESET_COUNT];
extern ColorSlot_t         g_slot[MAX_COLORS];
extern volatile uint16_t   g_sortCount[MAX_COLORS + 1];   /* [3] = bi loai */

extern QueueHandle_t       xSortQueue;      /* SortCmd_t    */
extern QueueHandle_t       xDisplayQueue;   /* DisplayMsg_t */
extern SemaphoreHandle_t   xCfgMutex;

void    App_SharedInit(void);               /* goi truoc khi tao task */
uint8_t Sort_Decide(uint8_t r, uint8_t g, uint8_t b, float weight_g);
void    Display_Post(SysState_t st, float weight_g, uint8_t slot);
void    ServoOledTask(void *arg);

#endif
