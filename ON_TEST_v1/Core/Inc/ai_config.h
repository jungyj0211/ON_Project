#ifndef INC_AI_CONFIG_H_
#define INC_AI_CONFIG_H_

#include <stdint.h>

#define AE_IN_DIM   (4)
#define AE_OUT_DIM  (4)
#define AE_COLS     (4)

#define AE_IN_SCALE       (0.003766168f)
#define AE_IN_ZERO_POINT  (-128)
#define AE_OUT_SCALE      (0.003898344f)
#define AE_OUT_ZERO_POINT (-128)

/* X-CUBE-AI Analyze/link result for the currently generated network.
 * Update these two values when the generated model or runtime changes. */
#define AI_RUNTIME_DATA_BYTES    (20412U)
#define AI_RUNTIME_BSS_BYTES     (792U)
#define AI_RAM_LIMIT_BYTES       (25000000U) /* 25 MB, decimal */

/* Live normal-data threshold measurement (10,000 samples):
 * P95=0.001028813, P99=0.002259826, P99.9=0.008074203, MAX=0.234501213 */
#define AE_THRESH  (0.011325965f) // P99.9 MSE threshold for normal/anomaly
#define AE_HI_LOW  (0.000452750f) // MSE mapped to HI 0.0
#define AE_HI_HIGH (0.000475430f) // MSE mapped to HI 1.0

// Fixed interval between RUL calculations, in hours.
// 1hour : 1.0f,	30min : 0.5f,	10min : 10.0f/60.0f,	1min : 1.0f/60.0f
#define RUL_CALC_INTERVAL_HOURS (1.0f)

extern const float AE_X_MIN[AE_COLS];
extern const float AE_X_MAX[AE_COLS];

void ai_minmax_scale(const float *x, float *x_scaled, int len);

static inline int ae_round_to_int(float value)
{
  return (value >= 0.0f) ? (int)(value + 0.5f) : (int)(value - 0.5f);
}

static inline void ae_quantize_in_vec(const float *x, int8_t *q, int len)
{
  for (int i = 0; i < len; ++i) {
    int value = ae_round_to_int(x[i] / AE_IN_SCALE + AE_IN_ZERO_POINT);
    if (value < -128) value = -128;
    if (value > 127) value = 127;
    q[i] = (int8_t)value;
  }
}

static inline void ae_dequantize_out_vec(const int8_t *q, float *y, int len)
{
  for (int i = 0; i < len; ++i) {
    y[i] = ((float)q[i] - AE_OUT_ZERO_POINT) * AE_OUT_SCALE;
  }
}

#endif
