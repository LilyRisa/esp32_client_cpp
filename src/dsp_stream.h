#ifndef DSP_STREAM_H
#define DSP_STREAM_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <AudioTools.h>

// EQ gồm 10 băng tần (32Hz → 16kHz)

extern AsyncWebServer asyncServer;
extern String deviceCode; // đã được load từ storage
extern bool dspEnabled; // đã được load từ storage

void initDspStream();
void applyDspConfig(JsonArray eq);
void saveDspConfig(JsonArray eq);
void setEqBand(uint8_t band, float gain);
void loadDspConfig();
void loopDspStream();

#endif