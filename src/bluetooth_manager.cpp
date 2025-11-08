#include "bluetooth_manager.h"
#include <SPIFFS.h>

BluetoothA2DPSink a2dp_sink;
String currentBtName = "CongMinhAudio";

void startBluetooth(String name) {
  currentBtName = name;
  a2dp_sink.start(currentBtName.c_str());
  Serial.printf("[BT] Started with name: %s\n", currentBtName.c_str());
}

void stopBluetooth() {
  a2dp_sink.end(true);
  Serial.println("[BT] Bluetooth stopped");
}

void reloadBluetooth(String name) {
  if (name != currentBtName && name.length() > 0) {
    stopBluetooth();
    delay(500);
    startBluetooth(name);
    saveBluetoothName(name);
  }
}

String getBluetoothName() {
  if (!SPIFFS.exists("/bt_name.txt")) return "CongMinhAudio";
  File f = SPIFFS.open("/bt_name.txt", "r");
  String name = f.readString();
  f.close();
  name.trim();
  return name.length() ? name : "CongMinhAudio";
}

void saveBluetoothName(String name) {
  File f = SPIFFS.open("/bt_name.txt", "w");
  if (f) {
    f.print(name);
    f.close();
    Serial.printf("[BT] Saved name: %s\n", name.c_str());
  }
}