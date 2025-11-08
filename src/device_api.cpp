#include "device_api.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "bluetooth_manager.h"
#include "storage.h"

const char *LARAVEL_SERVER = "http://dev.congminhstore.vn/api";

bool checkCodeValidity(String code)
{
    if (code == "")
        return false;
    HTTPClient http;
    http.begin(String(LARAVEL_SERVER) + "/check_code_device");
    http.addHeader("Content-Type", "application/json");

    String body = "{\"code\":\"" + code + "\"}";
    int httpCode = http.POST(body);

    if (httpCode > 0)
    {
        String resp = http.getString();
        Serial.println("[API] Check response: " + resp);
        if (resp.indexOf("\"valid\":true") != -1)
        {
            http.end();
            return true;
        }
    }
    http.end();
    return false;
}

void onWiFiConnected(String email)
{
    Serial.printf("Free heap BEFORE: %u bytes\n", ESP.getFreeHeap());
    String code = loadDeviceCode();
    String mac = WiFi.macAddress();
    //   String ip  = WiFi.localIP().toString();

    HTTPClient http;

    //  Kiểm tra mã có hợp lệ không
    if (code != "" && checkCodeValidity(code))
    {
        // Mã hợp lệ → cập nhật thông tin lên server
        http.begin(String(LARAVEL_SERVER) + "/update_device");
        http.addHeader("Content-Type", "application/json");
        String body = "{\"code\":\"" + code + "\",\"mac\":\"" + mac + "\"}";
        http.POST(body);
        http.end();
        Serial.println("[API] Device code valid & updated: " + code);
        return;
    }

    //  Không có mã hoặc mã sai → tạo mới
    http.begin(String(LARAVEL_SERVER) + "/create_code_device");
    http.addHeader("Content-Type", "application/json");
    String body = "{\"mac\":\"" + mac + "\",\"email\":\"" + email + "\"}";
    int httpCode = http.POST(body);

    if (httpCode > 0)
    {
        String resp = http.getString();
        Serial.println(resp);
        int p1 = resp.indexOf("\"code\"");
        if (p1 > 0)
        {
            int q1 = resp.indexOf('"', p1 + 7);
            int q2 = resp.indexOf('"', q1 + 1);
            if (q1 > 0 && q2 > q1)
            {
                code = resp.substring(q1 + 1, q2);
                saveDeviceCode(code); // ✅ lưu mã mới
                Serial.println("[API] Device code created & saved: " + code);
            }
        }
    }
    else
    {
        Serial.printf("[API] Failed to create code, error %d\n", httpCode);
    }
    http.end();
    Serial.printf("Free heap BEFORE: %u bytes\n", ESP.getFreeHeap());
}

void syncDeviceConfig()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    HTTPClient http;
    String deviceCode = loadDeviceCode();
    http.begin(String(LARAVEL_SERVER) + "?code=" + deviceCode);
    int httpCode = http.GET();

    if (httpCode == 200)
    {
        String payload = http.getString();
        StaticJsonDocument<256> doc;
        deserializeJson(doc, payload);
        String newName = doc["bt_name"].as<String>();

        if (newName.length() > 0)
        {
            Serial.printf("[BT] New name from server: %s\n", newName.c_str());
            reloadBluetooth(newName);
        }
    }
    else
    {
        Serial.printf("[HTTP] Error getting config: %d\n", httpCode);
    }

    http.end();
}