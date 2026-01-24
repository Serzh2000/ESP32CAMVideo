/*
  ESP32-CAM Telegram Video Recorder (AVI/MJPEG to SD Card)
  Modularized Version
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>
#include "esp_camera.h"
#include "SD_MMC.h"
#include "FS.h"

// Modules
#include "Config.h"
#include "AviUtils.h"
#include "WifiManager.h"
#include "TelegramManager.h"
#include "VideoRecorder.h"

// ==========================================
// GLOBALS & CONFIG
// ==========================================

// Runtime Variables
String ssid = "";
String password = "";

// Recording Settings
int recordDuration = DEFAULT_RECORD_DURATION;
int fps = DEFAULT_FPS;
int jpegQuality = DEFAULT_JPEG_QUALITY;
framesize_t frameSize = DEFAULT_FRAME_SIZE;

// State
bool isRecordingActive = false;
unsigned long lastBotCheckTime = 0;

Preferences preferences;

// ==========================================
// SETUP
// ==========================================
void setup() {
  // Power stabilization delay
  delay(2000);

  Serial.begin(115200);
  Serial.println("\n\n--- ESP32-CAM Video Recorder Starting ---");
  
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, HIGH); // Off
  
  // 1. Initialize Preferences
  preferences.begin("cam_config", false);
  
  // Load saved settings
  String savedSSID = preferences.getString("ssid", "");
  String savedPass = preferences.getString("pass", "");
  if (savedSSID != "") {
    ssid = savedSSID;
    password = savedPass;
    Serial.println("Loaded WiFi creds from preferences");
  }
  
  chatId = preferences.getString("chatId", "");
  if (chatId != "") Serial.println("Loaded ChatID: " + chatId);
  
  recordDuration = preferences.getInt("duration", DEFAULT_RECORD_DURATION);
  fps = preferences.getInt("fps", DEFAULT_FPS);
  
  // 2. Initialize SD Card
  Serial.println("Initializing SD Card...");
  
  // Hardware fix for SD Card stability (Error 0x107)
  // Set pins to known state before driver init
  pinMode(14, INPUT_PULLUP); // CLK
  pinMode(15, INPUT_PULLUP); // CMD
  pinMode(2, INPUT_PULLUP);  // D0
  pinMode(4, OUTPUT);        // Flash LED - Force OFF
  digitalWrite(4, LOW);
  pinMode(12, INPUT_PULLUP); // D2 (Unused in 1-bit)
  pinMode(13, INPUT_PULLUP); // D3 (Unused in 1-bit)

  if (!SD_MMC.begin("/sdcard", true)) { // 1-bit mode
    Serial.println("Ошибка монтирования SD карты (SD Card Mount Failed)");
    Serial.println("Попытка 2 через 1 сек...");
    
    // Retry once with delay
    delay(1000);
    if (!SD_MMC.begin("/sdcard", true)) {
      Serial.println("Критическая ошибка: SD карта не найдена или не читается.");
      Serial.println("1. Проверьте, вставлена ли карта.");
      Serial.println("2. Убедитесь, что формат FAT32.");
      Serial.println("3. Проверьте питание (нужно 5V 2A).");
      Serial.println("Система остановлена.");
      
      while(true) {
        // Fast error blink
        digitalWrite(LED_GPIO_NUM, !digitalRead(LED_GPIO_NUM));
        delay(100);
      }
    }
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD Card attached");
    return;
  }
  Serial.printf("SD Card Size: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
  
  // 3. Initialize Camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = frameSize;
  config.jpeg_quality = jpegQuality;
  config.fb_count = 2; // Double buffering
  
  if (psramFound()) {
    Serial.println("PSRAM found");
    config.fb_count = 2;
  } else {
    Serial.println("PSRAM not found");
    config.fb_count = 1;
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // 4. Connect to WiFi
  if (ssid == "") {
    Serial.println("No WiFi credentials. Starting Captive Portal...");
    startCaptivePortal(preferences);
  }

  Serial.printf("Connecting to %s...\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  unsigned long startAttempt = millis();
  bool wifiConnected = false;
  while(millis() - startAttempt < 20000) { // 20s timeout
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      break;
    }
    delay(500);
    Serial.print(".");
  }
  
  if (wifiConnected) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Secure client setup for Bot
    client.setInsecure();
    
    // Send startup message
    logToBot("Бот запущен. Готов к записи.");
    if (chatId == "") {
      Serial.println("ChatID not set. Send /start to bot.");
    }
  } else {
    Serial.println("\nWiFi Connect Failed. Starting Captive Portal...");
    startCaptivePortal(preferences); // This is blocking and will restart ESP on save
  }
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  unsigned long now = millis();
  
  // 1. Check for Bot Commands
  if (now - lastBotCheckTime > 3000) { // Check every 3s
    lastBotCheckTime = now;
    if (WiFi.status() == WL_CONNECTED) {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      if (numNewMessages > 0) {
        handleNewMessages(numNewMessages, isRecordingActive, recordDuration, fps, jpegQuality, frameSize, preferences);
      }
    }
  }
  
  // 2. Recording Cycle
  if (isRecordingActive) {
    String savedFile = recordVideo(recordDuration, fps, ssid, password);
    
    if (savedFile != "") {
      bool sent = sendVideoToTelegram(savedFile);
      if (sent) {
        logToBot("Видео успешно отправлено.");
        // Optional: Delete file after send
        SD_MMC.remove(savedFile); 
      } else {
        logToBot("Не удалось отправить видео.");
      }
    }
    
    // Loop continues immediately for next recording
  }
  
  yield();
}
