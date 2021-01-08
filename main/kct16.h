#ifndef __KCT16_H__
#define __KCT16_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "esp_system.h"
#include "esp_adc_cal.h"
#include <math.h>

#define DEFAULT_VREF 1100          //Vref Default
#define ADC_BITS 12                //Quantity of bit ESP32 ADC1
#define ADC_COUNTS (1 << ADC_BITS) //Left Shift Raized 2 times .: ADC_COUNTS = 24

static const char *TAG_KCT = "KCT16_DRIVER";

esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t KCT16_SENSOR = ADC_CHANNEL_7; //GPIO35 if ADC1
static const adc_atten_t atten_kct16 = ADC_ATTEN_DB_11;
static const adc_unit_t unit_kct16 = ADC_UNIT_1;
#define ADC_WIDTH_KCT16 ADC_WIDTH_BIT_12

int sampleI;
unsigned int samples;

double Irms;
double filteredI; //Filtered_ is the raw analog value minus the DC offset
double offsetI;   //Low-pass filter output
double ICAL;      //Calibration coefficient. Need to be set in order to obtain accurate results
double sqI, sumI;

float kwhValue = 0.0;
float sumKwh = 0.0;
float sumCost = 0.0;
float costKwh = 0.0;

void currentCalibration(double _ICAL);
double getIrms(int NUMBER_OF_SAMPLES);
// void vSensorTask(void *pvParameter);
void KCT16SensorRead(int pertime);

struct sensor
{
        double irms;
        float power;
        float kwh;
        float cost;
} sensorCurrent;

static void check_efuse(void)
{
        //Check TP is burned into eFuse
        if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
        {
                ESP_LOGI(TAG_KCT, "eFuse Two Point: Supported\n");
        }
        else
        {
                ESP_LOGI(TAG_KCT, "eFuse Two Point: NOT supported\n");
        }

        //Check Vref is burned into eFuse
        if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
        {
                ESP_LOGI(TAG_KCT, "eFuse Vref: Supported\n");
        }
        else
        {
                ESP_LOGI(TAG_KCT, "eFuse Vref: NOT supported\n");
        }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
        if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
        {
                ESP_LOGI(TAG_KCT, "Characterized using Two Point Value\n");
        }
        else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
        {
                ESP_LOGI(TAG_KCT, "Characterized using eFuse Vref\n");
        }
        else
        {
                ESP_LOGI(TAG_KCT, "Characterized using Default Vref\n");
        }
}

void currentCalibration(double _ICAL)
{
        ICAL = _ICAL;
        offsetI = ADC_COUNTS >> 1;
        int adjust = 0;

        while (adjust < 250)
        {
                getIrms(1480);
                adjust++;
                vTaskDelay(10 / portTICK_RATE_MS);
        }
}

double getIrms(int NUMBER_OF_SAMPLES)
{
        samples = NUMBER_OF_SAMPLES;
        int SupplyVoltage = 3300; //3V3 power supply ESP32

        sampleI = 0;

        for (unsigned int n = 0; n < samples; n++)
        {
                sampleI = adc1_get_raw((adc1_channel_t)KCT16_SENSOR);

                // O filtro passa-baixa digital extrai o 1.65 VDC offset,
                //subtraia isso - o sinal agora está centrado em 0 contagens.
                offsetI = (offsetI + (sampleI - offsetI) / ADC_COUNTS);
                filteredI = sampleI - offsetI;

                // Corrente RMS
                // 1) square current values
                sqI = filteredI * filteredI;
                // 2) sum
                sumI += sqI;
        }

        double I_RATIO = ICAL * ((SupplyVoltage / 1000.0) / (ADC_COUNTS));
        Irms = I_RATIO * sqrt(sumI / samples);

        //Reset accumulators
        sumI = 0;
        //--------------------------------------------------------------------------------------

        return Irms;
}

void KCT16SensorRead(int pertime)
{

        // ESP_LOGI(TAG_KCT, "Calculate a Current Irms...");
        sensorCurrent.irms = getIrms(1480);
        /*
            Os valores úteis para usar como parâmetro em calcIrms () são:
            1 ciclo:
            112 para sistemas 50 Hz , ou 93 para sistemas 60 Hz.

            O menor número ‘universal’:
            559 (100 ms, ou 5 ciclos de 50 Hz ou 6 ciclos de 60 Hz).
            O período de monitoramento recomendado:
            1676 (300 ms).
            */

        //Consumed = (Pot[W]/1000) x hours...Sent a packet in a interval of 5 seconds .: kwh = ((Irms * Volts * 5 seconds) / (1000 * 3600))
        // ESP_LOGI(TAG_KCT, "Calculate KWh Consumido...");
        kwhValue = (float)((sensorCurrent.irms * 230.0 * pertime) / (1000.0 * 3600.0));

        // ESP_LOGI(TAG_KCT, "Calculate Custo por KWh Consumido...");
        // costKwh = kwhValue * 0.85; // CPFL Tarif 2020 R$ 0.85
        costKwh = kwhValue * 0.13; // Thai Electricity prices for households, June 2020 (kWh, U.S. Dollar)

        sumKwh += kwhValue;
        sumCost += costKwh;

        sensorCurrent.kwh = sumKwh;
        sensorCurrent.cost = sumCost;

        sensorCurrent.power = sensorCurrent.irms * 230.0;

        ESP_LOGI(TAG_KCT, "Read IRMS Value: %lf (A) | Power Value: %f (watt) | kWh Value: %f (kWh) | Cost per kWh : R$ %f", sensorCurrent.irms, sensorCurrent.power, sensorCurrent.kwh, sensorCurrent.cost);
}

void init_kct16()
{
        adc1_config_width(ADC_WIDTH_KCT16);
        adc1_config_channel_atten(KCT16_SENSOR, atten_kct16);

        //Characterize ADC
        adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit_kct16, atten_kct16, ADC_WIDTH_KCT16, DEFAULT_VREF, adc_chars);
        print_char_val_type(val_type);

        ESP_LOGI(TAG_KCT, "KCT-16 currentCalibration...");
        currentCalibration(76);
}

#endif