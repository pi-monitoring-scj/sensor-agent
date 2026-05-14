#  Pi-Monitoring: Sensor Agent (Edge Device)

> **"Hardware Abstraction & High-Speed Data Acquisition"**
> 라즈베리 파이 엣지 환경에서 MPU-6050 센서의 레지스터를 직접 제어하고, 수집된 진동/기울기 데이터를 MQTT 브로커로 발행(Publish)하는 C언어 기반 데이터 수집 에이전트입니다.

##  Repository Purpose
본 레포지토리는 [Pi-Monitoring 통합 프로젝트](메인_레포지토리_링크_예정)의 물리 계층(Physical Layer)과 네트워크 계층을 연결하는 핵심 모듈입니다. 

센서 데이터를 읽는 작업은 파이썬으로도 가능하지만, **운영체제(OS)의 간섭을 최소화하고 하드웨어 레지스터 단위의 정밀한 제어와 고빈도 샘플링을 달성하기 위해 C언어와 Linux I2C-dev 인터페이스를 채택**했습니다.

---

##  Key Features

### 1. Direct Register Control (C & I2C-dev)
* Linux 커널의 `<linux/i2c-dev.h>`를 활용하여 MPU-6050 센서의 I2C 버스에 직접 접근합니다.
* 상위/하위 8비트 레지스터 데이터를 비트 연산(`<<`, `|`)으로 결합하고 2의 보수를 처리하여 Raw Data를 빠르고 정확하게 변환합니다.

### 2. Event-Driven Publishing (MQTT)
* `Eclipse Paho MQTT C Client` 라이브러리를 사용하여 경량 메시징 통신을 구현했습니다.
* 데이터를 파일이나 DB에 직접 쓰지 않고 `sensor/mpu6050/raw` 토픽으로 발행(Publish)함으로써, 데이터 수집부와 백엔드 처리부(FastAPI)의 의존성을 완전히 분리(Decoupling)했습니다.

### 3. Fail-safe & Error Handling
* 하드웨어 통신 단절(I2C 라인 불안정 등)이나 MQTT 브로커 접속 실패 시, 프로세스가 죽지 않고 재연결을 시도하도록 예외 처리 로직을 구성했습니다.

---

##  Tech Stack
* **Language:** `C`
* **OS / Environment:** `Ubuntu Server (Raspberry Pi aarch64)`, `Linux Kernel ABI`
* **Library:** `i2c-tools`, `Eclipse Paho MQTT C`
* **Hardware:** `MPU-6050 (I2C Address: 0x68)`

---

##  Getting Started

### Prerequisites
빌드 및 실행을 위해 아래의 C언어 라이브러리와 컴파일러가 필요합니다.

```bash
sudo apt-get update
sudo apt-get install build-essential libi2c-dev libssl-dev
# Paho MQTT C 라이브러리 설치 필요



언제든 수정 가능
