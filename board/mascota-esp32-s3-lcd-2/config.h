#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// Mascota IA - Waveshare ESP32-S3-LCD-2 (2", 240x320, ST7789T3)
// + micrófono INMP441 + amplificador MAX98357A + botón para hablar.
// Si cableas un módulo en otro pin, cambia SOLO el número de GPIO aquí.

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Micrófono y altavoz en buses I2S separados (como el montaje "bread-compact")
#define AUDIO_I2S_METHOD_SIMPLEX

// --- Micrófono INMP441 ---
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_7    // INMP441 SCK
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_2    // INMP441 WS
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_17   // INMP441 SD

// --- Amplificador MAX98357A ---
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_13   // MAX98357A BCLK
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_12   // MAX98357A LRC
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_15   // MAX98357A DIN

// --- Botones ---
#define BOOT_BUTTON_GPIO        GPIO_NUM_0    // botón BOOT de la placa
#define TALK_BUTTON_GPIO        GPIO_NUM_11   // botón externo "mantén para hablar" (a GND)

// --- Pantalla (interna de la placa, no tocar) ---
#define DISPLAY_SPI_MOSI_PIN    GPIO_NUM_38
#define DISPLAY_SPI_SCLK_PIN    GPIO_NUM_39
#define DISPLAY_SPI_CS_PIN      GPIO_NUM_45
#define DISPLAY_DC_PIN          GPIO_NUM_42
#define DISPLAY_RST_PIN         GPIO_NUM_NC
#define DISPLAY_BACKLIGHT_PIN   GPIO_NUM_1
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#define DISPLAY_WIDTH           240
#define DISPLAY_HEIGHT          320
#define DISPLAY_MIRROR_X        false
#define DISPLAY_MIRROR_Y        false
#define DISPLAY_SWAP_XY         false
#define DISPLAY_INVERT_COLOR    true
#define DISPLAY_RGB_ORDER       LCD_RGB_ELEMENT_ORDER_RGB
#define DISPLAY_OFFSET_X        0
#define DISPLAY_OFFSET_Y        0
#define DISPLAY_SPI_MODE        0

// --- Sensor de movimiento QMI8658 (interno de la placa) ---
#define IMU_I2C_SDA_PIN         GPIO_NUM_48
#define IMU_I2C_SCL_PIN         GPIO_NUM_47

// --- Medida de batería (interna, GPIO5 = ADC1 canal 4) ---
#define BATTERY_ADC_UNIT        ADC_UNIT_1
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_4
#define BATTERY_DIVIDER_UPPER   200000
#define BATTERY_DIVIDER_LOWER   100000

#endif // _BOARD_CONFIG_H_
