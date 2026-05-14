#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "MQTTClient.h"

// --- I2C 및 센서 설정 ---
#define I2C_BUS "/dev/i2c-1"
#define MPU6050_ADDR 0x68
#define PWR_MGMT_1 0x6B
#define ACCEL_XOUT_H 0x3B

// --- MQTT 브로커 설정 ---
// TODO: 향후 config 파일이나 환경변수로 분리할 부분
#define ADDRESS "tcp://localhost:1883" // Mosquitto 브로커 주소 (우선 로컬 기준)
#define CLIENTID "RaspberryPi_SensorAgent"
#define TOPIC "sensor/mpu6050/raw"
#define QOS 1
#define TIMEOUT 10000L

// I2C 레지스터 쓰기 함수
void write_register(int file, unsigned char reg, unsigned char value)
{
    unsigned char buffer[2] = {reg, value};
    if (write(file, buffer, 2) != 2)
    {
        perror("I2C 쓰기 에러");
        exit(1);
    }
}

// I2C 16비트 데이터 읽기 함수
short read_raw_data(int file, unsigned char reg)
{
    unsigned char buffer[1] = {reg};
    if (write(file, buffer, 1) != 1)
    {
        perror("I2C 레지스터 선택 에러");
        exit(1);
    }

    unsigned char data[2];
    if (read(file, data, 2) != 2)
    {
        perror("I2C 데이터 읽기 에러");
        exit(1);
    }

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
    if ((file = open(I2C_BUS, O_RDWR)) < 0)
    {
        perror("I2C 버스를 열 수 없습니다");
        exit(1);
    }
    if (ioctl(file, I2C_SLAVE, MPU6050_ADDR) < 0)
    {
        perror("MPU6050 장치에 접근할 수 없습니다");
        exit(1);
    }

    // 센서 깨우기
    write_register(file, PWR_MGMT_1, 0x00);
    printf("MPU6050 센서 초기화 완료!\n");

    // 2. MQTT 클라이언트 초기화 및 연결
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
    char payload[100];
    while (1)
    {
        short acc_x = read_raw_data(file, ACCEL_XOUT_H); // 센서에서 데이터 읽기

        // JSON 형태로 페이로드 구성
        sprintf(payload, "{\"accel_x\": %d}", acc_x);

        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = payload;
        pubmsg.payloadlen = (int)strlen(payload);
        pubmsg.qos = QOS;
        pubmsg.retained = 0;

        MQTTClient_deliveryToken token;
        MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);

        printf("Published: %s\n", payload);

        usleep(500000); // 0.5초 대기 (500,000 마이크로초)
    }

    // 종료 처리 (실제로는 Ctrl+C로 종료)
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    close(file);
    return 0;
}