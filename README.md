# ON_TEST_v1

`ON_TEST_v1`은 STM32H562에서 센싱한 데이터를 이용해 AI 이상검출을 수행하고, 온도와 AI 이상도를 이용해 잔여수명(`RUL`)을 계산하는 프로젝트입니다.

현재 실행 기능은 다음 두 가지입니다.

1. 실시간 정상/비정상 판정
2. 실시간 정상/비정상 판정 + RUL(%)

## AI 기능을 위해 추가된 파일

### AI 설정과 추론

| 파일 | 역할 |
|---|---|
| `ON_TEST_v1/Core/Inc/ai_config.h` | 입력 크기, 양자화 파라미터, 판정 임계값, HI 범위 설정 |
| `ON_TEST_v1/Core/Inc/ai_minmax.h` | 입력 정규화 함수 선언 |
| `ON_TEST_v1/Core/Src/ai_minmax.c` | 학습 데이터의 Min/Max 값과 Min-Max 정규화 구현 |
| `ON_TEST_v1/Core/Inc/ai_inference.h` | AI 추론 함수와 결과 구조체 선언 |
| `ON_TEST_v1/Core/Src/ai_inference.c` | 정규화, INT8 양자화, AI 실행, MSE·HI·정상/비정상 판정 |

### AI 모델 실행

| 파일/폴더 | 역할 |
|---|---|
| `ON_TEST_v1/X-CUBE-AI/App/app_x-cube-ai.c` | AI 네트워크 초기화 및 INT8 모델 실행 래퍼 |
| `ON_TEST_v1/X-CUBE-AI/App/app_x-cube-ai.h` | 초기화 함수와 `AI_ModelRun()` 선언 |
| `ON_TEST_v1/X-CUBE-AI/App/network*.c/.h` | X-CUBE-AI에서 생성된 네트워크 및 Weight 코드 |
| `ON_TEST_v1/Middlewares/ST/AI/` | X-CUBE-AI 실행 라이브러리와 헤더 |

`network.c`, `network_data.c` 등 X-CUBE-AI 자동 생성 파일은 직접 수정하지 않습니다.

### RUL 계산과 실시간 실행

| 파일 | 역할 |
|---|---|
| `ON_TEST_v1/Core/Inc/rul_calculation.h` | RUL 계산 함수 선언 |
| `ON_TEST_v1/Core/Src/rul_calculation.c` | 온도와 HI를 이용한 누적 손상도 및 RUL(%) 계산 |
| `ON_TEST_v1/Core/Inc/ai_test.h` | 실시간 실행에 전달할 센서 구조체와 함수 선언 |
| `ON_TEST_v1/Core/Src/ai_test.c` | 실시간 추론, 결과 출력, RUL 모드 및 1시간 호출 주기 관리 |

## main.c 적용 방법

AI 계산식은 `main.c`에 직접 구현하지 않고 별도 C 파일의 함수를 호출하도록 구성되어 있습니다.

### 1. AI 네트워크 초기화

주변장치 초기화가 끝난 후 부팅 시 한 번 호출합니다.

```c
MX_X_CUBE_AI_Init();
```

### 2. 실행 모드 초기화

메뉴에서 선택한 값에 따라 일반 실시간 추론 또는 RUL 모드를 시작합니다.

```c
g_active_runtime_test = AI_Test_MenuSelect();
AI_Test_LiveInference_Init(g_active_runtime_test == 2U);
```

- `1`: 실시간 정상/비정상 판정
- `2`: 실시간 정상/비정상 판정 + RUL(%)

### 3. AI 입력값 구성

ADC와 FFT 센싱이 끝나면 다음 순서로 입력값 네 개를 구성합니다.

```c
float model_input[4] = {
    raw_freq_khz,  /* FFT 피크 주파수, kHz */
    adc_pk,        /* FFT 피크 진폭, ADC count */
    vin_v,         /* 출력 전압, V */
    i_adc_v        /* 출력 전류, A */
};
```

입력 순서와 단위가 학습 데이터와 다르면 정상적인 결과가 나오지 않으므로 반드시 유지해야 합니다.

### 4. 실시간 추론 함수 호출

센싱값과 온도를 `AI_TestSensorData`에 넣고 실시간 처리 함수를 호출합니다.

```c
AI_TestSensorData sensor = {
    .model_input = {
        model_input[0],
        model_input[1],
        model_input[2],
        model_input[3]
    },
    .measured_freq_khz = raw_freq_khz,
    .measured_adc_pk = adc_pk,
    .measured_voltage_v = vin_v,
    .measured_current_a = i_adc_v,
    .temperature_c = temp_c,
    .temperature_valid = temp_valid
};

AI_Test_LiveInference_Process(&sensor);
```

`AI_Test_LiveInference_Process()` 내부에서 다음 작업이 수행됩니다.

```text
AI_infer()
  -> 입력 정규화
  -> INT8 양자화
  -> 모델 추론
  -> 복원오차(MSE) 계산
  -> HI 계산
  -> 정상/비정상 판정
  -> RUL 모드인 경우 RUL_claculate() 호출
```

즉, `main.c`에서는 센서값을 준비해 구조체에 넣고 함수만 호출하면 됩니다.

## AI 세부 설정

설정 위치는 `ON_TEST_v1/Core/Inc/ai_config.h`입니다.

### 모델 입출력

```c
#define AE_IN_DIM  (4)
#define AE_OUT_DIM (4)
#define AE_COLS    (4)
```

오토인코더 모델은 네 개의 값을 입력받고 네 개의 복원값을 출력합니다.

### INT8 양자화 설정

```c
#define AE_IN_SCALE       (0.003766168f)
#define AE_IN_ZERO_POINT  (-128)
#define AE_OUT_SCALE      (0.003898344f)
#define AE_OUT_ZERO_POINT (-128)
```

현재 생성된 INT8 모델에 맞춘 값이므로 모델을 변경하지 않는 한 수정하지 않습니다.

### 정상/비정상 임계값과 HI 범위

```c
#define AE_THRESH  (0.011325965f)
#define AE_HI_LOW  (0.000452750f)
#define AE_HI_HIGH (0.000475430f)
```

- `AE_THRESH`: MSE가 이 값 이상이면 비정상으로 판정
- `AE_HI_LOW`: HI가 `0.0`이 되는 MSE 기준
- `AE_HI_HIGH`: HI가 `1.0`이 되는 MSE 기준

정상/비정상 판정식:

```c
is_anomaly = (reconstruction_error >= AE_THRESH) ? 1U : 0U;
```

HI 계산식:

```text
HI = clamp((MSE - AE_HI_LOW) / (AE_HI_HIGH - AE_HI_LOW), 0, 1)
```

위 세 값은 현재 모델의 성능평가 결과에 맞춘 값이므로 변경하지 않습니다.

### Min-Max 정규화 범위

설정 위치는 `ON_TEST_v1/Core/Src/ai_minmax.c`입니다.

| 입력 순서 | 입력값 | Min | Max |
|---:|---|---:|---:|
| 0 | FFT 피크 주파수(kHz) | 80.078125 | 110.937507629 |
| 1 | FFT 피크 진폭(ADC count) | 0.712206006 | 11.872964859 |
| 2 | 출력 전압(V) | 27.366035461 | 27.631887436 |
| 3 | 출력 전류(A) | 2.729671001 | 3.293771982 |

```text
x_scaled = (x - min) / (max - min)
```

정규화 결과는 `0.0~1.0` 범위로 제한됩니다. 이 값도 현재 모델의 학습 데이터에 맞춘 값이므로 변경하지 않습니다.

### RUL 설정

```c
#define RUL_CALC_INTERVAL_HOURS (1.0f)
```

AI 추론은 실시간으로 계속 실행하며 RUL 손상도는 유효한 온도값을 사용해 실제 1시간마다 한 번 누적합니다.

현재 `cumulative_damage`, `hi_smooth`, `sample_count`는 RAM에만 저장됩니다. 전원이 꺼진 후에도 RUL 누적값을 유지하려면 이 상태들을 Flash에 저장하고 부팅 시 복원하는 기능이 추가로 필요합니다.
