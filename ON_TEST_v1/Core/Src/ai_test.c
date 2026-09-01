#include "ai_test.h"

#include <stdio.h>
#include <string.h>

#include "ai_inference.h"
#include "main.h"
#include "rul_calculation.h"

extern UART_HandleTypeDef huart6;

#define RUL_CALC_INTERVAL_MS \
  ((uint32_t)(RUL_CALC_INTERVAL_HOURS * 60.0f * 60.0f * 1000.0f))

typedef struct
{
  bool active;
  bool rul_enabled;
  bool done;
  uint32_t inference_count;
  uint32_t inference_error_count;
  uint32_t normal_prediction_count;
  uint32_t anomaly_prediction_count;
  uint32_t next_rul_tick;
  float rul_percent;
} AI_LiveState;

static AI_LiveState g_live;

static void Terminal_Clear(void)
{
  printf("\033[2J\033[H");
}

uint8_t AI_Test_MenuSelect(void)
{
  uint8_t key = 0U;

  for (;;)
  {
    Terminal_Clear();
    printf("========================================\r\n");
    printf("             AI 실시간 동작\r\n");
    printf("========================================\r\n");
    printf("1. 실시간 정상/비정상 판정\r\n");
    printf("2. 실시간 정상/비정상 판정 + RUL(%%)\r\n");
    printf("========================================\r\n");
    printf("동작 번호를 선택하세요: ");

    do
    {
      if (HAL_UART_Receive(&huart6, &key, 1U, HAL_MAX_DELAY) != HAL_OK)
      {
        key = 0U;
      }
    } while ((key == '\r') || (key == '\n'));

    printf("%c\r\n", key);
    if ((key == '1') || (key == '2'))
    {
      Terminal_Clear();
      return (uint8_t)(key - '0');
    }

    printf("잘못된 선택입니다. 1 또는 2를 입력하세요.\r\n");
    HAL_Delay(700U);
  }
}

void AI_Test_LiveInference_Init(bool rul_enabled)
{
  memset(&g_live, 0, sizeof(g_live));
  g_live.active = true;
  g_live.rul_enabled = rul_enabled;
  g_live.rul_percent = 100.0f;
  g_live.next_rul_tick = HAL_GetTick() + RUL_CALC_INTERVAL_MS;

  printf("\r\n[실시간 AI 추론%s]\r\n",
         rul_enabled ? " + RUL(%)" : "");
  printf("측정 횟수: 제한 없음\r\n");
  printf("종료 방법: q 입력\r\n");
  printf("현재 판정 임계값: %.9f\r\n", (double)AI_GetThreshold());
  if (rul_enabled)
  {
    printf("RUL 누적 주기: %.3f 시간\r\n",
           (double)RUL_CALC_INTERVAL_HOURS);
  }
  printf("----------------------------------------\r\n");
}

void AI_Test_LiveInference_Process(const AI_TestSensorData *sensor)
{
  AI_Result result;
  uint32_t now;

  if (!g_live.active || g_live.done || (sensor == NULL))
  {
    return;
  }

  result = AI_infer(sensor->model_input);
  if (result.status != 0)
  {
    ++g_live.inference_error_count;
    printf("[%lu] 추론 오류=%d\r\n",
           (unsigned long)g_live.inference_count, result.status);
    return;
  }

  ++g_live.inference_count;
  if (result.is_anomaly == 0U)
  {
    ++g_live.normal_prediction_count;
  }
  else
  {
    ++g_live.anomaly_prediction_count;
  }

  now = HAL_GetTick();
  if (g_live.rul_enabled && sensor->temperature_valid &&
      ((int32_t)(now - g_live.next_rul_tick) >= 0))
  {
    g_live.rul_percent = RUL_claculate(sensor->temperature_c, result.hi);
    g_live.next_rul_tick += RUL_CALC_INTERVAL_MS;
    if ((int32_t)(now - g_live.next_rul_tick) >= 0)
    {
      g_live.next_rul_tick = now + RUL_CALC_INTERVAL_MS;
    }
  }

  printf("[%lu] %.3f, %.3f, %.3f, %.3f / %s / HI=%.6f",
         (unsigned long)g_live.inference_count,
         (double)sensor->measured_freq_khz,
         (double)sensor->measured_adc_pk,
         (double)sensor->measured_voltage_v,
         (double)sensor->measured_current_a,
         result.is_anomaly ? "비정상" : "정상",
         (double)result.hi);

  if (g_live.rul_enabled)
  {
    if (sensor->temperature_valid)
    {
      printf(" / 온도=%.2f C / RUL=%.6f %%",
             (double)sensor->temperature_c,
             (double)g_live.rul_percent);
    }
    else
    {
      printf(" / 온도=INVALID / RUL 누적 보류");
    }
  }
  printf("\r\n");
}

bool AI_Test_LiveInference_IsDone(void)
{
  return g_live.done;
}

bool AI_Test_LiveInference_HandleRxByte(uint8_t byte)
{
  if (!g_live.active || g_live.done)
  {
    return false;
  }
  if ((byte != 'q') && (byte != 'Q'))
  {
    return false;
  }

  g_live.active = false;
  g_live.done = true;
  printf("\r\n실시간 AI 추론을 종료합니다. 측정=%lu, 정상=%lu, 비정상=%lu, 오류=%lu",
         (unsigned long)g_live.inference_count,
         (unsigned long)g_live.normal_prediction_count,
         (unsigned long)g_live.anomaly_prediction_count,
         (unsigned long)g_live.inference_error_count);
  if (g_live.rul_enabled)
  {
    printf(", RUL=%.6f %%", (double)g_live.rul_percent);
  }
  printf("\r\n");
  return true;
}
