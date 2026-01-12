// shared_flash.h
#pragma once
#ifndef ENABLE_OPENCV
#include <Arduino.h>
#include "esp_partition.h"


#ifndef SHARED_SUBTYPE
#define SHARED_SUBTYPE ((esp_partition_subtype_t)0x40) // 自定义 data 子类型
#endif

#ifndef SHARED_LABEL
#define SHARED_LABEL   "shared"
#endif

static constexpr size_t FLASH_SECTOR = 0x1000; // 4KB

// 查找共享分区
inline const esp_partition_t* shared_part() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, SHARED_SUBTYPE, SHARED_LABEL);
}

// 任意位置读取 len 字节
inline bool shared_read(size_t offset, void* out, size_t len) {
  const esp_partition_t* p = shared_part();
  if (!p || !out || len == 0) return false;
  if (offset + len > p->size) return false;
  return esp_partition_read(p, offset, out, len) == ESP_OK;
}

// 任意位置写入 len 字节（扇区级 RMW）
// 会：对涉及扇区 -> 读出 4KB -> 在内存里覆盖对应范围 -> 擦除该扇区 -> 回写 4KB
inline bool shared_write(size_t offset, const void* data, size_t len) {
  const esp_partition_t* p = shared_part();
  if (!p || !data || len == 0) return false;
  if (offset + len > p->size) return false;

  const uint8_t* src = static_cast<const uint8_t*>(data);

  // 覆盖的起止扇区（含首尾）
  size_t start_sector = offset / FLASH_SECTOR;
  size_t end_sector   = (offset + len - 1) / FLASH_SECTOR;

  // 临时 4KB 缓冲
  uint8_t* sector_buf = static_cast<uint8_t*>(malloc(FLASH_SECTOR));
  if (!sector_buf) return false;

  size_t written = 0;
  for (size_t sec = start_sector; sec <= end_sector; ++sec) {
    size_t sec_off_flash = sec * FLASH_SECTOR;

    // 先把整个扇区读到 RAM
    if (esp_partition_read(p, sec_off_flash, sector_buf, FLASH_SECTOR) != ESP_OK) {
      free(sector_buf); return false;
    }

    // 计算本扇区内需要更新的区间 [lo, hi)
    size_t lo = (sec == start_sector) ? (offset % FLASH_SECTOR) : 0;
    size_t hi_exclusive;
    if (sec == end_sector) {
      size_t tail_bytes = (offset + len) - sec * FLASH_SECTOR;
      hi_exclusive = tail_bytes;
    } else {
      hi_exclusive = FLASH_SECTOR;
    }
    size_t span = hi_exclusive - lo;

    // 将源数据拷入 RAM 缓冲的对应位置
    memcpy(sector_buf + lo, src + written, span);

    // 擦除该扇区
    if (esp_partition_erase_range(p, sec_off_flash, FLASH_SECTOR) != ESP_OK) {
      free(sector_buf); return false;
    }
    // 写回整个扇区
    if (esp_partition_write(p, sec_off_flash, sector_buf, FLASH_SECTOR) != ESP_OK) {
      free(sector_buf); return false;
    }

    written += span;
    // 让出一下 CPU，避免长时间阻塞
    delay(0);
  }

  free(sector_buf);
  return true;
}
void switch_to_factory_and_restart() {
  const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
  if (part) {
    esp_ota_set_boot_partition(part);
    // esp_restart();
  } else {
    // 未找到 factory 分区
  }
}


#else
#include "opencv/Arduino.hpp"
#endif
  //     // 写：从偏移 10 写入 N=6 字节 "ABCDEF"
  // const char msg[] = "ABCDEF";
  // bool okw = shared_write(10, msg, sizeof(msg)-1);
  // Serial.printf("write ok=%d\n", okw);

  // uint8_t buf[6] = {0};
  // bool okr = shared_read(10, buf, sizeof(buf));
  // Serial.printf("read ok=%d, data:", okr);
  // for (size_t i=0;i<sizeof(buf);++i) {
  //   Serial.printf(" %02X", buf[i]);
  // }
  // Serial.println();
  // while(1);