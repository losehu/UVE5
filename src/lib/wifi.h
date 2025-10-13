#include <Arduino.h>
#include <WiFi.h>

//================ 配置/类型 =================

enum WifiConnResult : uint8_t {
  WIFI_CONN_OK = 0,          // 已连接到目标 SSID（本次或之前已在连）
  WIFI_ALREADY_ON_SSID,      // 之前就连着目标 SSID
  WIFI_CONN_TIMEOUT,         // 超时未连上
  WIFI_CONN_FAILED,          // 参数/硬件异常
};

struct NetProbeResult {
  bool       reachable = false;  // 是否判定能上网
  uint32_t   rtt_ms    = 0;      // 从发起到确认的毫秒耗时
  const char* method   = "";     // 命中的检测方法
};

//=============== 主机名（基于芯片ID） =================

// 用 eFuse MAC 生成唯一主机名：esp32s3-XXXXXX（后24bit）
static String makeHostnameFromEfuse() {
  uint64_t efuse = ESP.getEfuseMac();              // 48-bit 唯一ID（MAC）
  uint32_t tail  = static_cast<uint32_t>(efuse & 0xFFFFFFULL);
  char buf[32];
  snprintf(buf, sizeof(buf), "esp32s3-%06X", tail);
  return String(buf);
}

//=============== Wi-Fi 辅助函数 =================

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// 确保连接到指定 AP（若已在该 SSID 上则不动作；若连着别的先断开再连）
// timeout_ms 建议 10~20s
WifiConnResult wifiEnsureConnected(const char* ssid, const char* pass, uint32_t timeout_ms = 15000) {
  if (!ssid || !*ssid) return WIFI_CONN_FAILED;

  if (wifiIsConnected()) {
    if (WiFi.SSID() == String(ssid)) {
      return WIFI_ALREADY_ON_SSID;
    }
    // 已连但不是目标 → 断开（第二个参数 true 会清除保存的凭据，避免自动回连旧 AP）
    WiFi.disconnect(true /*wifi off*/, true /*erase saved*/);
    delay(100);
  }
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(makeHostnameFromEfuse().c_str());
  WiFi.begin(ssid, pass);

  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeout_ms) {
    Serial.printf(".");
    delay(500);
  }
  Serial.println();
  Serial.printf("Final status: %d\n", WiFi.status());

  return (WiFi.status() == WL_CONNECTED) ? WIFI_CONN_OK : WIFI_CONN_TIMEOUT;
}

//=============== 外网探测（仅 223.5.5.5 & baidu） =================

// 直连 IP:port，返回是否可达，并输出连接 RTT（毫秒）
static bool tcpReachableIP_RTT(const IPAddress& ip, uint16_t port,
                               uint32_t /*timeout_ms*/, uint32_t& out_ms) {
  WiFiClient c;
  uint32_t t0 = millis();
  bool ok = c.connect(ip, port); // Arduino 核心常见为阻塞 connect
  out_ms = millis() - t0;
  if (ok) c.stop();
  return ok;
}

// HTTP 首行探测（Host 先 DNS 解析），返回是否拿到首行，并输出 RTT（毫秒）
static bool httpGetStatusLine_RTT(const char* host, uint16_t port, const char* path,
                                  String& outStatus, uint32_t timeout_ms, uint32_t& out_ms) {
  out_ms = 0;

  IPAddress ip;
  uint32_t t0 = millis();
  if (!WiFi.hostByName(host, ip)) return false;

  WiFiClient client;
  if (!client.connect(ip, port)) return false;

  client.print(String("GET ") + path + " HTTP/1.1\r\n"
               "Host: " + host + "\r\n"
               "Connection: close\r\n\r\n");

  while (millis() - t0 < timeout_ms) {
    if (client.available()) {
      outStatus = client.readStringUntil('\n');
      outStatus.trim();
      out_ms = millis() - t0;
      client.stop();
      return true;
    }
    delay(5);
  }
  client.stop();
  return false;
}

// 仅使用：
// 1) TCP 223.5.5.5:53（阿里DNS）
// 2) HTTP www.baidu.com（2xx/3xx 视为可达）
NetProbeResult internetProbeCN(uint32_t timeout_ms = 5000) {
  NetProbeResult r;
  if (!wifiIsConnected()) return r;

  // Step 1: 直连公共 DNS（不依赖 DNS）
  {
    IPAddress ali(223,5,5,5);
    uint32_t ms = 0;
    if (tcpReachableIP_RTT(ali, 53, timeout_ms, ms)) {
      r.reachable = true; r.rtt_ms = ms; r.method = "TCP 223.5.5.5:53";
      return r;
    }
  }

  // Step 2: 宽松判定（门户/劫持下可能返回 200/302，但说明能出网）
  {
    String status; uint32_t ms = 0;
    if (httpGetStatusLine_RTT("www.baidu.com", 80, "/", status, timeout_ms, ms)) {
      if (status.startsWith("HTTP/1.1 200") || status.startsWith("HTTP/1.0 200") ||
          status.startsWith("HTTP/1.1 30")  || status.startsWith("HTTP/1.0 30")) {
        r.reachable = true; r.rtt_ms = ms; r.method = "HTTP baidu 2xx/3xx";
        return r;
      }
    }
  }

  // 都失败
  return r;
}

// //=============== 可选：最小演示（打开下面宏即可在单文件里测试） ================
// // #define NET_UTILS_DEMO
// #ifdef NET_UTILS_DEMO
// void setup() {
//   Serial.begin(115200);
//   delay(200);

//   const char* SSID = "YOUR_SSID";
//   const char* PASS = "YOUR_PASS";

//   auto res = wifiEnsureConnected(SSID, PASS, 15000);
//   Serial.printf("wifiEnsureConnected -> %d\n", res);

//   if (wifiIsConnected()) {
//     Serial.print("Host: "); Serial.println(WiFi.getHostname());
//     Serial.print("IP  : "); Serial.println(WiFi.localIP());
//     auto pr = internetProbeCN(5000);
//     if (pr.reachable) {
//       Serial.printf("Internet OK, rtt=%ums via %s\n", pr.rtt_ms, pr.method);
//     } else {
//       Serial.println("Internet NOT reachable");
//     }
//   } else {
//     Serial.println("WiFi not connected.");
//   }
// }

// void loop() {
//   static uint32_t last = 0;
//   if (millis() - last > 10000) {
//     last = millis();
//     auto pr = internetProbeCN(3000);
//     Serial.printf("[link] %s, [net] %s (%ums via %s)\n",
//       wifiIsConnected() ? "UP" : "DOWN",
//       pr.reachable ? "OK" : "FAIL",
//       pr.rtt_ms, pr.method);
//   }
// }
// #endif
