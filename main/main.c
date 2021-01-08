/*
Project : 
Author : Kammanit Jitkul 
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "string.h"
#include "cJSON.h"
#include "time.h"

//Kammanit WSA Lib
#include "sim7020e.h"
#include "hdc1080.h"
#include "lightsensor.h"
#include "ds18b20.h"
#include "kct16.h"

static TaskHandle_t task_handles[portNUM_PROCESSORS];

char pub_data[64];
uint8_t counter_sec;
int counter_send;
time_t counter_longtime;
#define TIMER_SEND_MQTT 30 //seconds

static hdc1080_sensor_t *internal_temp_sensor;
float internal_temperature;
float internal_humidity;

#define I2C_NUM0_MASTER_SCL_IO 22        /*!< gpio number for I2C master clock */
#define I2C_NUM0_MASTER_SDA_IO 21        /*!< gpio number for I2C master data  */
#define I2C_NUM0_MASTER_FREQ_HZ 100000   /*!< I2C master clock frequency */
#define I2C_NUM0_MASTER_NUM I2C_NUM_0    /*!< I2C port number for master dev */
#define I2C_NUM0_MASTER_TX_BUF_DISABLE 0 /*!< I2C master do not need buffer */
#define I2C_NUM0_MASTER_RX_BUF_DISABLE 0 /*!< I2C master do not need buffer */

#define BH1750_SENSOR_ADDR 0x23    /*!< slave address for BH1750 sensor  Used scani2c --> High 0x5C , Low 0x23 */
#define BH1750_CMD_START 0x23      /*!< Operation mode BH1750_ONETIME_L_RESOLUTION */
#define WRITE_BIT I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN 0x1           /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0          /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                /*!< I2C ack value */
#define NACK_VAL 0x1               /*!< I2C nack value */

#undef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x)                         \
    do                                             \
    {                                              \
        esp_err_t rc = (x);                        \
        if (rc != ESP_OK)                          \
        {                                          \
            ESP_LOGE("err", "esp_err_t = %d", rc); \
            assert(0 && #x);                       \
        }                                          \
    } while (0);

uint8_t temprature_sens_read();

static void initI2C_NUM0()
{
    {
        i2c_config_t conf;
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = I2C_NUM0_MASTER_SDA_IO;
        conf.scl_io_num = I2C_NUM0_MASTER_SCL_IO;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = I2C_NUM0_MASTER_FREQ_HZ;

        int i2c_master_port = I2C_NUM0_MASTER_NUM;
        ESP_ERROR_CHECK(i2c_param_config(i2c_master_port, &conf));
        ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, conf.mode,
                                           I2C_NUM0_MASTER_RX_BUF_DISABLE,
                                           I2C_NUM0_MASTER_TX_BUF_DISABLE, 0));
    }
}

static esp_err_t i2c_master_sensor_bh1750(i2c_port_t i2c_num, uint8_t *data_h, uint8_t *data_l)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BH1750_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BH1750_CMD_START, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK)
    {
        return ret;
    }
    vTaskDelay(30 / portTICK_RATE_MS);
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BH1750_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data_h, ACK_VAL);
    i2c_master_read_byte(cmd, data_l, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

int read_bh1750()
{
    int ret;
    uint8_t sensor_data_h, sensor_data_l;
    int cnt = 0;
    int bh1750_val = 0;
    ret = i2c_master_sensor_bh1750(I2C_NUM0_MASTER_NUM, &sensor_data_h, &sensor_data_l);
    if (ret == ESP_ERR_TIMEOUT)
    {
        ESP_LOGE("BH1750", "I2C Timeout");
    }
    else if (ret == ESP_OK)
    {
        // printf("*******************\n");
        // printf("MASTER READ SENSOR( BH1750 )\n");
        // printf("*******************\n");
        // printf("data_h: %02x\n", sensor_data_h);
        // printf("data_l: %02x\n", sensor_data_l);
        // printf("sensor val: %.02f [Lux]\n", (sensor_data_h << 8 | sensor_data_l) / 1.2);
        bh1750_val = (sensor_data_h << 8 | sensor_data_l) / 1.2;
        return bh1750_val;
    }
    else
    {
        ESP_LOGE("BH1750", "%s: No ack, sensor not connected...skip...", esp_err_to_name(ret));
        return 0;
    }
    return 0;
}

static void scan_i2c()
{
    int i;
    esp_err_t espRc;
    printf("Starting scan\n");
    for (i = 3; i < 0x78; i++)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, 1 /* expect ack */);
        i2c_master_stop(cmd);

        espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 100 / portTICK_PERIOD_MS);
        if (espRc == 0)
        {
            printf("Found device at I2C_NUM_0 ID 0x%.2x\n", i);
        }
        i2c_cmd_link_delete(cmd);
    }
    // for (i = 3; i < 0x78; i++)
    // {
    //     i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    //     i2c_master_start(cmd);
    //     i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, 1 /* expect ack */);
    //     i2c_master_stop(cmd);

    //     espRc = i2c_master_cmd_begin(I2C_NUM_1, cmd, 100 / portTICK_PERIOD_MS);
    //     if (espRc == 0)
    //     {
    //         printf("Found device at I2C_NUM_1 ID 0x%.2x\n", i);
    //     }
    //     i2c_cmd_link_delete(cmd);
    // }
    printf("Scan complete \n");
}

void PublishtoThingBoards(void *pvParameter)
{

    if (counter_sec == TIMER_SEND_MQTT)
    {
        counter_send++;

        KCT16SensorRead(TIMER_SEND_MQTT);

        cJSON *root = cJSON_CreateObject();

        hdc1080_read(internal_temp_sensor, &internal_temperature, &internal_humidity);
        sprintf(pub_data, "%.1f", internal_temperature);
        cJSON_AddStringToObject(root, "In_T", pub_data);
        sprintf(pub_data, "%.1f", internal_humidity);
        cJSON_AddStringToObject(root, "In_H", pub_data);

        sprintf(pub_data, "%d", read_lightsensor());
        cJSON_AddStringToObject(root, "In_L", pub_data);

        sprintf(pub_data, "%d", read_bh1750());
        cJSON_AddStringToObject(root, "Ex_L", pub_data);

        float ds18b20_temp = ds18b20_get_temp();
        if (ds18b20_temp > 0 && ds18b20_temp < 100.00)
        {
            sprintf(pub_data, "%.1f", ds18b20_temp);
            cJSON_AddStringToObject(root, "Ex_T", pub_data);
        }

        sprintf(pub_data, "%.1f", ((temprature_sens_read() - 32) / 1.8));
        cJSON_AddStringToObject(root, "C_T", pub_data);

        sprintf(pub_data, "%.2f", sensorCurrent.irms);
        cJSON_AddStringToObject(root, "Amp", pub_data);

        sprintf(pub_data, "%.2f", sensorCurrent.power);
        cJSON_AddStringToObject(root, "Power", pub_data);

        sprintf(pub_data, "%.2f", sensorCurrent.kwh);
        cJSON_AddStringToObject(root, "Kwh", pub_data);

        sprintf(pub_data, "%.2f", sensorCurrent.cost);
        cJSON_AddStringToObject(root, "Ckwh", pub_data);

        sprintf(pub_data, "%d", esp_get_free_heap_size());
        cJSON_AddStringToObject(root, "Mem", pub_data);

        sprintf(pub_data, "%d", counter_send);
        cJSON_AddStringToObject(root, "Send", pub_data);

        char *get_value = NULL;

        get_value = GET_SIM7020E_COMMAND(&cmd_IMEI);
        cJSON_AddStringToObject(root, "IMEI", get_value);
        free(get_value);

        get_value = GET_SIM7020E_COMMAND(&cmd_CSQ);
        cJSON_AddStringToObject(root, "Signal", get_value);
        free(get_value);

        char *post_data = cJSON_PrintUnformatted(root);

        char *text = cJSON_Print(root);
        printf("JSON %s\n", text);
        free(text);

        cJSON_Delete(root);

        MQTTpublish(topic, 1, 1, 0, post_data);

        free(post_data);

        counter_sec = 0;
    }
    else
    {
        counter_sec++;
        counter_longtime++;
    }

    ESP_LOGI("periodic", "counter seconds : %d sec, %ld sec, %d count", counter_sec, counter_longtime, counter_send);
    ESP_LOGI("ESP32", "[APP] Free memory: %d bytes", esp_get_free_heap_size());
}

void ConnectToThingboards(void *pvParameters)
{

    printf("ConnectToThingboards running on core --> %d\r\n", xPortGetCoreID());

    //init UART
    init_NBIOT();

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &PublishtoThingBoards,
        .name = "periodic"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    /* Start the timers */
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000000));

    while (1)
    {
        //data inconmming
        if (MQTT_IS_CONNECTED)
        {
            char *mqtt_response = MQTTresponse();
            if (strcmp(mqtt_response, "FAILED") != 0)
            {
                printf("Message Response JSON --> %s\n", mqtt_response);
            }
        }
        vTaskDelay(10 / portTICK_RATE_MS);
    }
}

void app_main(void)
{
    ESP_LOGI("ESP32", "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI("ESP32", "[APP] IDF version: %s", esp_get_idf_version());

    //init Analog
    init_kct16();
    init_lightsensor();

    //init Digital
    ds18b20_init();

    //init I2C
    initI2C_NUM0();
    internal_temp_sensor = hdc1080_init_sensor(I2C_NUM_0, HDC1080_ADDR);
    hdc1080_set_resolution(internal_temp_sensor, hdc1080_14bit, hdc1080_14bit);

    if (internal_temp_sensor)
    {
        hdc1080_registers_t registers = hdc1080_get_registers(internal_temp_sensor);
        printf("Initialized HDC1080 internal_temp_sensor: manufacurer 0x%x, device id 0x%x\n", hdc1080_get_manufacturer_id(internal_temp_sensor), hdc1080_get_device_id(internal_temp_sensor));
        registers.acquisition_mode = 1;
        hdc1080_set_registers(internal_temp_sensor, registers);
    }
    else
    {
        printf("Could not initialize HDC1080\n");
    }
    scan_i2c();

    xTaskCreate(
        ConnectToThingboards,   /* Task function. */
        "ConnectToThingboards", /* String with name of task. */
        1024 * 8,               /* Stack size in bytes. */
        NULL,                   /* Parameter passed as input of the task */
        10,                     /* Priority of the task. */
        &task_handles[1]);      /* Task handle. */
}
