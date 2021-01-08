#ifndef SIM7020E_H_
#define SIM7020E_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_event.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

///Pin Config
#define BUF_SIZE (1024)
#define SIM7020E_TXD (GPIO_NUM_17)
#define SIM7020E_RXD (GPIO_NUM_16)
#define SIM7020E_RESET (GPIO_NUM_26)
#define SIM7020E_UART UART_NUM_1
#define SIM7020E_BUAD 115200

#define GSM_OK_Str "OK"
char buf_cmd[512];

//server MQTT
#define MQTTSERVER "iot2.wsa.cloud" //ip 139.59.106.36,host iot1.wsa.cloud
#define MQTTPORT "1883"
#define MQTT_USER "vGWgngM0qDLTkpLkMp3g" //username for mqtt server, username <= 100 characters
#define MQTT_PASS ""                     //password for mqtt server, password <= 100 characters

#define clientID "myclient"                     //Your client id < 120 characters
#define topic "v1/devices/me/telemetry"         //Your topic     < 128 characters
#define topic_sub "v1/devices/me/rpc/request/+" //Your topic     < 128 characters

//command is dynamic value
#define AT_CPSMS "AT+CPSMS=%d\r\n"                        // 0=OFF,1=ON
#define AT_CMQNEW "AT+CMQNEW=\"%s\",\"%s\",1200,1024\r\n" //server,port,timeout,buff
//AT+CMQCON=<mqtt_id>,<version>,<client_id>,<keepalive_interval>,<cleansession>,<will_flag>[,<will_options>][,<username>,<password>]
#define AT_CMQCON "AT+CMQCON=0,3,\"%s\",60,1,0,\"%s\",\"%s\"\r\n"
//AT+CMQPUB=<mqtt_id>,<topic>,<QoS>,<retained>,<dup>,<message_len>,<message>
#define AT_CMQPUB "AT+CMQPUB=0,\"%s\",%d,%d,%d,%d,\"%s\"\r\n"
// AT+CMQSUB=<mqtt_id>,<topic>,<QoS>
#define AT_CMQSUB "AT+CMQSUB=0,\"%s\",%d\r\n"
// AT+CMQUNSUB=<mqtt_id>,<topic>
#define AT_CMQUNSUB "AT+CMQUNSUB=0,\"%s\"\r\n"

typedef struct
{
    char *_topic;
    uint8_t _qos;
    uint8_t _retained;
    uint8_t _dup;
    uint16_t _len;
    char *_message;

} NBmqtt_t;

typedef struct
{
    char *cmd;
    char *cmdResponseOnOk;
    uint16_t timeoutMs;
    uint16_t delayMs;
    uint8_t skip;
} GSM_Cmd;

static GSM_Cmd cmd_AT =
    {
        .cmd = "AT\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_CMEE =
    {
        .cmd = "AT+CMEE=1\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_IMSI =
    {
        .cmd = "AT+CIMI\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_CSQ =
    {
        .cmd = "AT+CSQ\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_ICCID =
    {
        .cmd = "AT+CCID\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_IMEI =
    {
        .cmd = "AT+CGSN=1\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_FWversion =
    {
        .cmd = "AT+CGMR\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_GetPowerSAVE =
    {
        .cmd = "AT+CPSMS?\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_SetPowerSAVE =
    {
        .cmd = buf_cmd,
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

// cmd_GetNBStatus 0=NotConnect 1=Connected
static GSM_Cmd cmd_GetNBStatus =
    {
        .cmd = "AT+CGATT?\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};
// cmd_SetNBStatus 0=NotConnect 1=Connected
static GSM_Cmd cmd_SetNBStatus =
    {
        .cmd = "AT+CGATT=1\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

// Set Phone Functionality : 1 Full functionality
static GSM_Cmd cmd_SetPhoneFunc =
    {
        .cmd = "AT+CFUN=1\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_SetNewMQTT =
    {
        .cmd = buf_cmd,
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 10000,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_disconnectMQTT =
    {
        .cmd = "AT+CMQDISCON=0\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_GetMQTTstatus =
    {
        .cmd = "AT+CMQCON?\r\n",
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 100,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_setupMQTT =
    {
        .cmd = buf_cmd,
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 10000,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_MQTTpublish =
    {
        .cmd = buf_cmd,
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 1000,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_MQTTsubscribe =
    {
        .cmd = buf_cmd,
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 1000,
        .delayMs = 0,
        .skip = 0,
};

static GSM_Cmd cmd_MQTTUNsubscribe =
    {
        .cmd = buf_cmd,
        .cmdResponseOnOk = GSM_OK_Str,
        .timeoutMs = 1000,
        .delayMs = 0,
        .skip = 0,
};

extern bool MQTT_IS_CONNECTED;

void init_NBIOT();

char *read_line(uart_port_t uart_controller);

void SET_SIM7020E_COMMAND(GSM_Cmd *cmd);
char *GET_SIM7020E_COMMAND(GSM_Cmd *cmd);

uint16_t atCmd_waitResponse(char *, char *, int, char **, int);

void SetupMQTT(char *server, char *port, char *id, char *user, char *pass);

void MQTTpublish(char *mqtttopic, uint8_t qos, uint8_t retained, uint8_t dup, char *message);

void MQTTsubscribe(char *mqtttopic, uint8_t qos);

void MQTTUnsubscribe(char *mqtttopic);

char *MQTTresponse();

char *HexStringToString(char *dest, size_t size, const char *src);

void resetSIM7020E();

#endif