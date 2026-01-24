#include "TelegramManager.h"
#include "Config.h"
#include "SD_MMC.h"

// Define globals
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
String chatId = "";

void logToBot(String msg) {
  Serial.println("[LOG] " + msg);
  if (chatId != "") {
    // Only send important logs to bot to avoid rate limiting
    // For debug request, we send more than usual
    bot.sendMessage(chatId, msg, "");
  }
}

String getKeyboard() {
  // JSON for Reply Keyboard
  String json = "{";
  json += "\"keyboard\":[";
  json += "[\"▶️ Начать запись\", \"⏹ Остановить\"],";
  json += "[\"ℹ️ Статус\", \"❓ Помощь\"],";
  json += "[\"⏱ 30с\", \"⏱ 300с\"],";
  json += "[\"🎞 10 FPS\", \"🎞 25 FPS\"]";
  json += "],";
  json += "\"resize_keyboard\":true";
  json += "}";
  return json;
}

void handleNewMessages(int numNewMessages, bool &isRecordingActive, int &recordDuration, int &fps, int &jpegQuality, framesize_t &frameSize, Preferences &prefs) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    String chat_id = bot.messages[i].chat_id;
    
    Serial.println("Msg: " + text);

    if (text == "/start" || text == "❓ Помощь") {
      chatId = chat_id;
      prefs.putString("chatId", chatId);
      // Don't auto-start recording on /start
      isRecordingActive = false; 
      
      String welcome = "🤖 *ESP32-CAM Видео Бот*\n\n";
      welcome += "Текущие настройки:\n";
      welcome += "⏱ Длительность: " + String(recordDuration) + "с\n";
      welcome += "🎞 FPS: " + String(fps) + "\n";
      welcome += "🎨 Качество: " + String(jpegQuality) + "\n";
      welcome += "📐 Размер: " + String(frameSize) + "\n";
      
      welcome += "\nИспользуйте кнопки меню для управления.";
      
      bot.sendMessageWithReplyKeyboard(chatId, welcome, "Markdown", getKeyboard(), true);
    }
    else if (text == "/stop" || text == "⏹ Остановить") {
      isRecordingActive = false;
      bot.sendMessageWithReplyKeyboard(chatId, "⏹ Цикл записи остановлен. Жду команд.", "", getKeyboard(), true);
    }
    else if (text == "/record" || text == "▶️ Начать запись") {
      isRecordingActive = true;
      bot.sendMessageWithReplyKeyboard(chatId, "▶️ Запуск цикла записи...", "", getKeyboard(), true);
    }
    else if (text == "/status" || text == "ℹ️ Статус") {
      String stat = "Статус: " + String(isRecordingActive ? "АКТИВЕН" : "ОЖИДАНИЕ") + "\n";
      stat += "FPS: " + String(fps) + "\n";
      stat += "Длительность: " + String(recordDuration) + "с\n";
      stat += "Качество: " + String(jpegQuality) + "\n";
      stat += "Heap: " + String(ESP.getFreeHeap()) + "\n";
      stat += "SD Использовано: " + String(SD_MMC.usedBytes()/1024/1024) + "MB";
      bot.sendMessageWithReplyKeyboard(chatId, stat, "", getKeyboard(), true);
    }
    // --- Presets ---
    else if (text == "⏱ 30с") {
      recordDuration = 30;
      prefs.putInt("duration", recordDuration);
      bot.sendMessage(chatId, "Длительность установлена: 30с");
    }
    else if (text == "⏱ 300с") {
      recordDuration = 300;
      prefs.putInt("duration", recordDuration);
      bot.sendMessage(chatId, "Длительность установлена: 300с (5 мин)");
    }
    else if (text == "🎞 10 FPS") {
      fps = 10;
      prefs.putInt("fps", fps);
      bot.sendMessage(chatId, "FPS установлен: 10");
    }
    else if (text == "🎞 25 FPS") {
      fps = 25;
      prefs.putInt("fps", fps);
      bot.sendMessage(chatId, "FPS установлен: 25");
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

  // Prepare HTTP POST
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
  
  // Connect to API
  if (client.connect("api.telegram.org", 443)) {
    client.println("POST /bot" + String(BOT_TOKEN) + "/sendVideo HTTP/1.1");
    client.println("Host: api.telegram.org");
    client.println("Content-Type: multipart/form-data; boundary=" + boundary);
    client.println("Content-Length: " + String(totalLen));
    client.println();
    
    client.print(start_request);
    
    // Stream file
    uint8_t *buffer = (uint8_t*)malloc(4096);
    if (!buffer) {
        logToBot("Ошибка: Не хватает памяти для буфера отправки");
        file.close();
        return false;
    }
    
    size_t sent = 0;
    while (file.available()) {
      size_t read = file.read(buffer, 4096);
      client.write(buffer, read);
      sent += read;
      
      // Optional: Feed watchdog or print progress
      if (sent % (1024*1024) == 0) Serial.print("."); 
    }
    free(buffer);
    
    client.print(end_request);
    
    // Wait for response
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
