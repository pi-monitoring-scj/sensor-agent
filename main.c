#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>        // POSIX 운영체제 API (usleep, read, write, close 등)
#include <fcntl.h>         // 파일 제어 (open 등)
#include <sys/ioctl.h>     // 하드웨어 장치 입출력 제어
#include <linux/i2c-dev.h> // 리눅스 I2C 드라이버 인터페이스
#include "MQTTClient.h"    // Paho MQTT 클라이언트 라이브러리

// --- I2C 및 센서 설정 ---
#define I2C_BUS "/dev/i2c-1" // 라즈베리 파이의 I2C 버스 파일 경로
#define MPU6050_ADDR 0x68    // MPU6050 센서의 기본 I2C 슬레이브 주소
#define PWR_MGMT_1 0x6B      // 전원 관리 레지스터 주소 (센서 깨우기용)
#define ACCEL_XOUT_H 0x3B    // 가속도 X축 데이터가 시작되는 레지스터 주소

// --- MQTT 브로커 설정 ---
// TODO: 향후 config 파일이나 환경변수로 분리할 부분
#define ADDRESS "tcp://localhost:1883"     // Mosquitto 브로커 주소 (우선 로컬 기준)
#define CLIENTID "RaspberryPi_SensorAgent" // 브로커가 클라이언트를 식별할 ID
#define TOPIC "sensor/mpu6050/raw"         // 데이터를 보낼 경로(주제)
#define QOS 1                              // 서비스 품질 (최소 한 번 전달 보장)
#define TIMEOUT 10000L                     // 타임아웃 설정

// I2C 레지스터 쓰기 함수
// 센서의 특정 레지스터(reg)에 설정값(value)을 작성. 설정값을 변경하거나 센서를 작동시킬 때 사용.
void write_register(int file, unsigned char reg, unsigned char value)
{
    unsigned char buffer[2] = {reg, value};
    if (write(file, buffer, 2) != 2) // 레지스터 주소와 값을 묶어서 전송
    {
        perror("I2C 쓰기 에러");
        exit(1);
    }
}

// I2C 16비트 데이터 읽기 함수
// MPU6050은 데이터를 8비트씩 두 곳에 나눠 저장. 이를 하나로 합쳐 의미 있는 숫자(short 타입)로 작성.
short read_raw_data(int file, unsigned char reg)
{
    unsigned char buffer[1] = {reg};

    // 1. 어떤 레지스터를 읽을지 센서에 먼저 알림
    if (write(file, buffer, 1) != 1)
    {
        perror("I2C 레지스터 선택 에러");
        exit(1);
    }

    // 2. 센서로부터 2바이트(16비트) 데이터를 읽어옴
    unsigned char data[2];
    if (read(file, data, 2) != 2)
    {
        perror("I2C 데이터 읽기 에러");
        exit(1);
    }

    // 3. 상위 바이트와 하위 바이트를 합쳐서 16비트 정수로 변환
    short value = (data[0] << 8) | data[1];
    return value;
}

int main()
{
    int file;
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    // 1. I2C 장치 열기 (OS 하드웨어 접근)
    // I2C 연결: 리눅스에서는 모든 장치를 '파일'로 취급하므로 open으로 열고, ioctl로 내가 대화할 센서 주소를 고정.

    // I2C 버스 파일 열기
    if ((file = open(I2C_BUS, O_RDWR)) < 0)
    {
        perror("I2C 버스를 열 수 없습니다");
        exit(1);
    }
    // 슬레이브 주소 설정
    if (ioctl(file, I2C_SLAVE, MPU6050_ADDR) < 0)
    {
        perror("MPU6050 장치에 접근할 수 없습니다");
        exit(1);
    }

    // 센서 깨우기: PWR_MGMT_1 레지스터에 0을 써서 슬립 모드 해제
    write_register(file, PWR_MGMT_1, 0x00);
    printf("MPU6050 센서 초기화 완료!\n");

    // 2. MQTT 클라이언트 초기화 및 연결
    // MQTT 연결: MQTTClient_create로 객체를 만들고, MQTTClient_connect를 호출하여 서버에 접속.
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("MQTT 브로커 연결 실패, return code %d\n", rc);
        exit(1);
    }
    printf("MQTT 브로커 연결 성공! (%s)\n", ADDRESS);

    // 3. 센서 데이터 수집 및 Publish (무한 루프)
    // 반복 작업: 센서 데이터 읽기 → JSON 문자열 만들기 → MQTT로 쏘기 → 0.5초 쉬기를 무한 반복.
    char payload[100];
    while (1)
    {
        // 센서에서 X축 가속도 데이터 읽기
        short acc_x = read_raw_data(file, ACCEL_XOUT_H); // 센서에서 데이터 읽기

        // JSON 형태로 페이로드 구성 (예: {"accel_x": 1234})
        sprintf(payload, "{\"accel_x\": %d}", acc_x);

        // 메시지 객체 설정
        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = payload;                 // 보낼 내용
        pubmsg.payloadlen = (int)strlen(payload); // 내용의 길이
        pubmsg.qos = QOS;                         // QoS 설정
        pubmsg.retained = 0;                      // 브로커에 메시지 보관 여부(아니오)

        // 메시지 전송 (Publish)
        MQTTClient_deliveryToken token;
        MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);

        printf("Published: %s\n", payload);

        usleep(500000); // 0.5초 대기 (500,000 마이크로초)
    }

    // 종료 처리 (실제로는 Ctrl+C로 종료)
    // 프로그램이 끝날 때 네트워크 연결을 끊고, 열어두었던 I2C 파일을 닫아 자원을 해제.
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    close(file);
    return 0;
}