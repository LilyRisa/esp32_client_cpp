#include "web_server.h"
#include <WebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include "wifi_setup.h"
#include "storage.h"
#include "state_manager.h"

WebServer server(80);
DNSServer dnsServer;
#define DNS_PORT 53
 

int getWifiBars(int rssi);

void initWebServer()
{
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 1, 100));

  server.on("/", HTTP_GET, []()
            {
    File f = SPIFFS.open("/index.html", "r");
    server.streamFile(f, "text/html");
    f.close(); });

  server.serveStatic("/style.css", SPIFFS, "/style.css");
  server.serveStatic("/app.js", SPIFFS, "/app.js");

  server.on("/", HTTP_GET, []()
            {
  File f = SPIFFS.open("/index.html", "r");
  server.streamFile(f, "text/html");
  f.close(); });

  server.serveStatic("/style.css", SPIFFS, "/style.css");
  server.serveStatic("/app.js", SPIFFS, "/app.js");

  // Scan Wi-Fi
  server.on("/scan", HTTP_GET, [](){
    Serial.println("[WiFi] Scanning for networks...");

      // 🧹 Reset driver trước khi quét để tránh lỗi 0 kết quả
      WiFi.disconnect(true);
      delay(100);
      WiFi.mode(WIFI_AP_STA);
      delay(100);

      int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true

      if (n == 0) {
        server.send(200, "application/json", "[]");
        Serial.println("[WiFi] No networks found");
        return;
      }
    String json = "[";
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      if (ssid == "WIFI_SETUP_CONGMINHAUDIO" || ssid == "") continue;
      int rssi = WiFi.RSSI();
      int bars = getWifiBars(rssi);
      json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(rssi) + ",\"avg\":" + bars +"}";
      if (i < n - 1) json += ",";
    }
    json += "]";
    server.send(200, "application/json", json);
    WiFi.scanDelete();
  });

  // ✅ Lưu SSID/PASS/EMAIL rồi bắt đầu kết nối
  server.on("/start_connect", HTTP_POST, []()
            {
  if (server.hasArg("ssid") && server.hasArg("pass") && server.hasArg("email")) {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String email = server.arg("email");

    // Bắt đầu tiến trình kết nối Wi-Fi và tạo code
    connectStart = millis();
    startConnectToWiFi(ssid.c_str(), pass.c_str(), email);
    server.send(200, "application/json", "{\"ok\":1}");
  } else {
    server.send(400, "application/json", "{\"error\":\"missing_params\"}");
  } });

  // Trạng thái tiến trình
  server.on("/status", HTTP_GET, []()
{
            String code = loadDeviceCode();
            String stateStr;

            // ⚙️ Kiểm tra trạng thái thực tế của Wi-Fi
            wl_status_t wifiStatus = WiFi.status();

            if (connState == CONNECTING) {
              if (wifiStatus == WL_CONNECTED) {
                connState = CONNECTED;
              } else if (millis() - connectStart > CONNECT_MAX_MS) {
                connState = FAILED;
              }
            }

            // ⚙️ Xác định chuỗi trạng thái để trả về
            switch (connState) {
              case IDLE:        stateStr = "idle"; break;
              case CONNECTING:  stateStr = "connecting"; break;
              case CONNECTED:
                // Kiểm tra thật sự có Wi-Fi không
                stateStr = (wifiStatus == WL_CONNECTED) ? "connected" : "failed";
                break;
              case FAILED:      stateStr = "failed"; break;
              default:          stateStr = "unknown"; break;
            }

            // ✅ Nếu đang connected nhưng mất Wi-Fi -> failed
            if (connState == CONNECTED && wifiStatus != WL_CONNECTED) {
              connState = FAILED;
              stateStr = "failed";
            }

            // Giới hạn progress
            int safeProgress = constrain(connectProgress, 0, 100);

            // Code thiết bị
            if (code.isEmpty()) code = "";

            String json = "{";
            json += "\"state\":\"" + stateStr + "\",";
            json += "\"progress\":" + String(safeProgress) + ",";
            json += "\"wifi_status\":" + String((int)wifiStatus) + ",";
            json += "\"device_code\":\"" + code + "\"";
            json += "}";

            server.send(200, "application/json", json); 
        });

  server.begin();
  Serial.println("[HTTP] WebServer started");
}

int getWifiBars(int rssi)
{
  if (rssi >= -55)
    return 4; // 📶 Rất mạnh
  else if (rssi >= -67)
    return 3; // 📶 Mạnh
  else if (rssi >= -80)
    return 2; // 📶 Trung bình
  else if (rssi >= -90)
    return 1; // 📶 Yếu
  else
    return 0; // ❌ Mất sóng
}

void handleWebServer()
{
  dnsServer.processNextRequest();
  server.handleClient();
}