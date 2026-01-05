#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>

static const uint32_t DEFAULT_SCAN_INTERVAL_MS = 300000; // 5 minutes
static const uint16_t DEFAULT_MQTT_PORT = 1883;
static const uint16_t PING_TIMEOUT_MS = 250;
static const uint8_t MAX_WIFI_RETRIES = 30;
static const size_t JSON_CAPACITY = 8192;

struct StaticHost {
  String ip;
  int port = 0;
  String name;
};

struct Subnet {
  String cidr;
  IPAddress network;
  uint8_t prefix = 24;
  uint32_t firstHost = 0;
  uint32_t lastHost = 0;
};

struct Config {
  String wifi_ssid;
  String wifi_pass;
  String mqtt_host;
  uint16_t mqtt_port = DEFAULT_MQTT_PORT;
  String mqtt_user;
  String mqtt_pass;
  uint32_t scan_interval_ms = DEFAULT_SCAN_INTERVAL_MS;
  std::vector<Subnet> subnets;
  std::vector<StaticHost> static_hosts;
};

class ConfigStore {
public:
  ConfigStore();
  bool load();
  bool save();
  Config& data();
  const Config& data() const;
  String renderSubnets() const;
  String renderHosts() const;
  bool parseConfigPayload(const String& body);
  bool parseHostLine(const String& line, StaticHost& host) const;
  bool parseSubnet(const String& cidr, Subnet& out) const;
  bool ensureFsMounted();

private:
  Config config;
};
