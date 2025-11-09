#include "audio_source_manager.h"
#include "bluetooth_manager.h"

AudioSourceCM currentSource = NONE;

void initAudioManager() {
  Serial.println("[AudioManager] Initializing...");
  String btName = getBluetoothName();
  startBluetooth(btName); // mặc định khởi động Bluetooth
  currentSource = BLUETOOTH;
}

void switchSource(AudioSourceCM src) {
  if (src == currentSource) return;

  // Dừng nguồn hiện tại
  if (currentSource == BLUETOOTH) {
    stopBluetooth();
  }
  // if (currentSource == SOURCE_WIFI_STREAM) stopWifiStream(); // để trống cho tương lai

  // Bật nguồn mới
  if (src == BLUETOOTH) {
    String btName = getBluetoothName();
    startBluetooth(btName);
  } else if (src == WIFI_STREAM) {
    // startWifiStream(); // placeholder
    Serial.println("[AudioManager] Wi-Fi streaming not implemented yet");
  }

  currentSource = src;
  Serial.printf("[AudioManager] Switched source to %d\n", src);
}

AudioSourceCM getCurrentSource() {
  return currentSource;
}