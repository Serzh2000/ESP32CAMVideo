#ifndef CONFIG_H
#define CONFIG_H

#include "esp_camera.h"

// ==========================================
// PIN DEFINITIONS (AI Thinker)
// ==========================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define FLASH_GPIO_NUM     4
#define LED_GPIO_NUM      33 // Red LED on back of board (Inverted logic: LOW = ON)

// ==========================================
// НАСТРОЙКИ ПО УМОЛЧАНИЮ
// ==========================================
#define DEFAULT_RECORD_DURATION 300 // Длительность записи в секундах (300с = 5 мин)
#define DEFAULT_FPS 10              // Кадров в секунду (рекомендуется 10-25)
#define DEFAULT_JPEG_QUALITY 12     // Качество JPEG (10-63, меньше = лучше, но больше файл)
#define DEFAULT_FRAME_SIZE FRAMESIZE_QVGA // Разрешение (QVGA=320x240, VGA=640x480)
#define MAX_FILE_SIZE_MB 45         // Макс размер файла (Telegram лимит 50MB)
#define DEFAULT_FLASH_BRIGHTNESS 0  // Яркость вспышки при старте (0-255)
#define FLASH_LEDC_CHANNEL 2        // Канал PWM для вспышки
#define FLASH_LEDC_FREQ 5000        // Частота PWM
#define FLASH_LEDC_RES 8            // Разрешение PWM

// ==========================================
// УЧЕТНЫЕ ДАННЫЕ
// ==========================================
// Токен вашего Telegram бота (получите у @BotFather)
#define BOT_TOKEN "8061703653:AAF0D_mH6VgStlPhcDDvX0sWpU-ShGnifzw"
#endif
