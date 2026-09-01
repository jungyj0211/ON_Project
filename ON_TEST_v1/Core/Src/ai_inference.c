#include "ai_inference.h"

#include "main.h"
#include "ai_config.h"
#include "app_x-cube-ai.h"

static float g_ai_threshold = AE_THRESH;

static AI_Result AI_infer_impl(const float sensor_data[4],
                               uint32_t *elapsed_cycles,
                               float *error_output)
{
  AI_Result result = {
		  .status = -1,
		  .is_anomaly = 0U,
		  .hi = 0.0f
  };
  float input_scaled[AE_COLS];
  int8_t input_quantized[AE_COLS];
  int8_t output_quantized[AE_COLS];
  float output_dequantized[AE_COLS];
  float reconstruction_error = 0.0f;
  uint32_t start_cycles = 0U;

  if (elapsed_cycles != 0) {
    *elapsed_cycles = 0U;
  }
  if (error_output != 0) {
    *error_output = 0.0f;
  }
  if (sensor_data == 0) return result;

  /* TC-02 측정 시작: 유효한 센싱 데이터가 입력된 후 정규화를 시작하기 직전 */
  if (elapsed_cycles != 0) {
    start_cycles = DWT->CYCCNT;
  }

  /* 1단계: 센싱 데이터 4개를 학습 시 사용한 범위에 맞게 Min-Max 정규화 */
  ai_minmax_scale(sensor_data, input_scaled, AE_COLS);

  /* 2단계: 정규화된 실수 입력값을 모델 입력 형식인 INT8 값으로 양자화 */
  ae_quantize_in_vec(input_scaled, input_quantized, AE_COLS);

  /* 3단계: 경량화 AI 모델을 실행하여 INT8 형태의 추론 결과 생성 */
  result.status = AI_ModelRun(input_quantized, output_quantized);

  /* 모델 추론에 실패하면 실패 시점까지의 처리시간을 저장하고 종료 */
  if (result.status != 0) {
    if (elapsed_cycles != 0) {
      *elapsed_cycles = DWT->CYCCNT - start_cycles;
    }
    return result;
  }

  /* 4단계: 모델의 INT8 출력값을 복원오차 계산이 가능한 실수값으로 역양자화 */
  ae_dequantize_out_vec(output_quantized, output_dequantized, AE_COLS);

  /* 5단계: 정규화 입력값과 모델 복원값의 평균제곱오차(MSE) 계산 */
  for (int i = 0; i < AE_COLS; ++i) {
    float error = input_scaled[i] - output_dequantized[i];
    reconstruction_error += error * error;
  }
  reconstruction_error /= AE_COLS;
  if (error_output != 0) {
    *error_output = reconstruction_error;
  }

  /* 6단계: 복원오차를 기준으로 0.0~1.0 범위의 이상도(HI) 계산 */
  result.hi = (reconstruction_error - AE_HI_LOW) /
              (AE_HI_HIGH - AE_HI_LOW);
  if (result.hi < 0.0f) result.hi = 0.0f;
  if (result.hi > 1.0f) result.hi = 1.0f;

  /* 7단계: 복원오차와 임계값을 비교하여 정상(0) 또는 비정상(1) 판정 */
  result.is_anomaly =
      (reconstruction_error >= g_ai_threshold) ? 1U : 0U;

  /* TC-02 측정 종료: 정상/비정상 판정 결과가 저장된 직후 처리시간 계산 */
  if (elapsed_cycles != 0) {
    *elapsed_cycles = DWT->CYCCNT - start_cycles;
  }
  return result;
}

AI_Result AI_infer(const float sensor_data[4])
{
  return AI_infer_impl(sensor_data, 0, 0);
}

AI_Result AI_infer_timed(const float sensor_data[4], uint32_t *elapsed_cycles)
{
  return AI_infer_impl(sensor_data, elapsed_cycles, 0);
}

AI_Result AI_infer_with_error(const float sensor_data[4],
                              float *reconstruction_error)
{
  return AI_infer_impl(sensor_data, 0, reconstruction_error);
}

void AI_SetThreshold(float threshold)
{
  if ((threshold > 0.0f) && (threshold < 1.0f)) {
    g_ai_threshold = threshold;
  }
}

float AI_GetThreshold(void)
{
  return g_ai_threshold;
}
