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
// }

// void loop() {
//   // 1) USB->UART0
//   while (Serial.available() > 0) {
//     const int c = Serial.read();
//     if (c >= 0) {
//       Uart0.write((uint8_t)c);
//     }
//   }

//   // 2) UART0->USB
//   while (Uart0.available() > 0) {
//     const int c = Uart0.read();
//     if (c >= 0) {
//       Serial.write((uint8_t)c);
//     }
//   }

//   // 3) 周期性从 UART0 发包，验证 TX
//   static uint32_t lastMs = 0;
//   static uint32_t counter = 0;
//   const uint32_t now = millis();
//   if ((uint32_t)(now - lastMs) >= 1000u) {
//     lastMs = now;
//     Uart0.printf("[UART0] ping %lu\r\n", (unsigned long)counter++);
//   }
// }