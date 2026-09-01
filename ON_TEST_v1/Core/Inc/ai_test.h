#ifndef INC_AI_TEST_H_
#define INC_AI_TEST_H_

#include <stdbool.h>
#include <stdint.h>

#include "ai_config.h"

typedef struct
{
  float model_input[AE_COLS];
  float measured_freq_khz;
  float measured_adc_pk;
  float measured_voltage_v;
  float measured_current_a;
  float temperature_c;
  uint8_t temperature_valid;
} AI_TestSensorData;

uint8_t AI_Test_MenuSelect(void);
void AI_Test_LiveInference_Init(bool rul_enabled);
void AI_Test_LiveInference_Process(const AI_TestSensorData *sensor);
bool AI_Test_LiveInference_IsDone(void);
bool AI_Test_LiveInference_HandleRxByte(uint8_t byte);

#endif
