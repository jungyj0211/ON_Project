#ifndef INC_RUL_CALCULATION_H_
#define INC_RUL_CALCULATION_H_

/* The spelling is fixed to the integration API requested for this project. */
/* Return remaining useful life as a percentage from 0.0f to 100.0f. */
float RUL_claculate(float temperature_c,
                    float hi);

#endif
