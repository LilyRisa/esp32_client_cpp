// bluetooth_manager.cpp - phiên bản đã sửa (ring buffer, no malloc in callback)
#include "bluetooth_manager.h"
#include <SPIFFS.h>
#include "BluetoothA2DPSink.h"
#include "dsp_manager.h"
#include "driver/i2s.h"
#include "dsp_stream.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"

BluetoothA2DPSink a2dp_sink;
String currentBtName = "CongMinhAudio";

// Chân I2S xuất ra DAC UDA1334A
#define I2S_BCK_IO 26
#define I2S_WS_IO 25
#define I2S_DO_IO 22

// Bật/tắt DSP (do bạn quản lý khi load config)
// bool dspEnabled = false;

// Ringbuffer để truyền dữ liệu nhanh từ callback -> DSP task
static RingbufHandle_t rb = NULL;
static TaskHandle_t dspTaskHandle = NULL;

static const size_t RINGBUF_SIZE = 8 * 1024; // 8KB ring buffer (tùy chỉnh)
static const size_t MAX_PACKET = 4096;       // Gói lớn nhất chấp nhận (giảm nếu cần)


// ---------------- DSP task ----------------
void dspTask(void *param)
{
  size_t item_size = 0;
  uint8_t *item = nullptr;

  while (true)
  {
    // chờ data (blocking)
    item = (uint8_t *)xRingbufferReceive(rb, &item_size, portMAX_DELAY);
    if (item == NULL) continue;

    int sampleCount = item_size / 2; // 16-bit samples
    int16_t *samples = (int16_t *)item;

    if (dspEnabled)
    {
      processAudioBufferInt16(samples, sampleCount);
    }

    // Ghi ra I2S — len bytes = item_size
    size_t bytes_written = 0;
    esp_err_t res = i2s_write(I2S_NUM_0, (const char *)samples, item_size, &bytes_written, portMAX_DELAY);

    // Optional: debug nhẹ (không nên nhiều)
    // Serial.printf("[I2S] wrote %u/%u bytes res=%d\n", bytes_written, (unsigned)item_size, res);

    // Trả item về ringbuffer
    vRingbufferReturnItem(rb, (void *)item);
  }
}


// ---------------- on connection callback ----------------
void onBtConnection(esp_a2d_connection_state_t state, void *obj)
{
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED)
  {
    Serial.println("✅ Bluetooth connected, starting DSP task...");

    // 🔧 Khởi tạo I2S thủ công (vì ta override stream_reader)
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

    // Tạo DSP task nếu chưa có
    if (dspTaskHandle == NULL)
    {
      xTaskCreatePinnedToCore(dspTask, "DSPTask", 12288, NULL, 1, &dspTaskHandle, 0);
      Serial.println("🛠 DSP task created");
    }
  }

  if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
  {
    Serial.println("🔴 Bluetooth disconnected, stopping DSP task...");
    // Không xóa task ngay, có thể giữ để tái dùng; nếu muốn xóa: vTaskDelete(dspTaskHandle);
    // Uninstall I2S nếu bạn muốn:
    i2s_driver_uninstall(I2S_NUM_0);
    // dspTaskHandle = NULL; // nếu xóa task
  }
}


// ---------------- stream reader callback (RINGBUFFER) ----------------
void audio_data_callback(const uint8_t *data, uint32_t len)
{
  // RẤT QUAN TRỌNG: callback phải thật nhanh. Không sử dụng malloc/free, không in Serial.
  if (!rb) return;

  if (len == 0 || len > MAX_PACKET)
  {
    // gói quá lớn -> bỏ (hoặc cắt nếu muốn)
    return;
  }

  // xRingbufferSend sẽ copy dữ liệu vào buffer nội bộ (non-blocking với timeout 0)
  BaseType_t ok = xRingbufferSend(rb, (void *)data, len, 0);
  if (ok != pdTRUE)
  {
    // ring buffer đầy -> bỏ gói (không block)
    // (cũng có thể count dropped packets)
  }
}


// ---------------- start / stop bluetooth ----------------
void startBluetooth(String name)
{
  currentBtName = name;

  // --- Chuẩn bị ringbuffer trước khi start ---
  if (!rb)
  {
    rb = xRingbufferCreate(RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (!rb)
    {
      Serial.println("❌ Không tạo được ringbuffer!");
      return;
    }
  }

  // --- Cấu hình I2S pins (thư viện sẽ không install driver nếu set_stream_reader override) ---
  i2s_pin_config_t my_pin_config = {
      .bck_io_num = I2S_BCK_IO,
      .ws_io_num = I2S_WS_IO,
      .data_out_num = I2S_DO_IO,
      .data_in_num = I2S_PIN_NO_CHANGE};

  a2dp_sink.set_task_core(1);
  a2dp_sink.set_pin_config(my_pin_config);

  // ⚙️ Khởi tạo DSP EQ trước khi khởi động Bluetooth
  initDspManager(44100.0f); // Sample rate Bluetooth mặc định
  // loadDspConfig() nên set dspEnabled = true/false theo file

  // 🔊 Gán callback nhận dữ liệu (phải trước start)
  a2dp_sink.set_stream_reader(audio_data_callback);

  // set on connection state
  a2dp_sink.set_on_connection_state_changed(onBtConnection);

  // --- Khởi động Bluetooth A2DP ---
  a2dp_sink.start(currentBtName.c_str());
  Serial.printf("[BT] Started with name: %s\n", currentBtName.c_str());
}

void stopBluetooth()
{
  a2dp_sink.end(true);
  // Uninstall I2S and free ringbuffer if bạn muốn
  if (rb)
  {
    vRingbufferDelete(rb);
    rb = NULL;
  }
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