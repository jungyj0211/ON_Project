# ON_Project

STM32H562 기반 스마트 조명 디바이스의 센싱, 이상검출 AI 및 잔여수명(`RUL`) 계산 펌웨어 프로젝트입니다.

현재 AI·RUL 통합 기준 프로젝트는 [`ON_TEST_v1`](ON_TEST_v1/)입니다. 협력업체에서 AI 기능을 다른 펌웨어로 이식할 때는 이 프로젝트를 기준으로 사용하십시오.

> 상세한 파일별 설명과 이식 절차는 [`ON_TEST_v1/README.md`](ON_TEST_v1/README.md)를 참고하십시오.

## 현재 제공 기능

`ON_TEST_v1`은 UART 메뉴를 통해 다음 두 가지 동작을 제공합니다.

1. 실시간 정상/비정상 판정
2. 실시간 정상/비정상 판정 + RUL(%)

두 모드 모두 측정 횟수 제한 없이 실행되며 UART6에서 `q` 또는 `Q`를 입력하면 종료됩니다.

## 기준 개발 환경

| 항목 | 설정 |
|---|---|
| MCU | STM32H562RIT6 (Cortex-M33, LQFP64) |
| Toolchain | STM32CubeIDE |
| STM32CubeMX | 6.14.1 |
| STM32Cube DB | 6.0.141 |
| X-CUBE-AI | 10.2.0 |
| X-CUBE-ALGOBUILD | 1.4.0 |
| AI 모델 입출력 | INT8, 입력 4개 / 복원 출력 4개 |
| Activation 메모리 | 5,648 bytes |
| Weight 데이터 | 195,228 bytes |

## AI 처리 구조

```text
ADC/FFT 센싱
  -> 물리량 4개 구성
  -> Min-Max 정규화 및 0~1 제한
  -> INT8 양자화
  -> 오토인코더 추론
  -> INT8 출력 역양자화
  -> 입력과 복원 출력의 MSE 계산
  -> 정상/비정상 판정 및 HI 계산
  -> 선택 모드에서 온도와 HI를 이용한 RUL 계산
```

부팅 시 X-CUBE-AI 네트워크를 한 번 초기화합니다.

```c
MX_X_CUBE_AI_Init();
```

실시간 센싱 데이터가 준비되면 다음과 같이 추론합니다.

```c
float model_input[AE_COLS] = {
    peak_freq_khz,
    adc_pk_fft,
    output_voltage_v,
    output_current_a
};

AI_Result result = AI_infer(model_input);

if (result.status == 0) {
    /* result.is_anomaly: 0=정상, 1=비정상 */
    /* result.hi: 0.0~1.0 이상도 */
}
```

## 모델 입력 사양

입력 순서와 단위는 학습 조건에 포함되므로 이식 시 반드시 동일하게 유지해야 합니다.

| 인덱스 | 입력 | 단위 | Min | Max |
|---:|---|---|---:|---:|
| 0 | FFT 피크 주파수 | kHz | 80.078125 | 110.937507629 |
| 1 | FFT 피크 진폭 | ADC count | 0.712206006 | 11.872964859 |
| 2 | 출력 전압 | V | 27.366035461 | 27.631887436 |
| 3 | 출력 전류 | A | 2.729671001 | 3.293771982 |

정규화 공식은 다음과 같으며 결과는 `0.0~1.0`으로 제한됩니다.

```text
x_scaled = (x - min) / (max - min)
```

## AI 고정 설정

설정 위치: `ON_TEST_v1/Core/Inc/ai_config.h`

```c
#define AE_IN_SCALE       (0.003766168f)
#define AE_IN_ZERO_POINT  (-128)
#define AE_OUT_SCALE      (0.003898344f)
#define AE_OUT_ZERO_POINT (-128)

#define AE_THRESH  (0.011325965f)
#define AE_HI_LOW  (0.000452750f)
#define AE_HI_HIGH (0.000475430f)
```

- `AE_THRESH`: MSE 기반 정상/비정상 판정 임계값
- `AE_HI_LOW`: HI가 `0.0`이 되는 MSE 기준
- `AE_HI_HIGH`: HI가 `1.0`이 되는 MSE 기준
- `AE_IN_*`, `AE_OUT_*`: 현재 INT8 모델의 양자화 파라미터

이 값들은 현재 모델과 성능평가 결과에 종속됩니다. 모델을 재학습하거나 다시 양자화하지 않는 한 변경하지 마십시오. 특히 `AE_THRESH`를 임의로 재산정하거나 런타임에서 덮어쓰면 기존 성능평가 조건과 달라집니다.

정상/비정상 판정식:

```c
is_anomaly = (reconstruction_error >= AE_THRESH) ? 1U : 0U;
```

HI 계산식:

```text
HI = clamp((MSE - AE_HI_LOW) / (AE_HI_HIGH - AE_HI_LOW), 0, 1)
```

## AI 이식 대상 파일

| 파일 | 역할 |
|---|---|
| `ON_TEST_v1/Core/Inc/ai_config.h` | 입력 크기, 양자화값, 임계값, HI 범위, RUL 주기 |
| `ON_TEST_v1/Core/Inc/ai_inference.h` | AI 추론 API와 결과 구조체 |
| `ON_TEST_v1/Core/Src/ai_inference.c` | 정규화, 양자화, 추론, MSE, HI 및 판정 연결 |
| `ON_TEST_v1/Core/Inc/ai_minmax.h` | Min-Max API 선언 |
| `ON_TEST_v1/Core/Src/ai_minmax.c` | 학습 데이터 Min/Max 및 정규화 구현 |
| `ON_TEST_v1/X-CUBE-AI/App/app_x-cube-ai.h` | AI 실행 래퍼 선언 |
| `ON_TEST_v1/X-CUBE-AI/App/app_x-cube-ai.c` | 네트워크 초기화 및 INT8 입출력 실행 |
| `ON_TEST_v1/X-CUBE-AI/App/network*.c/.h` | 생성된 네트워크와 Weight 코드 |
| `ON_TEST_v1/Middlewares/ST/AI/` | X-CUBE-AI 런타임 헤더와 라이브러리 |

`network.c`, `network_data.c` 등 X-CUBE-AI 자동 생성 파일은 직접 수정하지 않는 것을 원칙으로 합니다.

현재 저장소에는 생성된 네트워크 C 코드와 분석 정보가 있지만 원본 ONNX 모델 파일은 포함되어 있지 않습니다. 다른 MCU나 X-CUBE-AI 버전에 맞춰 네트워크를 재생성해야 한다면 원본 모델을 별도로 전달받아야 합니다.

## 주요 추론 API

```c
AI_Result AI_infer(const float sensor_data[4]);
```

일반 실시간 추론에 사용합니다.

```c
AI_Result AI_infer_with_error(const float sensor_data[4],
                              float *reconstruction_error);
```

정상/비정상 결과와 원본 MSE가 모두 필요할 때 사용합니다.

```c
typedef struct {
    int status;          /* 0=성공, 음수=추론 오류 */
    uint8_t is_anomaly;  /* 0=정상, 1=비정상 */
    float hi;            /* 0.0~1.0 이상도 */
} AI_Result;
```

## RUL 계산

RUL 핵심 파일:

| 파일 | 역할 |
|---|---|
| `ON_TEST_v1/Core/Inc/rul_calculation.h` | RUL API 선언 |
| `ON_TEST_v1/Core/Src/rul_calculation.c` | 온도·HI 기반 누적 손상도와 RUL(%) 계산 |
| `ON_TEST_v1/Core/Src/ai_test.c` | 실시간 AI 출력, 온도 연결 및 RUL 호출 주기 관리 |

RUL 호출 형식:

```c
float rul_percent = RUL_claculate(temperature_c, result.hi);
```

현재 RUL 로직은 다음 순서로 동작합니다.

1. HI를 `0.0~1.0` 범위로 제한
2. EWMA로 HI 평활화
3. 온도에 따른 기준 수명 계산
4. 평활화된 HI에 따라 손상 속도를 1~3배로 가중
5. 누적 손상도를 `0.0~1.0`으로 제한
6. `(1 - cumulative_damage) * 100`을 RUL로 반환

RUL 계산 주기:

```c
#define RUL_CALC_INTERVAL_HOURS (1.0f)
```

AI 추론은 약 100ms마다 실행하며 RUL 손상도는 실제 1시간마다 한 번 누적합니다. 온도 측정값이 유효하지 않으면 해당 시점의 누적을 보류합니다.

### 중요: RUL 영구저장 미구현

현재 RUL 상태는 RAM에만 존재합니다.

```c
static float cumulative_damage;
static float hi_smooth;
static uint32_t sample_count;
```

따라서 MCU 전원을 끄거나 재시작하면 RUL이 다시 100%에서 시작합니다. 제품 적용 전 다음 기능을 구현해야 합니다.

- RUL 상태의 Flash 저장
- 부팅 시 최신 정상 상태 복원
- magic, version, sequence, CRC를 포함한 저장 레코드
- 동일 Flash 주소의 반복 erase를 피하기 위한 순차 로그 또는 wear leveling

권장 방식은 RUL을 1시간마다 계산한 직후 Flash 로그에 한 번 기록하는 것입니다.

## 온도 센싱

- 온도 입력 ADC: ADC2
- NTC 변환 함수: `ntc_voltage_to_temp_c_checked()`
- 구현 파일: `ON_TEST_v1/Core/Src/sensor_measure.c`
- 추론 연결 구조체: `AI_TestSensorData`

RUL 모드에서는 `temperature_valid == 1`일 때만 온도값을 사용합니다. 센서 오류값을 정상 온도로 처리하지 않도록 이식 대상에서도 동일한 유효성 검사를 유지해야 합니다.

## 협력업체 이식 순서

1. 대상 프로젝트에 X-CUBE-AI 런타임과 생성된 `network*` 파일을 추가합니다.
2. 시스템 초기화 시 `MX_X_CUBE_AI_Init()`을 한 번 호출합니다.
3. `ai_config`, `ai_minmax`, `ai_inference` 모듈을 추가합니다.
4. 정확한 순서와 단위로 네 개의 모델 입력을 구성합니다.
5. `AI_infer()`를 호출하고 반환된 `status`를 먼저 확인합니다.
6. `is_anomaly`와 `hi`를 상위 제어 또는 표시 기능에 연결합니다.
7. RUL이 필요하면 유효한 온도와 HI를 1시간 주기의 RUL 계산에 전달합니다.
8. 제품 적용 전에 RUL 상태의 Flash 저장과 부팅 복원을 구현합니다.
9. 실제 장치 데이터로 입력값, MSE 분포 및 판정 결과가 기준 펌웨어와 일치하는지 비교합니다.

## 저장소 내 프로젝트

저장소에는 개발 과정에서 생성된 여러 펌웨어가 포함되어 있습니다. AI·RUL 이식 기준은 `ON_TEST_v1`이며 `copy`, `backup`, `performance_test` 및 이전 실험 프로젝트를 최종 납품 기준으로 사용하지 마십시오.

## 빌드

STM32CubeIDE에서 `ON_TEST_v1`을 Import한 뒤 `Debug` 구성을 빌드합니다. 최근 전체 빌드는 오류 없이 완료되었습니다.

현재 기존 통신 및 비활성 코드에서 발생하는 경고가 남아 있으므로, 협력업체용 최소 프로젝트로 분리할 경우 사용하지 않는 모듈을 추가로 정리하는 것을 권장합니다.

## 라이선스 및 배포 주의사항

X-CUBE-AI 생성 코드와 런타임은 `ON_TEST_v1/LICENSE_X-CUBE-AI.txt` 및 ST 라이선스 조건을 확인해야 합니다. 외부 전달 전 AI 모델, 생성 Weight 코드 및 펌웨어의 배포 권한도 별도로 확인하십시오.
