#pragma once
#include <WebServer.h>
#include <DNSServer.h>
#include <functional>
#include "config_store.h"
#include "network_scanner.h"
#include "mqtt_manager.h"

class WebApp {
public:
  WebApp(ConfigStore& store, NetworkScanner& scanner, MqttManager& mqtt);
  void begin();
  void handle();
  void setWifiStatusProvider(std::function<bool()> wifiUpFn, std::function<String()> wifiIpFn, std::function<bool()> captiveFn);
  void triggerScan();

private:
  void handleRoot();
  void handleConfig();
  void handleStatus();
  void handleScanResults();
  void handleSave();
  void handleScan();
  void handleNotFound();

  WebServer server{80};
  ConfigStore& store;
  NetworkScanner& scanner;
  MqttManager& mqtt;
  std::function<bool()> wifiUp;
  std::function<String()> wifiIp;
  std::function<bool()> isCaptive;
};
