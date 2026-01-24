#ifndef TELEGRAM_MANAGER_H
#define TELEGRAM_MANAGER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Preferences.h>
#include "esp_camera.h"

// Expose these so other modules can use them if needed
extern WiFiClientSecure client;
extern UniversalTelegramBot bot;
extern String chatId;

void logToBot(String msg);
bool sendVideoToTelegram(String filename);
void handleNewMessages(int numNewMessages, bool &isRecordingActive, int &recordDuration, int &fps, int &jpegQuality, framesize_t &frameSize, Preferences &prefs);
String getKeyboard();

#endif
