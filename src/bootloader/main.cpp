#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <base64.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include "../app/driver/st7565.h"
#include <string.h>

// UART0 for flashing (same as app K5 port): TX=GPIO43, RX=GPIO44
static HardwareSerial Uart0(0);
static constexpr int UART0_TX_PIN = 43;
static constexpr int UART0_RX_PIN = 44;
static constexpr uint32_t UART0_FLASH_BAUD = 115200;

// LCD backlight (simple always-on for bootloader UI)
static constexpr int BACKLIGHT_IO = 8;

static bool gLcdInited = false;
static int gLastPct = -1;
static uint32_t gLastUiMs = 0;

static inline void fb_clear_all()
{
  memset(gStatusLine, 0x00, sizeof(gStatusLine));
  memset(gFrameBuffer, 0x00, sizeof(gFrameBuffer));
}

static inline void fb_set_pixel(int x, int y, bool on)
{
  if (x < 0 || x >= (int)LCD_WIDTH || y < 0 || y >= (int)LCD_HEIGHT)
    return;

  const int page = (y >> 3);      // 0..7
  const uint8_t bit = 1u << (y & 7);

  if (page == 0)
  {
    if (on)
      gStatusLine[x] |= bit;
    else
      gStatusLine[x] &= (uint8_t)~bit;
  }
  else
  {
    const int idx = page - 1; // 0..6
    if (idx < 0 || idx >= (int)FRAME_LINES)
      return;
    if (on)
      gFrameBuffer[idx][x] |= bit;
    else
      gFrameBuffer[idx][x] &= (uint8_t)~bit;
  }
}

static void fb_fill_rect(int x, int y, int w, int h, bool on)
{
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      fb_set_pixel(xx, yy, on);
}

static void fb_draw_rect(int x, int y, int w, int h, bool on)
{
  for (int xx = x; xx < x + w; xx++)
  {
    fb_set_pixel(xx, y, on);
    fb_set_pixel(xx, y + h - 1, on);
  }
  for (int yy = y; yy < y + h; yy++)
  {
    fb_set_pixel(x, yy, on);
    fb_set_pixel(x + w - 1, yy, on);
  }
}

// Minimal 5x7 font for digits + uppercase letters + space + symbols used.
// Each glyph is 5 columns, LSB at top.
static const uint8_t *glyph5x7(char c)
{
  // Digits 0-9
  static const uint8_t DIGITS[10][5] = {
      {0x3E, 0x45, 0x49, 0x51, 0x3E}, // 0
      {0x00, 0x21, 0x7F, 0x01, 0x00}, // 1
      {0x23, 0x45, 0x49, 0x51, 0x21}, // 2
      {0x22, 0x41, 0x49, 0x49, 0x36}, // 3
      {0x0C, 0x14, 0x24, 0x7F, 0x04}, // 4
      {0x72, 0x51, 0x51, 0x51, 0x4E}, // 5
      {0x1E, 0x29, 0x49, 0x49, 0x06}, // 6
      {0x40, 0x47, 0x48, 0x50, 0x60}, // 7
      {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
      {0x30, 0x49, 0x49, 0x4A, 0x3C}  // 9
  };

  // Uppercase A-Z
  static const uint8_t UPPER[26][5] = {
      {0x3F, 0x48, 0x48, 0x48, 0x3F}, // A
      {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
      {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
      {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
      {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
      {0x7F, 0x48, 0x48, 0x48, 0x40}, // F
      {0x3E, 0x41, 0x49, 0x49, 0x2E}, // G
      {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
      {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
      {0x02, 0x01, 0x41, 0x7E, 0x40}, // J
      {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
      {0x7F, 0x01, 0x01, 0x01, 0x01}, // L
      {0x7F, 0x20, 0x10, 0x20, 0x7F}, // M
      {0x7F, 0x10, 0x08, 0x04, 0x7F}, // N
      {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
      {0x7F, 0x48, 0x48, 0x48, 0x30}, // P
      {0x3E, 0x41, 0x45, 0x42, 0x3D}, // Q
      {0x7F, 0x48, 0x4C, 0x4A, 0x31}, // R
      {0x32, 0x49, 0x49, 0x49, 0x26}, // S
      {0x40, 0x40, 0x7F, 0x40, 0x40}, // T
      {0x7E, 0x01, 0x01, 0x01, 0x7E}, // U
      {0x7C, 0x02, 0x01, 0x02, 0x7C}, // V
      {0x7E, 0x01, 0x06, 0x01, 0x7E}, // W
      {0x63, 0x14, 0x08, 0x14, 0x63}, // X
      {0x70, 0x08, 0x07, 0x08, 0x70}, // Y
      {0x43, 0x45, 0x49, 0x51, 0x61}  // Z
  };

  static const uint8_t SPACE[5] = {0, 0, 0, 0, 0};
  static const uint8_t DASH[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t COLON[5] = {0x00, 0x12, 0x00, 0x12, 0x00};
  static const uint8_t DOT[5] = {0x00, 0x01, 0x00, 0x00, 0x00};
  static const uint8_t PCT[5] = {0x63, 0x13, 0x08, 0x64, 0x63};

  if (c >= '0' && c <= '9')
    return DIGITS[c - '0'];
  if (c >= 'A' && c <= 'Z')
    return UPPER[c - 'A'];
  if (c == ' ')
    return SPACE;
  if (c == '-')
    return DASH;
  if (c == ':')
    return COLON;
  if (c == '.')
    return DOT;
  if (c == '%')
    return PCT;
  return SPACE;
}

static void fb_draw_char(int x, int y, char c)
{
  const uint8_t *g = glyph5x7(c);
  for (int col = 0; col < 5; col++)
  {
    uint8_t bits = g[col];
    for (int row = 0; row < 7; row++)
    {
      const bool on = (bits >> row) & 1;
      fb_set_pixel(x + col, y + row, on);
    }
  }
}

static void fb_draw_text(int x, int y, const char *s)
{
  int cx = x;
  while (*s)
  {
    char c = *s++;
    if (c >= 'a' && c <= 'z')
      c = (char)(c - 'a' + 'A');
    fb_draw_char(cx, y, c);
    cx += 6;
    if (cx > (int)LCD_WIDTH - 6)
      break;
  }
}

static void lcd_init_once()
{
  if (gLcdInited)
    return;
  pinMode(BACKLIGHT_IO, OUTPUT);
  analogWrite(BACKLIGHT_IO, 255);
  ST7565_Init();
  fb_clear_all();
  ST7565_BlitAll();
  gLcdInited = true;
}

static void lcd_render(const char *title, const char *stage, int pct)
{
  if (!gLcdInited)
    return;

  fb_clear_all();

  // Title on status line (y=0)
  fb_draw_text(0, 0, title);

  // Stage text
  fb_draw_text(0, 12, stage);

  // Percent text
  char buf[16];
  if (pct < 0)
    pct = 0;
  if (pct > 100)
    pct = 100;
  snprintf(buf, sizeof(buf), "%3d%%", pct);
  fb_draw_text(0, 22, buf);

  // Progress bar
  const int barX = 0;
  const int barY = 36;
  const int barW = 124;
  const int barH = 12;
  fb_draw_rect(barX, barY, barW, barH, true);
  const int fillW = (barW - 2) * pct / 100;
  fb_fill_rect(barX + 1, barY + 1, fillW, barH - 2, true);

  ST7565_BlitAll();
}

static void lcd_update_throttled(const char *title, const char *stage, int pct)
{
  // Throttle to avoid slowing serial flashing too much.
  const uint32_t now = millis();
  if (pct == gLastPct && (now - gLastUiMs) < 250)
    return;
  gLastPct = pct;
  gLastUiMs = now;
  lcd_render(title, stage, pct);
}
// WiFi配置
const char *ssid = "CVPU";
const char *password = "CVPU123456";

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

// ===== 键盘矩阵：按住 MENU 开机进入串口烧录 =====
// 来自主固件的键盘定义：
// - ROW0: GPIO48
// - COL0: GPIO14 (KEY4)
// MENU 键位于 (COL0, ROW0)
static bool is_menu_held_on_boot()
{
  static const int ROW0 = 48;
  static const int COL0 = 14;
  static const int COL1 = 13;
  static const int COL2 = 12;
  static const int COL3 = 11;

  pinMode(ROW0, INPUT_PULLUP);
  pinMode(COL0, OUTPUT);
  pinMode(COL1, OUTPUT);
  pinMode(COL2, OUTPUT);
  pinMode(COL3, OUTPUT);

  // 默认全部拉高
  digitalWrite(COL0, HIGH);
  digitalWrite(COL1, HIGH);
  digitalWrite(COL2, HIGH);
  digitalWrite(COL3, HIGH);
  delay(2);

  // 选中 COL0（拉低），读取 ROW0
  digitalWrite(COL0, LOW);
  digitalWrite(COL1, HIGH);
  digitalWrite(COL2, HIGH);
  digitalWrite(COL3, HIGH);
  delay(2);

  int low_cnt = 0;
  for (int i = 0; i < 6; ++i)
  {
    if (digitalRead(ROW0) == LOW)
      low_cnt++;
    delay(1);
  }

  // 释放列线
  digitalWrite(COL0, HIGH);
  delay(1);

  return low_cnt >= 5;
}

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
  (void)max_wait_ms;
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
    lcd_update_throttled("UVE5 BL", "WAIT GO", 0);
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
  lcd_init_once();
  lcd_update_throttled("UVE5 BL", "UART MODE", 0);

  if (!wait_go(s, 60000))
  {
    s.println("ERR: no GO");
    lcd_update_throttled("UVE5 BL", "ERR NO GO", 0);
    return false;
  }

  lcd_update_throttled("UVE5 BL", "HDR", 0);

  // 头：MAGIC + total + chunk
  uint32_t magic = 0, total = 0;
  uint16_t chunk = 0;
  if (!read_exact(s, (uint8_t *)&magic, 4, 10000) || magic != MAGIC)
  {
    s.println("ERR: magic");
    lcd_update_throttled("UVE5 BL", "ERR MAGIC", 0);
    return false;
  }
  if (!read_exact(s, (uint8_t *)&total, 4, 5000) || total < 16 || total > MAX_FW_SIZE)
  {
    s.println("ERR: size");
    lcd_update_throttled("UVE5 BL", "ERR SIZE", 0);
    return false;
  }
  if (!read_exact(s, (uint8_t *)&chunk, 2, 5000) || chunk == 0 || chunk > MAX_CHUNK)
  {
    s.println("ERR: chunk");
    lcd_update_throttled("UVE5 BL", "ERR CHUNK", 0);
    return false;
  }

  const esp_partition_t *part = find_app0();
  if (!part)
  {
    s.println("ERR: part");
    lcd_update_throttled("UVE5 BL", "ERR PART", 0);
    return false;
  }
  if (total > part->size)
  {
    s.println("ERR: too_big");
    lcd_update_throttled("UVE5 BL", "ERR BIG", 0);
    return false;
  }

  // 擦除需要的区域（可能耗时数秒）
  esp_ota_handle_t h = 0;
  lcd_update_throttled("UVE5 BL", "ERASE", 0);
  if (esp_ota_begin(part, total, &h) != ESP_OK)
  {
    s.println("ERR: begin");
    lcd_update_throttled("UVE5 BL", "ERR BEGIN", 0);
    return false;
  }

  // 擦除完成 → 告知 PC 可以发第 0 块
  // 用 "STRT" 避免包含 'A'/'E' 字节，防止主机误判 ACK/NAK。
  // 并在首包到来前重复发送，降低主机漏读导致的卡死概率。
  uint32_t lastStartMs = 0;
  const uint32_t startAnnounceBegin = millis();
  while (!s.available() && (millis() - startAnnounceBegin) < 5000)
  {
    if (millis() - lastStartMs > 300)
    {
      s.println("STRT");
      s.flush();
      lastStartMs = millis();
    }
    delay(5);
  }
  // IMPORTANT: Do not drain after START.
  // The host typically begins sending chunk0 immediately after it sees START.
  // Draining here can accidentally consume the beginning of chunk0 and cause
  // a predictable first-chunk retry (E_TIMEOUT / E_SEQ_HDR).

  lcd_update_throttled("UVE5 BL", "WRITE", 0);

  uint8_t *buf = (uint8_t *)malloc(chunk);
  if (!buf)
  {
    s.println("ERR: malloc");
    esp_ota_end(h);
    return false;
  }

  uint32_t expect_seq = 0, written = 0;
  int lastPct = -1;

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

    const int pct = (int)((written * 100ULL) / total);
    if (pct != lastPct)
    {
      lcd_update_throttled("UVE5 BL", "WRITE", pct);
      lastPct = pct;
    }
    s.write('A');
    s.flush();
    delay(0);
  }

  free(buf);

  if (esp_ota_end(h) != ESP_OK)
  {
    s.println("ERR: end");
    lcd_update_throttled("UVE5 BL", "ERR END", 100);
    return false;
  }
  if (esp_ota_set_boot_partition(part) != ESP_OK)
  {
    s.println("ERR: set");
    lcd_update_throttled("UVE5 BL", "ERR SET", 100);
    return false;
  }

  s.println("OK");
  s.flush();
  lcd_update_throttled("UVE5 BL", "OK", 100);
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
  Uart0.begin(UART0_FLASH_BAUD, SERIAL_8N1, UART0_RX_PIN, UART0_TX_PIN);
  pinMode(3, INPUT_PULLDOWN);
  pinMode(2, INPUT_PULLDOWN);
    Serial.println("引导已经启动....");


   if (is_menu_held_on_boot())
  {
    Serial.println("检测到MENU按住：进入串口烧录...");

      // Flash over UART0 (GPIO43/44). Keep Serial (USB CDC) for logs.
      (void)receive_and_flash_app0(Uart0);
    delay(1000); // 等待串口稳定
  }
  else
  {
    Serial.println("未按MENU：启动app00...");
    delay(1000); // 等待串口稳定
  }

  jump_to_app0_and_restart();
}
void loop()
{
  // 主循环可以执行其他任务
}