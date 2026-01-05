#include "config_store.h"

namespace {
  const char* CONFIG_PATH = "/config.json";
}

static uint32_t ipToInt(const IPAddress &ip)
{
  return (static_cast<uint32_t>(ip[0]) << 24) | (static_cast<uint32_t>(ip[1]) << 16) |
         (static_cast<uint32_t>(ip[2]) << 8) | static_cast<uint32_t>(ip[3]);
}

static IPAddress intToIp(uint32_t value)
{
  return IPAddress((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
}

ConfigStore::ConfigStore() {}
bool ConfigStore::ensureFsMounted() {
  static bool fsReady = false;
  if (fsReady) return true;
  fsReady = LittleFS.begin(true);
  if (fsReady) {
    Serial.println("LittleFS mounted");
  } else {
    Serial.println("LittleFS mount failed");
  }
  return fsReady;
}

bool ConfigStore::parseSubnet(const String &cidr, Subnet &out) const
{
  int slash = cidr.indexOf('/');
  if (slash < 0) return false;
  IPAddress net;
  if (!net.fromString(cidr.substring(0, slash))) return false;
  int prefix = cidr.substring(slash + 1).toInt();
  if (prefix < 1 || prefix > 30) return false;

  uint32_t mask = prefix == 32 ? 0xFFFFFFFF : (0xFFFFFFFF << (32 - prefix));
  uint32_t netInt = ipToInt(net) & mask;
  uint32_t hostCount = prefix == 32 ? 1 : ((1UL << (32 - prefix)) - 2);
  uint32_t first = hostCount ? netInt + 1 : netInt;
  uint32_t last = hostCount ? (netInt + hostCount) : netInt;

  out.cidr = cidr;
  out.network = intToIp(netInt);
  out.prefix = static_cast<uint8_t>(prefix);
  out.firstHost = first;
  out.lastHost = last;
  return true;
}

bool ConfigStore::parseHostLine(const String &line, StaticHost &host) const
{
  String token = line;
  token.trim();
  if (!token.length()) return false;

  int separator = token.indexOf('|');
  if (separator < 0) separator = token.indexOf('#');  // Support both | and # separators
  String meta = separator >= 0 ? token.substring(separator + 1) : "";
  String ipPort = separator >= 0 ? token.substring(0, separator) : token;

  int colon = ipPort.indexOf(':');
  if (colon > 0) {
    host.ip = ipPort.substring(0, colon);
    host.port = ipPort.substring(colon + 1).toInt();
  } else {
    host.ip = ipPort;
    host.port = 0;
  }
  host.ip.trim();
  host.name = meta;
  host.name.trim();
  return host.ip.length();
}

bool ConfigStore::load()
{
  if (!ensureFsMounted()) return false;
  if (!LittleFS.exists(CONFIG_PATH)) {
    Serial.println("Config file missing, using defaults");
    return false;
  }

  File f = LittleFS.open(CONFIG_PATH, "r");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.println("Config parse failed");
    return false;
  }

  config.wifi_ssid = doc["wifi"]["ssid"].as<String>();
  config.wifi_pass = doc["wifi"]["pass"].as<String>();
  config.mqtt_host = doc["mqtt"]["host"].as<String>();
  config.mqtt_port = doc["mqtt"]["port"] | DEFAULT_MQTT_PORT;
  config.mqtt_user = doc["mqtt"]["user"].as<String>();
  config.mqtt_pass = doc["mqtt"]["pass"].as<String>();
  config.scan_interval_ms = doc["scan_interval_ms"] | DEFAULT_SCAN_INTERVAL_MS;
  config.resolve_names = doc["resolve_names"] | true;

  config.subnets.clear();
  JsonArray subs = doc["subnets"].as<JsonArray>();
  if (!subs.isNull()) {
    for (JsonVariant v : subs) {
      Subnet s;
      if (v.is<JsonObject>()) {
        JsonObject obj = v.as<JsonObject>();
        s.cidr = obj["cidr"].as<String>();
        s.name = obj["name"].as<String>();
        s.cidr.trim();
        if (s.cidr.length() && parseSubnet(s.cidr, s)) config.subnets.push_back(s);
      } else {
        String cidr = v.as<String>();
        cidr.trim();
        if (cidr.length() && parseSubnet(cidr, s)) config.subnets.push_back(s);
      }
    }
  }

  config.static_hosts.clear();
  JsonArray hosts = doc["static_hosts"].as<JsonArray>();
  if (!hosts.isNull()) {
    for (JsonObject obj : hosts) {
      StaticHost h;
      h.ip = obj["ip"].as<String>();
      h.ip.trim();
      h.port = obj["port"] | 0;
      h.name = obj["name"].as<String>();
      if (h.ip.length()) config.static_hosts.push_back(h);
    }
  }
  return true;
}

bool ConfigStore::save()
{
  if (!ensureFsMounted()) return false;

  JsonDocument doc;
  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["ssid"] = config.wifi_ssid;
  wifi["pass"] = config.wifi_pass;

  JsonObject mqtt = doc["mqtt"].to<JsonObject>();
  mqtt["host"] = config.mqtt_host;
  mqtt["port"] = config.mqtt_port;
  mqtt["user"] = config.mqtt_user;
  mqtt["pass"] = config.mqtt_pass;

  doc["scan_interval_ms"] = config.scan_interval_ms;
  doc["resolve_names"] = config.resolve_names;

  JsonArray subs = doc["subnets"].to<JsonArray>();
  for (const auto &s : config.subnets) {
    JsonObject o = subs.add<JsonObject>();
    o["cidr"] = s.cidr;
    o["name"] = s.name;
  }

  JsonArray hosts = doc["static_hosts"].to<JsonArray>();
  for (const auto &h : config.static_hosts) {
    JsonObject obj = hosts.add<JsonObject>();
    obj["ip"] = h.ip;
    obj["port"] = h.port;
    obj["name"] = h.name;
  }

  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  Serial.println("Config saved");
  return true;
}

bool ConfigStore::parseConfigPayload(const String &body)
{
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) return false;

  config.wifi_ssid = doc["wifi_ssid"].as<String>();
  config.wifi_pass = doc["wifi_pass"].as<String>();
  config.mqtt_host = doc["mqtt_host"].as<String>();
  config.mqtt_port = doc["mqtt_port"] | DEFAULT_MQTT_PORT;
  config.mqtt_user = doc["mqtt_user"].as<String>();
  config.mqtt_pass = doc["mqtt_pass"].as<String>();
  config.scan_interval_ms = doc["scan_interval_ms"] | DEFAULT_SCAN_INTERVAL_MS;
  config.resolve_names = doc["resolve_names"] | true;

  config.subnets.clear();
  JsonArray subs = doc["subnets"].as<JsonArray>();
  if (!subs.isNull()) {
    for (JsonVariant v : subs) {
      Subnet s;
      String cidr = v.as<String>();
      cidr.trim();
      if (cidr.length() && parseSubnet(cidr, s)) config.subnets.push_back(s);
    }
  }

  config.static_hosts.clear();
  JsonArray hosts = doc["hosts"].as<JsonArray>();
  if (!hosts.isNull()) {
    for (JsonVariant v : hosts) {
      StaticHost h;
      if (parseHostLine(v.as<String>(), h)) config.static_hosts.push_back(h);
    }
  }
  return true;
}

bool ConfigStore::parseTargetsPayload(const String &body)
{
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) return false;

  config.subnets.clear();
  JsonArray subs = doc["subnets"].as<JsonArray>();
  if (!subs.isNull()) {
    for (JsonVariant v : subs) {
      Subnet s;
      if (v.is<JsonObject>()) {
        JsonObject obj = v.as<JsonObject>();
        s.cidr = obj["cidr"].as<String>();
        s.name = obj["name"].as<String>();
        s.cidr.trim();
        if (s.cidr.length() && parseSubnet(s.cidr, s)) config.subnets.push_back(s);
      } else {
        String value = v.as<String>();
        value.trim();
        if (value.length()) {
          int hashIndex = value.indexOf('#');
          if (hashIndex > 0) {
            s.cidr = value.substring(0, hashIndex);
            s.name = value.substring(hashIndex + 1);
            s.cidr.trim();
            s.name.trim();
          } else {
            s.cidr = value;
            s.name.clear();
          }
          if (s.cidr.length() && parseSubnet(s.cidr, s)) config.subnets.push_back(s);
        }
      }
    }
  }

  config.static_hosts.clear();
  JsonArray hosts = doc["hosts"].as<JsonArray>();
  if (!hosts.isNull()) {
    for (JsonVariant v : hosts) {
      StaticHost h;
      if (parseHostLine(v.as<String>(), h)) config.static_hosts.push_back(h);
    }
  }
  return true;
}

String ConfigStore::renderSubnets() const
{
  String combined;
  for (const auto &s : config.subnets) combined += s.cidr + "\n";
  return combined;
}

String ConfigStore::renderHosts() const
{
  String combined;
  for (const auto &h : config.static_hosts) {
    combined += h.ip;
    if (h.port) combined += ":" + String(h.port);
    if (h.name.length()) combined += "|" + h.name;
    combined += "\n";
  }
  return combined;
}

Config& ConfigStore::data() { return config; }
const Config& ConfigStore::data() const { return config; }
