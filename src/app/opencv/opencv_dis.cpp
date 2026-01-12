#include "opencv_dis.hpp"
extern "C" {
#include "../driver/st7565.h"
}

#include "Arduino.hpp"
cv::Mat display;
char* windowName = "UVE5";
 bool getScreenSize(int& width, int& height) {
#if defined(_WIN32)
  width = GetSystemMetrics(SM_CXSCREEN);
  height = GetSystemMetrics(SM_CYSCREEN);
  return width > 0 && height > 0;
#elif defined(__APPLE__)
  const auto displayId = CGMainDisplayID();
  const CGRect bounds = CGDisplayBounds(displayId);
  width = static_cast<int>(bounds.size.width);
  height = static_cast<int>(bounds.size.height);
  return width > 0 && height > 0;
#elif defined(__linux__)
  Display* display = XOpenDisplay(nullptr);
  if (!display) {
    return false;
  }
  const int screen = DefaultScreen(display);
  width = DisplayWidth(display, screen);
  height = DisplayHeight(display, screen);
  XCloseDisplay(display);
  return width > 0 && height > 0;
#else
  (void)width;
  (void)height;
  return false;
#endif
}

 void drawTest(cv::Mat& img) {
  img.setTo(cv::Scalar(20, 20, 20));

  cv::circle(img, cv::Point(16, 16), 2, cv::Scalar(0, 255, 0), cv::FILLED);
  cv::circle(img, cv::Point(112, 48), 2, cv::Scalar(0, 0, 255), cv::FILLED);
  cv::circle(img, cv::Point(64, 32), 2, cv::Scalar(255, 255, 0), cv::FILLED);

  cv::line(img, cv::Point(0, 0), cv::Point(127, 63), cv::Scalar(255, 0, 0), 1);
  cv::line(img, cv::Point(0, 63), cv::Point(127, 0), cv::Scalar(255, 0, 255), 1);
  cv::line(img, cv::Point(0, 32), cv::Point(127, 32), cv::Scalar(0, 255, 255), 1);
}




 // 全局帧缓冲区
uint8_t gStatusLine[LCD_WIDTH];
uint8_t gFrameBuffer[FRAME_LINES][LCD_WIDTH];

// ST7565命令定义
static const uint8_t ST7565_CMD_SOFTWARE_RESET = 0xE2;
static const uint8_t ST7565_CMD_BIAS_SELECT = 0xA2;
static const uint8_t ST7565_CMD_COM_DIRECTION = 0xC0;
static const uint8_t ST7565_CMD_SEG_DIRECTION = 0xA0;
static const uint8_t ST7565_CMD_INVERSE_DISPLAY = 0xA6;
static const uint8_t ST7565_CMD_ALL_PIXEL_ON = 0xA4;
static const uint8_t ST7565_CMD_REGULATION_RATIO = 0x20;
static const uint8_t ST7565_CMD_SET_EV = 0x81;
static const uint8_t ST7565_CMD_POWER_CIRCUIT = 0x28;
static const uint8_t ST7565_CMD_SET_START_LINE = 0x40;
static const uint8_t ST7565_CMD_DISPLAY_ON_OFF = 0xAE;



// 初始化命令序列
static uint8_t cmds[] = {
    ST7565_CMD_BIAS_SELECT | 0,            // Select bias setting: 1/9
    ST7565_CMD_COM_DIRECTION | (0 << 3),   // Set output direction of COM: normal
    ST7565_CMD_SEG_DIRECTION | 1,          // Set scan direction of SEG: reverse
    ST7565_CMD_INVERSE_DISPLAY | 0,        // Inverse Display: false
    ST7565_CMD_ALL_PIXEL_ON | 0,           // All Pixel ON: false - normal display
    ST7565_CMD_REGULATION_RATIO | (4 << 0),// Regulation Ratio 5.0
    ST7565_CMD_SET_EV,                     // Set contrast
    31,                                     // Contrast value
};

// 内部辅助函数
static inline void CS_LOW() {
    digitalWrite(ST7565_PIN_CS, LOW);
}

static inline void CS_HIGH() {
    digitalWrite(ST7565_PIN_CS, HIGH);
}

static inline void A0_LOW() {
    digitalWrite(ST7565_PIN_A0, LOW);
}

static inline void A0_HIGH() {
    digitalWrite(ST7565_PIN_A0, HIGH);
}

// 硬件复位
void ST7565_HardwareReset(void) {
    digitalWrite(ST7565_PIN_RST, HIGH);
    delay(1);
    digitalWrite(ST7565_PIN_RST, LOW);
    delay(20);
    digitalWrite(ST7565_PIN_RST, HIGH);
    delay(120);
}

// 写入单字节命令
void ST7565_WriteByte(uint8_t value) {
    A0_LOW();  // 命令模式
    CS_LOW();
    CS_HIGH();
}

// 选择列和行
void ST7565_SelectColumnAndLine(uint8_t column, uint8_t line) {
    A0_LOW();  // 命令模式
    CS_LOW();

    CS_HIGH();
}




// 绘制一行数据 (内部函数)
static void DrawLine(uint8_t column, uint8_t line, const uint8_t *lineBuffer, unsigned size_defVal) {
    ST7565_SelectColumnAndLine(column + 4, line);
    
    A0_HIGH();  // 数据模式
    CS_LOW();
    
    if (lineBuffer) {
        // 发送缓冲区数据
        for (unsigned i = 0; i < size_defVal; i++) {
        }
    } else {
        // 填充固定值 - 这里的 size_defVal 参数实际表示填充值
        for (unsigned i = 0; i < LCD_WIDTH; i++) {
        }
    }
    
    CS_HIGH();
}
void ST7565_Init(void) {
  const int width = 128;
  const int height = 64;
  const int pixelW = 7;
  const int pixelH = 10;

  cv::Mat img(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
  drawTest(img);

  cv::namedWindow(windowName, cv::WINDOW_NORMAL);

  int screenW = 0;
  int screenH = 0;
  const bool hasScreen = getScreenSize(screenW, screenH);
  int targetW = width * pixelW;
  int targetH = height * pixelH;
  if (hasScreen) {
    const double maxW = screenW * 0.75;
    const double maxH = screenH * 0.75;
    const double scale = std::min(maxW / targetW, maxH / targetH);
    if (scale < 1.0) {
      targetW = static_cast<int>(targetW * scale);
      targetH = static_cast<int>(targetH * scale);
    }
  }

  cv::resize(img, display, cv::Size(targetW, targetH), 0, 0, cv::INTER_NEAREST);
  cv::resizeWindow(windowName, targetW, targetH);
//   cv::imshow(windowName, display);
//   cv::waitKey(0);
//   return 0;
    ST7565_FillScreen(0x00);

}



// 绘制指定位置的一行
void ST7565_DrawLine(const unsigned int column, const unsigned int line, const uint8_t *pBitmap, const unsigned int size) {
    DrawLine(column, line, pBitmap, size);
}

// 刷新整个屏幕
void ST7565_BlitFullScreen(void) {
    ST7565_WriteByte(0x40);  // 设置起始行
    
    for (unsigned line = 0; line < FRAME_LINES; line++) {
        DrawLine(0, line + 1, gFrameBuffer[line], LCD_WIDTH);
    }
}

// 刷新单行
void ST7565_BlitLine(unsigned line) {
    ST7565_WriteByte(0x40);  // 设置起始行
    DrawLine(0, line + 1, gFrameBuffer[line], LCD_WIDTH);
}

// 刷新状态行
void ST7565_BlitStatusLine(void) {
    ST7565_WriteByte(0x40);  // 设置起始行
    DrawLine(0, 0, gStatusLine, LCD_WIDTH);
}

// 填充整个屏幕
void ST7565_FillScreen(uint8_t value) {
    for (unsigned i = 0; i < 8; i++) {
        DrawLine(0, i, nullptr, value);
    }
}

// 修复接口故障
void ST7565_FixInterfGlitch(void) {
    for (uint8_t i = 0; i < sizeof(cmds); i++) {
        ST7565_WriteByte(cmds[i]);
    }
}



