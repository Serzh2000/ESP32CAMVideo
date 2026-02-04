#ifndef TELEGRAM_MANAGER_H
#define TELEGRAM_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Preferences.h>
#include "esp_camera.h"

// Делаем доступными для других модулей
extern WiFiClientSecure client;
extern UniversalTelegramBot bot;
extern String chatId;

void logToBot(String msg);
bool sendVideoToTelegram(String filename);
void handleNewMessages(int numNewMessages, bool &isRecordingActive, int &recordDuration, int &fps, int &jpegQuality, framesize_t &frameSize, int &flashBrightness, Preferences &prefs);
String getKeyboard();
bool checkStopCommand();

#endif
