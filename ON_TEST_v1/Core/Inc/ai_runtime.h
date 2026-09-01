#ifndef INC_AI_RUNTIME_H_
#define INC_AI_RUNTIME_H_

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
} AI_RuntimeInput;

void AI_Runtime_Init(void);
void AI_Runtime_Process(const AI_RuntimeInput *input);

#endif
