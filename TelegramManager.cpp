#include "TelegramManager.h"
#include "Config.h"
#include "SD_MMC.h"

// Глобальные переменные
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
String chatId = "";

void logToBot(String msg) {
  Serial.println("[LOG] " + msg);
  if (chatId != "") {
    // Механизм повторной попытки
    for (int i = 0; i < 2; i++) {
        // Гарантируем чистое состояние
        client.stop();
        client.setInsecure(); 
        
        if (bot.sendMessage(chatId, msg, "")) {
            return; // Success
        }
        
        if (i == 0) {
            Serial.println("Предупреждение: Не удалось отправить лог. Повтор...");
            delay(1000);
        }
    }
    Serial.println("Ошибка: Не удалось отправить лог после повторных попыток.");
  }
}

String getMainKeyboard() {
  String json = "[";
  json += "[\"▶️ Начать запись\", \"⏹ Остановить\"],";
  json += "[\"⚙ Настройки\", \"ℹ️ Статус\"]";
  json += "]";
  return json;
}

String getSettingsKeyboard() {
  String json = "[";
  json += "[\"⏱ Длительность\", \"🎞 FPS\"],";
  json += "[\"🔦 Фонарик\", \"🔙 Назад\"]";
  json += "]";
  return json;
}

String getDurationKeyboard() {
   String json = "[";
   json += "[\"⏱ 30с\", \"⏱ 5 мин\"],";
   json += "[\"⏱ 15 мин\", \"⏱ 30 мин\"],";
   json += "[\"🔙 Назад\"]";
   json += "]";
   return json;
}

String getFPSKeyboard() {
   String json = "[";
   json += "[\"🎞 10\", \"🎞 15\", \"🎞 20\"],";
   json += "[\"🎞 25\", \"🎞 30\", \"🔙 Назад\"]";
   json += "]";
   return json;
}

String getFlashKeyboard() {
    String json = "[";
    json += "[\"🔦 Выкл\", \"🔦 Слабый\"],";
    json += "[\"🔦 Средний\", \"🔦 Макс\"],";
    json += "[\"🔙 Назад\"]";
    json += "]";
    return json;
}

String getKeyboard() {
    return getMainKeyboard();
}

bool checkStopCommand() {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  // Quick check for messages
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    text.trim();
    if (text == "/stop" || text == "⏹ Остановить") {
      bot.sendMessageWithReplyKeyboard(bot.messages[i].chat_id, "⏹ Остановка записи...", "", getMainKeyboard(), true);
      return true;
    }
  }
  return false;
}

void handleNewMessages(int numNewMessages, bool &isRecordingActive, int &recordDuration, int &fps, int &jpegQuality, framesize_t &frameSize, int &flashBrightness, Preferences &prefs) {
  Serial.printf("Handling %d messages. Free Heap: %d\n", numNewMessages, ESP.getFreeHeap());
  
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    String chat_id = bot.messages[i].chat_id;
    
    text.trim();
    Serial.println("Telegram Msg: [" + text + "] from: " + chat_id);

    // --- Навигация и основные команды ---

    if (text == "/start" || text == "/Start" || text == "❓ Помощь" || text == "🔙 Назад") {
      chatId = chat_id;
      prefs.putString("chatId", chatId);
      isRecordingActive = false; 
      
      String welcome = "🤖 ESP32-CAM Видео Бот\n\n";
      welcome += "Текущие настройки:\n";
      welcome += "⏱ Длительность: " + String(recordDuration) + "с\n";
      welcome += "🎞 FPS: " + String(fps) + "\n";
      welcome += "🔦 Яркость: " + String(map(flashBrightness, 0, 255, 0, 100)) + "%\n";
      
      bot.sendMessageWithReplyKeyboard(chatId, welcome, "", getMainKeyboard(), true);
    }
    else if (text == "/stop" || text == "⏹ Остановить") {
      isRecordingActive = false;
      bot.sendMessageWithReplyKeyboard(chatId, "⏹ Запись остановлена.", "", getMainKeyboard(), true);
    }
    else if (text == "/record" || text == "▶️ Начать запись") {
      isRecordingActive = true;
      bot.sendMessageWithReplyKeyboard(chatId, "▶️ Запись началась...", "", getMainKeyboard(), true);
    }
    else if (text == "/status" || text == "ℹ️ Статус") {
      String stat = "Статус: " + String(isRecordingActive ? "АКТИВЕН" : "ОЖИДАНИЕ") + "\n";
      stat += "FPS: " + String(fps) + "\n";
      stat += "Время: " + String(recordDuration) + "с\n";
      stat += "Свет: " + String(flashBrightness) + "/255\n";
      stat += "SD Free: " + String((SD_MMC.totalBytes() - SD_MMC.usedBytes())/1024/1024) + "MB";
      bot.sendMessageWithReplyKeyboard(chatId, stat, "", getMainKeyboard(), true);
    }
    
    // --- Меню настроек ---
    
    else if (text == "⚙ Настройки") {
        bot.sendMessageWithReplyKeyboard(chatId, "Выберите категорию настроек:", "", getSettingsKeyboard(), true);
    }
    else if (text == "⏱ Длительность") {
        bot.sendMessageWithReplyKeyboard(chatId, "Выберите длительность видео:", "", getDurationKeyboard(), true);
    }
    else if (text == "🎞 FPS") {
        bot.sendMessageWithReplyKeyboard(chatId, "Выберите FPS (кадров в секунду):", "", getFPSKeyboard(), true);
    }
    else if (text == "🔦 Фонарик") {
        bot.sendMessageWithReplyKeyboard(chatId, "Выберите яркость фонарика:", "", getFlashKeyboard(), true);
    }

    // --- Обработчики конкретных настроек ---

    // Duration
    else if (text == "⏱ 30с") {
        recordDuration = 30;
        prefs.putInt("duration", recordDuration);
        bot.sendMessage(chatId, "✅ Длительность: 30 сек");
    }
    else if (text == "⏱ 5 мин") {
        recordDuration = 300;
        prefs.putInt("duration", recordDuration);
        bot.sendMessage(chatId, "✅ Длительность: 5 мин");
    }
    else if (text == "⏱ 15 мин") {
        recordDuration = 900;
        prefs.putInt("duration", recordDuration);
        bot.sendMessage(chatId, "✅ Длительность: 15 мин");
    }
    else if (text == "⏱ 30 мин") {
        recordDuration = 1800;
        prefs.putInt("duration", recordDuration);
        bot.sendMessage(chatId, "✅ Длительность: 30 мин");
    }
    else if (text.startsWith("/duration ")) {
        int val = text.substring(10).toInt();
        if (val >= 30 && val <= 1800) {
            recordDuration = val;
            prefs.putInt("duration", recordDuration);
            bot.sendMessage(chatId, "✅ Длительность установлена: " + String(val) + " сек");
        } else {
            bot.sendMessage(chatId, "⚠️ Ошибка: диапазон 30 - 1800 сек.");
        }
    }

    // FPS
    else if (text.startsWith("🎞 ")) {
        int val = text.substring(3).toInt(); // "🎞 10" -> 10
        if (val >= 10 && val <= 30) {
            fps = val;
            prefs.putInt("fps", fps);
            bot.sendMessage(chatId, "✅ FPS установлен: " + String(val));
        }
    }
    else if (text.startsWith("/fps ")) {
        int val = text.substring(5).toInt();
        if (val >= 10 && val <= 30) {
            fps = val;
            prefs.putInt("fps", fps);
            bot.sendMessage(chatId, "✅ FPS установлен: " + String(val));
        } else {
             bot.sendMessage(chatId, "⚠️ Ошибка: диапазон 10 - 30.");
        }
    }

    // Flashlight
    else if (text == "🔦 Выкл") {
        flashBrightness = 0;
        ledcWrite(FLASH_GPIO_NUM, flashBrightness);
        prefs.putInt("flash", flashBrightness);
        bot.sendMessage(chatId, "✅ Фонарик выключен");
    }
    else if (text == "🔦 Слабый") {
        flashBrightness = 20; // ~8%
        ledcWrite(FLASH_GPIO_NUM, flashBrightness);
        prefs.putInt("flash", flashBrightness);
        bot.sendMessage(chatId, "✅ Фонарик: Слабый");
    }
    else if (text == "🔦 Средний") {
        flashBrightness = 100; // ~40%
        ledcWrite(FLASH_GPIO_NUM, flashBrightness);
        prefs.putInt("flash", flashBrightness);
        bot.sendMessage(chatId, "✅ Фонарик: Средний");
    }
    else if (text == "🔦 Макс") {
        flashBrightness = 255;
        ledcWrite(FLASH_GPIO_NUM, flashBrightness);
        prefs.putInt("flash", flashBrightness);
        bot.sendMessage(chatId, "✅ Фонарик: Максимум");
    }
    else if (text.startsWith("/flash ")) {
        int val = text.substring(7).toInt();
        if (val >= 0 && val <= 255) {
            flashBrightness = val;
            ledcWrite(FLASH_GPIO_NUM, flashBrightness);
            prefs.putInt("flash", flashBrightness);
            bot.sendMessage(chatId, "✅ Яркость: " + String(val));
        } else {
            bot.sendMessage(chatId, "⚠️ 0 - 255");
        }
    }
    
    // --- Числовые команды (Быстрая настройка) ---
    else if (text.toInt() != 0 || text == "0") {
        int val = text.toInt();
        
        // Если число маленькое (10-30), считаем это FPS
        if (val >= 10 && val <= 30) {
             fps = val;
             prefs.putInt("fps", fps);
             bot.sendMessage(chatId, "✅ FPS установлен: " + String(val));
        }
        // Если число побольше (30-1800), считаем это длительностью
        else if (val >= 30 && val <= 1800) {
             recordDuration = val;
             prefs.putInt("duration", recordDuration);
             bot.sendMessage(chatId, "✅ Длительность установлена: " + String(val) + " сек");
        }
        else {
             bot.sendMessage(chatId, "⚠️ Непонятное число.\nFPS: 10-30\nВремя: 30-1800", "");
        }
    }
    
    // Поддержка устаревших команд или запасной вариант
    else {
        // ...
    }
  }
}

bool sendVideoToTelegram(String filename) {
  File file = SD_MMC.open(filename, FILE_READ);
  if (!file) {
    logToBot("Ошибка: Не могу открыть файл для отправки: " + filename);
    return false;
  }
  
  size_t fileSize = file.size();
  
  if (fileSize == 0) {
    logToBot("Ошибка: Файл пуст: " + filename);
    file.close();
    return false;
  }
  
  logToBot("Отправка видео (" + String(fileSize/1024.0/1024.0, 2) + " MB)...");

  // Подготовка HTTP POST запроса
  String start_request = "";
  String end_request = "";
  String boundary = "------------------------ESP32CAMBotBoundary";
  
  start_request += "--" + boundary + "\r\n";
  start_request += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  start_request += chatId + "\r\n";
  start_request += "--" + boundary + "\r\n";
  start_request += "Content-Disposition: form-data; name=\"video\"; filename=\"video.avi\"\r\n";
  start_request += "Content-Type: video/x-msvideo\r\n\r\n";
  
  end_request += "\r\n--" + boundary + "--\r\n";
  
  size_t totalLen = start_request.length() + fileSize + end_request.length();
  
  // Подключение к API
  if (client.connect("api.telegram.org", 443)) {
    client.println("POST /bot" + String(BOT_TOKEN) + "/sendVideo HTTP/1.1");
    client.println("Host: api.telegram.org");
    client.println("Content-Type: multipart/form-data; boundary=" + boundary);
    client.println("Content-Length: " + String(totalLen));
    client.println();
    
    client.print(start_request);
    
    // Потоковая передача файла
    uint8_t *buffer = (uint8_t*)malloc(4096);
    if (!buffer) {
        client.stop(); // Остановить клиент перед логгированием (который может попытаться отправить сообщение)
        logToBot("Ошибка: Не хватает памяти для буфера отправки");
        file.close();
        return false;
    }
    
    size_t sent = 0;
    while (file.available()) {
      size_t read = file.read(buffer, 4096);
      client.write(buffer, read);
      sent += read;
      
      // Опционально: Сброс watchdog или вывод прогресса
      if (sent % (1024*1024) == 0) Serial.print("."); 
    }
    free(buffer);
    
    client.print(end_request);
    
    // Ожидание ответа
    unsigned long wait = millis();
    bool success = false;
    while (client.connected() && millis() - wait < 20000) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        if (line.indexOf("\"ok\":true") != -1) {
          success = true;
        }
        // Serial.println(line); // Debug response
      }
    }
    client.stop();
    file.close();
    
    return success;
  } else {
    logToBot("Ошибка: Не удалось подключиться к api.telegram.org");
    file.close();
    return false;
  }
}
