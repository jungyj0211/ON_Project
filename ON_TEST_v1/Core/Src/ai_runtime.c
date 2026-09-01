#include "ai_runtime.h"

#include <stdio.h>
#include <string.h>

#include "ai_inference.h"
#include "main.h"
#include "rul.h"

#define RUL_CALC_INTERVAL_MS \
  ((uint32_t)(RUL_CALC_INTERVAL_HOURS * 60.0f * 60.0f * 1000.0f))

typedef struct
{
  uint32_t inference_count;
  uint32_t inference_error_count;
  uint32_t normal_prediction_count;
  uint32_t anomaly_prediction_count;
  uint32_t next_rul_tick;
  float rul_percent;
} AI_RuntimeState;

static AI_RuntimeState g_runtime;

void AI_Runtime_Init(void)
{
  memset(&g_runtime, 0, sizeof(g_runtime));
  g_runtime.rul_percent = 100.0f;
  g_runtime.next_rul_tick = HAL_GetTick() + RUL_CALC_INTERVAL_MS;

  printf("\r\n[실시간 AI 추론 + RUL(%%)]\r\n");
  printf("측정 횟수: 제한 없음\r\n");
  printf("현재 판정 임계값: %.9f\r\n", (double)AI_GetThreshold());
  printf("RUL 누적 주기: %.3f 시간\r\n",
         (double)RUL_CALC_INTERVAL_HOURS);
  printf("----------------------------------------\r\n");
}

void AI_Runtime_Process(const AI_RuntimeInput *input)
{
  AI_Result result;
  uint32_t now;

  if (input == NULL)
  {
    return;
  }

  result = AI_infer(input->model_input);
  if (result.status != 0)
  {
    ++g_runtime.inference_error_count;
    printf("[%lu] 추론 오류=%d\r\n",
           (unsigned long)g_runtime.inference_count, result.status);
    return;
  }

  ++g_runtime.inference_count;
  if (result.is_anomaly == 0U)
  {
    ++g_runtime.normal_prediction_count;
  }
  else
  {
    ++g_runtime.anomaly_prediction_count;
  }

  now = HAL_GetTick();
  if (input->temperature_valid &&
      ((int32_t)(now - g_runtime.next_rul_tick) >= 0))
  {
    g_runtime.rul_percent = RUL_Update(input->temperature_c, result.hi);
    g_runtime.next_rul_tick += RUL_CALC_INTERVAL_MS;
    if ((int32_t)(now - g_runtime.next_rul_tick) >= 0)
    {
      g_runtime.next_rul_tick = now + RUL_CALC_INTERVAL_MS;
    }
  }

  printf("[%lu] %.3f, %.3f, %.3f, %.3f / %s / HI=%.6f",
         (unsigned long)g_runtime.inference_count,
         (double)input->measured_freq_khz,
         (double)input->measured_adc_pk,
         (double)input->measured_voltage_v,
         (double)input->measured_current_a,
         result.is_anomaly ? "비정상" : "정상",
         (double)result.hi);

  if (input->temperature_valid)
  {
    printf(" / 온도=%.2f C / RUL=%.6f %%",
           (double)input->temperature_c,
           (double)g_runtime.rul_percent);
  }
  else
  {
    printf(" / 온도=INVALID / RUL 누적 보류");
  }
  printf("\r\n");
}
