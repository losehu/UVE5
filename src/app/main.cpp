#include <Arduino.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "lib/shared_flash.h"
#include "lib/wifi.h"
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// === 自定义广播名称（手机可见） ===
const char* BLE_NAME = "ESP32S3_BLE_DEMO";

// === 全局对象 ===
BLEServer* pServer = nullptr;
BLEAdvertising* pAdvertising = nullptr;


// ====== 自动重连与超时控制 ======
const unsigned long WIFI_TIMEOUT_MS = 15000;  // 连接超时（15秒）



#include "esp_ota_ops.h"
#include "esp_system.h"

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

void setup() {
switch_to_factory_and_restart();
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n[APP0] Hello from main app!");



 // 初始化 BLE
  BLEDevice::init(BLE_NAME);

  // 创建 BLE 服务器（即 GATT Server）
  pServer = BLEDevice::createServer();

  // 获取广播对象
  pAdvertising = BLEDevice::getAdvertising();

  // ====== 配置广播数据 ======
  BLEAdvertisementData advData;
  advData.setName(BLE_NAME);               // 设备名称
  advData.setManufacturerData("ESP32S3");  // 制造商数据，可自定义

  // 设定广播数据
  pAdvertising->setAdvertisementData(advData);

  // ====== 开始广播 ======
  pAdvertising->start();

  Serial.println("✅ 蓝牙广播已启动！");
  Serial.println("📱 现在可以在手机上扫描到设备名：ESP32S3_BLE_DEMO");


    pinMode(13, INPUT_PULLUP);
}






void loop() {

  // TODO: 你的主应用逻辑...
  static uint32_t t0 = millis();
  if (millis() - t0 > 1000) {
    Serial.println("[BLE11] 广播中...");

  delay(5000);
    t0 = millis();
  }
}





