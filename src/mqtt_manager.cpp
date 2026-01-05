#include "mqtt_manager.h"

MqttManager::MqttManager(WiFiClient& netClient, Config& cfg) : mqtt(netClient), config(cfg) {}

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
    if (!lastMqttConnected) Serial.println("MQTT connected");
    lastMqttConnected = true;
    mqttReason = "connected";
    return;
  }

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
    publishAvailability(AVAIL_ON);
  } else {
    int state = mqtt.state();
    if (!lastMqttConnected || state != lastMqttState) {
      Serial.print("MQTT connect failed, state ");
      Serial.println(state);
    }
    lastMqttConnected = false;
    lastMqttState = state;
    mqttReason = String("connect_failed_") + state;
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
    String objectId = "overwatch_subnet_" + String(s.cidr);
    objectId.replace("/", "_");
    objectId.replace(".", "_");
    String topic = "homeassistant/sensor/" + objectId + "/config";
    JsonDocument doc;
    doc["name"] = "Network " + s.cidr + " online";
    doc["uniq_id"] = objectId;
    doc["stat_t"] = "esp-overwatch/network/" + s.cidr + "/online_count";
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
    bool ok = mqtt.publish(topic.c_str(), payload.c_str(), true);
    if (!ok) Serial.print("Failed to publish discovery message:");
    else Serial.print("Discovery subnet published: ");
    Serial.print(topic); Serial.print(" -> "); Serial.println(payload);
  }

  for (const auto &h : hosts)
  {
    String objectId = "overwatch_host_" + String(h.ip);
    objectId.replace("/", "_");
    objectId.replace(".", "_");
    objectId.replace(":", "_");
    String topic = "homeassistant/binary_sensor/" + objectId + "/config";
    JsonDocument doc;
    doc["name"] = h.name.length() ? h.name : ("Host " + h.ip);
    doc["uniq_id"] = objectId;
    doc["stat_t"] = "esp-overwatch/host/" + h.ip + "/status";
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
    bool ok = mqtt.publish(topic.c_str(), payload.c_str(), true);
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
