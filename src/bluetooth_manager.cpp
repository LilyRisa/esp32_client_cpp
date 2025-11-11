#include "bluetooth_manager.h"
#include <SPIFFS.h>
#include "BluetoothA2DPSink.h"
#include "dsp_manager.h"
#include "driver/i2s.h"
#include "dsp_stream.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include <ArduinoJson.h>

BluetoothA2DPSink a2dp_sink;
String currentBtName = "CongMinhAudio";

#define I2S_BCK_IO 26
#define I2S_WS_IO 25
#define I2S_DO_IO 22

static RingbufHandle_t rb = NULL;
static TaskHandle_t dspTaskHandle = NULL;
static const size_t RINGBUF_SIZE = 8 * 1024;
static const size_t MAX_PACKET = 4096;

// extern bool dspEnabled;

// ---------------- DSP task ----------------
void dspTask(void *param)
{
  size_t item_size = 0;
  uint8_t *item = nullptr;

  while (true)
  {
    item = (uint8_t *)xRingbufferReceive(rb, &item_size, portMAX_DELAY);
    if (!item)
      continue;

    int sampleCount = item_size / 2;
    int16_t *samples = (int16_t *)item;

    if (dspEnabled)
      processAudioBufferInt16(samples, sampleCount);

    size_t bytes_written = 0;
    i2s_write(I2S_NUM_0, (const char *)samples, item_size, &bytes_written, portMAX_DELAY);
    vRingbufferReturnItem(rb, (void *)item);
  }
}

// ---------------- on connection callback ----------------
void onBtConnection(esp_a2d_connection_state_t state, void *obj)
{
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED)
  {
    Serial.println("✅ Bluetooth connected, starting DSP task...");
     Serial.printf("[Heap] start bluetooth ♥ %u bytes, Vcc: %.2fV\n",
              ESP.getFreeHeap(),
              analogRead(A0) * (3.3 / 4095.0));

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0};
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_IO,
        .ws_io_num = I2S_WS_IO,
        .data_out_num = I2S_DO_IO,
        .data_in_num = I2S_PIN_NO_CHANGE};
    i2s_set_pin(I2S_NUM_0, &pin_config);

    if (dspTaskHandle == NULL)
    {
      xTaskCreatePinnedToCore(dspTask, "DSPTask", 12288, NULL, 1, &dspTaskHandle, 0);
      Serial.println("🛠 DSP task created");
    }

    if (ESP.getFreeHeap() > 80000)
    {
      initDspStream();
      Serial.printf("🌐 WebSocket started (heap=%u)\n", ESP.getFreeHeap());
    }
    else
    {
      Serial.println("⚠️ Not enough heap for WebSocket!");
    }

  }

  if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
  {
    Serial.println("🔴 Bluetooth disconnected");
    i2s_driver_uninstall(I2S_NUM_0);
    wsClient.disconnect();
  }
}

// ---------------- stream reader callback ----------------
void audio_data_callback(const uint8_t *data, uint32_t len)
{
  if (!rb || len == 0 || len > MAX_PACKET)
    return;

  xRingbufferSend(rb, (void *)data, len, 0);
}

// ---------------- start / stop bluetooth ----------------
void startBluetooth(String name)
{
 

  currentBtName = name;

  if (!rb)
  {
    rb = xRingbufferCreate(RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (!rb)
    {
      Serial.println("❌ Không tạo được ringbuffer!");
      return;
    }
  }

  i2s_pin_config_t my_pin_config = {
      .bck_io_num = I2S_BCK_IO,
      .ws_io_num = I2S_WS_IO,
      .data_out_num = I2S_DO_IO,
      .data_in_num = I2S_PIN_NO_CHANGE};

  a2dp_sink.set_task_core(1);
  a2dp_sink.set_pin_config(my_pin_config);

  // ⚙️ 1️⃣ Khởi tạo DSP engine
  initDspManager(44100.0f);

  // ⚙️ 3️⃣ Load EQ từ file (và bật dspEnabled)
  loadDspConfig();

  // ⚙️ 4️⃣ Nếu vẫn chưa có EQ (file lỗi), áp EQ mặc định thủ công
  if (!dspEnabled)
  {
    float defaultEQ[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    applyEqFromArray(defaultEQ);

    Serial.println("✅ Applied default EQ manually");
  }

  // 🔊 Gán callback nhận dữ liệu
  a2dp_sink.set_stream_reader(audio_data_callback);
  a2dp_sink.set_on_connection_state_changed(onBtConnection);

  a2dp_sink.start(currentBtName.c_str());
  Serial.printf("[BT] Started with name: %s\n", currentBtName.c_str());
  Serial.printf("Free heap: %u, min heap: %u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
}

void stopBluetooth()
{
  a2dp_sink.end(true);
  Serial.println("[BT] Bluetooth stopped");
}

void reloadBluetooth(String name)
{
  if (name != currentBtName && name.length() > 0)
  {
    stopBluetooth();
    delay(500);
    startBluetooth(name);
    saveBluetoothName(name);
  }
}

String getBluetoothName()
{
  if (!SPIFFS.exists("/bt_name.txt"))
    return "CongMinhAudio";
  File f = SPIFFS.open("/bt_name.txt", "r");
  String name = f.readString();
  f.close();
  name.trim();
  return name.length() ? name : "CongMinhAudio";
}

void saveBluetoothName(String name)
{
  File f = SPIFFS.open("/bt_name.txt", "w");
  if (f)
  {
    f.print(name);
    f.close();
    Serial.printf("[BT] Saved name: %s\n", name.c_str());
  }
}