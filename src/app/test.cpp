// #include <Arduino.h>
// #include "esp_ota_ops.h"
// #include "esp_partition.h"
// #include "lib/shared_flash.h"
// #include "lib/wifi.h"
// #include <WiFi.h>
// #include <BLEDevice.h>
// #include <BLEUtils.h>
// #include <BLEServer.h>
// #include "driver/st7565.h"

// // === 自定义广播名称（手机可见） ===
// const char* BLE_NAME = "ESP32S3_BLE_DEMO";

// // === 全局对象 ===
// BLEServer* pServer = nullptr;
// BLEAdvertising* pAdvertising = nullptr;

// // ====== 自动重连与超时控制 ======
// const unsigned long WIFI_TIMEOUT_MS = 15000;  // 连接超时（15秒）

// void switch_to_factory_and_restart() {
//   const esp_partition_t* part = esp_partition_find_first(
//       ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
//   if (part) {
//     esp_ota_set_boot_partition(part);
//     // esp_restart();
//   } else {
//     // 未找到 factory 分区
//   }
// }

// // 引脚诊断函数
// void pinDiagnostic() {
//   Serial.println("\n=== Pin Diagnostic ===");
  
//   // 测试所有引脚
//   Serial.println("Testing RST pin...");
//   digitalWrite(ST7565_PIN_RST, HIGH);
//   delay(100);
//   digitalWrite(ST7565_PIN_RST, LOW);
//   delay(100);
//   digitalWrite(ST7565_PIN_RST, HIGH);
//   Serial.println("  RST: OK");
  
//   Serial.println("Testing CS pin...");
//   digitalWrite(ST7565_PIN_CS, HIGH);
//   delay(100);
//   digitalWrite(ST7565_PIN_CS, LOW);
//   delay(100);
//   digitalWrite(ST7565_PIN_CS, HIGH);
//   Serial.println("  CS: OK");
  
//   Serial.println("Testing A0 pin...");
//   digitalWrite(ST7565_PIN_A0, HIGH);
//   delay(100);
//   digitalWrite(ST7565_PIN_A0, LOW);
//   delay(100);
//   digitalWrite(ST7565_PIN_A0, HIGH);
//   Serial.println("  A0: OK");
  
//   Serial.println("=== Diagnostic Complete ===\n");
// }

// // 简单测试函数 - 只测试基本功能
// void simpleTest() {
//   Serial.println("\n=== ST7565 Simple Test ===");
  
//   // 测试1: 清屏
//   Serial.println("Test 1: Clear screen (0x00)");
//   ST7565_FillScreen(0x00);
//   delay(2000);
  
//   // 测试2: 全屏点亮
//   Serial.println("Test 2: Fill screen (0xFF)");
//   ST7565_FillScreen(0xFF);
//   delay(3000);
  
//   // 测试3: 交替测试
//   Serial.println("Test 3: Alternating test");
//   for (int i = 0; i < 5; i++) {
//     Serial.printf("  Iteration %d: 0xAA\n", i+1);
//     ST7565_FillScreen(0xAA);
//     delay(500);
//     Serial.printf("  Iteration %d: 0x55\n", i+1);
//     ST7565_FillScreen(0x55);
//     delay(500);
//   }
  
//   Serial.println("=== Simple Test Complete ===\n");
// }

// // ST7565 测试函数
// void testST7565() {
//   Serial.println("\n=== ST7565 LCD Full Test ===");
  
//   // 测试1: 清屏
//   Serial.println("Test 1: Clear screen (0x00)");
//   ST7565_FillScreen(0x00);
//   delay(1000);
  
//   // 测试2: 全屏点亮
//   Serial.println("Test 2: Fill screen (0xFF)");
//   ST7565_FillScreen(0xFF);
//   delay(2000);
  
//   // 测试3: 棋盘格模式
//   Serial.println("Test 3: Checkerboard pattern");
//   for (int i = 0; i < FRAME_LINES; i++) {
//     for (int j = 0; j < LCD_WIDTH; j++) {
//       gFrameBuffer[i][j] = ((i + j) % 2) ? 0xFF : 0x00;
//     }
//   }
//   ST7565_BlitFullScreen();
//   delay(2000);
  
//   // 测试4: 水平条纹
//   Serial.println("Test 4: Horizontal stripes");
//   for (int i = 0; i < FRAME_LINES; i++) {
//     uint8_t pattern = (i % 2) ? 0xFF : 0x00;
//     memset(gFrameBuffer[i], pattern, LCD_WIDTH);
//   }
//   ST7565_BlitFullScreen();
//   delay(2000);
  
//   // 测试5: 垂直条纹
//   Serial.println("Test 5: Vertical stripes");
//   for (int i = 0; i < FRAME_LINES; i++) {
//     for (int j = 0; j < LCD_WIDTH; j++) {
//       gFrameBuffer[i][j] = (j % 8 < 4) ? 0xFF : 0x00;
//     }
//   }
//   ST7565_BlitFullScreen();
//   delay(2000);
  
//   // 测试6: 渐变效果
//   Serial.println("Test 6: Gradient pattern");
//   for (int i = 0; i < FRAME_LINES; i++) {
//     for (int j = 0; j < LCD_WIDTH; j++) {
//       gFrameBuffer[i][j] = (uint8_t)(j * 2);
//     }
//   }
//   ST7565_BlitFullScreen();
//   delay(2000);
  
//   // 测试7: 绘制边框
//   Serial.println("Test 7: Draw border");
//   memset(gFrameBuffer, 0x00, sizeof(gFrameBuffer));
//   // 顶部和底部
//   memset(gFrameBuffer[0], 0xFF, LCD_WIDTH);
//   memset(gFrameBuffer[FRAME_LINES-1], 0xFF, LCD_WIDTH);
//   // 左侧和右侧
//   for (int i = 0; i < FRAME_LINES; i++) {
//     gFrameBuffer[i][0] = 0xFF;
//     gFrameBuffer[i][1] = 0xFF;
//     gFrameBuffer[i][LCD_WIDTH-1] = 0xFF;
//     gFrameBuffer[i][LCD_WIDTH-2] = 0xFF;
//   }
//   ST7565_BlitFullScreen();
//   delay(2000);
  
//   // 测试8: 动画效果 - 移动的竖线
//   Serial.println("Test 8: Moving vertical line");
//   for (int pos = 0; pos < LCD_WIDTH; pos += 2) {
//     memset(gFrameBuffer, 0x00, sizeof(gFrameBuffer));
//     for (int i = 0; i < FRAME_LINES; i++) {
//       if (pos < LCD_WIDTH) gFrameBuffer[i][pos] = 0xFF;
//     }
//     ST7565_BlitFullScreen();
//     delay(20);
//   }
  
//   // 测试9: 状态行测试
//   Serial.println("Test 9: Status line test");
//   memset(gFrameBuffer, 0x00, sizeof(gFrameBuffer));
//   for (int i = 0; i < LCD_WIDTH; i++) {
//     gStatusLine[i] = (i % 16 < 8) ? 0xFF : 0x00;
//   }
//   ST7565_BlitStatusLine();
//   ST7565_BlitFullScreen();
//   delay(2000);
  
//   // 测试10: 逐行刷新测试
//   Serial.println("Test 10: Line by line refresh");
//   memset(gFrameBuffer, 0x00, sizeof(gFrameBuffer));
//   for (int line = 0; line < FRAME_LINES; line++) {
//     memset(gFrameBuffer[line], 0xFF, LCD_WIDTH);
//     ST7565_BlitLine(line);
//     delay(300);
//   }
//   delay(1000);
  
//   // 清屏
//   ST7565_FillScreen(0x00);
//   Serial.println("=== Test Complete ===\n");
// }

// void setup() {
//   switch_to_factory_and_restart();
//   Serial.begin(115200);
//   delay(2000);

//   // RST IO45
// //CS IO7
// //MOSI IO4
// //CLK IO6
// //A0 IO5
// // left
//   pinMode(4, OUTPUT);
//   pinMode(5, OUTPUT);
//   pinMode(6, OUTPUT);
//   pinMode(7, OUTPUT);
//   pinMode(8, OUTPUT);
//   pinMode(45, OUTPUT);
// // down
//   pinMode(0, OUTPUT);
//   pinMode(1, OUTPUT);
//   pinMode(2, OUTPUT);
//   pinMode(3, OUTPUT);
//   pinMode(33, OUTPUT);
//   pinMode(34, OUTPUT);
//   pinMode(47, OUTPUT);

// // right
//   pinMode(17, OUTPUT);
//   pinMode(18, OUTPUT);
//   pinMode(21, OUTPUT);
//   pinMode(43, OUTPUT);
//   pinMode(48, OUTPUT);

// //up

//   pinMode(9, OUTPUT);
//   pinMode(10, OUTPUT);
//   pinMode(11, OUTPUT);
//   pinMode(12, OUTPUT);
//   pinMode(13, OUTPUT);
//   pinMode(14, OUTPUT);
//   pinMode(44, OUTPUT);

// }

// void loop() {
// Serial.println("=== Test start ===\n");

// digitalWrite(4,HIGH);
// digitalWrite(5,HIGH);
// digitalWrite(6,HIGH);
// digitalWrite(7,HIGH);
// digitalWrite(8,HIGH);
// digitalWrite(45,HIGH);

// digitalWrite(0,HIGH);
// digitalWrite(1,HIGH);
// digitalWrite(2,HIGH);
// digitalWrite(3,HIGH);
// digitalWrite(33,HIGH);
// digitalWrite(34,HIGH);
// digitalWrite(47,HIGH);

// digitalWrite(17,HIGH);
// digitalWrite(18,HIGH);
// digitalWrite(21,HIGH);
// digitalWrite(48,HIGH);
// digitalWrite(43,HIGH);



//   digitalWrite(9, HIGH);
//   digitalWrite(10, HIGH);
//   digitalWrite(11, HIGH);
//   digitalWrite(12, HIGH);
//   digitalWrite(13, HIGH);
//   digitalWrite(14, HIGH);
//   digitalWrite(44, HIGH);
//   delay(1000);
// digitalWrite(4,LOW);
// digitalWrite(5,LOW);
// digitalWrite(6,LOW);
// digitalWrite(7,LOW);
// digitalWrite(8,LOW);
// digitalWrite(45,LOW);

// digitalWrite(0,LOW);
// digitalWrite(1,LOW);
// digitalWrite(2,LOW);
// digitalWrite(3,LOW);
// digitalWrite(33,LOW);
// digitalWrite(34,LOW);
// digitalWrite(47,LOW);

// digitalWrite(17,LOW);
// digitalWrite(18,LOW);
// digitalWrite(21,LOW);
// digitalWrite(48,LOW);
// digitalWrite(43,LOW);


//   digitalWrite(9, LOW);
//   digitalWrite(10, LOW);
//   digitalWrite(11, LOW);
//   digitalWrite(12, LOW);
//   digitalWrite(13, LOW);
//   digitalWrite(14, LOW);
//   digitalWrite(44, LOW);
//   delay(1000);


// }

// //51,9,10,11,12,13 zuo
// //5 8 7 6 39 38 37 xia 
// //you 49 36 24 23 27 ---17 18 21 48 43
// //xia 14～19 50