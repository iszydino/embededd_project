#ifndef TASK_COLOR_CFG_H
#define TASK_COLOR_CFG_H

/* Chon moi truong: make ENV=BENCH | BOX | CONVEYOR */
#if defined(ENV_BENCH)
  #define ENV_NAME      "BENCH"
  #define CAL_DARK_R    1700U
  #define CAL_DARK_G    1350U
  #define CAL_DARK_B    1420U
  #define CAL_WHITE_R   6500U
  #define CAL_WHITE_G   6900U
  #define CAL_WHITE_B   8800U

#elif defined(ENV_BOX)
  #define ENV_NAME      "BOX"
  /* TAM = BENCH. DO LAI trong hop kin roi thay vao */
  #define CAL_DARK_R    1060U
  #define CAL_DARK_G    1040U
  #define CAL_DARK_B    1300U
  #define CAL_WHITE_R   6740U
  #define CAL_WHITE_G   7020U
  #define CAL_WHITE_B   8920U

#elif defined(ENV_CONVEYOR)
  #define ENV_NAME      "CONVEYOR"
  /* TAM = BENCH. DO LAI tren bang tai that */
  #define CAL_DARK_R    1060U
  #define CAL_DARK_G    1040U
  #define CAL_DARK_B    1300U
  #define CAL_WHITE_R   6740U
  #define CAL_WHITE_G   7020U
  #define CAL_WHITE_B   8920U

#else
  #error "Chua chon moi truong: make ENV=BENCH | BOX | CONVEYOR"
#endif

/* Thong so chung */
#define COLOR_WINDOW_MS     50U    /* cua so dem moi kenh */
#define COLOR_SAMPLES       3U     /* so mau cho loc da so */
#define COLOR_MIN_LEVEL     40U    /* thang 0..255, duoi muc nay = khong co vat */
#define COLOR_DOMINANCE_X10 13U    /* kenh troi >= 1.3 lan kenh khac */
#define COLOR_WHITE_RATIO   7U     /* min/max >= 0.7 thi la trang (x10) */

/* 0 = do dinh ky, 1 = cho task_color_trigger() */
#define COLOR_TRIGGER_MODE  1
#define COLOR_PERIOD_MS     500U   /* chu ky o che do 0 */
#define COLOR_SETTLE_MS     100U   /* cho vat on dinh sau trigger (che do 1) */

#endif
