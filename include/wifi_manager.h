#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include "config_store.h"

class WifiManager {
 public:
  explicit WifiManager(Config& config);
  void begin();
  void loop();
  void ensureConnected();
  bool isCaptive() const;
  bool isWifiUp() const;
  String ip() const;

 private:
  bool connectWifi();
  void startCaptivePortal();
  void tryReconnectFromCaptive();

  Config& config;
  DNSServer dns;
  bool captive = false;
  bool lastWifiConnected = false;
  
  unsigned long lastCaptiveRetryMs = 0;
  static constexpr unsigned long CAPTIVE_RETRY_INTERVAL_MS = 300000; // 5 minutes
  static constexpr uint16_t DNS_PORT = 53;
};
