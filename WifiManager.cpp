#include "WifiManager.h"
#include <DNSServer.h>
#include <WebServer.h>
#include "Config.h"

// Глобальные переменные для Captive Portal (внутренние для этого модуля)
DNSServer dnsServer;
WebServer server(80);

void startCaptivePortal(Preferences &prefs) {
  Serial.println("Запуск Captive Portal...");
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-CAM-Setup");
  Serial.print("IP адрес точки доступа: ");
  Serial.println(WiFi.softAPIP());

  // Настройка DNS для перенаправления всех доменов на ESP32
  dnsServer.start(53, "*", WiFi.softAPIP());

  // Настройка веб-сервера
  server.on("/", HTTP_GET, []() {
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=no'>";
    html += "<meta charset='UTF-8'>";
    html += "<title>ESP32-CAM Настройка WiFi</title>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;}";
    html += "input{padding:10px;margin:10px;width:90%;font-size:16px;}";
    html += "input[type='submit']{background:#007bff;color:white;border:none;cursor:pointer;}";
    html += "</style></head><body>";
    html += "<h2>Настройка WiFi</h2>";
    html += "<form action='/save' method='POST'>";
    html += "<input type='text' name='s' placeholder='Имя сети (SSID)' required><br>";
    html += "<input type='password' name='p' placeholder='Пароль'><br>";
    html += "<input type='submit' value='Сохранить и подключиться'>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
  });

  // Обработчик сохранения настроек
  // Захватываем ссылку на prefs (безопасно, так как startCaptivePortal не возвращает управление)
  server.on("/save", HTTP_POST, [&prefs]() {
    String s = server.arg("s");
    String p = server.arg("p");
    
    if (s.length() > 0) {
      prefs.putString("ssid", s);
      prefs.putString("pass", p);
      
      String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=no'><meta charset='UTF-8'></head><body>";
      html += "<h2>Сохранено!</h2><p>Перезагрузка...</p></body></html>";
      server.send(200, "text/html", html);
      
      delay(2000);
      ESP.restart();
    } else {
      server.send(200, "text/html", "Ошибка: Требуется имя сети. <a href='/'>Назад</a>");
    }
  });

  // Перенаправление всех остальных запросов на главную страницу (Captive Portal)
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("Веб-сервер запущен. Ожидание настройки...");
  
  // Бесконечный цикл обработки (Блокирующий)
  while(true) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 200) { // Быстрое мигание в режиме точки доступа
        lastBlink = millis();
        digitalWrite(LED_GPIO_NUM, !digitalRead(LED_GPIO_NUM));
    }
    yield();
  }
}
