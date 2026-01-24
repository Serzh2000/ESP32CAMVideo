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
// DEFAULTS
// ==========================================
#define DEFAULT_RECORD_DURATION 300 // 5 minutes
#define DEFAULT_FPS 10
#define DEFAULT_JPEG_QUALITY 12
#define DEFAULT_FRAME_SIZE FRAMESIZE_QVGA
#define MAX_FILE_SIZE_MB 45 // Leave room for 50MB limit

// ==========================================
// CREDENTIALS
// ==========================================
// Define Bot Token here to ensure visibility across modules
#define BOT_TOKEN "7952780114:AAHhgbLQ8o3CAIOdAZ2bl0WR4c28Ie2wqp8"

#endif
