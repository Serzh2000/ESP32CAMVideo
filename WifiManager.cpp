#include "WifiManager.h"
#include <DNSServer.h>
#include <WebServer.h>
#include "Config.h"

// Globals for Captive Portal (internal to this unit)
DNSServer dnsServer;
WebServer server(80);

void startCaptivePortal(Preferences &prefs) {
  Serial.println("Starting Captive Portal...");
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-CAM-Setup");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Setup DNS to redirect all domains to ESP32
  dnsServer.start(53, "*", WiFi.softAPIP());

  // Setup Web Server
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

  // We need to capture preferences reference in lambda? 
  // Lambdas with captures cannot be converted to function pointers used by WebServer?
  // Actually WebServer.on takes std::function or similar, so capture is fine.
  // But wait, 'prefs' is a reference passed to startCaptivePortal. 
  // It might be unsafe to capture it by reference if startCaptivePortal returns. 
  // BUT startCaptivePortal contains a while(true) loop, so it never returns.
  // So capturing by reference is safe.
  
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

  // Redirect all other requests to captive portal
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("Web server started. Waiting for configuration...");
  
  // Blink loop (Blocking)
  while(true) {
    dnsServer.processNextRequest();
    server.handleClient();
    
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 200) { // Fast blink for AP mode
        lastBlink = millis();
        digitalWrite(LED_GPIO_NUM, !digitalRead(LED_GPIO_NUM));
    }
    yield();
  }
}
