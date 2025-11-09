#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <Arduino.h>
#include "BluetoothA2DPSink.h"

extern BluetoothA2DPSink a2dp_sink;
void startBluetooth(String name);
void stopBluetooth();
void reloadBluetooth(String name);
String getBluetoothName();
void saveBluetoothName(String name);

#endif