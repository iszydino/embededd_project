#ifndef COLOR_H
#define COLOR_H
#include <stdint.h>

/* ================= PROFILE MOI TRUONG (chon bang make ENV=...) ================= */
#if defined(ENV_BENCH)
  #define ENV_NAME        "BENCH"
  /* nen (dark) do bang che kin, tan so Hz */
  #define CAL_DARK_R      1060U
  #define CAL_DARK_G      1040U
  #define CAL_DARK_B      1300U
  /* giay trang cach 2 cm, tan so tho Hz */
  #define CAL_WHITE_R     6740U
  #define CAL_WHITE_G     7020U
  #define CAL_WHITE_B     8920U
  #define CAL_MIN_LEVEL   40U     /* duoi muc nay coi nhu khong co vat (thang 0..255) */
  #define CAL_DOMINANCE   13U     /* kenh troi >= 1.3 lan kenh khac (x10) */
  #define CAL_WHITE_RATIO 7U      /* min/max >= 0.7 thi coi la trang (x10) */

#elif defined(ENV_CONVEYOR)
  #define ENV_NAME        "CONVEYOR"
  /* TAM THOI = BENCH. DO LAI tren bang tai that roi thay vao */
  #define CAL_DARK_R      1520U
  #define CAL_DARK_G      1600U
  #define CAL_DARK_B      2040U
  #define CAL_WHITE_R     8080U
  #define CAL_WHITE_G     8440U
  #define CAL_WHITE_B     10600U
  #define CAL_MIN_LEVEL   40U
  #define CAL_DOMINANCE   13U
  #define CAL_WHITE_RATIO 7U

#else
  #error "Chua chon moi truong: dung make ENV=BENCH hoac make ENV=CONVEYOR"
#endif

typedef enum {
    CLR_NONE = 0, CLR_RED, CLR_GREEN, CLR_BLUE, CLR_WHITE, CLR_UNKNOWN
} color_id_t;

/* tru nen, chuan hoa theo giay trang ve thang 0..255 */
static inline uint32_t color_norm(uint32_t f, uint32_t dark, uint32_t white)
{
    uint32_t span = (white > dark) ? (white - dark) : 1U;
    uint32_t v = (f > dark) ? (f - dark) : 0U;
    v = (v * 255UL) / span;
    return (v > 255U) ? 255U : v;
}

static inline color_id_t color_classify(uint32_t fr, uint32_t fg, uint32_t fb,
                                        uint32_t *nr, uint32_t *ng, uint32_t *nb)
{
    uint32_t r = color_norm(fr, CAL_DARK_R, CAL_WHITE_R);
    uint32_t g = color_norm(fg, CAL_DARK_G, CAL_WHITE_G);
    uint32_t b = color_norm(fb, CAL_DARK_B, CAL_WHITE_B);
    uint32_t mx = r, mn = r;
    if (g > mx) { mx = g; }
    if (b > mx) { mx = b; }
    if (g < mn) { mn = g; }
    if (b < mn) { mn = b; }
    *nr = r; *ng = g; *nb = b;

    if (mx < CAL_MIN_LEVEL)                              { return CLR_NONE; }
    if ((mn * 10U) >= (mx * CAL_WHITE_RATIO))            { return CLR_WHITE; }
    if ((r * 10U >= g * CAL_DOMINANCE) && (r * 10U >= b * CAL_DOMINANCE)) { return CLR_RED; }
    if ((g * 10U >= r * CAL_DOMINANCE) && (g * 10U >= b * CAL_DOMINANCE)) { return CLR_GREEN; }
    if ((b * 10U >= r * CAL_DOMINANCE) && (b * 10U >= g * CAL_DOMINANCE)) { return CLR_BLUE; }
    return CLR_UNKNOWN;
}

static inline const char *color_name(color_id_t c)
{
    switch (c) {
    case CLR_NONE:  return "NONE";
    case CLR_RED:   return "RED";
    case CLR_GREEN: return "GREEN";
    case CLR_BLUE:  return "BLUE";
    case CLR_WHITE: return "WHITE";
    default:        return "UNKNOWN";
    }
}
#endif
