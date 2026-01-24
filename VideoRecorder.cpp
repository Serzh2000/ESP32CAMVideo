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
  
  // Buffer for header
  uint8_t buf[avi_header_size];

  // Write Header Placeholder
  prepare_avi_header_buffer(buf, 320, 240, fps); // Assuming QVGA 320x240
  file.write(buf, avi_header_size);
  
  // Disable WiFi to prevent Brownouts/Crashes during heavy SD writing + Camera usage
  Serial.println("Disabling WiFi for recording stability...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  Serial.printf("Free Heap: %u\n", ESP.getFreeHeap());
  
  unsigned long startTime = millis();
  int frames = 0;
  int frames_size = 0;
  unsigned long lastFrameTime = 0;
  int interval = 1000 / fps;
  
  // Index for seeking
  std::vector<AviIndexEntry> idx;
  idx.reserve(recordDuration * fps); // Pre-allocate
  uint32_t current_movi_offset = 4; // Start after "movi" tag
  
  // LED Status
  unsigned long lastBlink = 0;
  bool ledState = false;
  
  // Recording Loop
  while ((millis() - startTime) < (recordDuration * 1000)) {
    unsigned long now = millis();
    
    // Blink LED
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
      Serial.println("Bad frame, skipping...");
      if(fb) esp_camera_fb_return(fb);
      continue;
    }
    
    // Save length for stats/index before returning fb
    size_t frameLen = fb->len;
    
    // Index
    idx.push_back({current_movi_offset, (uint32_t)frameLen});
    
    // Write Chunk Header
    file.write((uint8_t*)"00dc", 4);
    
    // Calculate Padding
    uint32_t rem = frameLen % 4;
    uint32_t pad = (rem == 0) ? 0 : 4 - rem;
    uint32_t totalLen = frameLen + pad;
    
    print_quartet(totalLen, file);
    
    // Write Data
    file.write(fb->buf, frameLen);
    
    // Write Padding
    for (int i=0; i<pad; i++) file.write(0);
    
    esp_camera_fb_return(fb);
    
    frames++;
    frames_size += (8 + totalLen);
    current_movi_offset += (8 + totalLen);
    
    // Check Size
    if (file.size() > (MAX_FILE_SIZE_MB * 1024 * 1024)) {
      Serial.println("Stopping: Max file size reached");
      break;
    }
    
    if (frames % 50 == 0) {
      file.flush(); // Ensure data is written and size updated
      Serial.printf("Rec: %d frames | %d s | %.2f MB | Heap: %u | Last Frame: %u B\n", 
        frames, (millis()-startTime)/1000, file.size()/1024.0/1024.0, ESP.getFreeHeap(), frameLen);
    }
    yield();
  }
  
  // Finish AVI
  digitalWrite(LED_GPIO_NUM, HIGH); // Turn off LED
  
  // Re-enable WiFi
  Serial.println("Recording finished. Reconnecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long wifiWait = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiWait < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Reconnected.");
  client.setInsecure(); // Refresh SSL context (global client from TelegramManager)
  
  if (file.size() < avi_header_size) {
    logToBot("Ошибка: Файл слишком мал (" + String(file.size()) + " байт). Запись не удалась.");
    file.close();
    return "";
  }
  
  unsigned long duration = (millis() - startTime) / 1000;
  float actual_fps = (duration > 0) ? (float)frames / duration : fps;
  
  String stats = "Готово. F:" + String(frames) + " T:" + String(duration) + "с FPS:" + String(actual_fps, 1) + " Размер:" + String(file.size()/1024.0/1024.0, 2) + "MB";
  logToBot(stats);
  
  // Write Index (idx1)
  file.write((uint8_t*)"idx1", 4);
  uint32_t idx_size = idx.size() * 16;
  print_quartet(idx_size, file);
  
  for (const auto& entry : idx) {
    file.write((uint8_t*)"00dc", 4);
    print_quartet(16, file); // Flags: 0x10 (AVIIF_KEYFRAME)
    print_quartet(entry.offset, file);
    print_quartet(entry.size, file);
  }
  
  // Patch Header
  write_avi_header(file, buf, frames, 320, 240, actual_fps, frames_size);
  
  file.close();
  return filename;
}
