
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "task_color.h"
#include "task_color_cfg.h"
#include "app_ipc.h"
#include "hal_tcs3200.h"

// Cấu hình dung lượng Stack cho task COLOR
#ifndef TASK_COLOR_STACK_WORDS
#define TASK_COLOR_STACK_WORDS  (configMINIMAL_STACK_SIZE + 128)
#endif

// Cấu hình độ ưu tiên của task COLOR
#ifndef TASK_COLOR_PRIO
#define TASK_COLOR_PRIO         (tskIDLE_PRIORITY + 2)
#endif

// Lưu handle để các task khác có thể kích hoạt COLOR
static TaskHandle_t xColorTaskHandle = NULL;


// Chuẩn hóa giá trị màu về khoảng 0 đến 255
static uint32_t prvNorm(uint32_t f, uint32_t dark, uint32_t white)
{
    // Tính khoảng giá trị giữa mức tối và mức trắng
    uint32_t span = (white > dark) ? (white - dark) : 1U;

    // Trừ giá trị nền, nếu nhỏ hơn nền thì lấy bằng 0
    uint32_t v = (f > dark) ? (f - dark) : 0U;

    // Đưa giá trị về thang 0 đến 255
    v = (v * 255UL) / span;

    // Giới hạn giá trị tối đa là 255
    return (v > 255U) ? 255U : v;
}


// Phân loại màu dựa trên ba giá trị R, G, B
static ColorId_t prvClassify(uint32_t r, uint32_t g, uint32_t b)
{
    // Tìm giá trị RGB lớn nhất và nhỏ nhất
    uint32_t mx = r, mn = r;

    if (g > mx) { mx = g; }
    if (b > mx) { mx = b; }

    if (g < mn) { mn = g; }
    if (b < mn) { mn = b; }

    // Nếu cả ba thành phần đều thấp thì coi như không có vật
    if (mx < COLOR_MIN_LEVEL) {
        return COLOR_NONE;
    }

    // Nếu ba thành phần gần bằng nhau thì nhận diện màu trắng
    if ((mn * 10U) >= (mx * COLOR_WHITE_RATIO)) {
        return COLOR_WHITE;
    }

    // Nếu màu đỏ lớn hơn đáng kể hai màu còn lại
    if ((r * 10U >= g * COLOR_DOMINANCE_X10) &&
        (r * 10U >= b * COLOR_DOMINANCE_X10)) {
        return COLOR_RED;
    }

    // Nếu màu xanh lá lớn hơn đáng kể hai màu còn lại
    if ((g * 10U >= r * COLOR_DOMINANCE_X10) &&
        (g * 10U >= b * COLOR_DOMINANCE_X10)) {
        return COLOR_GREEN;
    }

    // Nếu màu xanh dương lớn hơn đáng kể hai màu còn lại
    if ((b * 10U >= r * COLOR_DOMINANCE_X10) &&
        (b * 10U >= g * COLOR_DOMINANCE_X10)) {
        return COLOR_BLUE;
    }

    // Không xác định được màu
    return COLOR_UNKNOWN;
}


// Lọc đa số trong 3 lần đo
// Nếu có ít nhất 2 mẫu giống nhau thì lấy màu đó
static ColorId_t prvMajority(const ColorId_t s[COLOR_SAMPLES])
{
    if ((s[0] == s[1]) || (s[0] == s[2])) {
        return s[0];
    }

    if (s[1] == s[2]) {
        return s[1];
    }

    // Nếu cả ba mẫu khác nhau thì trả về UNKNOWN
    return COLOR_UNKNOWN;
}


// Đo một mẫu màu từ cảm biến TCS3200
static BaseType_t prvSample(ColorId_t *id, ColorData_t *d)
{
    tcs_rgb_t f;

    // Đọc ba thành phần RGB từ cảm biến
    if (hal_tcs3200_read_rgb(COLOR_WINDOW_MS, &f) != HAL_TCS_OK) {
        return pdFALSE;
    }

    // Lưu giá trị RGB thô
    d->r = f.r;
    d->g = f.g;
    d->b = f.b;

    // Chuẩn hóa từng thành phần RGB về 0 đến 255
    uint32_t nr = prvNorm(f.r, CAL_DARK_R, CAL_WHITE_R);
    uint32_t ng = prvNorm(f.g, CAL_DARK_G, CAL_WHITE_G);
    uint32_t nb = prvNorm(f.b, CAL_DARK_B, CAL_WHITE_B);

    // Lưu giá trị RGB đã chuẩn hóa
    d->nr = (uint8_t)nr;
    d->ng = (uint8_t)ng;
    d->nb = (uint8_t)nb;

    // Phân loại màu của mẫu vừa đo
    *id = prvClassify(nr, ng, nb);

    return pdTRUE;
}


// Hàm thực thi chính của task COLOR
static void prvColorTask(void *pvParameters)
{
    (void)pvParameters;

    // Lưu thời điểm bắt đầu để sử dụng khi chạy theo chu kỳ
    TickType_t xLastWakeTime = xTaskGetTickCount();

    // Task hoạt động liên tục
    for (;;) {

#if COLOR_TRIGGER_MODE

        // Chờ CONTROL gửi tín hiệu yêu cầu đo màu
        // Nếu chờ quá 1000 ms mà không có tín hiệu thì quay lại đầu vòng lặp
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0U) {
            continue;
        }

        // Chờ cảm biến ổn định trước khi đo
        vTaskDelay(pdMS_TO_TICKS(COLOR_SETTLE_MS));

#endif

        // Mảng lưu kết quả phân loại của 3 lần đo
        ColorId_t s[COLOR_SAMPLES];

        // Biến lưu dữ liệu RGB
        ColorData_t d;

        // Ban đầu giả sử việc đọc cảm biến thành công
        BaseType_t ok = pdTRUE;

        // Thực hiện đo màu 3 lần
        for (uint32_t i = 0;
             (i < COLOR_SAMPLES) && (ok == pdTRUE);
             i++) {

            // Đọc một mẫu RGB và lưu kết quả phân loại
            ok = prvSample(&s[i], &d);
        }

        // Chỉ xử lý nếu cả 3 lần đo đều thành công
        if (ok == pdTRUE) {

            // Lấy kết quả màu xuất hiện nhiều nhất trong 3 mẫu
            d.id = prvMajority(s);

            // Kiểm tra Queue đã được tạo chưa
            if (xColorQueue != NULL) {

                // Gửi kết quả màu sang CONTROL
                // Thời gian chờ bằng 0 nên không chờ nếu Queue đầy
                (void)xQueueSend(xColorQueue, &d, 0);
            }
        }

#if !COLOR_TRIGGER_MODE

        // Nếu không dùng chế độ Trigger thì đo màu theo chu kỳ
        vTaskDelayUntil(
            &xLastWakeTime,
            pdMS_TO_TICKS(COLOR_PERIOD_MS)
        );

#else

        // Không cần dùng biến thời gian trong chế độ Trigger
        (void)xLastWakeTime;

#endif
    }
}


// Tạo task COLOR trong FreeRTOS
BaseType_t task_color_create(void)
{
    return xTaskCreate(
        prvColorTask,             // Hàm thực thi của task
        "COLOR_TASK",             // Tên task
        TASK_COLOR_STACK_WORDS,   // Dung lượng Stack
        NULL,                     // Tham số truyền vào task
        TASK_COLOR_PRIO,          // Độ ưu tiên
        &xColorTaskHandle         // Lưu handle của task
    );
}


// Hàm để CONTROL kích hoạt task COLOR
void task_color_trigger(void)
{
    if (xColorTaskHandle != NULL) {
        // Gửi thông báo để đánh thức task COLOR
        (void)xTaskNotifyGive(xColorTaskHandle);
    }
}


// Kích hoạt task COLOR từ chương trình ngắt ISR
void task_color_trigger_fromisr(BaseType_t *pxWoken)
{
    if (xColorTaskHandle != NULL) {
        // Gửi thông báo từ ngắt để đánh thức task COLOR
        vTaskNotifyGiveFromISR(xColorTaskHandle, pxWoken);
    }
}
