/*
 HardwareSerial.h - Hardware serial library for Wiring
 Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 Modified 28 September 2010 by Mark Sproul
 Modified 14 August 2012 by Alarus
 Modified 3 December 2013 by Matthijs Kooijman
 Modified 18 December 2014 by Ivan Grokhotkov (esp8266 platform support)
 Modified 31 March 2015 by Markus Sattler (rewrite the code for UART0 + UART1 support in ESP8266)
 Modified 25 April 2015 by Thomas Flayols (add configuration different from 8N1 in ESP8266)
 Modified 13 October 2018 by Jeroen Döll (add baudrate detection)
 Baudrate detection example usage (detection on Serial1):
   void setup() {
     Serial.begin(115200);
     delay(100);
     Serial.println();

     Serial1.begin(0, SERIAL_8N1, -1, -1, true, 11000UL);  // Passing 0 for baudrate to detect it, the last parameter is a timeout in ms

     unsigned long detectedBaudRate = Serial1.baudRate();
     if(detectedBaudRate) {
       Serial.printf("Detected baudrate is %lu\n", detectedBaudRate);
     } else {
       Serial.println("No baudrate detected, Serial1 will not work!");
     }
   }

 Pay attention: the baudrate returned by baudRate() may be rounded, eg 115200 returns 115201
 */

#ifndef HardwareSerial_h
#define HardwareSerial_h

#include <inttypes.h>
#include "Arduino.hpp"

enum SerialConfig {
    SERIAL_5N1 = 0x8000010,
    SERIAL_6N1 = 0x8000014,
    SERIAL_7N1 = 0x8000018,
    SERIAL_8N1 = 0x800001c,
    SERIAL_5N2 = 0x8000030,
    SERIAL_6N2 = 0x8000034,
    SERIAL_7N2 = 0x8000038,
    SERIAL_8N2 = 0x800003c,
    SERIAL_5E1 = 0x8000012,
    SERIAL_6E1 = 0x8000016,
    SERIAL_7E1 = 0x800001a,
    SERIAL_8E1 = 0x800001e,
    SERIAL_5E2 = 0x8000032,
    SERIAL_6E2 = 0x8000036,
    SERIAL_7E2 = 0x800003a,
    SERIAL_8E2 = 0x800003e,
    SERIAL_5O1 = 0x8000013,
    SERIAL_6O1 = 0x8000017,
    SERIAL_7O1 = 0x800001b,
    SERIAL_8O1 = 0x800001f,
    SERIAL_5O2 = 0x8000033,
    SERIAL_6O2 = 0x8000037,
    SERIAL_7O2 = 0x800003b,
    SERIAL_8O2 = 0x800003f
};



typedef enum {
    UART_NO_ERROR,
    UART_BREAK_ERROR,
    UART_BUFFER_FULL_ERROR,
    UART_FIFO_OVF_ERROR,
    UART_FRAME_ERROR,
    UART_PARITY_ERROR
} hardwareSerial_error_t;


#ifndef ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE
  #define ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE 2048
#endif

#ifndef ARDUINO_SERIAL_EVENT_TASK_PRIORITY
  #define ARDUINO_SERIAL_EVENT_TASK_PRIORITY (configMAX_PRIORITIES-1)
#endif

#ifndef ARDUINO_SERIAL_EVENT_TASK_RUNNING_CORE
  #define ARDUINO_SERIAL_EVENT_TASK_RUNNING_CORE -1
#endif

// UART0 pins are defined by default by the bootloader.
// The definitions for SOC_* should not be changed unless the bootloader pins
// have changed and you know what you are doing.

#ifndef SOC_RX0
  #if CONFIG_IDF_TARGET_ESP32
    #define SOC_RX0 (gpio_num_t)3
  #elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
    #define SOC_RX0 (gpio_num_t)44
  #elif CONFIG_IDF_TARGET_ESP32C3
    #define SOC_RX0 (gpio_num_t)20
  #endif
#endif

#ifndef SOC_TX0
  #if CONFIG_IDF_TARGET_ESP32
    #define SOC_TX0 (gpio_num_t)1
  #elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
    #define SOC_TX0 (gpio_num_t)43
  #elif CONFIG_IDF_TARGET_ESP32C3
    #define SOC_TX0 (gpio_num_t)21
  #endif
#endif

// Default pins for UART1 are arbitrary, and defined here for convenience.
#if SOC_UART_NUM > 1
#ifndef RX1
    #if CONFIG_IDF_TARGET_ESP32
      #define RX1 (gpio_num_t)9
    #elif CONFIG_IDF_TARGET_ESP32S2
      #define RX1 (gpio_num_t)18
    #elif CONFIG_IDF_TARGET_ESP32C3
      #define RX1 (gpio_num_t)18
    #elif CONFIG_IDF_TARGET_ESP32S3
      #define RX1 (gpio_num_t)15
    #endif
  #endif

  #ifndef TX1
    #if CONFIG_IDF_TARGET_ESP32
      #define TX1 (gpio_num_t)10
    #elif CONFIG_IDF_TARGET_ESP32S2
      #define TX1 (gpio_num_t)17
    #elif CONFIG_IDF_TARGET_ESP32C3
      #define TX1 (gpio_num_t)19
    #elif CONFIG_IDF_TARGET_ESP32S3
      #define TX1 (gpio_num_t)16
    #endif
  #endif
#endif /* SOC_UART_NUM > 1 */

// Default pins for UART2 are arbitrary, and defined here for convenience.

#if SOC_UART_NUM > 2
  #ifndef RX2
    #if CONFIG_IDF_TARGET_ESP32
      #define RX2 (gpio_num_t)16
    #elif CONFIG_IDF_TARGET_ESP32S3
      #define RX2 (gpio_num_t)19
    #endif
  #endif

  #ifndef TX2
    #if CONFIG_IDF_TARGET_ESP32
      #define TX2 (gpio_num_t)17
    #elif CONFIG_IDF_TARGET_ESP32S3
      #define TX2 (gpio_num_t)20
    #endif
  #endif
#endif /* SOC_UART_NUM > 2 */

extern void serialEventRun(void) __attribute__((weak));

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 0
#endif
#if ARDUINO_USB_CDC_ON_BOOT //Serial used for USB CDC
#else
#endif
#if SOC_UART_NUM > 1
extern HardwareSerial Serial1;
#endif
#if SOC_UART_NUM > 2
extern HardwareSerial Serial2;
#endif

#endif // !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
#endif // HardwareSerial_h
