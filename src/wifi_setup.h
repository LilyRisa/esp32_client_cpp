#pragma once
#include <Arduino.h>

extern String targetSSID;
extern String targetPASS;

void startWiFiAP();
void startConnectToWiFi(const char* ssid, const char* pass, String email);
void handleWiFiEvents();