# ON_TEST_v1

## 프로젝트 개요

- 대상 MCU: STM32H562RIT6
- 기준 프로젝트: `ON_TEST_v1`
- 주요 기능
  - 실시간 AI 이상검출
  - 정상/비정상 판정
  - 이상도 `HI` 계산
  - 온도 및 HI 기반 RUL(%) 계산
- 동작 방식
  - 부팅 후 실시간 이상검출과 RUL 계산 자동 실행
  - 측정 횟수 제한 없이 계속 실행

## AI 관련 추가 파일

### 설정 및 전처리

- `ON_TEST_v1/Core/Inc/ai_config.h`
  - AI 입력 및 출력 크기
  - INT8 양자화 파라미터
  - 정상/비정상 판정 임계값
  - HI 계산 범위
  - RUL 계산 주기
- `ON_TEST_v1/Core/Inc/ai_minmax.h`
  - Min-Max 정규화 함수 선언
- `ON_TEST_v1/Core/Src/ai_minmax.c`
  - 학습 데이터 Min/Max 값
  - 입력값 정규화
  - 정규화 결과의 `0.0~1.0` 제한

### AI 추론

- `ON_TEST_v1/Core/Inc/ai_inference.h`
  - `AI_Result` 구조체
  - AI 추론 함수 선언
- `ON_TEST_v1/Core/Src/ai_inference.c`
  - Min-Max 정규화 호출
  - INT8 입력 양자화
  - AI 모델 실행
  - INT8 출력 역양자화
  - 복원오차 MSE 계산
  - HI 계산
  - 정상/비정상 판정

### AI 모델 및 런타임

- `ON_TEST_v1/X-CUBE-AI/App/app_x-cube-ai.c`
  - AI 네트워크 초기화
  - `AI_ModelRun()` 구현
- `ON_TEST_v1/X-CUBE-AI/App/app_x-cube-ai.h`
  - AI 초기화 및 실행 함수 선언
- `ON_TEST_v1/X-CUBE-AI/App/network*.c/.h`
  - X-CUBE-AI 생성 네트워크
  - 모델 Weight 데이터
- `ON_TEST_v1/Middlewares/ST/AI/`
  - X-CUBE-AI 런타임 라이브러리 및 헤더
- 주의사항
  - `network.c`, `network_data.c` 등 자동 생성 파일 직접 수정 금지

## X-CUBE-AI 및 ONNX 모델 적용

- X-CUBE-AI 설치
- ONNX 네트워크 등록
- 코드 생성 및 main.c 연결
- 문서: [X_CUBE_AI_SETUP.md](X_CUBE_AI_SETUP.md)
- 주의사항
  - X-CUBE-AI 및 CubeMX가 생성한 파일은 코드 재생성 시 덮어쓸 수 있음
  - 자동 생성 파일의 사용자 코드는 반드시 `USER CODE BEGIN`과 `USER CODE END` 사이에 작성
  - 해당 영역 밖에 작성한 코드는 `.ioc`의 `Generate Code` 실행 시 삭제될 수 있음

```c
/* USER CODE BEGIN 2 */
int AI_ModelRun(const int8_t input[4], int8_t output[4])
{
    /* 사용자 AI 실행 코드 */
}
/* USER CODE END 2 */
```

### RUL 및 실시간 실행

- `ON_TEST_v1/Core/Inc/rul.h`
  - RUL 계산 함수 선언
- `ON_TEST_v1/Core/Src/rul.c`
  - HI 평활화
  - 온도별 기준 수명 계산
  - 누적 손상도 계산
  - RUL(%) 반환
- `ON_TEST_v1/Core/Inc/ai_runtime.h`
  - 실시간 센서 데이터 구조체
  - 실시간 처리 함수 선언
- `ON_TEST_v1/Core/Src/ai_runtime.c`
  - 실시간 AI 추론 호출
  - 정상/비정상 및 HI 출력
  - 온도 유효성 확인
  - 1시간 단위 RUL 호출

## main.c 적용 구조

### 1. AI 초기화

- 호출 위치: 주변장치 초기화 이후
- 호출 횟수: 부팅 시 1회

```c
MX_X_CUBE_AI_Init();
```

### 2. 실시간 추론 및 RUL 초기화

```c
AI_Runtime_Init();
```

- 메뉴 선택 없이 실시간 이상검출과 RUL 계산 시작
- RUL 초기 표시값: `100%`
- RUL 누적 주기 타이머 시작

### 3. AI 입력 구성

```c
float model_input[4] = {
    raw_freq_khz,
    adc_pk,
    vin_v,
    i_adc_v
};
```

- 입력 순서
  - `[0]`: FFT 피크 주파수
  - `[1]`: FFT 피크 진폭
  - `[2]`: 출력 전압
  - `[3]`: 출력 전류
- 입력 단위
  - FFT 피크 주파수: `kHz`
  - FFT 피크 진폭: `ADC count`
  - 출력 전압: `V`
  - 출력 전류: `A`
- 필수 조건
  - 입력 순서 변경 금지
  - 입력 단위 변경 금지

### 4. 센서 데이터 전달

```c
AI_RuntimeInput input = {
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

AI_Runtime_Process(&input);
```

- `main.c` 담당 작업
  - ADC 센싱
  - FFT 처리
  - AI 입력 4개 구성
  - 온도 및 온도 유효 여부 구성
  - 실시간 처리 함수 호출
- 별도 모듈 담당 작업
  - 정규화
  - 양자화 및 역양자화
  - AI 모델 추론
  - MSE 및 HI 계산
  - 정상/비정상 판정
  - RUL 계산

## AI 처리 순서

```text
AI_Runtime_Process()
  -> AI_infer()
     -> ai_minmax_scale()
     -> ae_quantize_in_vec()
     -> AI_ModelRun()
     -> ae_dequantize_out_vec()
     -> MSE 계산
     -> HI 계산
     -> 정상/비정상 판정
  -> RUL_Update()
```

## AI 세부 설정

### 모델 입출력 크기

- 설정 파일: `ON_TEST_v1/Core/Inc/ai_config.h`
- 입력: 4개
- 출력: 4개

```c
#define AE_IN_DIM  (4)
#define AE_OUT_DIM (4)
#define AE_COLS    (4)
```

### INT8 양자화 설정

```c
#define AE_IN_SCALE       (0.003766168f)
#define AE_IN_ZERO_POINT  (-128)
#define AE_OUT_SCALE      (0.003898344f)
#define AE_OUT_ZERO_POINT (-128)
```

- 현재 INT8 모델 전용 설정
- 모델 미변경 시 수정 금지

### 정상/비정상 판정 설정

```c
#define AE_THRESH  (0.011325965f)
#define AE_HI_LOW  (0.000452750f)
#define AE_HI_HIGH (0.000475430f)
```

- `AE_THRESH`
  - MSE 기반 정상/비정상 판정 임계값
  - `MSE >= AE_THRESH`: 비정상
  - `MSE < AE_THRESH`: 정상
- `AE_HI_LOW`
  - HI `0.0` 기준 MSE
- `AE_HI_HIGH`
  - HI `1.0` 기준 MSE
- 현재 모델 및 성능평가 기준값
- 세 값 모두 변경 금지

```text
HI = clamp((MSE - AE_HI_LOW) / (AE_HI_HIGH - AE_HI_LOW), 0, 1)
```

### Min-Max 정규화 설정

- 설정 파일: `ON_TEST_v1/Core/Src/ai_minmax.c`

| 순서 | 입력 | Min | Max |
|---:|---|---:|---:|
| 0 | FFT 피크 주파수(kHz) | 80.078125 | 110.937507629 |
| 1 | FFT 피크 진폭(ADC count) | 0.712206006 | 11.872964859 |
| 2 | 출력 전압(V) | 27.366035461 | 27.631887436 |
| 3 | 출력 전류(A) | 2.729671001 | 3.293771982 |

```text
x_scaled = (x - min) / (max - min)
```

- 정규화 결과 범위: `0.0~1.0`
- 현재 학습 데이터 기준값
- 모델 미변경 시 수정 금지

## RUL 설정

### 계산 주기

```c
#define RUL_CALC_INTERVAL_HOURS (1.0f)
```

- AI 추론: 실시간 반복
- RUL 손상도 누적: 1시간마다 1회
- 온도값 무효 시 RUL 누적 보류

### 계산 입력 및 출력

```c
float rul_percent = RUL_Update(temperature_c, result.hi);
```

- 입력
  - 측정 온도: `temperature_c`
  - AI 이상도: `result.hi`
- 출력
  - RUL: `0.0~100.0%`

### 현재 저장 방식

- RAM 저장 항목
  - `cumulative_damage`
  - `hi_smooth`
  - `sample_count`
- 전원 차단 시 누적값 초기화
- 전원 재인가 후 누적 유지 기능 미구현

### Flash 저장 필요사항

- 수명 계산값은 디바이스가 꺼져도 이전 값 유지 필요
- RUL 누적 상태의 Flash 메모리 저장 필요
- 디바이스 부팅 시 Flash에 저장된 최신 상태 복원 필요
- Flash 저장 시점에 대한 정책 필요
  - 예: 1시간 단위 RUL 계산 직후 저장
  - 예: G/W에서 디바이스 OFF 신호 수신 시 저장
  - 예: 일정 수준 이상 RUL 값이 변경될 때 저장
