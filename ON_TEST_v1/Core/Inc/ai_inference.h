#ifndef INC_AI_INFERENCE_H_
#define INC_AI_INFERENCE_H_

#include <stdint.h>

typedef struct {
  int status;             /* 0: success, negative: inference error */
  uint8_t is_anomaly;     /* 0: normal, 1: anomaly */
  float hi;               /* anomaly-oriented HI: 0.0 normal, 1.0 anomaly */
} AI_Result;

AI_Result AI_infer(const float sensor_data[4]);
AI_Result AI_infer_timed(const float sensor_data[4], uint32_t *elapsed_cycles);
AI_Result AI_infer_with_error(const float sensor_data[4],
                              float *reconstruction_error);
void AI_SetThreshold(float threshold);
float AI_GetThreshold(void);

#endif
