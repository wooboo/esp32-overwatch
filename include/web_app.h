#pragma once
#include <ESPAsyncWebServer.h>
#include <functional>
#include "config_store.h"
#include "network_scanner.h"
#include "mqtt_manager.h"

class WebApp {
public:
  WebApp(ConfigStore& store, NetworkScanner& scanner, MqttManager& mqtt);
  void begin();
  void setWifiStatusProvider(std::function<bool()> wifiUpFn, std::function<String()> wifiIpFn, std::function<bool()> captiveFn);
  void triggerScan();
  void broadcastStatus();
  void broadcastScanResults();

private:
  void setupRoutes();
  void setupWebSocket();
  void handleWsMessage(AsyncWebSocketClient* client, const String& message);
  String buildStatusJson();
  String buildConfigJson();
  String buildScanResultsJson();
  void broadcastJson(const String& type, const String& data);

  AsyncWebServer server{80};
  AsyncWebSocket ws{"/ws"};
  ConfigStore& store;
  NetworkScanner& scanner;
  MqttManager& mqtt;
  std::function<bool()> wifiUp;
  std::function<String()> wifiIp;
  std::function<bool()> isCaptive;
};
