#include <Arduino.h>
#include "wifi_setup.h"
#include "web_server.h"
#include "device_api.h"
#include "storage.h"
#include "bluetooth_manager.h"
#include "audio_source_manager.h"
#include "dsp_stream.h"

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
  initDspStream();

}

void loop() {
  // Serial.printf("Free heap: %u, min heap: %u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
  handleWiFiEvents();    // xử lý tiến trình kết nối Wi-Fi
  handleWebServer();     // phục vụ web request
  loopDspStream(); 

}