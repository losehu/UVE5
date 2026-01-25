// #include <Arduino.h>

// // UART0: TX=GPIO43, RX=GPIO44
// static constexpr int UART0_TX_PIN = 43;
// static constexpr int UART0_RX_PIN = 44;
// static constexpr uint32_t UART0_BAUD = 115200;

// // 使用 UART0（硬件串口 0）
// static HardwareSerial Uart0(0);

// void setup() {
//   // USB 串口：用于日志（通常是 CDC）
//   Serial.begin(115200);
//   delay(1200);
//   Serial.println("\n=== UART0 Test (GPIO43 TX, GPIO44 RX) ===");
//   Serial.println("USB Serial <-> UART0 bridge enabled.");
//   Serial.println("- Connect USB-TTL: TTL_RX<-GPIO43, TTL_TX->GPIO44, GND common");
//   Serial.println("- Or short GPIO43 <-> GPIO44 for loopback\n");

//   // UART0：按你指定的引脚映射
//   Uart0.begin(UART0_BAUD, SERIAL_8N1, UART0_RX_PIN, UART0_TX_PIN);
//   Serial.printf("UART0 started: baud=%lu TX=%d RX=%d\n", (unsigned long)UART0_BAUD, UART0_TX_PIN, UART0_RX_PIN);
// pinMode(41,OUTPUT);
// }

// void loop() {
//   // 1) USB->UART0
//  digitalWrite(41,HIGH);

// delay(1000);
//     digitalWrite(41,LOW);
//     delay(1000);
// Serial.println("Hello World");
// }