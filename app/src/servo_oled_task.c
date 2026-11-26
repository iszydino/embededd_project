/*
 * SERVO + OLED TASK (1 task) - STM32F103C8T6 + FreeRTOS
 * Servo : TIM4 CH3 (PB8), PSC = 71, ARR = 19999 (1 tick = 1 us, 50 Hz), khoi tao trong bsp_hal.c
 * OLED  : SSD1306 128x64, I2C1 (PB6 SCL, PB7 SDA)
 * Nut   : PA2 NEXT, PA3 UP, PA4 EN (pull-up, nhan = 0)
 * Mau   : nhan ColorId_t tu task_color, khong con dung HSV
 * Yeu cau: configTICK_RATE_HZ = 1000
 */
#include "app_sort.h"
#include "app_ipc.h"
#include "bsp_pins.h"
#include "bsp_hal.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>

const ColorPreset_t g_presets[PRESET_COUNT] = {
    { "RED",   COLOR_RED   },
    { "GREEN", COLOR_GREEN },
    { "BLUE",  COLOR_BLUE  },
    { "WHITE", COLOR_WHITE },
};

/* Mac dinh: K1 = RED, K2 = GREEN, K3 = BLUE */
ColorSlot_t g_slot[MAX_COLORS] = {
    { 1, 0, 0.0f, 0.0f },
    { 1, 1, 0.0f, 0.0f },
    { 1, 2, 0.0f, 0.0f },
};

volatile uint16_t g_sortCount[MAX_COLORS + 1] = { 0 };

QueueHandle_t     xSortQueue    = NULL;
QueueHandle_t     xDisplayQueue = NULL;
SemaphoreHandle_t xCfgMutex     = NULL;

void App_SharedInit(void)
{
    xSortQueue    = xQueueCreate(4, sizeof(SortCmd_t));
    xDisplayQueue = xQueueCreate(1, sizeof(DisplayMsg_t));
    xCfgMutex     = xSemaphoreCreateMutex();
}

void Display_Post(SysState_t st, float weight_g, uint8_t slot)
{
    DisplayMsg_t m = { st, weight_g, slot, COLOR_NONE, 0U };
    if (xDisplayQueue) xQueueOverwrite(xDisplayQueue, &m);
}

void Display_PostWeigh(ColorId_t color, float weight_g, uint8_t final)
{
    DisplayMsg_t m = { STATE_WEIGHING, weight_g, SLOT_REJECT, color, final };
    if (xDisplayQueue) xQueueOverwrite(xDisplayQueue, &m);
}

/* Task dieu khien goi ham nay de biet san pham vao khay nao */
uint8_t Sort_Decide(ColorId_t color, float weight_g)
{
    uint8_t result = SLOT_REJECT;
    if (color < COLOR_RED || color > COLOR_WHITE) { return result; }

    xSemaphoreTake(xCfgMutex, portMAX_DELAY);
    for (uint8_t i = 0; i < MAX_COLORS; i++) {
        if (!g_slot[i].enabled) continue;
        if (g_presets[g_slot[i].preset].id != color) continue;
        if (g_slot[i].w_max > 0.0f &&
            (weight_g < g_slot[i].w_min || weight_g > g_slot[i].w_max)) continue;
        result = i;
        break;
    }
    xSemaphoreGive(xCfgMutex);
    return result;
}

/* ====================================================================
 *  2. SERVO (may trang thai, khong chan)
 * ==================================================================== */
#define SERVO_TIM        htim4
#define SERVO_CH         TIM_CHANNEL_3
#define SERVO_MIN_US     500
#define SERVO_MAX_US     2500

#define ANGLE_HOME       90
#define ANGLE_REJECT     150
static const uint8_t k_slotAngle[MAX_COLORS] = { 30, 60, 120 };   /* K1, K2, K3 */

#define SERVO_STEP_DEG   2          /* moi buoc di chuyen (do)       */
#define SERVO_STEP_MS    10         /* chu ky moi buoc               */
#define SERVO_HOLD_MS    800        /* giu cua gat cho SP truot xuong */

typedef enum { SV_IDLE, SV_MOVE_OUT, SV_HOLD, SV_MOVE_BACK } ServoPhase_t;

static int      s_cur = ANGLE_HOME;
static int      s_target = ANGLE_HOME;
static uint32_t s_lastStep = 0;

static void Servo_Write(int angle)
{
    uint32_t us = SERVO_MIN_US +
                  (uint32_t)angle * (SERVO_MAX_US - SERVO_MIN_US) / 180U;
    __HAL_TIM_SET_COMPARE(&SERVO_TIM, SERVO_CH, us);
}

/* Di chuyen theo thoi gian thuc (khong bi giat khi OLED dang ve).
 * Tra ve 1 khi da toi goc dich. */
static uint8_t Servo_Step(uint32_t now)
{
    if (s_cur == s_target) return 1;

    uint32_t n = (now - s_lastStep) / SERVO_STEP_MS;
    if (n == 0) return 0;
    s_lastStep += n * SERVO_STEP_MS;

    int d = (int)n * SERVO_STEP_DEG;
    if (s_cur < s_target) { s_cur += d; if (s_cur > s_target) s_cur = s_target; }
    else                  { s_cur -= d; if (s_cur < s_target) s_cur = s_target; }

    Servo_Write(s_cur);
    return (s_cur == s_target);
}

/* ====================================================================
 *  3. NUT BAM (chong doi: 2 lan doc lien tiep giong nhau)
 * ==================================================================== */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       prev, stable;
} Btn_t;

static uint8_t Btn_Event(Btn_t *b)          /* 1 = vua duoc nhan */
{
    uint8_t raw = (HAL_GPIO_ReadPin(b->port, b->pin) == GPIO_PIN_RESET);
    uint8_t ev = 0;
    if (raw == b->prev && raw != b->stable) { b->stable = raw; ev = raw; }
    b->prev = raw;
    return ev;
}

/* ====================================================================
 *  4. OLED
 * ==================================================================== */
static const char *StateText(SysState_t s)
{
    switch (s) {
    case STATE_IDLE:     return "CHO SAN PHAM";
    case STATE_WEIGHING: return "DANG CAN...";
    case STATE_COLOR:    return "NHAN DIEN MAU";
    case STATE_SORTING:  return "DANG PHAN LOAI";
    case STATE_DONE:     return "HOAN THANH";
    default:             return "";
    }
}

static uint8_t PresetUsedByOther(uint8_t slot, uint8_t preset)
{
    for (uint8_t i = 0; i < MAX_COLORS; i++)
        if (i != slot && g_slot[i].enabled && g_slot[i].preset == preset) return 1;
    return 0;
}

static uint8_t NextFreePreset(uint8_t slot)     /* mau ke tiep chua bi khay khac dung */
{
    uint8_t p = g_slot[slot].preset;
    for (uint8_t n = 0; n < PRESET_COUNT; n++) {
        p = (uint8_t)((p + 1) % PRESET_COUNT);
        if (!PresetUsedByOther(slot, p)) return p;
    }
    return g_slot[slot].preset;
}

static const char *ColorName(ColorId_t id)
{
    for (uint8_t i = 0; i < PRESET_COUNT; i++) {
        if (g_presets[i].id == id) { return g_presets[i].name; }
    }
    return "KHAC";
}

/* Man hinh rieng cua buoc can: chi co mau va khoi luong */
static void DrawWeigh(const DisplayMsg_t *m)
{
    char buf[40];
    ssd1306_Fill(Black);

    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString((char *)(m->final ? "KET QUA CAN" : "DANG CAN..."), Font_7x10, White);

    snprintf(buf, sizeof(buf), "Mau: %s", ColorName(m->color));
    ssd1306_SetCursor(0, 18);
    ssd1306_WriteString(buf, Font_7x10, White);

    int32_t w10 = (int32_t)(m->weight_g * 10.0f + (m->weight_g >= 0 ? 0.5f : -0.5f));
    int32_t a   = (w10 < 0) ? -w10 : w10;
    snprintf(buf, sizeof(buf), "%s%d.%d g", (w10 < 0) ? "-" : "",
             (int)(a / 10), (int)(a % 10));
    ssd1306_SetCursor(0, 38);
    ssd1306_WriteString(buf, Font_11x18, White);
    ssd1306_UpdateScreen();
}

static void DrawRun(const DisplayMsg_t *m)
{
    char buf[40];
    ssd1306_Fill(Black);

    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString((char *)StateText(m->state), Font_7x10, White);

    int32_t w10 = (int32_t)(m->weight_g * 10.0f + (m->weight_g >= 0 ? 0.5f : -0.5f));
    int32_t a   = (w10 < 0) ? -w10 : w10;
    snprintf(buf, sizeof(buf), "KL: %s%d.%d g", (w10 < 0) ? "-" : "",
             (int)(a / 10), (int)(a % 10));
    ssd1306_SetCursor(0, 14);
    ssd1306_WriteString(buf, Font_7x10, White);

    const char *cname = "---";
    char kbuf[8] = "";
    if (m->state != STATE_IDLE) {
        if (m->slot < MAX_COLORS) {
            xSemaphoreTake(xCfgMutex, portMAX_DELAY);
            cname = g_presets[g_slot[m->slot].preset].name;
            xSemaphoreGive(xCfgMutex);
            snprintf(kbuf, sizeof(kbuf), " K%d", m->slot + 1);
        } else {
            cname = "KHAC";
            snprintf(kbuf, sizeof(kbuf), " LOAI");
        }
    }
    snprintf(buf, sizeof(buf), "Mau: %s%s", cname, kbuf);
    ssd1306_SetCursor(0, 28);
    ssd1306_WriteString(buf, Font_7x10, White);

    snprintf(buf, sizeof(buf), "1:%02u 2:%02u 3:%02u X:%02u",
             (unsigned)g_sortCount[0], (unsigned)g_sortCount[1],
             (unsigned)g_sortCount[2], (unsigned)g_sortCount[3]);
    ssd1306_SetCursor(0, 44);
    ssd1306_WriteString(buf, Font_6x8, White);

    ssd1306_SetCursor(0, 56);
    ssd1306_WriteString("NEXT: cai dat mau", Font_6x8, White);
    ssd1306_UpdateScreen();
}

static void DrawSettings(uint8_t sel)
{
    char buf[40];
    ssd1306_Fill(Black);

    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("CAI DAT MAU", Font_7x10, White);

    xSemaphoreTake(xCfgMutex, portMAX_DELAY);
    for (uint8_t i = 0; i < MAX_COLORS; i++) {
        snprintf(buf, sizeof(buf), "%c K%d [%s] %s",
                 (i == sel) ? '>' : ' ', i + 1,
                 g_slot[i].enabled ? "ON " : "OFF",
                 g_slot[i].enabled ? g_presets[g_slot[i].preset].name : "---");
        ssd1306_SetCursor(0, 14 + i * 12);
        ssd1306_WriteString(buf, Font_7x10, White);
    }
    xSemaphoreGive(xCfgMutex);

    ssd1306_SetCursor(0, 56);
    ssd1306_WriteString("N:slot U:mau E:bat", Font_6x8, White);
    ssd1306_UpdateScreen();
}

/* ====================================================================
 *  5. TASK DUY NHAT: SERVO + OLED + NUT BAM
 * ==================================================================== */
#define LOOP_MS   10

void ServoOledTask(void *arg)
{
    (void)arg;

    Btn_t bNext = { BTN_NEXT_GPIO_Port, BTN_NEXT_Pin, 0, 0 };
    Btn_t bUp   = { BTN_UP_GPIO_Port,   BTN_UP_Pin,   0, 0 };
    Btn_t bEn   = { BTN_EN_GPIO_Port,   BTN_EN_Pin,   0, 0 };

    DisplayMsg_t msg = { STATE_IDLE, 0.0f, SLOT_REJECT, COLOR_NONE, 0U };
    SortCmd_t    cmd = { SLOT_REJECT, 0.0f };
    ServoPhase_t phase = SV_IDLE;
    uint32_t     holdStart = 0;
    uint8_t      inSettings = 0, sel = 0, dirty = 1;

    HAL_TIM_PWM_Start(&SERVO_TIM, SERVO_CH);
    Servo_Write(ANGLE_HOME);
    ssd1306_Init();

    for (;;) {
        uint32_t now = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* --- (a) Nhan trang thai tu Sensor task --- */
        DisplayMsg_t in;
        if (xQueueReceive(xDisplayQueue, &in, 0) == pdPASS) {
            msg = in;
            dirty = 1;
        }

        /* --- (b) May trang thai servo --- */
        switch (phase) {
        case SV_IDLE:
            if (xQueueReceive(xSortQueue, &cmd, 0) == pdPASS) {
                s_target = (cmd.slot < MAX_COLORS) ? k_slotAngle[cmd.slot]
                                                   : ANGLE_REJECT;
                s_lastStep = now;
                msg.state = STATE_SORTING;
                msg.weight_g = cmd.weight_g;
                msg.slot = cmd.slot;
                dirty = 1;
                phase = SV_MOVE_OUT;
            }
            break;

        case SV_MOVE_OUT:
            if (Servo_Step(now)) { holdStart = now; phase = SV_HOLD; }
            break;

        case SV_HOLD:
            if (now - holdStart >= SERVO_HOLD_MS) {
                s_target = ANGLE_HOME;
                s_lastStep = now;
                phase = SV_MOVE_BACK;
            }
            break;

        case SV_MOVE_BACK:
            if (Servo_Step(now)) {
                g_sortCount[(cmd.slot < MAX_COLORS) ? cmd.slot : MAX_COLORS]++;
                xEventGroupSetBits(xEvt, EVT_SORT_DONE);
                msg.state = STATE_DONE;
                dirty = 1;
                phase = SV_IDLE;
            }
            break;
        }

        /* --- (c) Nut bam / menu cai dat 3 mau --- */
        if (Btn_Event(&bNext)) {
            if (!inSettings)                { inSettings = 1; sel = 0; }
            else if (++sel >= MAX_COLORS)   { inSettings = 0; sel = 0; }
            dirty = 1;
        }
        if (inSettings) {
            if (Btn_Event(&bUp)) {
                xSemaphoreTake(xCfgMutex, portMAX_DELAY);
                g_slot[sel].preset = NextFreePreset(sel);
                xSemaphoreGive(xCfgMutex);
                dirty = 1;
            }
            if (Btn_Event(&bEn)) {
                xSemaphoreTake(xCfgMutex, portMAX_DELAY);
                g_slot[sel].enabled ^= 1;
                if (g_slot[sel].enabled && PresetUsedByOther(sel, g_slot[sel].preset))
                    g_slot[sel].preset = NextFreePreset(sel);   /* tranh trung mau */
                xSemaphoreGive(xCfgMutex);
                dirty = 1;
            }
        } else {
            (void)Btn_Event(&bUp);      /* bo su kien thua khi o man hinh chay */
            (void)Btn_Event(&bEn);
        }

        /* --- (d) Ve OLED chi khi co thay doi (tranh lam giat servo) --- */
        if (dirty) {
            if (inSettings) DrawSettings(sel);
            else if (msg.state == STATE_WEIGHING) DrawWeigh(&msg);
            else            DrawRun(&msg);
            dirty = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(LOOP_MS));
    }
}


BaseType_t task_servo_oled_create(void)
{
    return xTaskCreate(ServoOledTask, "SERVO_OLED", TASK_SERVO_OLED_STACK,
                       NULL, TASK_SERVO_OLED_PRIO, NULL);
}
