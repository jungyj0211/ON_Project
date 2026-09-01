# STM32CubeIDE에 X-CUBE-AI 및 ONNX 모델 추가

### 적용 기준

- STM32CubeMX: `6.14.1`
- X-CUBE-AI: `10.2.0`
- 대상 MCU: `STM32H562RIT6`
- 모델 형식: 양자화된 `ONNX`
- 모델 입력: INT8 4개
- 모델 출력: INT8 4개
- 네트워크 이름: `network`

### 1. X-CUBE-AI 설치

- STM32CubeIDE에서 프로젝트의 `.ioc` 파일 열기
- `Help > Manage Embedded Software Packages` 실행
- STMicroelectronics Software Packs에서 `X-CUBE-AI 10.2.0` 설치
- 설치 완료 후 `.ioc` 화면 다시 열기

### 2. X-CUBE-AI 컴포넌트 활성화

- `.ioc` 화면에서 `Software Packs > Select Components` 실행
- `STMicroelectronics.X-CUBE-AI` 선택
- `Artificial Intelligence` 또는 AI Core 컴포넌트 활성화
- X-CUBE-AI 설정 화면으로 이동

### 3. ONNX 네트워크 등록

- X-CUBE-AI 설정 화면에서 네트워크 추가
- ONNX 모델 파일 선택
- 네트워크 이름을 `network`로 지정
- 모델 분석(`Analyze`) 실행
- 분석 성공 여부 확인

현재 프로젝트의 모델 설정:

| 항목 | 설정 |
|---|---|
| Model type | ONNX |
| Network name | `network` |
| Runtime | STM32Cube.AI MCU runtime |
| Compression | None |
| Copy weights to RAM | false |
| Input allocation | true |
| Output allocation | true |
| Custom layer | false |
| `noOnnxIoTranspose` | false |

### 4. 모델 입출력 확인

- 입력 텐서 형식: `S8(INT8)`
- 입력 크기: `4 bytes`
- 출력 텐서 형식: `S8(INT8)`
- 출력 크기: `4 bytes`
- 입력 및 출력 개수가 현재 코드와 일치하는지 확인
- 모델이 Float 또는 다른 크기로 표시될 경우 현재 `ai_config.h` 사용 불가
- 모델 분석 결과가 다를 경우 양자화 파라미터와 추론 코드 재설정 필요

### 5. 코드 생성

- `Project Manager`에서 Toolchain을 `STM32CubeIDE`로 선택
- `Generate Code` 실행
- 생성 결과 확인
  - `X-CUBE-AI/App/app_x-cube-ai.c`
  - `X-CUBE-AI/App/app_x-cube-ai.h`
  - `X-CUBE-AI/App/network.c`
  - `X-CUBE-AI/App/network.h`
  - `X-CUBE-AI/App/network_data.c`
  - `X-CUBE-AI/App/network_data.h`
  - `X-CUBE-AI/App/network_data_params.c/.h`
  - `Middlewares/ST/AI/`

### 6. 모델 실행 래퍼 추가

- 파일: `X-CUBE-AI/App/app_x-cube-ai.h`
- `USER CODE` 영역에 함수 선언 추가

```c
int AI_ModelRun(const int8_t input[4], int8_t output[4]);
```

- 파일: `X-CUBE-AI/App/app_x-cube-ai.c`
- `USER CODE` 영역에 모델 실행 함수 추가

```c
int AI_ModelRun(const int8_t input[4], int8_t output[4])
{
    if ((network == AI_HANDLE_NULL) || (input == NULL) || (output == NULL)) {
        return -1;
    }

    memcpy(AI_HANDLE_PTR(ai_input[0].data), input, 4U);

    if (ai_network_run(network, ai_input, ai_output) != 1) {
        return -2;
    }

    memcpy(output, AI_HANDLE_PTR(ai_output[0].data), 4U);
    return 0;
}
```

- `memcpy()` 사용을 위한 `<string.h>` 포함 확인
- 함수는 반드시 CubeMX 코드 생성 시 유지되는 `USER CODE` 영역에 작성

### 7. AI 모듈 추가

- 다음 파일을 대상 프로젝트의 `Core/Inc`, `Core/Src`에 추가
  - `ai_config.h`
  - `ai_minmax.h/.c`
  - `ai_inference.h/.c`
  - `ai_runtime.h/.c`
  - `rul.h/.c`
- 프로젝트 Include Path 확인
  - `Core/Inc`
  - `X-CUBE-AI/App`
  - `Middlewares/ST/AI/Inc`
- X-CUBE-AI 런타임 라이브러리 링크 여부 확인

### 8. main.c 연결

- 헤더 추가

```c
#include "ai_runtime.h"
#include "app_x-cube-ai.h"
```

- 주변장치 초기화 후 AI 네트워크 및 Runtime 초기화

```c
MX_X_CUBE_AI_Init();
AI_Runtime_Init();
```

- 센싱 완료 후 입력값 구성 및 Runtime 호출

```c
AI_RuntimeInput input = {
    .model_input = {
        raw_freq_khz,
        adc_pk,
        output_voltage_v,
        output_current_a
    },
    .measured_freq_khz = raw_freq_khz,
    .measured_adc_pk = adc_pk,
    .measured_voltage_v = output_voltage_v,
    .measured_current_a = output_current_a,
    .temperature_c = temperature_c,
    .temperature_valid = temperature_valid
};

AI_Runtime_Process(&input);
```

### 9. 빌드 및 동작 확인

- 프로젝트 `Clean`
- 전체 프로젝트 `Build`
- 확인 항목
  - `network` 생성 및 초기화 성공
  - `AI_ModelRun()` 반환값 `0`
  - 입력/출력 크기 각각 4 bytes
  - MSE 및 HI 출력 확인
  - 정상/비정상 결과 출력 확인
  - 온도 유효 시 RUL 출력 확인
- `.ioc`에서 다시 코드를 생성한 경우 확인할 항목
  - `AI_ModelRun()` USER CODE 유지 여부
  - `main.c`의 AI 초기화 및 Runtime 호출 유지 여부
  - `ai_config.h`의 임계값 및 양자화값 변경 여부

