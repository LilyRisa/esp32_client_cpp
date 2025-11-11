#include "dsp_manager.h"
#include <Arduino.h>
#include <math.h>
#include <ArduinoJson.h>
#include "dsp_stream.h"

#define NUM_BANDS 10

// ⚙️ Cấu hình EQ
static const float FREQS[NUM_BANDS] = {32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
static const float Q = 1.0f;
static float sampleRate = 44100.0f;

// 🎛️ Bộ nhớ cho EQ
static float coeffs[NUM_BANDS][5];     // hệ số filter (b0,b1,b2,a1,a2)
static float stateL[NUM_BANDS][4];     // trạng thái từng băng cho kênh trái (x1,x2,y1,y2)
static float stateR[NUM_BANDS][4];     // trạng thái từng băng cho kênh phải
static float eqGain[NUM_BANDS];        // gain hiện tại từng band

// 🎧 Sinh hệ số biquad dạng Parametric EQ (RBJ Cookbook)
static void genPeq(float *c, float freq, float gainDb) {
    float A = powf(10.0f, gainDb / 40.0f);
    float w0 = 2.0f * M_PI * freq / sampleRate;
    float alpha = sinf(w0) / (2.0f * Q);
    float cosw0 = cosf(w0);

    float b0 = 1 + alpha * A;
    float b1 = -2 * cosw0;
    float b2 = 1 - alpha * A;
    float a0 = 1 + alpha / A;
    float a1 = -2 * cosw0;
    float a2 = 1 - alpha / A;

    c[0] = b0 / a0;
    c[1] = b1 / a0;
    c[2] = b2 / a0;
    c[3] = a1 / a0;
    c[4] = a2 / a0;
}

// 🔧 Khởi tạo EQ
void initDspManager(float sampleRateHz) {
    sampleRate = sampleRateHz;
    for (int i = 0; i < NUM_BANDS; i++) {
        eqGain[i] = 0;
        genPeq(coeffs[i], FREQS[i], 0);
        for (int j = 0; j < 4; j++) {
            stateL[i][j] = 0;
            stateR[i][j] = 0;
        }
    }
    Serial.println("✅ DSP Manager initialized (Stereo 10-band EQ)");
}

// 🎚️ Đặt gain cho từng dải
void setEqBand(uint8_t band, float gainDb) {
    if (band >= NUM_BANDS) return;
    eqGain[band] = gainDb;
    genPeq(coeffs[band], FREQS[band], gainDb);

    // reset state tránh click khi thay đổi mạnh
    for (int j = 0; j < 4; j++) {
        stateL[band][j] = 0;
        stateR[band][j] = 0;
    }

    Serial.printf("🎚️ Band %d (%.0f Hz) = %.1f dB\n", band, FREQS[band], gainDb);
}

// 🎵 Áp dụng toàn bộ từ mảng float
void applyEqFromArray(const float gains[NUM_BANDS]) {
    for (int i = 0; i < NUM_BANDS; i++) {
        setEqBand(i, gains[i]);
    }
    Serial.println("🎧 EQ configuration applied");
}

// 🎵 Áp dụng EQ từ JSON array (Laravel gửi xuống)
void applyEqFromJson(JsonArray eq) {
    int i = 0;
    for (JsonVariant v : eq) {
        if (i >= NUM_BANDS) break;
        setEqBand(i, v.as<float>());
        i++;
    }
    Serial.println("📡 EQ updated from JSON");
}

// 🧠 Lọc buffer PCM 16-bit (stereo)
void processAudioBufferInt16(int16_t *samples, int len) {
    int frames = len / 2;
    float left, right;
    const float outGain = 0.6f;  // giảm 40% biên độ tổng để tránh méo DAC

    for (int frame = 0; frame < frames; frame++) {
        left  = samples[frame * 2]     / 32768.0f;
        right = samples[frame * 2 + 1] / 32768.0f;

        for (int band = 0; band < NUM_BANDS; band++) {
            float *c = coeffs[band];
            float *sL = stateL[band];
            float yL = c[0]*left + c[1]*sL[0] + c[2]*sL[1] - c[3]*sL[2] - c[4]*sL[3];
            sL[1] = sL[0]; sL[0] = left;
            sL[3] = sL[2]; sL[2] = yL;
            left = yL;

            float *sR = stateR[band];
            float yR = c[0]*right + c[1]*sR[0] + c[2]*sR[1] - c[3]*sR[2] - c[4]*sR[3];
            sR[1] = sR[0]; sR[0] = right;
            sR[3] = sR[2]; sR[2] = yR;
            right = yR;
        }

        // Giảm tổng biên độ
        left *= outGain;
        right *= outGain;

        // Giới hạn tránh méo
        if (left > 1.0f) left = 1.0f;
        if (left < -1.0f) left = -1.0f;
        if (right > 1.0f) right = 1.0f;
        if (right < -1.0f) right = -1.0f;

        samples[frame * 2]     = (int16_t)(left * 32767);
        samples[frame * 2 + 1] = (int16_t)(right * 32767);
    }
}