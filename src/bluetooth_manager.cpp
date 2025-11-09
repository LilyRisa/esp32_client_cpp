#include "bluetooth_manager.h"
#include <SPIFFS.h>
#include "BluetoothA2DPSink.h"

BluetoothA2DPSink a2dp_sink;
String currentBtName = "CongMinhAudio";

// Chân I2S xuất ra DAC UDA1334A
#define I2S_BCK_IO 26
#define I2S_WS_IO 25
#define I2S_DO_IO 22

void startBluetooth(String name) {
  currentBtName = name;

  // --- Cấu hình I2S ---
  i2s_pin_config_t my_pin_config = {
    .bck_io_num = I2S_BCK_IO,
    .ws_io_num = I2S_WS_IO,
    .data_out_num = I2S_DO_IO,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  
  a2dp_sink.set_pin_config(my_pin_config);

  // --- Khởi động Bluetooth A2DP ---
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