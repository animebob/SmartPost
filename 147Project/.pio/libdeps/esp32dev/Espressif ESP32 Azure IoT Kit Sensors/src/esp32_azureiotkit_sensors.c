/* Copyright (c) Microsoft Corporation.
   Licensed under the MIT License. */

#include "esp32_azureiotkit_sensors.h"

#include <stdio.h>
#include <math.h>

#include "driver/i2c.h"
#include "hts221.h"
#include "bh1750.h"
#include "mpu6050.h"
#include "fbm320.h"
#include "mag3110.h"
#include "iot_ssd1306.h"
#include "oled.h"
#include "led.h"

#define I2C_MASTER_SCL_IO 26        /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 25        /*!< gpio number for I2C master data  */
#define I2C_MASTER_FREQ_HZ 100000   /*!< I2C master clock frequency */

#define BUTTON_IO_NUM  0
#define BUTTON_ACTIVE_LEVEL   0

#define SHAKE_THRESHOLD 2

static i2c_bus_handle_t i2c_bus = NULL;
static hts221_handle_t hts221 = NULL;
static bh1750_handle_t bh1750 = NULL;
static fbm320_handle_t fbm320 = NULL;
static mag3110_handle_t mag3110 = NULL;
static mpu6050_handle_t mpu6050 = NULL;
static ssd1306_handle_t oled = NULL;
static float range_per_digit = 0;

/**
 * @brief i2c master initialization
 */
static void i2c_master_init()
{
    int i2c_master_port = I2C_NUM_0;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.clk_flags = 0;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    i2c_bus = iot_i2c_bus_create(i2c_master_port, &conf);
}

static void init_humiture_sensor()
{
    hts221 = iot_hts221_create(i2c_bus, HTS221_I2C_ADDRESS);
    hts221_config_t hts221_config;
    hts221_config.avg_h = HTS221_AVGH_32;
    hts221_config.avg_t = HTS221_AVGT_16;
    hts221_config.odr = HTS221_ODR_1HZ;
    hts221_config.bdu_status = HTS221_DISABLE;
    hts221_config.heater_status = HTS221_DISABLE;
    /*
     * TODO: investigate the following and uncomment calls to set up hts221.
     * The following calls cause a panic on core 1 of ESP32 module due to call to i2c_master_cmd_begin.
     * However, EspExceptionDecoder was not able to decode the callstack below.
     * Guru Meditation Error: Core  1 panic'ed (StoreProhibited). Exception was unhandled.

     * Core  1 register dump:
     * PC      : 0x4008139e  PS      : 0x00060031  A0      : 0x800814d9  A1      : 0x3ffbedf0  
     * A2      : 0x00000000  A3      : 0x3ffb8b00  A4      : 0x00000000  A5      : 0x00000000  
     * A6      : 0xffffff00  A7      : 0x6001301c  A8      : 0x3ffb8d80  A9      : 0x3ff53000  
     * A10     : 0x00000901  A11     : 0x0000001b  A12     : 0x3ffbdb8c  A13     : 0x00000001  
     * A14     : 0x00000000  A15     : 0x00000800  SAR     : 0x0000001d  EXCCAUSE: 0x0000001d  
     * EXCVADDR: 0x00000008  LBEG    : 0x00000000  LEND    : 0x00000000  LCOUNT  : 0x00000000  


     * Backtrace: 0x4008139b:0x3ffbedf0 0x400814d6:0x3ffbee30 0x400857ad:0x3ffbee60 0x400ed017:0x3ffc26f0 0x400d71d6:0x3ffc2710 0x4008a4b1:0x3ffc2730
     */
    //iot_hts221_set_config(hts221, &hts221_config);
    //iot_hts221_set_activate(hts221);
}

static void init_ambient_light_sensor()
{
    bh1750 = iot_bh1750_create(i2c_bus, BH1750_I2C_ADDRESS);
    bh1750_cmd_measure_t cmd_measure = BH1750_CONTINUE_4LX_RES;
    iot_bh1750_power_on(bh1750);
    iot_bh1750_set_measure_mode(bh1750, cmd_measure);
}

static void init_motion_sensor()
{
    uint8_t range;
    mpu6050 = iot_mpu6050_create(i2c_bus, MPU6050_I2C_ADDRESS);

    mpu6050_init(mpu6050);
    mpu6050_get_full_scale_accel_range(mpu6050, &range);

    switch (range)
    {
    case 0:
        range_per_digit = .000061f;
        break;
    case 1:
        range_per_digit = .000122f;
        break;
    case 2:
        range_per_digit = .000244f;
        break;
    case 3:
        range_per_digit = .0004882f;
        break;
    default:
        range_per_digit = .000061f;
        break;
    }
}

static void init_barometer_sensor()
{
    fbm320 = iot_fbm320_create(i2c_bus, FBM320_I2C_ADDRESS);
    fbm320_init(fbm320);
}

static void init_magnetometer_sensor()
{
    mag3110 = iot_mag3110_create(i2c_bus, MAG3110_I2C_ADDRESS);
    mag3110_start(mag3110);
}

static void init_oled()
{
    oled = iot_ssd1306_create(i2c_bus, SSD1306_I2C_ADDRESS);
    oled_init(oled);
}

void esp32_azureiotkit_initialize_sensors()
{
    i2c_master_init();
    init_humiture_sensor();
    init_ambient_light_sensor();
    init_motion_sensor();
    init_barometer_sensor();
    init_magnetometer_sensor();
    init_oled();
    initialize_leds();
}

void  esp32_azureiotkit_oled_show_message(const uint8_t *message, uint32_t messageLength)
{
    oled_clean(oled);
    oled_show_string(oled, message, messageLength);
}

void  esp32_azureiotkit_oled_clean_screen()
{
    oled_clean(oled);
}

float  esp32_azureiotkit_get_temperature()
{
    int16_t temperature;
    if (hts221 == NULL)
    {
        return 0;
    }
    iot_hts221_get_temperature(hts221, &temperature);

    return (float)temperature / 10;
}

float  esp32_azureiotkit_get_humidity()
{
    int16_t humidity;
    if (hts221 == NULL)
    {
        return 0;
    }
    iot_hts221_get_humidity(hts221, &humidity);

    return (float)humidity / 10;
}

float  esp32_azureiotkit_get_ambientLight()
{

    int ret;
    float bh1750_data;
    if (bh1750 == NULL)
    {
        return 0;
    }

    ret = iot_bh1750_get_data(bh1750, &bh1750_data);
    if (ret != ESP_OK)
    {
        printf("No ack, sensor not connected...\n");
        return 0;
    }
    return bh1750_data;
}

void  esp32_azureiotkit_get_pitch_roll_accel(int *pitch, int *roll, int *accelX, int *accelY, int *accelZ)
{
    mpu6050_acceleration_t result;
    int16_t norm_accel_x;
    int16_t norm_accel_y;
    int16_t norm_accel_z;

    mpu6050_get_acceleration(mpu6050, &result);

    norm_accel_x = result.accel_x * range_per_digit * 9.80665f;
    norm_accel_y = result.accel_y * range_per_digit * 9.80665f;
    norm_accel_z = result.accel_z * range_per_digit * 9.80665f;
    *accelX = norm_accel_x;
    *accelY = norm_accel_y;
    *accelZ = norm_accel_z;
    
    *pitch = -(atan2(norm_accel_x, 
            sqrt(norm_accel_y * norm_accel_y + norm_accel_z * norm_accel_z)) * 180.0) / 3.1415;
    *roll = (atan2(norm_accel_y, norm_accel_z) * 180.0) / 3.1415;
}

void  esp32_azureiotkit_get_pressure_altitude(float *pressure, float *altitude)
{
    int32_t real_p, real_t, abs_alt;

    fbm320_update_data(fbm320);
    fbm320_read_data(fbm320, &real_p, &real_t);

    *pressure = real_p / 1000.0; // convert pa to Kpa
    abs_altitude(fbm320, real_p, &abs_alt);
    *altitude = abs_alt / 1000.0;
}

void  esp32_azureiotkit_get_magnetometer(int *magnetometerX, int *magnetometerY, int *magnetometerZ)
{
    uint16_t x = 0, y = 0, z = 0;
    bool ready;
    mag3110_data_ready(mag3110, &ready);
    if (ready)
    {
        mag3110_read_mag(mag3110, &x, &y, &z);
    }

    *magnetometerX = x;
    *magnetometerY = y;
    *magnetometerZ = z;
}

void  esp32_azureiotkit_led1_set_state(uint32_t ulLedState)
{
    toggle_azure_led(ulLedState);
}

void  esp32_azureiotkit_led2_set_state(uint32_t ulLedState)
{
    toggle_wifi_led(ulLedState);
}
