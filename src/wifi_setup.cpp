#include "wifi_setup.h"
#include <WiFi.h>
#include "device_api.h"
#include "web_server.h"
#include "state_manager.h"


String targetSSID = "";
String targetPASS = "";
String userEmail = "";


void startWiFiAP()
{
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 1, 100), IPAddress(192, 168, 1, 100), IPAddress(255, 255, 255, 0));
  WiFi.softAP("WIFI_SETUP_CONGMINHAUDIO", "12345678");
  Serial.println("[WiFi] AP started: WIFI_SETUP_CONGMINHAUDIO  192.168.1.100");
}

void startConnectToWiFi(const char *ssid, const char *pass, String email)
{
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, pass);

  targetSSID = ssid;
  targetPASS = pass;
  userEmail = email;

  connState = CONNECTING;
  connectProgress = 0;
  connectStart = millis();

  Serial.printf("[WiFi] Connecting to %s ...\n", ssid);
}

void handleWiFiEvents()
{
  if (connState == CONNECTING)
  {
    unsigned long now = millis();
    unsigned long elapsed = now - connectStart;
    connectProgress = map(elapsed, 0, CONNECT_MAX_MS, 0, 90);
    if (WiFi.status() == WL_CONNECTED)
    {
      connState = CONNECTED;
      connectProgress = 100;
      
      Serial.printf("[WiFi] Connected: %s\n", targetSSID.c_str());
      server.stop(); 
      
      onWiFiConnected(userEmail); // Gọi sang device_api

      delay(4000);
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      
      Serial.println("[WiFi] Access Point turned off. ESP now only in STA mode.");
    }
    else if (elapsed >= CONNECT_MAX_MS)
    {
      connState = FAILED;
      connectProgress = 100;
      Serial.println("[WiFi] Connection failed");
    }
  }
}