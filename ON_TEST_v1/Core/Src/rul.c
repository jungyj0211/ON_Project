#include "rul.h"

#include <stdint.h>

#include "ai_config.h"

#define RUL_EWMA_ALPHA     (2.0f / 25.0f)
#define RUL_DAMAGE_ALPHA   (2.0f)
#define RUL_MIN_LIFE_HOUR  (1000.0f)

/* Runtime state in RAM. These values are reset when the MCU restarts. */
static float cumulative_damage;
static float hi_smooth;
static uint32_t sample_count;

static float clampf(float value, float low, float high)
{
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

static float life_at_temperature(float temperature_c)
{
  float life_hour;

  if (temperature_c < 70.0f) {
    life_hour = 100000.0f;
  } else if (temperature_c <= 80.0f) {
    life_hour = 100000.0f - 5500.0f * (temperature_c - 70.0f);
  } else {
    life_hour = 45000.0f - 3000.0f * (temperature_c - 80.0f);
  }

  return (life_hour < RUL_MIN_LIFE_HOUR) ? RUL_MIN_LIFE_HOUR : life_hour;
}

float RUL_Update(float temperature_c, float hi)
{
  hi = clampf(hi, 0.0f, 1.0f);

  if (sample_count == 0U) {
    hi_smooth = hi;
  } else {
    hi_smooth = (1.0f - RUL_EWMA_ALPHA) * hi_smooth +
                RUL_EWMA_ALPHA * hi;
  }
  ++sample_count;

  if (RUL_CALC_INTERVAL_HOURS > 0.0f) {
    float damage_factor = clampf(
        1.0f + RUL_DAMAGE_ALPHA * hi_smooth, 1.0f, 3.0f);

    cumulative_damage +=
        RUL_CALC_INTERVAL_HOURS / life_at_temperature(temperature_c) *
        damage_factor;
    cumulative_damage = clampf(cumulative_damage, 0.0f, 1.0f);
  }

  return (1.0f - cumulative_damage) * 100.0f;
}
