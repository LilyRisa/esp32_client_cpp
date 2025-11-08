#include "storage.h"
#include <SPIFFS.h>

void initStorage() {
  if(!SPIFFS.begin(true)) {
    Serial.println("[FS] Mount failed");
  }
}

void saveDeviceCode(String code) {
  File f = SPIFFS.open("/device_code.txt", "w");
  if (!f) return;
  f.print(code);
  f.close();
  Serial.println("[FS] Saved code: " + code);
}

String loadDeviceCode() {
  if (!SPIFFS.exists("/device_code.txt")) return "";
  File f = SPIFFS.open("/device_code.txt", "r");
  String s = f.readString();
  f.close();
  s.trim();
  return s;
}