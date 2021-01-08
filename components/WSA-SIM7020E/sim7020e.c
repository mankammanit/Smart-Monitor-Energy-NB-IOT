#include "sim7020e.h"
#include <errno.h> /* for errno */

static const char *TAG = "[SIM7020E DRIVER]";

bool MQTT_IS_CONNECTED = false;
uint8_t Count_InitMODULE = 0;

static void Configure_Uart(int uart_num, int tx_pin, int rx_pin, int buad)
{
        uart_config_t uart_config = {
            .baud_rate = buad,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 122,
        };

        if (uart_driver_install(uart_num, BUF_SIZE * 2, 0, 0, NULL, 0) < 0)
        {
                ESP_LOGE(TAG, "uart_driver_install() error!");
        }

        if (uart_param_config(uart_num, &uart_config) < 0)
        {
                ESP_LOGE(TAG, "uart_param_config() error!");
        }
        if (uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE,
                         UART_PIN_NO_CHANGE) < 0)
        {
                ESP_LOGE(TAG, "uart_set_pin() error!");
        }
}

void cstringcpy(char *src, char *dest)
{
        while (*src)
        {
                *(dest++) = *(src++);
        }
        *dest = '\0';
}

char *combine(char const *key, char const *uri)
{
        int len = strlen(key) + strlen(uri) + 1; /* don't the '\0' terminator */

        char *pResult = malloc(len);

        if (pResult)
        {
                /* we got a buffer */
                strcpy(pResult, uri);
                strcat(pResult, key);
        }

        return pResult; /* will return NULL on failure */
                        /* caller is responsible for calling free() */
}

// C substring function definition
void substring(char s[], char sub[], int p, int l)
{
        int c = 0;

        while (c < l)
        {
                sub[c] = s[p + c - 1];
                c++;
        }
        sub[c] = '\0';
}

void string2hexString(char *input, char *output)
{
        int loop;
        int i;

        i = 0;
        loop = 0;

        while (input[loop] != '\0')
        {
                sprintf((char *)(output + i), "%02X", input[loop]);
                loop += 1;
                i += 2;
        }
        //insert NULL at the end of the output string
        output[i++] = '\0';
}

// There are _many_ ways to do this step.
unsigned char2digit(int ch)
{
        static const char Hex[] = "0123456789ABCDEF0123456789abcdef";
        char *p = memchr(Hex, ch, 32);
        if (p)
        {
                return (unsigned)(p - Hex) % 16;
        }
        return (unsigned)-1; // error value
}

// Return NULL with ill-formed string
char *HexStringToString(char *dest, size_t size, const char *src)
{
        char *p = dest;
        if (size <= 0)
        {
                return NULL;
        }
        size--;
        while (*src)
        {
                if (size == 0)
                        return NULL;
                size--;

                unsigned msb = char2digit(*src++);
                if (msb > 15)
                        return NULL;
                unsigned lsb = char2digit(*src++);
                if (lsb > 15)
                        return NULL;
                char ch = (char)(msb * 16 + lsb);

                // Optionally test for embedded null character
                if (ch == 0)
                        return NULL;

                *p++ = ch;
        }
        *p = '\0';
        return dest;
}

int indexOf(char *val, char *find)
{
        char *s;
        int x = 0;
        s = strstr(val, find); // search for string "hassasin" in buff
        if (s != NULL)         // if successful then s now points at "hassasin"
        {
                // printf("Found string at index = %d\n", s - req);
                x = s - val;
                return x;
        } // index of "hassasin" in buff can be found by pointer subtraction
        else
        {
                // printf("String not found\n"); // `strstr` returns NULL if search string not found
                return -1;
        }
        return -1;
}

void removeChar(char *str, char garbage)
{

        char *src, *dst;
        for (src = dst = str; *src != '\0'; src++)
        {
                *dst = *src;
                if (*dst != garbage)
                        dst++;
        }
        *dst = '\0';
}

void resetSIM7020E()
{
        gpio_set_direction(SIM7020E_RESET, GPIO_MODE_OUTPUT);
        vTaskDelay(100 / portTICK_RATE_MS);

        gpio_set_level(SIM7020E_RESET, 0);
        vTaskDelay(1000 / portTICK_RATE_MS);

        gpio_set_level(SIM7020E_RESET, 1);
        vTaskDelay(2000 / portTICK_RATE_MS);
}

bool CheckNBstatus()
{
        char *get_value = GET_SIM7020E_COMMAND(&cmd_GetNBStatus);
        int status_nb = atoi(get_value);
        free(get_value);
        return status_nb;
}

bool attachNetwork()
{
        bool status = false;
        if (!CheckNBstatus()) //ไม่เชื่อมต่อ
        {
                for (uint8_t i = 0; i < 60; i++)
                {
                        SET_SIM7020E_COMMAND(&cmd_SetPhoneFunc);
                        SET_SIM7020E_COMMAND(&cmd_SetNBStatus); // กรณี cmd_GetNBStatus == 0
                        vTaskDelay(1000 / portTICK_RATE_MS);
                        if (CheckNBstatus())
                        {
                                status = true;
                                break;
                        }
                        printf(".");
                }
        }
        else
        {
                status = true;
        }
        uart_flush(SIM7020E_UART);
        return status;
}

void InitSIM7020E(void *arg)
{
        if (Count_InitMODULE >= 5)
        {
                printf("---------- INIT FAILED > 5 ESP_RESTART ----------\n");
                esp_restart();
        }

        resetSIM7020E();

        SET_SIM7020E_COMMAND(&cmd_AT);

        SET_SIM7020E_COMMAND(&cmd_CMEE);

        sprintf(buf_cmd, AT_CPSMS, 0);
        SET_SIM7020E_COMMAND(&cmd_SetPowerSAVE);
        memset(buf_cmd, 0, sizeof(buf_cmd));

        char *get_value = NULL;

        get_value = GET_SIM7020E_COMMAND(&cmd_IMSI);
        printf("IMSI: %s\n", get_value);
        free(get_value);

        get_value = GET_SIM7020E_COMMAND(&cmd_ICCID);
        printf("ICCID: %s\n", get_value);
        free(get_value);

        get_value = GET_SIM7020E_COMMAND(&cmd_IMEI);
        printf("IMEI: %s\n", get_value);
        free(get_value);

        get_value = GET_SIM7020E_COMMAND(&cmd_CSQ);
        printf("CSQ: %s\n", get_value);
        free(get_value);

        get_value = GET_SIM7020E_COMMAND(&cmd_FWversion);
        printf("FWversion: %s\n", get_value);
        free(get_value);

        get_value = GET_SIM7020E_COMMAND(&cmd_GetPowerSAVE);
        printf("PowerSAVE: %s\n", get_value);
        free(get_value);

        if (attachNetwork())
        {
                printf("---------- NB Status Connected ----------\n");
        }
        else
        {
                printf("---------- NB Status Disconnected ----------\n");
                esp_restart();
        }

        SetupMQTT(MQTTSERVER, MQTTPORT, clientID, MQTT_USER, MQTT_PASS);

        MQTTsubscribe(topic_sub, 1);

        Count_InitMODULE = 0;
}

void init_NBIOT()
{
        Configure_Uart(SIM7020E_UART, SIM7020E_TXD, SIM7020E_RXD, SIM7020E_BUAD);
        ESP_LOGI("INIT_UART", "SIM7020E = %d", SIM7020E_UART);

        InitSIM7020E(NULL);
}

bool MQTTstatus()
{
        char *get_value = GET_SIM7020E_COMMAND(&cmd_GetMQTTstatus);
        int status_mqtt = atoi(get_value);
        free(get_value);
        return status_mqtt;
}

void SetupMQTT(char *server, char *port, char *id, char *user, char *pass)
{
        if (MQTTstatus())
        {
                MQTT_IS_CONNECTED = false;
                printf("---------- MQTT Status Connected ----------\n");
                SET_SIM7020E_COMMAND(&cmd_disconnectMQTT);
                printf("---------- MQTT Status Disconnected ----------\n");
        }
        else
        {
                printf("---------- MQTT Status Disconnected ----------\n");
                MQTT_IS_CONNECTED = false;
        }

        if (!MQTT_IS_CONNECTED)
        {
                sprintf(buf_cmd, AT_CMQNEW, server, port);
                SET_SIM7020E_COMMAND(&cmd_SetNewMQTT);
                memset(buf_cmd, 0, sizeof(buf_cmd));

                sprintf(buf_cmd, AT_CMQCON, id, user, pass);
                SET_SIM7020E_COMMAND(&cmd_setupMQTT);
                memset(buf_cmd, 0, sizeof(buf_cmd));

                printf("---------- MQTT Status Connected ----------\n");

                MQTT_IS_CONNECTED = true;
        }
}

void MQTTpublish(char *mqtttopic, uint8_t qos, uint8_t retained, uint8_t dup, char *message)
{

        int len = strlen(message);
        char buf[(len * 2) + 1];

        string2hexString(message, buf);

        // printf("%s<->%s\n", message, buf);
        if (MQTT_IS_CONNECTED)
        {
                sprintf(buf_cmd, AT_CMQPUB, mqtttopic, qos, retained, dup, strlen(buf), buf);
                SET_SIM7020E_COMMAND(&cmd_MQTTpublish);
                memset(buf_cmd, 0, sizeof(buf_cmd));
        }
}

void MQTTsubscribe(char *mqtttopic, uint8_t qos)
{
        sprintf(buf_cmd, AT_CMQSUB, mqtttopic, qos);
        SET_SIM7020E_COMMAND(&cmd_MQTTsubscribe);
        memset(buf_cmd, 0, sizeof(buf_cmd));
}

void MQTTUnsubscribe(char *mqtttopic)
{
        sprintf(buf_cmd, AT_CMQUNSUB, mqtttopic);
        SET_SIM7020E_COMMAND(&cmd_MQTTUNsubscribe);
        memset(buf_cmd, 0, sizeof(buf_cmd));
}

// read a line from the UART controller
char *read_line(uart_port_t uart_controller)
{
        static char line[2048];
        char *ptr = line;
        //printf("\nread_line on UART: %d\n", (int) uart_controller);
        while (1)
        {
                int num_read = uart_read_bytes(uart_controller, (unsigned char *)ptr, 1, 10 / portTICK_RATE_MS); //portMAX_DELAY);
                if (num_read == 1)
                {
                        // new line found, terminate the string and return
                        // printf("received char: %c", *ptr);
                        if (*ptr == '\n')
                        {
                                ptr++;
                                *ptr = '\0';
                                return line;
                        }
                        // else move to the next char
                        ptr++;
                }
                else
                {
                        // printf("num_read=%d", num_read);
                        *ptr = 10;
                        ptr++;
                        *ptr = 0;
                        return line;
                }
        }
}

char *MQTTresponse()
{
        char *sresp = read_line(SIM7020E_UART);
        int tot = strlen(sresp);

        if (strstr(sresp, "+CMQPUB: 0") != NULL)
        {

                // ESP_LOGI(TAG, "MQTT RESPONSE BEFORE: [%s]", sresp);
                // ESP_LOG_BUFFER_HEXDUMP(TAG, sresp, tot, ESP_LOG_INFO);

                uint16_t index = indexOf(sresp, ",");
                substring(sresp, sresp, index + 2, tot);
                // printf("this sub string1 -->%s\n", sresp);

                index = indexOf(sresp, ",");
                substring(sresp, sresp, index + 2, tot);
                // printf("this sub string2 -->%s\n", sresp);

                index = indexOf(sresp, ",");
                substring(sresp, sresp, index + 2, tot);
                // printf("this sub string3 -->%s\n", sresp);

                index = indexOf(sresp, ",");
                substring(sresp, sresp, index + 2, tot);
                // printf("this sub string4 -->%s\n", sresp);

                index = indexOf(sresp, ",");
                substring(sresp, sresp, index + 2, tot);
                // printf("this sub string5 -->%s\n", sresp);

                index = indexOf(sresp, ",");
                substring(sresp, sresp, index + 2, tot);
                // printf("this sub string6 -->%s\n", sresp);

                index = indexOf(sresp, "\"");
                substring(sresp, sresp, index + 2, tot);
                // printf("this sub string7 -->%s\n", sresp);

                static char str_cv[512];
                memset(str_cv, 0, sizeof(str_cv));
                static char str_res[512];
                memset(str_res, 0, sizeof(str_res));

                strcpy(str_cv, sresp);

                for (int i = 0; i < strlen(str_cv); i++)
                {
                        if (str_cv[i] != 0x22)
                                str_res[i] = str_cv[i];
                }

                char *dest = HexStringToString(str_res, sizeof(str_res), str_res);
                // printf("%-24s --> '%s'\n", str_res, dest ? dest : "NULL");

                // ESP_LOGI(TAG, "MQTT RESPONSE AFTER: [%s]", str_res);
                // ESP_LOG_BUFFER_HEXDUMP(TAG, str_res, strlen(str_res), ESP_LOG_INFO);

                if (strcmp(dest, "NULL") != 0)
                        return dest;
                else
                        return "FAILED";
        }

        return "FAILED";
}

void SET_SIM7020E_COMMAND(GSM_Cmd *command)
{
        int res = 0, send_fail = 0;
        do
        {
                res = atCmd_waitResponse(command->cmd, command->cmdResponseOnOk, command->timeoutMs, NULL, 0);
                vTaskDelay(500 / portTICK_PERIOD_MS);
                send_fail = send_fail + 1;
                // printf("res: %d,send_fail: %d\n", res, send_fail);
        } while (!res && send_fail < 10);
        //NBiot Setup Failed --> reint NBiot
        if (send_fail >= 10)
        {
                // reinit SIM7020E
                Count_InitMODULE++;
                MQTT_IS_CONNECTED = false;
                InitSIM7020E(NULL);
        }
}

char *GET_SIM7020E_COMMAND(GSM_Cmd *command)
{
        char *req = NULL;
        int len = 0;
        int res = 0, send_fail = 0;
        static char message_return[128];
        static char char_output[128];
        static char char_output2[128];
        static char sresp[128];
        static char stsub[128];
        memset(char_output, 0, sizeof(char_output));
        memset(char_output2, 0, sizeof(char_output2));
        memset(sresp, 0, sizeof(sresp));
        memset(stsub, 0, sizeof(stsub));
        memset(message_return, 0, sizeof(message_return));
        do
        {
                send_fail = send_fail + 1;
                len = atCmd_waitResponse(command->cmd, command->cmdResponseOnOk, command->timeoutMs, &req, 0);
                vTaskDelay(10 / portTICK_PERIOD_MS);

                // printf("Send failed --> %d\r\n", send_fail);

                // ESP_LOG_BUFFER_HEXDUMP(TAG, req, len, ESP_LOG_INFO);
                // ESP_LOGI(TAG, "AT RESPONSE indexOf [OK] --> %d\r\n", indexOf(req, "OK\r\n"));
                // ESP_LOGI(TAG, "AT RESPONSE [GET] Before: [%s]", req);

                if (indexOf(req, "OK\r\n") != -1)
                {

                        if (strstr(req, "AT+CIMI") != NULL)
                        {
                                //getCIMI
                                substring(req, stsub, 11, len);
                                // printf("this sub string -->%s\n", stsub);
                                // ESP_LOG_BUFFER_HEXDUMP(TAG, stsub, len, ESP_LOG_INFO);

                                int my_position = 0;
                                for (int i = 0; i < len; i++)
                                {
                                        if (stsub[i] == 0x0D)
                                        {
                                                // printf("position [0x0D]= %d\n", i);
                                                my_position = i;
                                                break;
                                        }
                                }

                                for (int i = 0; i < my_position; i++)
                                {
                                        if (stsub[i] != 0x0D)
                                        {
                                                // ESP_LOGI(TAG, "txt[%d]: [%c]", i, stsub[i]);
                                                sresp[i] = stsub[i];
                                        }
                                }
                                // ESP_LOG_BUFFER_HEXDUMP(TAG, sresp, len, ESP_LOG_INFO);
                                // ESP_LOGI(TAG, "AT RESPONSE [GET] After: [%s]", sresp);
                                free(req);
                                res = 1;
                        }
                        else if (strstr(req, "AT+CSQ") != NULL)
                        {
                                //getCSQ EX.rssi = (2 * rssi) - 113;
                                substring(req, stsub, 16, len);
                                // printf("this sub string -->%s\n", stsub);
                                for (int i = 0; i < len; i++)
                                {
                                        if (stsub[i] != 0x2c)
                                                char_output[i] = stsub[i];
                                }
                                // // printf("get CSQ char before:%s\n", char_output);
                                int8_t rssi_i = atoi(char_output);
                                rssi_i = (2 * rssi_i) - 113;
                                sprintf(char_output, "%d", rssi_i);
                                // // printf("get CSQ int:%d\n", rssi_i);
                                // printf("get CSQ char after:%s\n", char_output);
                                free(req);
                                res = 1;
                        }
                        else if (strstr(req, "AT+CCID") != NULL)
                        {
                                //getICCID
                                substring(req, stsub, 11, len);
                                // printf("this sub string -->%s\n", stsub);
                                // ESP_LOG_BUFFER_HEXDUMP(TAG, stsub, len, ESP_LOG_INFO);

                                int my_position = 0;
                                for (int i = 0; i < len; i++)
                                {
                                        if (stsub[i] == 0x0D)
                                        {
                                                // printf("position [0x0D]= %d\n", i);
                                                my_position = i;
                                                break;
                                        }
                                }

                                for (int i = 0; i < my_position; i++)
                                {
                                        if (stsub[i] != 0x0D)
                                        {
                                                // ESP_LOGI(TAG, "txt[%d]: [%c]", i, stsub[i]);
                                                sresp[i] = stsub[i];
                                        }
                                }
                                // ESP_LOG_BUFFER_HEXDUMP(TAG, sresp, len, ESP_LOG_INFO);
                                // ESP_LOGI(TAG, "AT RESPONSE [GET] After: [%s]", sresp);
                                free(req);
                                res = 1;
                        }
                        else if (strstr(req, "AT+CGSN") != NULL)
                        {
                                //getIMEI
                                substring(req, stsub, 20, len);
                                // printf("this sub string -->%s\n", stsub);
                                // ESP_LOG_BUFFER_HEXDUMP(TAG, stsub, len, ESP_LOG_INFO);

                                int my_position = 0;
                                for (int i = 0; i < len; i++)
                                {
                                        if (stsub[i] == 0x0D)
                                        {
                                                // printf("position [0x0D]= %d\n", i);
                                                my_position = i;
                                                break;
                                        }
                                }

                                for (int i = 0; i < my_position; i++)
                                {
                                        if (stsub[i] != 0x0D)
                                        {
                                                // ESP_LOGI(TAG, "txt[%d]: [%c]", i, stsub[i]);
                                                sresp[i] = stsub[i];
                                        }
                                }
                                // ESP_LOG_BUFFER_HEXDUMP(TAG, sresp, len, ESP_LOG_INFO);
                                // ESP_LOGI(TAG, "AT RESPONSE [GET] After: [%s]", sresp);
                                free(req);
                                res = 1;
                        }
                        else if (strstr(req, "AT+CGMR") != NULL)
                        {
                                //getFWversion
                                substring(req, stsub, 11, len);
                                // printf("this sub string -->%s\n", stsub);
                                // ESP_LOG_BUFFER_HEXDUMP(TAG, stsub, len, ESP_LOG_INFO);

                                int my_position = 0;
                                for (int i = 0; i < len; i++)
                                {
                                        if (stsub[i] == 0x0D)
                                        {
                                                // printf("position [0x0D]= %d\n", i);
                                                my_position = i;
                                                break;
                                        }
                                }

                                for (int i = 0; i < my_position; i++)
                                {
                                        if (stsub[i] != 0x0D)
                                        {
                                                // ESP_LOGI(TAG, "txt[%d]: [%c]", i, stsub[i]);
                                                sresp[i] = stsub[i];
                                        }
                                }
                                // ESP_LOG_BUFFER_HEXDUMP(TAG, sresp, len, ESP_LOG_INFO);
                                // ESP_LOGI(TAG, "AT RESPONSE [GET] After: [%s]", sresp);
                                free(req);
                                res = 1;
                        }
                        else if (strstr(req, "AT+CPSMS") != NULL)
                        {
                                //getPowerSAVE
                                substring(req, stsub, 21, len);
                                // printf("this sub string -->%s\n", stsub);
                                // ESP_LOG_BUFFER_HEXDUMP(TAG, stsub, len, ESP_LOG_INFO);

                                int my_position = 0;
                                for (int i = 0; i < len; i++)
                                {
                                        if (stsub[i] == 0x0D)
                                        {
                                                // printf("position [0x0D]= %d\n", i);
                                                my_position = i;
                                                break;
                                        }
                                }

                                for (int i = 0; i < my_position; i++)
                                {
                                        if (stsub[i] != 0x0D)
                                        {
                                                // ESP_LOGI(TAG, "txt[%d]: [%c]", i, stsub[i]);
                                                sresp[i] = stsub[i];
                                        }
                                }
                                // ESP_LOG_BUFFER_HEXDUMP(TAG, sresp, len, ESP_LOG_INFO);
                                // ESP_LOGI(TAG, "AT RESPONSE [GET] After: [%s]", sresp);
                                free(req);
                                res = 1;
                        }
                        else if (strstr(req, "AT+CGATT") != NULL)
                        {
                                //getNBStatus
                                substring(req, stsub, 21, len);
                                // printf("this sub string -->%s\n", stsub);
                                // ESP_LOG_BUFFER_HEXDUMP(TAG, stsub, len, ESP_LOG_INFO);

                                int my_position = 0;
                                for (int i = 0; i < len; i++)
                                {
                                        if (stsub[i] == 0x0D)
                                        {
                                                // printf("position [0x0D]= %d\n", i);
                                                my_position = i;
                                                break;
                                        }
                                }

                                for (int i = 0; i < my_position; i++)
                                {
                                        if (stsub[i] != 0x0D)
                                        {
                                                // ESP_LOGI(TAG, "txt[%d]: [%c]", i, stsub[i]);
                                                sresp[i] = stsub[i];
                                        }
                                }

                                // ESP_LOG_BUFFER_HEXDUMP(TAG, sresp, len, ESP_LOG_INFO);
                                // ESP_LOGI(TAG, "AT RESPONSE [GET] After: [%s]", sresp);
                                free(req);
                                res = 1;
                        }
                        else if (strstr(req, "AT+CMQCON") != NULL)
                        {
                                //getMQTTStatus
                                substring(req, stsub, 25, len);
                                // printf("this sub string -->%s\n", stsub);
                                // ESP_LOG_BUFFER_HEXDUMP(TAG, stsub, len, ESP_LOG_INFO);

                                int my_position = 0;
                                for (int i = 0; i < len; i++)
                                {
                                        if (stsub[i] == 0x2C)
                                        {
                                                // printf("position [0x2C]= %d\n", i);
                                                my_position = i;
                                                break;
                                        }
                                }

                                for (int i = 0; i < my_position; i++)
                                {
                                        if (stsub[i] != 0x2C)
                                        {
                                                // ESP_LOGI(TAG, "txt[%d]: [%c]", i, stsub[i]);
                                                sresp[i] = stsub[i];
                                        }
                                }

                                // ESP_LOG_BUFFER_HEXDUMP(TAG, sresp, len, ESP_LOG_INFO);
                                // ESP_LOGI(TAG, "AT RESPONSE [GET] After: [%s]", sresp);

                                free(req);
                                res = 1;
                        }
                }
                else
                {
                        free(req);
                        res = 0;
                }

        } while (!res && send_fail < 100);

        if (send_fail >= 100)
        {
                // reinit SIM7020E
                Count_InitMODULE++;
                MQTT_IS_CONNECTED = false;
                InitSIM7020E(NULL);
        }
        else
        {
                return combine(char_output, sresp);
        }

        return combine(char_output, "FAILED");
}

static void infoCommand(char *cmd, char *info)
{
        char buf[strlen(cmd) + 2];
        memset(buf, 0, strlen(cmd) + 2);

        for (int i = 0; i < strlen(cmd); i++)
        {
                if ((cmd[i] != 0x00) && ((cmd[i] < 0x20) || (cmd[i] > 0x7F)))
                        buf[i] = '.';
                else
                        buf[i] = cmd[i];
                if (buf[i] == '\0')
                        break;
        }
        ESP_LOGI(TAG, "%s [%s]", info, buf);
}

uint16_t atCmd_waitResponse(char *cmd, char *resp, int timeout, char **response, int size)
{

        char sresp[512] = {'\0'};

        int len, res = 1, idx = 0, tot = 0, timeoutCnt = 0;

        const int DATA_BUF_SIZE = BUF_SIZE;
        uint8_t *data = (uint8_t *)malloc(DATA_BUF_SIZE + 1);

        // ** Send command to GSM
        vTaskDelay(100 / portTICK_PERIOD_MS);
        uart_flush(SIM7020E_UART);

        if (cmd != NULL)
        {
                // infoCommand(cmd, "AT COMMAND:");
                uart_write_bytes(SIM7020E_UART, (const char *)cmd, strlen(cmd));
                uart_wait_tx_done(SIM7020E_UART, 100 / portTICK_RATE_MS);
        }
        if (response != NULL)
        {
                // Read GSM response into buffer
                char *pbuf = *response;
                len = uart_read_bytes(SIM7020E_UART, data, DATA_BUF_SIZE, timeout / portTICK_RATE_MS);
                while (len > 0)
                {
                        if ((tot + len) >= size)
                        {
                                char *ptemp = realloc(pbuf, size + 512);
                                if (ptemp == NULL)
                                        return 0;
                                size += 512;
                                pbuf = ptemp;
                        }
                        memcpy(pbuf + tot, data, len);
                        tot += len;
                        response[tot] = '\0';
                        len = uart_read_bytes(SIM7020E_UART, data, DATA_BUF_SIZE, 10 / portTICK_RATE_MS);
                }
                *response = pbuf;
                free(data);
                return tot;
        }

        // ** Wait for and check the response
        idx = 0;
        while (1)
        {
                memset(data, 0, 512);
                len = 0;
                len = uart_read_bytes(SIM7020E_UART, data, DATA_BUF_SIZE, 500 / portTICK_RATE_MS);
                if (len > 0)
                {
                        // ESP_LOGI(TAG, "len > 0");
                        for (int i = 0; i < len; i++)
                        {
                                if (idx < 512)
                                {
                                        if ((data[i] >= 0x20) && (data[i] < 0x80))
                                                sresp[idx++] = data[i];
                                        else
                                                sresp[idx++] = 0x2E;
                                }
                        }

                        tot += len;
                }
                else
                {
                        if (tot > 0)
                        {
                                // ESP_LOGI(TAG, "tot > 0");
                                // Check the response "OK"
                                if (strstr(sresp, resp) != NULL)
                                {

                                        ESP_LOGI(TAG, "AT RESPONSE [SET]: %s", sresp);
                                        // ESP_LOG_BUFFER_HEXDUMP(TAG, sresp, tot, ESP_LOG_INFO);
                                        break;
                                }

                                else
                                {

                                        ESP_LOGE(TAG, "AT RESPONSE [SET]: %s", sresp);
                                        res = 0;
                                        break;
                                }
                        }
                }

                timeoutCnt += 10;
                if (timeoutCnt > timeout)
                {
                        // timeout
                        ESP_LOGE(TAG, "AT: TIMEOUT");
                        res = 0;
                        break;
                }
        }
        free(data);

        return res;
}
