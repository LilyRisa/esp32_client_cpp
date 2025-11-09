#include <Arduino.h>
#include "wifi_setup.h"
#include "web_server.h"
#include "device_api.h"
#include "storage.h"
#include "bluetooth_manager.h"
#include "audio_source_manager.h"

enum ConnState {
  IDLE,
  CONNECTING,
  CONNECTED,
  FAILED
};

void setup() {
  Serial.begin(115200);
  delay(200);

  initStorage();         // SPIFFS
  startWiFiAP();         // Tạo Access Point
  initWebServer();       // WebServer + route
  // startBluetooth("Congminhaudio");
  initAudioManager();

}

void loop() {
  handleWiFiEvents();    // xử lý tiến trình kết nối Wi-Fi
  handleWebServer();     // phục vụ web request

}