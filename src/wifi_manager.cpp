#include "wifi_manager.h"

constexpr uint16_t WifiManager::DNS_PORT;

WifiManager::WifiManager(Config& cfg) : config(cfg) {}

void WifiManager::begin()
{
  ensureConnected();
}

bool WifiManager::connectWifi()
{
  if (!config.wifi_ssid.length()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifi_ssid.c_str(), config.wifi_pass.c_str());
  for (uint8_t i = 0; i < MAX_WIFI_RETRIES; i++) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(500);
  }
  return WiFi.status() == WL_CONNECTED;
}

void WifiManager::startCaptivePortal()
{
  captive = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32NetMon", "esp32config");
  IPAddress apIP(192, 168, 4, 1);
  IPAddress net(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, net);
  dns.start(DNS_PORT, "*", apIP);
  Serial.println("Captive portal at http://192.168.4.1");
  lastCaptiveRetryMs = millis();
}

void WifiManager::tryReconnectFromCaptive()
{
  unsigned long now = millis();
  if (now - lastCaptiveRetryMs >= CAPTIVE_RETRY_INTERVAL_MS) {
    Serial.println("Attempting WiFi reconnection from captive portal");
    lastCaptiveRetryMs = now;
    
    if (connectWifi()) {
      Serial.print("WiFi reconnected: ");
      Serial.println(WiFi.localIP());
      captive = false;
      dns.stop();
      WiFi.mode(WIFI_STA);
    } else {
      Serial.println("WiFi reconnection failed, staying in captive portal");
    }
  }
}

void WifiManager::ensureConnected()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!lastWifiConnected) {
      Serial.print("WiFi connected: ");
      Serial.println(WiFi.localIP());
    }
    lastWifiConnected = true;
    return;
  }
  lastWifiConnected = false;
  if (captive) return;
  if (!connectWifi()) {
    Serial.println("WiFi connect failed, starting captive portal");
    startCaptivePortal();
  }
}

void WifiManager::loop()
{
  if (captive) {
    dns.processNextRequest();
    tryReconnectFromCaptive();
  }
  ensureConnected();
}

bool WifiManager::isCaptive() const { return captive; }
bool WifiManager::isWifiUp() const { return WiFi.status() == WL_CONNECTED && !captive; }
String WifiManager::ip() const { return isWifiUp() ? WiFi.localIP().toString() : ""; }
