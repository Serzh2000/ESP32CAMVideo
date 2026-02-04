/*
  ESP32-CAM Telegram Video Recorder
  Модульная версия
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

// Подключаем наши модули
#include "Config.h"
#include "AviUtils.h"
#include "WifiManager.h"
#include "TelegramManager.h"
#include "VideoRecorder.h"

// ==========================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ И НАСТРОЙКИ
// ==========================================

// Переменные времени выполнения
String ssid = "";
String password = "";

// Настройки записи (загружаются из памяти)
int recordDuration = DEFAULT_RECORD_DURATION;
int fps = DEFAULT_FPS;
int jpegQuality = DEFAULT_JPEG_QUALITY;
framesize_t frameSize = DEFAULT_FRAME_SIZE;
int flashBrightness = DEFAULT_FLASH_BRIGHTNESS;

// Состояние
bool isRecordingActive = false; // Активна ли циклическая запись
unsigned long lastBotCheckTime = 0; // Время последней проверки сообщений

Preferences preferences; // Объект для сохранения настроек в энергонезависимую память

// ==========================================
// НАСТРОЙКА (SETUP)
// ==========================================
void setup() {
  // Задержка для стабилизации питания при старте
  delay(2000);

  Serial.begin(115200);
  Serial.println("\n\n--- ESP32-CAM Video Recorder Запуск ---");
  
  // Настройка красного светодиода (индикатор работы)
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, HIGH); // Выкл (инвертированная логика)
  
  // 1. Инициализация хранилища настроек (Preferences)
  preferences.begin("cam_config", false);
  
  // Загрузка сохраненных настроек WiFi
  String savedSSID = preferences.getString("ssid", "");
  String savedPass = preferences.getString("pass", "");
  if (savedSSID != "") {
    ssid = savedSSID;
    password = savedPass;
    Serial.println("Загружены настройки WiFi из памяти");
  }
  
  // Загрузка ID чата и параметров записи
  chatId = preferences.getString("chatId", "");
  if (chatId != "") Serial.println("Загружен ChatID: " + chatId);
  
  recordDuration = preferences.getInt("duration", DEFAULT_RECORD_DURATION);
  fps = preferences.getInt("fps", DEFAULT_FPS);
  flashBrightness = preferences.getInt("flash", DEFAULT_FLASH_BRIGHTNESS);
  
  // 2. Инициализация SD карты
  Serial.println("Инициализация SD карты...");
  
  // Аппаратный фикс для стабильности SD карты (предотвращает ошибку 0x107)
  // Устанавливаем пины в известное состояние перед инициализацией драйвера
  pinMode(14, INPUT_PULLUP); // CLK
  pinMode(15, INPUT_PULLUP); // CMD
  pinMode(2, INPUT_PULLUP);  // D0
  pinMode(4, OUTPUT);        // Flash LED - Принудительно выключаем
  digitalWrite(4, LOW);
  pinMode(12, INPUT_PULLUP); // D2 (Не используется в 1-битном режиме)
  pinMode(13, INPUT_PULLUP); // D3 (Не используется в 1-битном режиме)

  if (!SD_MMC.begin("/sdcard", true)) { // true = 1-битный режим (освобождает пины для вспышки)
    Serial.println("Ошибка монтирования SD карты (SD Card Mount Failed)");
    Serial.println("Попытка 2 через 1 сек...");
    
    // Повторная попытка через секунду
    delay(1000);
    if (!SD_MMC.begin("/sdcard", true)) {
      Serial.println("Критическая ошибка: SD карта не найдена или не читается.");
      Serial.println("1. Проверьте, вставлена ли карта.");
      Serial.println("2. Убедитесь, что формат FAT32.");
      Serial.println("3. Проверьте питание (нужно 5V 2A).");
      Serial.println("Система остановлена.");
      
      while(true) {
        // Быстрое мигание при ошибке
        digitalWrite(LED_GPIO_NUM, !digitalRead(LED_GPIO_NUM));
        delay(100);
      }
    }
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("SD карта не вставлена");
    return;
  }
  Serial.printf("Размер SD карты: %lluMB\n", SD_MMC.cardSize() / (1024 * 1024));
  
  // 3. Инициализация Камеры
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
  config.fb_count = 2; // Двойная буферизация для плавности
  
  // Проверка наличия PSRAM (доп. память)
  if (psramFound()) {
    Serial.println("PSRAM найдена (это хорошо)");
    config.fb_count = 2;
  } else {
    Serial.println("PSRAM не найдена (будет тормозить)");
    config.fb_count = 1;
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Ошибка инициализации камеры: 0x%x", err);
    return;
  }
  
  // 4. Подключение к WiFi
  if (ssid == "") {
    Serial.println("Нет сохраненных настроек WiFi. Запуск режима точки доступа (Captive Portal)...");
    startCaptivePortal(preferences); // Это заблокирует выполнение до сохранения настроек
  }

  Serial.printf("Подключение к WiFi: %s...\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  unsigned long startAttempt = millis();
  bool wifiConnected = false;
  // Ждем подключения 20 секунд
  while(millis() - startAttempt < 20000) { 
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      break;
    }
    delay(500);
    Serial.print(".");
  }
  
  if (wifiConnected) {
    Serial.println("\nWiFi подключен успешно");
    Serial.print("IP адрес: ");
    Serial.println(WiFi.localIP());
    
    // Настройка безопасного клиента для Telegram (без проверки сертификата)
    client.setInsecure();
    client.setHandshakeTimeout(30000); // Тайм-аут рукопожатия 30 сек
    
    // Ждем немного для стабилизации сети
    delay(2000);
    
    // Настройка PWM для фонарика
    // ESP32 Arduino Core v3.0+ использует ledcAttach(pin, freq, res)
    ledcAttach(FLASH_GPIO_NUM, FLASH_LEDC_FREQ, FLASH_LEDC_RES);
    ledcWrite(FLASH_GPIO_NUM, flashBrightness);

    // Отправка приветственного сообщения
    logToBot("Бот запущен. Готов к работе.");
    if (chatId == "") {
      Serial.println("ChatID не установлен. Отправьте /start боту.");
    }
  } else {
    Serial.println("\nНе удалось подключиться к WiFi. Запуск режима настройки (Captive Portal)...");
    startCaptivePortal(preferences); // Запуск точки доступа для ввода новых данных
  }
}

// ==========================================
// ГЛАВНЫЙ ЦИКЛ (LOOP)
// ==========================================
void loop() {
  unsigned long now = millis();
  
  // 1. Проверка команд от Бота
  if (now - lastBotCheckTime > 3000) { // Проверяем каждые 3 секунды
    lastBotCheckTime = now;
    if (WiFi.status() == WL_CONNECTED) {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      if (numNewMessages > 0) {
        // Обработка входящих сообщений
        handleNewMessages(numNewMessages, isRecordingActive, recordDuration, fps, jpegQuality, frameSize, flashBrightness, preferences);
      }
    }
  }
  
  // 2. Цикл записи видео
  if (isRecordingActive) {
    // Запись видео файла
    String savedFile = recordVideo(recordDuration, fps, ssid, password);
    
    if (savedFile != "") {
      // Отправка в Telegram
      bool sent = sendVideoToTelegram(savedFile);
      if (sent) {
        logToBot("Видео успешно отправлено.");
        // Удаляем файл с карты памяти после успешной отправки, чтобы не забивать место
        SD_MMC.remove(savedFile); 
      } else {
        logToBot("Не удалось отправить видео.");
      }
    }
    
    // Цикл продолжается сразу же (loop() перезапустится)
  }
  
  yield(); // Даем время системным процессам WiFi
}
