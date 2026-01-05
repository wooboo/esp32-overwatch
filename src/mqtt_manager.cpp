#include "mqtt_manager.h"

MqttManager::MqttManager(Config& cfg) : mqtt(wifiClient), config(cfg) {}

void MqttManager::loop() { mqtt.loop(); }

bool MqttManager::isConnected() { return mqtt.connected(); }

const String& MqttManager::reason() const { return mqttReason; }

PubSubClient& MqttManager::client() { return mqtt; }

void MqttManager::ensureConnected(bool wifiConnected, bool captivePortal)
{
  if (captivePortal) { mqttReason = "captive_portal"; return; }
  if (!config.mqtt_host.length()) { mqttReason = "no_host"; return; }
  if (!wifiConnected) { mqttReason = "wifi_offline"; return; }
  if (mqtt.connected()) {
    if (!lastMqttConnected) {
      Serial.println("MQTT connected");
      mqttBackoffMs = 1000;
    }
    lastMqttConnected = true;
    mqttReason = "connected";
    return;
  }

  unsigned long now = millis();
  if (now - lastMqttAttemptMs < mqttBackoffMs) {
    mqttReason = String("backoff_") + (mqttBackoffMs / 1000) + "s";
    return;
  }

  lastMqttAttemptMs = now;
  mqtt.setServer(config.mqtt_host.c_str(), config.mqtt_port);
  bool ok;
  if (config.mqtt_user.length()) {
    ok = mqtt.connect(
      "esp-overwatch",
      config.mqtt_user.c_str(),
      config.mqtt_pass.c_str(),
      AVAIL_TOPIC,
      0,
      true,
      AVAIL_OFF
    );
  } else {
    ok = mqtt.connect("esp-overwatch", AVAIL_TOPIC, 0, true, AVAIL_OFF);
  }

  if (ok) {
    Serial.println("MQTT connected");
    lastMqttConnected = true;
    mqttReason = "connected";
    mqttBackoffMs = 1000;
    publishAvailability(AVAIL_ON);
  } else {
    int state = mqtt.state();
    if (!lastMqttConnected || state != lastMqttState) {
      Serial.print("MQTT connect failed, state ");
      Serial.print(state);
      Serial.print(", next retry in ");
      Serial.print(mqttBackoffMs / 1000);
      Serial.println("s");
    }
    lastMqttConnected = false;
    lastMqttState = state;
    mqttReason = String("connect_failed_") + state;
    
    mqttBackoffMs = min(mqttBackoffMs * BACKOFF_MULTIPLIER, MAX_BACKOFF_MS);
  }
}

void MqttManager::publishAvailability(const char* payload)
{
  mqtt.publish(AVAIL_TOPIC, payload, true);
}

void MqttManager::publishDiscovery(const std::vector<Subnet>& subnets, const std::vector<StaticHost>& hosts)
{
  if (!mqtt.connected()) {
    Serial.println("Discovery skipped (MQTT not connected)");
    return;
  }
  Serial.println("Publishing Home Assistant discovery");
  for (const auto &s : subnets)
  {
    char objectId[64];
    snprintf(objectId, sizeof(objectId), "overwatch_subnet_%s", s.cidr.c_str());
    for (char* p = objectId; *p; ++p) {
      if (*p == '/' || *p == '.') *p = '_';
    }
    char topic[128];
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s/config", objectId);
    char name[64];
    snprintf(name, sizeof(name), "Network %s online", s.cidr.c_str());
    char statTopic[64];
    snprintf(statTopic, sizeof(statTopic), "esp-overwatch/network/%s/online_count", s.cidr.c_str());
    JsonDocument doc;
    doc["name"] = name;
    doc["uniq_id"] = objectId;
    doc["stat_t"] = statTopic;
    doc["unit_of_meas"] = "hosts";
    doc["state_class"] = "measurement";
    doc["avty_t"] = AVAIL_TOPIC;
    doc["pl_avail"] = AVAIL_ON;
    doc["pl_not_avail"] = AVAIL_OFF;
    JsonObject dev = doc["dev"].to<JsonObject>();
    JsonArray ids = dev["ids"].to<JsonArray>();
    ids.add("esp-overwatch");
    dev["name"] = "ESP32 Overwatch";
    dev["mdl"] = "XIAO ESP32C3";
    dev["mf"] = "Seeed";
    String payload;
    serializeJson(doc, payload);
    bool ok = mqtt.publish(topic, payload.c_str(), true);
    if (!ok) Serial.print("Failed to publish discovery message:");
    else Serial.print("Discovery subnet published: ");
    Serial.print(topic); Serial.print(" -> "); Serial.println(payload);
  }

  for (const auto &h : hosts)
  {
    char objectId[64];
    snprintf(objectId, sizeof(objectId), "overwatch_host_%s", h.ip.c_str());
    for (char* p = objectId; *p; ++p) {
      if (*p == '/' || *p == '.' || *p == ':') *p = '_';
    }
    char topic[128];
    snprintf(topic, sizeof(topic), "homeassistant/binary_sensor/%s/config", objectId);
    char name[64];
    if (h.name.length()) {
      snprintf(name, sizeof(name), "%s", h.name.c_str());
    } else {
      snprintf(name, sizeof(name), "Host %s", h.ip.c_str());
    }
    char statTopic[64];
    snprintf(statTopic, sizeof(statTopic), "esp-overwatch/host/%s/status", h.ip.c_str());
    JsonDocument doc;
    doc["name"] = name;
    doc["uniq_id"] = objectId;
    doc["stat_t"] = statTopic;
    doc["pl_on"] = "online";
    doc["pl_off"] = "offline";
    doc["avty_t"] = AVAIL_TOPIC;
    doc["pl_avail"] = AVAIL_ON;
    doc["pl_not_avail"] = AVAIL_OFF;
    JsonObject dev = doc["dev"].to<JsonObject>();
    JsonArray ids = dev["ids"].to<JsonArray>();
    ids.add("esp-overwatch");
    dev["name"] = "ESP32 Overwatch";
    dev["mdl"] = "XIAO ESP32C3";
    dev["mf"] = "Seeed";
    String payload;
    serializeJson(doc, payload);
    bool ok = mqtt.publish(topic, payload.c_str(), true);
    if (!ok) Serial.print("Failed to publish discovery message: ");
    else Serial.print("Discovery host published: ");
    Serial.print(topic); Serial.print(" -> "); Serial.println(payload);
  }
}

void MqttManager::publishOnlineCount(const Subnet &subnet, int count)
{
  String topic = "esp-overwatch/network/" + subnet.cidr + "/online_count";
  mqtt.publish(topic.c_str(), String(count).c_str(), true);
}

void MqttManager::publishHostStatus(const StaticHost &host, bool online)
{
  String topic = "esp-overwatch/host/" + host.ip + "/status";
  mqtt.publish(topic.c_str(), online ? "online" : "offline", true);
}

void MqttManager::publishHostStatusIp(const String &ip, bool online)
{
  String topic = "esp-overwatch/host/" + ip + "/status";
  mqtt.publish(topic.c_str(), online ? "online" : "offline", true);
}

void MqttManager::publishNewHost(const String &ip)
{
  String topic = "esp-overwatch/host/" + ip + "/discovered";
  mqtt.publish(topic.c_str(), "1", false);
}

void MqttManager::publishFoundCount(const Subnet& subnet, int count)
{
  String topic = "esp-overwatch/network/" + subnet.cidr + "/found_count";
  mqtt.publish(topic.c_str(), String(count).c_str(), false);
}
