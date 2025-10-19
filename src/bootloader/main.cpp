#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <base64.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include <WiFi.h>
#include "esp_wifi.h"
// WiFi配置
const char *ssid = "losehu";
const char *password = "12345678";

// 固件下载配置
const char *firmwareURL = "https://k5.losehu.com:438/files/firmware.bin";
const char *username = "family";
const char *password_auth = "losehuyyds";

// 查找 app0 (ota_0)
static const esp_partition_t *find_app0()
{
  const esp_partition_t *p =
      esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "app0");
  if (!p)
    p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  return p;
}
static void jump_to_app0_and_restart()
{
  const esp_partition_t *app0 = find_app0();
  if (app0)
    esp_ota_set_boot_partition(app0);
  esp_restart();
}
void connectToWiFi()
{
  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_STA,
                        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
  delay(50);
  Serial.println("正在连接WiFi...");
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi连接成功!");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\nWiFi连接失败!");
    return;
  }
}

void checkForFirmwareUpdate()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi未连接，无法检查更新");
    return;
  }
  // WiFiClientSecure::setBufferSizes(4096, 4096);

  // 创建安全客户端
  WiFiClientSecure client;
  Serial.println("开始固件更新流程...");
  delay(50);

  // 配置SSL
  client.setInsecure(); // 跳过证书验证

  HTTPClient http;

  // 设置基本认证
  String auth = String(username) + ":" + String(password_auth);
  String authBase64 = base64::encode(auth);

  http.begin(client, firmwareURL);
  delay(50);
  http.addHeader("Authorization", "Basic " + authBase64);
  delay(50);

  Serial.println("开始下载固件...");
  delay(50);
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Min Free Heap: %d bytes\n", ESP.getMinFreeHeap());
  Serial.printf("Max Alloc Heap: %d bytes\n", ESP.getMaxAllocHeap());
  Serial.printf("PSRAM Size: %d bytes\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());

  int httpCode = http.GET();
  delay(50);
  Serial.println("get...");
  delay(50);

  if (httpCode == HTTP_CODE_OK)
  {
    // 获取文件大小
    int contentLength = http.getSize();
    Serial.printf("固件大小: %d 字节 (约 %.2f MB)\n",
                  contentLength, contentLength / 1024.0 / 1024.0);

    // 检查可用闪存空间
    if (contentLength > (ESP.getFreeSketchSpace() - 0x1000))
    {
      Serial.println("错误: 固件太大，闪存空间不足");
      http.end();
      return;
    }

    Serial.println("开始写入闪存...");

    // 开始更新到app0分区
    if (Update.begin(contentLength, U_FLASH))
    {
      Serial.println("更新进程已启动");

      // 获取数据流
      WiFiClient *stream = http.getStreamPtr();

      // 显示进度
      size_t written = 0;
      size_t totalSize = contentLength;
      uint32_t lastProgress = 0;

      // 分块读取和写入
      while (http.connected() && written < totalSize)
      {
        // 可用数据大小
        size_t size = stream->available();

        if (size)
        {
          // 读取数据块
          uint8_t buf[4096]; // 1KB缓冲区 - 很小的RAM占用
          Serial.println("读https流");

          size_t readSize = stream->readBytes(buf, ((size > sizeof(buf)) ? sizeof(buf) : size));
          if (readSize == 0)
          {
            Serial.println("空流");

            continue;
          } // 防止空读导致崩溃
          Serial.println("写入闪存");

          // 写入到闪存
          if (Update.write(buf, readSize) != readSize)
          {
            Serial.println("写入闪存失败");
            break;
          }

          written += readSize;

          // 显示进度（每5%或更多时更新）
          uint32_t progress = (written * 100) / totalSize;
          if (progress >= lastProgress + 5)
          {
            Serial.printf("更新进度: %d%%\n", progress);
            lastProgress = progress;
          }
        }else
        {
          Serial.println("未读取到数据");
        }
        delay(50);
      }

      Serial.printf("写入完成: %d/%d 字节\n", written, totalSize);

      if (written == totalSize)
      {
        Serial.println("固件数据完全写入");

        if (Update.end(true))
        { // true表示验证成功
          Serial.println("固件更新成功!");
          Serial.println("设备将在1秒后重启...");
          delay(1000);
          ESP.restart();
        }
        else
        {
          Serial.printf("更新结束失败: %s\n", Update.errorString());
        }
      }
      else
      {
        Serial.println("错误: 固件数据不完整");
        Update.end(false);
      }
    }
    else
    {
      Serial.printf("无法开始更新: %s\n", Update.errorString());
    }
  }
  else
  {
    Serial.printf("HTTP请求失败，错误代码: %d\n", httpCode);
  }

  http.end();
  Serial.println("固件更新流程结束");
}

// ========= 可按需修改 =========
static const int BOOT_PIN = 1;                       // 拉低进入更新模式
static const uint32_t MAX_FW_SIZE = 3 * 1024 * 1024; // 最大镜像大小（防呆）
// =================================

static const uint32_t MAGIC = 0x32445055; // 'U''P''D''2' 小端
static const size_t MAX_CHUNK = 2048;     // 允许的最大单块（PC端可 <= 这个值）

// ---- 错误码（设备返回 'E'<code>，PC据此重传）----
enum Err : uint8_t
{
  E_SEQ_HDR = 1,
  E_LEN = 2,
  E_OVER = 3,
  E_DATA = 4,
  E_CRC = 5,
  E_WRITE = 6,
  E_TIMEOUT = 7
};

// ---- CRC32 (0xEDB88320) ----
static uint32_t crc32_update(uint32_t crc, const uint8_t *d, size_t n)
{
  crc = ~crc;
  while (n--)
  {
    crc ^= *d++;
    for (int i = 0; i < 8; ++i)
      crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
  }
  return ~crc;
}

// ---- 串口辅助 ----
static void drain_input(Stream &s, uint32_t quiet_ms = 20, uint32_t max_ms = 150)
{
  uint32_t start = millis(), last = start;
  while (true)
  {
    while (s.available())
    {
      (void)s.read();
      last = millis();
    }
    if (millis() - last >= quiet_ms)
      break;
    if (millis() - start > max_ms)
      break;
    delay(1);
  }
}

static bool read_exact(Stream &s, uint8_t *buf, size_t n, uint32_t timeout_ms)
{
  uint32_t t0 = millis();
  size_t got = 0;
  while (got < n)
  {
    int av = s.available();
    if (av > 0)
    {
      got += s.readBytes((char *)buf + got, n - got);
      t0 = millis();
    }
    else if (millis() - t0 > timeout_ms)
      return false;
    else
      delay(1);
  }
  return true;
}

static void wait_serial_ready(Stream &s, uint32_t max_wait_ms = 10000)
{
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0 < max_wait_ms))
  {
    delay(10);
  }
  drain_input(s, 30, 200);
}

// 设备端循环发 READY，直到收到 GO（大小写/CRLF 都接受）
static bool wait_go(Stream &s, uint32_t max_wait_ms = 60000)
{
  wait_serial_ready(s);
  uint32_t last = 0, start = millis();
  String line;
  while (true)
  {
    if (millis() - last > 300)
    {
      s.println("READY");
      s.flush();
      last = millis();
    }
    if (s.available())
    {
      line = s.readStringUntil('\n');
      line.trim();
      line.toUpperCase();
      if (line == "GO")
        return true;
    }
    delay(5);
  }
}

// ====== 核心流程：握手 + 头 + 擦除 + START + 分块 + ACK/NAK ======
static bool receive_and_flash_app0(Stream &s)
{
  if (!wait_go(s, 60000))
  {
    s.println("ERR: no GO");
    return false;
  }

  // 头：MAGIC + total + chunk
  uint32_t magic = 0, total = 0;
  uint16_t chunk = 0;
  if (!read_exact(s, (uint8_t *)&magic, 4, 10000) || magic != MAGIC)
  {
    s.println("ERR: magic");
    return false;
  }
  if (!read_exact(s, (uint8_t *)&total, 4, 5000) || total < 16 || total > MAX_FW_SIZE)
  {
    s.println("ERR: size");
    return false;
  }
  if (!read_exact(s, (uint8_t *)&chunk, 2, 5000) || chunk == 0 || chunk > MAX_CHUNK)
  {
    s.println("ERR: chunk");
    return false;
  }

  const esp_partition_t *part = find_app0();
  if (!part)
  {
    s.println("ERR: part");
    return false;
  }
  if (total > part->size)
  {
    s.println("ERR: too_big");
    return false;
  }

  // 擦除需要的区域（可能耗时数秒）
  esp_ota_handle_t h = 0;
  if (esp_ota_begin(part, total, &h) != ESP_OK)
  {
    s.println("ERR: begin");
    return false;
  }

  // 擦除完成 → 告知 PC 可以发第 0 块
  s.println("START");
  s.flush();
  drain_input(s, 30, 200);

  uint8_t *buf = (uint8_t *)malloc(chunk);
  if (!buf)
  {
    s.println("ERR: malloc");
    esp_ota_end(h);
    return false;
  }

  uint32_t expect_seq = 0, written = 0;

  while (written < total)
  {
    uint32_t seq = 0, crc_rx = 0;
    uint16_t len = 0;

    if (!read_exact(s, (uint8_t *)&seq, 4, 20000))
    {
      s.write('E');
      s.write((uint8_t)E_SEQ_HDR);
      drain_input(s);
      continue;
    }
    if (!read_exact(s, (uint8_t *)&len, 2, 20000) || len == 0 || len > chunk)
    {
      s.write('E');
      s.write((uint8_t)E_LEN);
      drain_input(s);
      continue;
    }
    if ((uint32_t)len > total - written)
    {
      s.write('E');
      s.write((uint8_t)E_OVER);
      drain_input(s);
      continue;
    }
    if (!read_exact(s, buf, len, 60000))
    {
      s.write('E');
      s.write((uint8_t)E_DATA);
      drain_input(s);
      continue;
    }
    if (!read_exact(s, (uint8_t *)&crc_rx, 4, 10000))
    {
      s.write('E');
      s.write((uint8_t)E_TIMEOUT);
      drain_input(s);
      continue;
    }

    if (seq < expect_seq)
    {
      s.write('A');
      s.flush();
      delay(0);
      continue;
    } // 重复包，直接ACK
    if (seq != expect_seq)
    {
      s.write('E');
      s.write((uint8_t)E_SEQ_HDR);
      drain_input(s);
      continue;
    }

    uint32_t crc = crc32_update(0, buf, len);
    if (crc != crc_rx)
    {
      s.write('E');
      s.write((uint8_t)E_CRC);
      drain_input(s);
      continue;
    }

    if (esp_ota_write(h, buf, len) != ESP_OK)
    {
      s.write('E');
      s.write((uint8_t)E_WRITE);
      drain_input(s);
      continue;
    }

    written += len;
    expect_seq++;
    s.write('A');
    s.flush();
    delay(0);
  }

  free(buf);

  if (esp_ota_end(h) != ESP_OK)
  {
    s.println("ERR: end");
    return false;
  }
  if (esp_ota_set_boot_partition(part) != ESP_OK)
  {
    s.println("ERR: set");
    return false;
  }

  s.println("OK");
  s.flush();
  delay(1000); // 留点时间给主机把尾巴读完
  esp_restart();
  return true;
}
void disableAMPDU()
{
  // 获取当前WiFi配置
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  // 禁用AMPDU RX和TX
  cfg.ampdu_rx_enable = 0; // 禁用AMPDU接收
  cfg.ampdu_tx_enable = 0; // 禁用AMPDU发送

  // 重新初始化WiFi
  esp_wifi_deinit();
  esp_wifi_init(&cfg);
}
void setup()
{

  psramInit();

  Serial.begin(115200);
  pinMode(3, INPUT_PULLDOWN);
  pinMode(2, INPUT_PULLDOWN);

  if (digitalRead(1) == HIGH || 1)
  {
    Serial.println("wifi联网烧录...");
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // 提高发射功率
    disableAMPDU();
    delay(1000); // 等待串口稳定
    // 连接WiFi
    connectToWiFi();
    // 检查并执行固件更新
    checkForFirmwareUpdate();
  }
  else if (digitalRead(2) == HIGH)
  {
    Serial.println("串口烧录...");

    (void)receive_and_flash_app0(Serial);
    delay(1000); // 等待串口稳定
  }
  else
  {
    Serial.println("启动app0...");
    delay(1000); // 等待串口稳定
  }

  jump_to_app0_and_restart();
}
void loop()
{
  // 主循环可以执行其他任务
}