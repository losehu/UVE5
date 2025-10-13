#include <Arduino.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"

// ========= 可按需修改 =========
static const int BOOT_PIN = 1;           // 拉低进入更新模式
static const uint32_t MAX_FW_SIZE = 3*1024*1024; // 最大镜像大小（防呆）
// =================================

static const uint32_t MAGIC = 0x32445055; // 'U''P''D''2' 小端
static const size_t   MAX_CHUNK = 2048;   // 允许的最大单块（PC端可 <= 这个值）

// ---- 错误码（设备返回 'E'<code>，PC据此重传）----
enum Err : uint8_t { E_SEQ_HDR=1, E_LEN=2, E_OVER=3, E_DATA=4, E_CRC=5, E_WRITE=6, E_TIMEOUT=7 };

// ---- CRC32 (0xEDB88320) ----
static uint32_t crc32_update(uint32_t crc, const uint8_t* d, size_t n){
  crc = ~crc;
  while(n--){
    crc ^= *d++;
    for(int i=0;i<8;++i) crc = (crc>>1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
  }
  return ~crc;
}

// ---- 串口辅助 ----
static void drain_input(Stream& s, uint32_t quiet_ms=20, uint32_t max_ms=150){
  uint32_t start = millis(), last = start;
  while (true) {
    while (s.available()) { (void)s.read(); last = millis(); }
    if (millis() - last >= quiet_ms) break;
    if (millis() - start > max_ms) break;
    delay(1);
  }
}

static bool read_exact(Stream& s, uint8_t* buf, size_t n, uint32_t timeout_ms){
  uint32_t t0 = millis(); size_t got=0;
  while(got<n){
    int av = s.available();
    if(av>0){ got += s.readBytes((char*)buf+got, n-got); t0 = millis(); }
    else if(millis()-t0 > timeout_ms) return false;
    else delay(1);
  }
  return true;
}

static void wait_serial_ready(Stream& s, uint32_t max_wait_ms=10000){
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0 < max_wait_ms)) { delay(10); }
  drain_input(s, 30, 200);
}

// 设备端循环发 READY，直到收到 GO（大小写/CRLF 都接受）
static bool wait_go(Stream& s, uint32_t max_wait_ms=60000){
  wait_serial_ready(s);
  uint32_t last = 0, start = millis(); String line;
  while (true) {
    if (millis() - last > 300) {
      s.println("READY"); s.flush();
      last = millis();
    }
    if (s.available()) {
      line = s.readStringUntil('\n'); line.trim(); line.toUpperCase();
      if (line == "GO") return true;
    }
    if (millis() - start > max_wait_ms) return false;
    delay(5);
  }
}

// 查找 app0 (ota_0)
static const esp_partition_t* find_app0(){
  const esp_partition_t* p =
      esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "app0");
  if(!p) p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  return p;
}

// ====== 核心流程：握手 + 头 + 擦除 + START + 分块 + ACK/NAK ======
static bool receive_and_flash_app0(Stream& s){
  if(!wait_go(s, 60000)){ s.println("ERR: no GO"); return false; }

  // 头：MAGIC + total + chunk
  uint32_t magic=0,total=0; uint16_t chunk=0;
  if(!read_exact(s,(uint8_t*)&magic,4,10000) || magic!=MAGIC){ s.println("ERR: magic"); return false; }
  if(!read_exact(s,(uint8_t*)&total,4,5000) || total<16 || total>MAX_FW_SIZE){ s.println("ERR: size"); return false; }
  if(!read_exact(s,(uint8_t*)&chunk,2,5000) || chunk==0 || chunk>MAX_CHUNK){ s.println("ERR: chunk"); return false; }

  const esp_partition_t* part = find_app0();
  if(!part){ s.println("ERR: part"); return false; }
  if(total > part->size){ s.println("ERR: too_big"); return false; }

  // 擦除需要的区域（可能耗时数秒）
  esp_ota_handle_t h=0;
  if(esp_ota_begin(part, total, &h) != ESP_OK){ s.println("ERR: begin"); return false; }

  // 擦除完成 → 告知 PC 可以发第 0 块
  s.println("START"); s.flush();
  drain_input(s, 30, 200);

  uint8_t* buf = (uint8_t*)malloc(chunk);
  if(!buf){ s.println("ERR: malloc"); esp_ota_end(h); return false; }

  uint32_t expect_seq=0, written=0;

  while(written < total){
    uint32_t seq=0, crc_rx=0; uint16_t len=0;

    if(!read_exact(s,(uint8_t*)&seq,4,20000)){ s.write('E'); s.write((uint8_t)E_SEQ_HDR); drain_input(s); continue; }
    if(!read_exact(s,(uint8_t*)&len,2,20000) || len==0 || len>chunk){
      s.write('E'); s.write((uint8_t)E_LEN); drain_input(s); continue;
    }
    if((uint32_t)len > total - written){
      s.write('E'); s.write((uint8_t)E_OVER); drain_input(s); continue;
    }
    if(!read_exact(s, buf, len, 60000)){ s.write('E'); s.write((uint8_t)E_DATA); drain_input(s); continue; }
    if(!read_exact(s,(uint8_t*)&crc_rx,4,10000)){ s.write('E'); s.write((uint8_t)E_TIMEOUT); drain_input(s); continue; }

    if(seq < expect_seq){ s.write('A'); s.flush(); delay(0); continue; } // 重复包，直接ACK
    if(seq != expect_seq){ s.write('E'); s.write((uint8_t)E_SEQ_HDR); drain_input(s); continue; }

    uint32_t crc = crc32_update(0, buf, len);
    if(crc != crc_rx){ s.write('E'); s.write((uint8_t)E_CRC); drain_input(s); continue; }

    if(esp_ota_write(h, buf, len) != ESP_OK){ s.write('E'); s.write((uint8_t)E_WRITE); drain_input(s); continue; }

    written += len;
    expect_seq++;
    s.write('A'); s.flush(); delay(0);
  }

  free(buf);

  if(esp_ota_end(h) != ESP_OK){ s.println("ERR: end"); return false; }
  if(esp_ota_set_boot_partition(part) != ESP_OK){ s.println("ERR: set"); return false; }

  s.println("OK"); s.flush();
  delay(1000);                 // 留点时间给主机把尾巴读完
  esp_restart();
  return true;
}

static void jump_to_app0_and_restart(){
  const esp_partition_t* app0 = find_app0();
  if (app0) esp_ota_set_boot_partition(app0);
  esp_restart();
}

void setup(){
  pinMode(BOOT_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  delay(2000);

  if (digitalRead(BOOT_PIN) == LOW) {
    // 更新模式
    (void)receive_and_flash_app0(Serial);
    // 失败就留在 factory
    Serial.println("Update failed, stay in factory");
  } else {
    // 正常启动 app0
    jump_to_app0_and_restart();
  }
}

void loop(){ delay(1000); }
