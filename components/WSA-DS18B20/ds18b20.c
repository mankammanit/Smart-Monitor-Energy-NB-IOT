
#include "ds18b20.h"

int init = 0;
/// Sends one bit to bus
void ds18b20_send(char bit)
{
    gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DS_GPIO, 0);
    ets_delay_us(5);
    if (bit == 1)
        gpio_set_level(DS_GPIO, 1);
    ets_delay_us(80);
    gpio_set_level(DS_GPIO, 1);
}
// Reads one bit from bus
unsigned char ds18b20_read(void)
{
    unsigned char PRESENCE = 0;
    gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DS_GPIO, 0);
    ets_delay_us(2);
    gpio_set_level(DS_GPIO, 1);
    ets_delay_us(15);
    gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
    if (gpio_get_level(DS_GPIO) == 1)
        PRESENCE = 1;
    else
        PRESENCE = 0;
    return (PRESENCE);
}
// Sends one byte to bus
void ds18b20_send_byte(char data)
{
    unsigned char i;
    unsigned char x;
    for (i = 0; i < 8; i++)
    {
        x = data >> i;
        x &= 0x01;
        ds18b20_send(x);
    }
    ets_delay_us(100);
}
// Reads one byte from bus
unsigned char ds18b20_read_byte(void)
{
    unsigned char i;
    unsigned char data = 0;
    for (i = 0; i < 8; i++)
    {
        if (ds18b20_read())
            data |= 0x01 << i;
        ets_delay_us(15);
    }
    return (data);
}
// Sends reset pulse
unsigned char ds18b20_RST_PULSE(void)
{
    unsigned char PRESENCE;
    gpio_set_direction(DS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DS_GPIO, 0);
    ets_delay_us(500);
    gpio_set_level(DS_GPIO, 1);
    gpio_set_direction(DS_GPIO, GPIO_MODE_INPUT);
    ets_delay_us(30);
    if (gpio_get_level(DS_GPIO) == 0)
        PRESENCE = 1;
    else
        PRESENCE = 0;
    ets_delay_us(470);
    if (gpio_get_level(DS_GPIO) == 1)
        PRESENCE = 1;
    else
        PRESENCE = 0;
    return PRESENCE;
}
// Returns temperature from sensor
float ds18b20_get_temp(void)
{
    if (init == 1)
    {
        unsigned char check;
        char temp1 = 0, temp2 = 0;
        check = ds18b20_RST_PULSE();
        if (check == 1)
        {
            ds18b20_send_byte(0xCC);
            ds18b20_send_byte(0x44);
            vTaskDelay(750 / portTICK_RATE_MS);
            check = ds18b20_RST_PULSE();
            ds18b20_send_byte(0xCC);
            ds18b20_send_byte(0xBE);
            temp1 = ds18b20_read_byte();
            temp2 = ds18b20_read_byte();
            check = ds18b20_RST_PULSE();
            float temp = 0;
            if ((int)temp1 == 255 && (int)temp2 == 255)
            {
                return 0;
            }
            else
            {
                temp = (float)(temp1 + (temp2 * 256)) / 16;
                return temp;
            }
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}
void ds18b20_init()
{
    gpio_pad_select_gpio(DS_GPIO);
    init = 1;
}