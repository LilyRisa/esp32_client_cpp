#ifndef DSP_MANAGER_H
#define DSP_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// 🎚️ Khởi tạo bộ xử lý DSP EQ
void initDspManager(float sampleRateHz = 44100.0f);

// 🎛️ Cập nhật từng băng EQ (gain dB)
void setEqBand(uint8_t band, float gainDb);

// 🎧 Áp dụng toàn bộ cấu hình EQ từ JSON hoặc mảng
void applyEqFromJson(JsonArray eq);
void applyEqFromArray(const float gains[10]);

// 🧠 Xử lý dữ liệu âm thanh PCM 16-bit (stereo)
void processAudioBufferInt16(int16_t *samples, int len);

// (Tùy chọn) xử lý dữ liệu float (mono hoặc custom)
void processAudioBuffer(float *buf, int len);

#endif