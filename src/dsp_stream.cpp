#include "dsp_stream.h"
#include "storage.h"
#include <ArduinoJson.h>
#include <dsp_manager.h>
#include <WebSocketsClient.h> // ⚡ dùng thư viện WebSocket client
#include <SPIFFS.h>

WebSocketsClient wsClient;
String deviceCode;

bool dspEnabled = false;


// 💾 Lưu cấu hình EQ
void saveDspConfig(JsonArray eq)
{
  File f = SPIFFS.open("/dsp_config.txt", "w");
  if (!f)
    return;
  serializeJson(eq, f);
  f.close();
}

// 🔁 Tải lại cấu hình EQ
void loadDspConfig() {
  File f = SPIFFS.open("/dsp_config.txt", "r");
  if (!f) {
    Serial.println("⚠️ Không tìm thấy /dsp_config.txt → phát nhạc thô");
    dspEnabled = false;
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.println("⚠️ Lỗi đọc EQ JSON → phát nhạc thô");
    dspEnabled = false;
    return;
  }

  JsonArray eq = doc.as<JsonArray>();
  if (eq.isNull() || eq.size() == 0) {
    Serial.println("⚠️ EQ rỗng → phát nhạc thô");
    dspEnabled = false;
    return;
  }

  applyEqFromJson(eq);
  dspEnabled = true;
  Serial.println("🎚️ EQ đã được tải và áp dụng");
}

// 📩 Xử lý dữ liệu nhận được từ WebSocket server (Laravel)
void handleWsMessage(const char *payload, size_t length)
{
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err)
  {
    Serial.println("JSON ngoài không hợp lệ!");
    return;
  }

  const char *outerEvent = doc["event"];
  const char *channel = doc["channel"];
  const char *dataStr = doc["data"]; // inner JSON string

  if (!dataStr)
  {
    Serial.println("Không có trường 'data'");
    return;
  }

  // Parse JSON bên trong data
  StaticJsonDocument<256> inner;
  DeserializationError err2 = deserializeJson(inner, dataStr);
  if (err2)
  {
    Serial.println("JSON bên trong 'data' không hợp lệ!");
    return;
  }

  String code = inner["code"].as<String>();
  String event = inner["event"].as<String>();
  JsonArray eq = inner["eq"].as<JsonArray>();

  if (code == deviceCode && event == "dsp.update")
  {
    applyEqFromJson(eq);
    saveDspConfig(eq);
    loadDspConfig();
    Serial.println("🎧 Cập nhật DSP từ server thành công!");
  }
  else
  {
    Serial.println("❌ Mã thiết bị không khớp, bỏ qua!");
  }
}

// 🔌 Sự kiện WebSocket client
void onWsEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    // Serial.println("❌ WebSocket bị ngắt, thử lại sau...");
    break;
  case WStype_CONNECTED:
    Serial.println("🔌 WebSocket đã kết nối!");
    // gửi mã nhận diện ngay sau khi kết nối
    wsClient.sendTXT("{\"event\":\"pusher:subscribe\",\"data\":{\"channel\":\"public-channel\"}}");
    break;
  case WStype_TEXT:
    handleWsMessage((const char *)payload, length);
    break;
  default:
    break;
  }
}

// 🚀 Khởi tạo kết nối tới server WebSocket Laravel
void initDspStream()
{
  deviceCode = loadDeviceCode();
  Serial.println("🔑 Device Code: " + deviceCode);

  loadDspConfig();

  // ⚙️ Địa chỉ WebSocket server (Laravel / VPS)
  // 🔸 Dạng ws:// hoặc wss:// nếu có SSL
  const char *ws_host = "spe.congminhstore.vn"; // 🔧 đổi domain bạn
  const uint16_t ws_port = 6001;                        // nếu SSL thì 443, không thì 80
  const char *ws_path = "/app/dsp";                   // Laravel endpoint bạn tự định nghĩa

  wsClient.begin(ws_host, ws_port, ws_path); // dùng SSL
  // wsClient.begin(ws_host, ws_port, ws_path);  // nếu chưa dùng SSL

  wsClient.onEvent([](WStype_t type, uint8_t *payload, size_t length)
                   { onWsEvent(type, payload, length); });

  wsClient.setReconnectInterval(5000); // Tự reconnect mỗi 5s
  Serial.println("✅ DSP WebSocket client initialized");
}

void loopDspStream()
{
  wsClient.loop();
}