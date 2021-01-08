#ifndef DS18B20_H_
#define DS18B20_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"

#define DS_GPIO (GPIO_NUM_13) //resistor on board 4.7K

void ds18b20_send(char bit);
unsigned char ds18b20_read(void);
void ds18b20_send_byte(char data);
unsigned char ds18b20_read_byte(void);
unsigned char ds18b20_RST_PULSE(void);
float ds18b20_get_temp(void);
void ds18b20_init();

#endif