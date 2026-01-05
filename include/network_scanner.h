#pragma once
#include <Arduino.h>
#include <ESP32Ping.h>
#include <WiFi.h>
#include <set>
#include "config_store.h"
#include "mqtt_manager.h"

struct SubnetScanResult { String cidr; int online = 0; };
struct HostScanResult { String ip; int port = 0; String name; bool online = false; };

class NetworkScanner {
public:
  explicit NetworkScanner(Config& cfg, MqttManager& mqtt);
  bool start();
  void step();
  bool active() const;
  unsigned long lastCompletedMs() const;
  const std::vector<SubnetScanResult>& subnetResults() const;
  const std::vector<HostScanResult>& hostResults() const;
  int foundCount() const;

private:
  bool portOpen(const String& ip, int port);
  bool pingHost(const IPAddress& ip);
  void finishScan();
  void finishSubnet();

  Config& config;
  MqttManager& mqtt;
  bool scanning = false;
  bool mqttReady = false;
  size_t hostIndex = 0;
  size_t subnetIndex = 0;
  uint32_t subnetCursor = 0;
  int currentOnline = 0;
  int foundOnlineCount = 0;
  int foundOnlineCountSubnet = 0;
  std::set<String> prevOnlineHosts;
  std::set<String> currentOnlineHosts;
  std::set<String> seenHosts;
  std::vector<SubnetScanResult> lastSubnetResults;
  std::vector<HostScanResult> lastHostResults;
  unsigned long lastScanCompletedMs = 0;
  unsigned long lastScanStartMs = 0;
};
