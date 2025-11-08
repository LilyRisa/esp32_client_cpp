#pragma once
#include <Arduino.h>
void initStorage();
void saveDeviceCode(String code);
String loadDeviceCode();