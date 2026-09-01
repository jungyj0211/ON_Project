#include "ai_config.h"

const float AE_X_MIN[AE_COLS] = {
  80.078125f, 0.712206006f, 27.366035461f, 2.729671001f
};

const float AE_X_MAX[AE_COLS] = {
  110.937507629f, 11.872964859f, 27.631887436f, 3.293771982f
};

void ai_minmax_scale(const float *x, float *x_scaled, int len)
{
  for (int i = 0; i < len; ++i) {
    float value = (x[i] - AE_X_MIN[i]) / (AE_X_MAX[i] - AE_X_MIN[i]);
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    x_scaled[i] = value;
  }
}
