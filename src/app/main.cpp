#include <Arduino.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "lib/shared_flash.h"

#include "driver/st7565.h"
#include "driver/eeprom.h"
#include "driver/i2c1.h"
#include "driver/keyboard.h"
#include "driver/adc1.h"
#include "driver/backlight.h"
#include "driver/uart1.h"
#include "driver/bk1080.h"
#include "driver/bk4819.h"
// 按键名称映射表
const char* keyNames[] = {
    "0",      // KEY_0
    "1",      // KEY_1
    "2",      // KEY_2
    "3",      // KEY_3
    "4",      // KEY_4
    "5",      // KEY_5
    "6",      // KEY_6
    "7",      // KEY_7
    "8",      // KEY_8
    "9",      // KEY_9
    "MENU",   // KEY_MENU
    "UP",     // KEY_UP
    "DOWN",   // KEY_DOWN
    "EXIT",   // KEY_EXIT
    "STAR",   // KEY_STAR
    "F",      // KEY_F
    "PTT",    // KEY_PTT
    "SIDE2",  // KEY_SIDE2
    "SIDE1",  // KEY_SIDE1
    "INVALID" // KEY_INVALID
};

static uint8_t lastKeyCode = KEY_INVALID;
static uint32_t lastKeyPressTime = 0;

// 电池监测变量
uint16_t gBatteryVoltages[4] = {0};
uint16_t gBatteryCurrent = 0;
uint8_t gBatteryCheckCounter = 0;
uint16_t gBatteryCalibration[6] = {1900, 2000, 0, 1600, 0, 2300}; // 默认校准值

// 电压转百分比查找表 (单位: 0.01V)
const uint16_t batteryVoltageTable[][2] = {
    {890, 100},  // 8.9V - 100%
    {840, 90},   // 8.4V - 90%
    {810, 80},   // 8.1V - 80%
    {790, 70},   // 7.9V - 70%
    {770, 60},   // 7.7V - 60%
    {750, 50},   // 7.5V - 50%
    {730, 40},   // 7.3V - 40%
    {710, 30},   // 7.1V - 30%
    {690, 20},   // 6.9V - 20%
    {670, 10},   // 6.7V - 10%
    {650, 5},    // 6.5V - 5%
    {630, 0},    // 6.3V - 0%
};

// 电压转百分比函数
unsigned int BATTERY_VoltsToPercent(unsigned int voltage_10mV) {
    for (unsigned int i = 0; i < sizeof(batteryVoltageTable)/sizeof(batteryVoltageTable[0]); i++) {
        if (voltage_10mV >= batteryVoltageTable[i][0]) {
            return batteryVoltageTable[i][1];
        }
    }
    return 0;
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

 

/**
 * 在显示屏上显示按键测试信息
 */
void testKeyboard_Display(void) {
  static uint32_t lastUpdateTime = 0;
  uint32_t currentTime = millis();
  
  // 每100ms扫描一次按键
  if (currentTime - lastUpdateTime < 100) {
    return;
  }
  lastUpdateTime = currentTime;
  
  uint8_t keyCode = KEYBOARD_Poll();
  
  // 清屏
  ST7565_FillScreen(0x00);
  
  // 显示标题
  memset(gFrameBuffer[0], 0x00, LCD_WIDTH);
  
  // 在帧缓冲区绘制文本（简单方式：使用char显示）
  char titleStr[30];
  sprintf(titleStr, "=== KEY TEST ===");
  
  // 第一行：标题
  snprintf(titleStr, sizeof(titleStr), "KEY TEST");
  Serial.printf("Display update: %s\n", titleStr);
  
  // 第二行：当前按键状态
  char keyStr[40];
  if (keyCode != KEY_INVALID) {
    snprintf(keyStr, sizeof(keyStr), "Key: %s (0x%02X)", keyNames[keyCode], keyCode);
    lastKeyCode = keyCode;
    lastKeyPressTime = currentTime;
  } else {
    // 显示上次按键的信息（保持显示2秒）
    if (currentTime - lastKeyPressTime < 2000 && lastKeyCode != KEY_INVALID) {
      snprintf(keyStr, sizeof(keyStr), "Last: %s", keyNames[lastKeyCode]);
    } else {
      snprintf(keyStr, sizeof(keyStr), "No key pressed");
    }
  }
  
  // 第三行：按键代码说明
  char infoStr[40];
  snprintf(infoStr, sizeof(infoStr), "Code range: 0-%d", KEY_INVALID);
  
  // 第四行：实时显示
  char timeStr[40];
  snprintf(timeStr, sizeof(timeStr), "Time: %lu ms", currentTime);
  
  // 在串口输出（因为直接在屏幕上绘制文本需要字体库）
  Serial.printf("[%lu] %s\n", currentTime, keyStr);
  
  // 简单的图形化显示：用像素条显示按键状态
  if (keyCode != KEY_INVALID) {
    // 填充第2行为黑色表示有按键
    for (int col = 0; col < LCD_WIDTH; col++) {
      gFrameBuffer[1][col] = 0xFF;
    }
  }
  
  // 填充第3行显示按键代码的条形图
  if (keyCode != KEY_INVALID && keyCode < KEY_INVALID) {
    int barWidth = (keyCode * LCD_WIDTH) / KEY_INVALID;
    for (int col = 0; col < barWidth && col < LCD_WIDTH; col++) {
      gFrameBuffer[2][col] = 0xFF;
    }
  }
  
  // 更新显示
  ST7565_BlitFullScreen();
}


void setup() {
  switch_to_factory_and_restart();
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n=== BK4819 Radio IC Test ===");
  Serial.println("Initializing devices...");
  
  // 初始化设备
  ST7565_Init();
  I2C_Init();
  KEYBOARD_Init();
  ADC_Configure();
  BACKLIGHT_InitHardware();
  UART_Init(115200);
  
  Serial.println("All devices initialized!");
  
  // 初始化 BK4819
  Serial.println("\nInitializing BK4819...");
  BK4819_Init();
  Serial.println("BK4819 initialized!");
  
  // 设置频率为 145.000 MHz (UHF 业余频道)
  uint32_t frequency = 145000000;  // 145 MHz in Hz
  Serial.printf("Setting frequency to %.3f MHz\n", frequency / 1000000.0f);
  BK4819_SetFrequency(frequency);
  
  // 打开接收
  Serial.println("Turning on RX...");
  BK4819_RX_TurnOn();
  
  Serial.println("\n=== Test Started ===");
  Serial.println("Testing BK4819 read operations...\n");
}

// 测试计数器
static uint32_t testCounter = 0;
static uint32_t lastUpdateTime = 0;

void loop() {
  uint32_t currentTime = millis();
  
  // 每 500ms 进行一次测试
  if (currentTime - lastUpdateTime < 500) {
    delay(10);
    return;
  }
  lastUpdateTime = currentTime;
  
  testCounter++;
  
  // 读取 RSSI (接收信号强度)
  int16_t rssi = BK4819_GetRSSI_dBm();
  Serial.printf("[%04lu] RSSI: %d dBm\n", testCounter, rssi);
  
  // 读取 RX 增益
  int8_t rxGain = BK4819_GetRxGain_dB();
  Serial.printf("        RX Gain: %d dB\n", rxGain);
  
  // 读取音频幅度输出
  uint16_t audioAmp = BK4819_GetVoiceAmplitudeOut();
  Serial.printf("        Audio Amplitude: %u\n", audioAmp);
  
  // 读取 glitch 指示器 (干扰指示)
  uint8_t glitchInd = BK4819_GetGlitchIndicator();
  Serial.printf("        Glitch Indicator: %u\n", glitchInd);
  
  // 读取 noise 指示器
  uint8_t noiseInd = BK4819_GetExNoiceIndicator();
  Serial.printf("        Noise Indicator: %u\n", noiseInd);
  
  // 每 10 次读取一次完整信息
  if (testCounter % 10 == 0) {
    Serial.println("---");
    
    // 检查是否有 DTMF 码
    uint8_t dtmfCode = BK4819_GetDTMF_5TONE_Code();
    if (dtmfCode != 0xFF) {
      Serial.printf("DTMF Code detected: 0x%02X\n", dtmfCode);
    }
    
    // 检查 CSS 扫描结果
    uint32_t cdcssFreq;
    uint16_t ctcssFreq;
    BK4819_CssScanResult_t cssResult = BK4819_GetCxCSSScanResult(&cdcssFreq, &ctcssFreq);
    if (cssResult != BK4819_CSS_RESULT_NOT_FOUND) {
      if (cssResult == BK4819_CSS_RESULT_CDCSS) {
        Serial.printf("CDCSS Code found: %lu\n", cdcssFreq);
      } else if (cssResult == BK4819_CSS_RESULT_CTCSS) {
        Serial.printf("CTCSS Tone found: %u Hz\n", ctcssFreq);
      }
    }
    
    Serial.println("");
  }
  
  // 按键测试：PTT 按钮用于测试发送
  uint8_t keyCode = KEYBOARD_Poll();
  if (keyCode == KEY_PTT) {
    Serial.println("\n*** PTT Key Pressed - Testing TX ***");
    
    // 进入 TX 模式
    BK4819_PrepareTransmit();
    BK4819_TxOn_Beep();
    
    // 播放 1000Hz 音调 (1 秒)
    BK4819_PlayTone(1000, false);
    delay(1000);
    BK4819_EnterTxMute();
    
    // 关闭 TX
    delay(500);
    BK4819_ExitSubAu();
    
    Serial.println("TX Test Complete\n");
  }
  
  // 按键测试：MENU 用于改变频率
  if (keyCode == KEY_MENU) {
    // 切换到 146 MHz
    uint32_t newFreq = 146000000;
    Serial.printf("\n*** Switching to %.3f MHz ***\n", newFreq / 1000000.0f);
    BK4819_SetFrequency(newFreq);
  }
}
