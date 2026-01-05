#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "config_store.h"

class MqttManager {
public:
  MqttManager(Config& config);
  void ensureConnected(bool wifiConnected, bool captivePortal);
  void loop();
  bool isConnected();
  const String& reason() const;
  PubSubClient& client();

  void publishAvailability(const char* payload);
  void publishDiscovery(const std::vector<Subnet>& subnets, const std::vector<StaticHost>& hosts);
  void publishOnlineCount(const Subnet& subnet, int count);
  void publishHostStatus(const StaticHost& host, bool online);
  void publishHostStatusIp(const String& ip, bool online);
  void publishNewHost(const String& ip);
  void publishFoundCount(const Subnet& subnet, int count);

private:
  WiFiClient wifiClient;
  PubSubClient mqtt;
  Config& config;
  bool lastMqttConnected = false;
  int lastMqttState = 0;
  String mqttReason = "init";
  
  // Backoff timing variables
  unsigned long lastMqttAttemptMs = 0;
  unsigned long mqttBackoffMs = 1000; // Start with 1 second
  static constexpr unsigned long MAX_BACKOFF_MS = 60000; // Max 60 seconds
  static constexpr unsigned long BACKOFF_MULTIPLIER = 2;
  
  static constexpr const char* AVAIL_TOPIC = "esp-overwatch/availability";
  static constexpr const char* AVAIL_ON = "online";
  static constexpr const char* AVAIL_OFF = "offline";
};
