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

  Config& config;
  DNSServer dns;
  bool captive = false;
  bool lastWifiConnected = false;
  static constexpr uint16_t DNS_PORT = 53;
};
