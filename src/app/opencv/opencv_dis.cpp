#include "opencv_dis.hpp"
extern "C" {
#include "../driver/st7565.h"
}

#include "Arduino.hpp"
#include <algorithm>
#include <atomic>
#include <vector>

static const char* windowName = "UVE5";
static GLFWwindow* gWindow = nullptr;
static int gTargetW = 0;
static int gTargetH = 0;
static std::atomic<int> gLastKey{-1};
static std::atomic<int> gHeldKey{-1};
static std::vector<uint8_t> gLcdCanvas;
static std::vector<uint8_t> gLcdRgba;
static GLuint gTexture = 0;
static bool gGlReady = false;
static bool gLcdDirty = false;

extern "C" void OPENCV_ShutdownDisplay(void)
{
  if (gTexture != 0) {
    glDeleteTextures(1, &gTexture);
    gTexture = 0;
  }
  if (gWindow) {
    glfwDestroyWindow(gWindow);
    gWindow = nullptr;
  }
  if (gGlReady) {
    glfwTerminate();
    gGlReady = false;
  }
  gLcdCanvas.clear();
  gLcdRgba.clear();
  gTargetW = 0;
  gTargetH = 0;
  gLastKey.store(-1);
  gHeldKey.store(-1);
}

static void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  (void)window;
  if (width <= 0 || height <= 0) {
    return;
  }
  gTargetW = width;
  gTargetH = height;
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  (void)window;
  (void)scancode;
  (void)mods;
  if (action == GLFW_PRESS || action == GLFW_REPEAT) {
    gHeldKey.store(key);
    gLastKey.store(key);
  } else if (action == GLFW_RELEASE) {
    if (gHeldKey.load() == key) {
      gHeldKey.store(-1);
    }
  }
}

int OPENCV_PollKey(void)
{
  if (gWindow) {
    glfwPollEvents();
  }
  const int held = gHeldKey.load();
  if (held >= 0) {
    return held;
  }
  return gLastKey.exchange(-1);
}

static void UpdateDisplay()
{
  if (!gWindow || !gGlReady || gLcdCanvas.empty() || gTargetW <= 0 || gTargetH <= 0) {
    return;
  }
  if (glfwWindowShouldClose(gWindow)) {
    return;
  }

  const size_t pixelCount = static_cast<size_t>(LCD_WIDTH) * static_cast<size_t>(LCD_HEIGHT);
  if (gLcdRgba.size() != pixelCount * 4) {
    gLcdRgba.resize(pixelCount * 4, 0);
  }

  for (size_t i = 0; i < pixelCount; ++i) {
    const uint8_t v = gLcdCanvas[i];
    const size_t base = i * 4;
    if (v == 0) {
      gLcdRgba[base + 0] = 0;
      gLcdRgba[base + 1] = 0;
      gLcdRgba[base + 2] = 0;
      gLcdRgba[base + 3] = 255;
    } else {
      gLcdRgba[base + 0] = 251;
      gLcdRgba[base + 1] = 229;
      gLcdRgba[base + 2] = 122;
      gLcdRgba[base + 3] = 255;
    }
  }

  glBindTexture(GL_TEXTURE_2D, gTexture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, LCD_WIDTH, LCD_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE,
                  gLcdRgba.data());

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glEnable(GL_TEXTURE_2D);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(0.0f, 0.0f);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(static_cast<float>(gTargetW), 0.0f);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(static_cast<float>(gTargetW), static_cast<float>(gTargetH));
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(0.0f, static_cast<float>(gTargetH));
  glEnd();

  glfwSwapBuffers(gWindow);
}

static inline void PresentIfDirty()
{
  if (!gLcdDirty) {
    return;
  }
  gLcdDirty = false;
  UpdateDisplay();
}
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
    
    if (!gLcdCanvas.empty()) {
        if (line < 8 && column < LCD_WIDTH) {
            unsigned count = lineBuffer ? size_defVal : LCD_WIDTH;
            if (count > LCD_WIDTH - column) {
                count = LCD_WIDTH - column;
            }
            const uint8_t fillByte = lineBuffer ? 0 : static_cast<uint8_t>(size_defVal);

            for (unsigned i = 0; i < count; i++) {
                const uint8_t byte = lineBuffer ? lineBuffer[i] : fillByte;
                const unsigned x = column + i;
                for (unsigned bit = 0; bit < 8; bit++) {
                    const unsigned y = static_cast<unsigned>(line) * 8 + bit;
                    if (y < LCD_HEIGHT && x < LCD_WIDTH) {
                        const size_t idx = static_cast<size_t>(y) * LCD_WIDTH + x;
                        if (idx < gLcdCanvas.size()) {
                            gLcdCanvas[idx] = (byte & (1u << bit)) ? 0 : 255;
                        }
                    }
                }
            }
        }
        gLcdDirty = true;
    }
    
    CS_HIGH();
}
void ST7565_Init(void) {
  const int width = 128;
  const int height = 64;
  const int pixelW = 7;
  const int pixelH = 10;

  if (!glfwInit()) {
    return;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);

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
  gWindow = glfwCreateWindow(targetW, targetH, windowName, nullptr, nullptr);
  if (!gWindow) {
    glfwTerminate();
    return;
  }
  glfwMakeContextCurrent(gWindow);
  glfwSwapInterval(1);
  glfwSetKeyCallback(gWindow, KeyCallback);
  glfwSetFramebufferSizeCallback(gWindow, FramebufferSizeCallback);

  int fbW = 0;
  int fbH = 0;
  glfwGetFramebufferSize(gWindow, &fbW, &fbH);
  FramebufferSizeCallback(gWindow, fbW, fbH);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glGenTextures(1, &gTexture);
  glBindTexture(GL_TEXTURE_2D, gTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gLcdCanvas.assign(static_cast<size_t>(LCD_WIDTH) * LCD_HEIGHT, 0);
  gLcdRgba.assign(static_cast<size_t>(LCD_WIDTH) * LCD_HEIGHT * 4, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LCD_WIDTH, LCD_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               gLcdRgba.data());

  gGlReady = true;
  ST7565_BlitFullScreen(); // 初始化时清屏
  PresentIfDirty();

}



// 绘制指定位置的一行
void ST7565_DrawLine(const unsigned int column, const unsigned int line, const uint8_t *pBitmap, const unsigned int size) {
    DrawLine(column, line, pBitmap, size);
    PresentIfDirty();
    
}

// 刷新整个屏幕
void ST7565_BlitFullScreen(void) {
    ST7565_WriteByte(0x40);  // 设置起始行
    
    for (unsigned line = 0; line < FRAME_LINES; line++) {
        DrawLine(0, line + 1, gFrameBuffer[line], LCD_WIDTH);
    }
    PresentIfDirty();
}

// 刷新单行
void ST7565_BlitLine(unsigned line) {
    ST7565_WriteByte(0x40);  // 设置起始行
    DrawLine(0, line + 1, gFrameBuffer[line], LCD_WIDTH);
    PresentIfDirty();
}

// 刷新状态行
void ST7565_BlitStatusLine(void) {
    ST7565_WriteByte(0x40);  // 设置起始行
    DrawLine(0, 0, gStatusLine, LCD_WIDTH);
    PresentIfDirty();
}

// 填充整个屏幕
void ST7565_FillScreen(uint8_t value) {
    for (unsigned i = 0; i < 8; i++) {
        DrawLine(0, i, nullptr, value);
    }
    PresentIfDirty();
}

// 修复接口故障
void ST7565_FixInterfGlitch(void) {
    for (uint8_t i = 0; i < sizeof(cmds); i++) {
        ST7565_WriteByte(cmds[i]);
    }
}
