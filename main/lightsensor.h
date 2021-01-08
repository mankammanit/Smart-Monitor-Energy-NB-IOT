#ifndef __LIGHTSENSOR_H__
#define __LIGHTSENSOR_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "esp_system.h"
#include "esp_adc_cal.h"

esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t LIGHT_SENSOR = ADC_CHANNEL_6; //GPIO34 if ADC1
static const adc_atten_t atten_light = ADC_ATTEN_DB_11;
static const adc_unit_t unit_light = ADC_UNIT_1;
#define ADC_WIDTH_LIGHT ADC_WIDTH_BIT_12

#define NO_OF_SAMPLES 64 //Multisampling

void init_lightsensor()
{
        adc1_config_width(ADC_WIDTH_LIGHT);
        adc1_config_channel_atten(LIGHT_SENSOR, atten_light);
}

int read_lightsensor()
{
        int light_read = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++)
        {
                if (unit_light == ADC_UNIT_1)
                {
                        light_read += adc1_get_raw((adc1_channel_t)LIGHT_SENSOR);
                }
                else
                {
                        int raw;
                        adc2_get_raw((adc2_channel_t)LIGHT_SENSOR, ADC_WIDTH_LIGHT, &raw);
                        light_read += raw;
                }
        }
        light_read /= NO_OF_SAMPLES;

        return light_read;
}

#endif