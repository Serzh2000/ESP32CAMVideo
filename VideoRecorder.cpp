#include "VideoRecorder.h"
#include "Config.h"
#include "AviUtils.h"
#include "TelegramManager.h"
#include "SD_MMC.h"
#include <WiFi.h>
#include <vector>

String recordVideo(int recordDuration, int fps, String ssid, String password) {
  logToBot("Начало цикла записи...");
  
  String filename = "/video" + String(millis()) + ".avi";
  
  if (SD_MMC.exists(filename)) {
    SD_MMC.remove(filename);
  }
  
  File file = SD_MMC.open(filename, FILE_WRITE);
  if (!file) {
    logToBot("Ошибка: Не удалось открыть файл для записи");
    return "";
  }
  
  // Буфер для заголовка
  uint8_t buf[avi_header_size];

  // Запись пустого заголовка (будет обновлен позже)
  prepare_avi_header_buffer(buf, 320, 240, fps); // Предполагаем QVGA 320x240
  file.write(buf, avi_header_size);
  
  // Примечание: Мы оставляем WiFi включенным для получения команд остановки
  // ВНИМАНИЕ: Это увеличивает потребление энергии. Требуется хорошее питание.
  // WiFi.disconnect(true);
  // WiFi.mode(WIFI_OFF);
  
  Serial.printf("Free Heap: %u\n", ESP.getFreeHeap());
  
  unsigned long startTime = millis();
  int frames = 0;
  int frames_size = 0;
  unsigned long lastFrameTime = 0;
  int interval = 1000 / fps;
  
  // Индекс для перемотки
  std::vector<AviIndexEntry> idx;
  idx.reserve(recordDuration * fps); // Предварительное выделение памяти
  uint32_t current_movi_offset = 4; // Начинаем после тега "movi"
  
  // Состояние светодиода
  unsigned long lastBlink = 0;
  bool ledState = false;
  
  // Таймер проверки команд
  unsigned long lastCmdCheck = 0;
  
  // Цикл записи
  while ((millis() - startTime) < (recordDuration * 1000)) {
    unsigned long now = millis();
    
    // Проверка команды остановки каждые 5 секунд
    // Это вызовет паузу в видео на ~1-2 секунды
    if (now - lastCmdCheck > 5000) {
        lastCmdCheck = now;
        Serial.print("Chk Cmd... ");
        if (checkStopCommand()) {
            Serial.println("Stop command received!");
            break;
        }
        Serial.println("OK");
    }

    // Мигание светодиодом
    if (now - lastBlink > 1000) {
        ledState = !ledState;
        digitalWrite(LED_GPIO_NUM, ledState ? LOW : HIGH); // LOW is ON
        lastBlink = now;
        
        int timeLeft = recordDuration - ((now - startTime) / 1000);
        Serial.printf("Recording... %d s left\n", timeLeft);
    }
    
    if (now - lastFrameTime < interval) {
      continue;
    }
    lastFrameTime = now;
    
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb || fb->len == 0) {
      Serial.println("Битый кадр, пропуск...");
      if(fb) esp_camera_fb_return(fb);
      continue;
    }
    
    // Сохраняем длину кадра для индекса перед возвратом fb
    size_t frameLen = fb->len;
    
    // Индекс
    idx.push_back({current_movi_offset, (uint32_t)frameLen});
    
    // Запись заголовка чанка
    file.write((uint8_t*)"00dc", 4);
    
    // Расчет выравнивания (padding)
    uint32_t rem = frameLen % 4;
    uint32_t pad = (rem == 0) ? 0 : 4 - rem;
    uint32_t totalLen = frameLen + pad;
    
    print_quartet(totalLen, file);
    
    // Запись данных
    file.write(fb->buf, frameLen);
    
    // Запись выравнивания
    for (int i=0; i<pad; i++) file.write(0);
    
    esp_camera_fb_return(fb);
    
    frames++;
    frames_size += (8 + totalLen);
    current_movi_offset += (8 + totalLen);
    
    // Проверка размера
    if (file.size() > (MAX_FILE_SIZE_MB * 1024 * 1024)) {
      Serial.println("Остановка: Достигнут макс. размер файла");
      break;
    }
    
    if (frames % 50 == 0) {
      file.flush(); // Ensure data is written and size updated
      Serial.printf("Rec: %d frames | %d s | %.2f MB | Heap: %u | Last Frame: %u B\n", 
        frames, (millis()-startTime)/1000, file.size()/1024.0/1024.0, ESP.getFreeHeap(), frameLen);
    }
    yield();
  }
  
  // Завершение AVI файла
  digitalWrite(LED_GPIO_NUM, HIGH); // Выключить LED
  
  // Переподключение WiFi если нужно (он должен быть включен)
  if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi потерян. Переподключение...");
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid.c_str(), password.c_str());
      unsigned long wifiWait = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - wifiWait < 20000) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("\nWiFi подключен.");
  }
  
  client.setInsecure(); // Обновление SSL контекста
  
  if (file.size() < avi_header_size) {
    logToBot("Ошибка: Файл слишком мал (" + String(file.size()) + " байт). Запись не удалась.");
    file.close();
    return "";
  }
  
  unsigned long duration = (millis() - startTime) / 1000;
  float actual_fps = (duration > 0) ? (float)frames / duration : fps;
  
  String stats = "Готово. F:" + String(frames) + " T:" + String(duration) + "с FPS:" + String(actual_fps, 1) + " Размер:" + String(file.size()/1024.0/1024.0, 2) + "MB";
  logToBot(stats);
  
  // Запись индекса (idx1)
  file.write((uint8_t*)"idx1", 4);
  uint32_t idx_size = idx.size() * 16;
  print_quartet(idx_size, file);
  
  for (const auto& entry : idx) {
    file.write((uint8_t*)"00dc", 4);
    print_quartet(16, file); // Flags: 0x10 (AVIIF_KEYFRAME)
    print_quartet(entry.offset, file);
    print_quartet(entry.size, file);
  }
  
  // Обновление заголовка
  write_avi_header(file, buf, frames, 320, 240, actual_fps, frames_size);
  
  file.close();
  return filename;
}
