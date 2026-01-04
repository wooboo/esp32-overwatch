#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <ESP32Ping.h>
#include <vector>
#include <set>

bool fsReady = false;

bool ensureFsMounted()
{
  if (fsReady)
    return true;
  fsReady = LittleFS.begin(true);
  if (fsReady)
  {
    Serial.println("LittleFS mounted");
  }
  else
  {
    Serial.println("LittleFS mount failed");
  }
  return fsReady;
}

namespace
{
  const char *CONFIG_PATH = "/config.json";
  const uint16_t DNS_PORT = 53;
  const uint32_t DEFAULT_SCAN_INTERVAL_MS = 300000; // 5 minutes
  const uint16_t DEFAULT_MQTT_PORT = 1883;
  const uint16_t PING_TIMEOUT_MS = 250;
  const uint8_t MAX_WIFI_RETRIES = 30;
  const size_t JSON_CAPACITY = 8192;
  const char *AVAIL_TOPIC = "esp-overwatch/availability";
  const char *AVAIL_ON = "online";
  const char *AVAIL_OFF = "offline";
  const uint8_t SCAN_STEP_BUDGET = 1; // number of hosts to process per loop iteration
}

struct StaticHost
{
  String ip;
  int port = 0;
  String name;
};
struct Subnet
{
  String cidr;
  IPAddress network;
  uint8_t prefix = 24;
  uint32_t firstHost = 0;
  uint32_t lastHost = 0;
};

struct Config
{
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

struct SubnetScanResult
{
  String cidr;
  int online = 0;
};
struct HostScanResult
{
  String ip;
  int port = 0;
  String name;
  bool online = false;
};

WebServer server(80);
DNSServer dnsServer;
WiFiClient wifiClient;
PubSubClient mqtt_client(wifiClient);
Config config;
bool captivePortal = false;
std::set<String> seenHosts;
String mqttReason = "init";
bool lastWifiConnected = false;
bool lastMqttConnected = false;
int lastMqttState = 0;
std::vector<SubnetScanResult> lastSubnetResults;
std::vector<HostScanResult> lastHostResults;
unsigned long lastScanCompletedMs = 0;
unsigned long lastScanStartMs = 0;
std::set<String> prevOnlineHosts;

struct ScanState
{
  bool active = false;
  bool mqttReady = false;
  size_t hostIndex = 0;
  size_t subnetIndex = 0;
  uint32_t subnetCursor = 0;
  int currentOnline = 0;
  int foundOnlineCount = 0;
  std::set<String> currentOnlineHosts;
};

ScanState scanState;

uint32_t ipToInt(const IPAddress &ip)
{
  return (static_cast<uint32_t>(ip[0]) << 24) | (static_cast<uint32_t>(ip[1]) << 16) |
         (static_cast<uint32_t>(ip[2]) << 8) | static_cast<uint32_t>(ip[3]);
}

IPAddress intToIp(uint32_t value)
{
  return IPAddress((value >> 24) & 0xFF, (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
}

String sanitizeId(String value)
{
  value.replace(".", "_");
  value.replace("/", "_");
  value.replace(":", "_");
  return value;
}

bool parseSubnet(const String &cidr, Subnet &out)
{
  int slash = cidr.indexOf('/');
  if (slash < 0)
    return false;
  IPAddress net;
  if (!net.fromString(cidr.substring(0, slash)))
    return false;
  int prefix = cidr.substring(slash + 1).toInt();
  if (prefix < 1 || prefix > 30)
    return false; // avoid huge or broadcast-only ranges

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

bool loadConfig()
{
  if (!ensureFsMounted())
    return false;
  if (!LittleFS.exists(CONFIG_PATH))
  {
    Serial.println("Config file missing, using defaults");
    return false;
  }

  File f = LittleFS.open(CONFIG_PATH, "r");
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err)
  {
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

  config.subnets.clear();
  JsonArray subs = doc["subnets"].as<JsonArray>();
  if (!subs.isNull())
  {
    for (JsonVariant v : subs)
    {
      Subnet s;
      String cidr = v.as<String>();
      cidr.trim();
      if (cidr.length() && parseSubnet(cidr, s))
        config.subnets.push_back(s);
    }
  }

  config.static_hosts.clear();
  JsonArray hosts = doc["static_hosts"].as<JsonArray>();
  if (!hosts.isNull())
  {
    for (JsonObject obj : hosts)
    {
      StaticHost h;
      h.ip = obj["ip"].as<String>();
      h.ip.trim();
      h.port = obj["port"] | 0;
      h.name = obj["name"].as<String>();
      if (h.ip.length())
        config.static_hosts.push_back(h);
    }
  }

  Serial.println("Config loaded");
  Serial.print("WiFi SSID: ");
  Serial.println(config.wifi_ssid);
  Serial.print("MQTT host: ");
  Serial.print(config.mqtt_host);
  Serial.print(":");
  Serial.println(config.mqtt_port);
  return true;
}

bool saveConfig()
{
  if (!ensureFsMounted())
    return false;

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

  JsonArray subs = doc["subnets"].to<JsonArray>();
  for (const auto &s : config.subnets)
    subs.add(s.cidr);

  JsonArray hosts = doc["static_hosts"].to<JsonArray>();
  for (const auto &h : config.static_hosts)
  {
    JsonObject obj = hosts.add<JsonObject>();
    obj["ip"] = h.ip;
    obj["port"] = h.port;
    obj["name"] = h.name;
  }

  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f)
    return false;
  serializeJson(doc, f);
  f.close();
  Serial.println("Config saved");
  return true;
}

bool parseHostLine(const String &line, StaticHost &host)
{
  String token = line;
  token.trim();
  if (!token.length())
    return false;

  int pipe = token.indexOf('|');
  String meta = pipe >= 0 ? token.substring(pipe + 1) : "";
  String ipPort = pipe >= 0 ? token.substring(0, pipe) : token;

  int colon = ipPort.indexOf(':');
  if (colon > 0)
  {
    host.ip = ipPort.substring(0, colon);
    host.port = ipPort.substring(colon + 1).toInt();
  }
  else
  {
    host.ip = ipPort;
    host.port = 0;
  }
  host.ip.trim();
  host.name = meta;
  host.name.trim();
  return host.ip.length();
}

bool parseConfigPayload(const String &body)
{
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err)
    return false;

  config.wifi_ssid = doc["wifi_ssid"].as<String>();
  config.wifi_pass = doc["wifi_pass"].as<String>();
  config.mqtt_host = doc["mqtt_host"].as<String>();
  config.mqtt_port = doc["mqtt_port"] | DEFAULT_MQTT_PORT;
  config.mqtt_user = doc["mqtt_user"].as<String>();
  config.mqtt_pass = doc["mqtt_pass"].as<String>();
  config.scan_interval_ms = doc["scan_interval_ms"] | DEFAULT_SCAN_INTERVAL_MS;

  config.subnets.clear();
  JsonArray subs = doc["subnets"].as<JsonArray>();
  if (!subs.isNull())
  {
    for (JsonVariant v : subs)
    {
      Subnet s;
      String cidr = v.as<String>();
      cidr.trim();
      if (cidr.length() && parseSubnet(cidr, s))
        config.subnets.push_back(s);
    }
  }

  config.static_hosts.clear();
  JsonArray hosts = doc["hosts"].as<JsonArray>();
  if (!hosts.isNull())
  {
    for (JsonVariant v : hosts)
    {
      StaticHost h;
      if (parseHostLine(v.as<String>(), h))
        config.static_hosts.push_back(h);
    }
  }
  return true;
}

bool pingHost(const IPAddress &ip)
{
  return Ping.ping(ip, 1);
}

bool portOpen(const String &ip, int port)
{
  WiFiClient c;
  return c.connect(ip.c_str(), port, PING_TIMEOUT_MS);
}

void publishDiscovery()
{
  if (!mqtt_client.connected())
  {
    Serial.println("Discovery skipped (MQTT not connected)");
    return;
  }
  Serial.println("Publishing Home Assistant discovery");
  for (const auto &s : config.subnets)
  {
    String objectId = "overwatch_subnet_" + sanitizeId(s.cidr);
    String topic = "homeassistant/sensor/" + objectId + "/config";
    JsonDocument doc;
    doc["name"] = "Network " + s.cidr + " online";
    doc["uniq_id"] = objectId;
    doc["stat_t"] = "esp-overwatch/network/" + s.cidr + "/online_count";
    doc["dev_cla"] = "none";
    doc["unit_of_meas"] = "hosts";
    doc["avty_t"] = AVAIL_TOPIC;
    doc["pl_avail"] = AVAIL_ON;
    doc["pl_not_avail"] = AVAIL_OFF;
    JsonObject dev = doc["dev"].to<JsonObject>();
    dev["ids"] = "esp-overwatch";
    dev["name"] = "ESP32 Overwatch";
    dev["mdl"] = "XIAO ESP32C3";
    dev["mf"] = "Seeed";
    String payload;
    serializeJson(doc, payload);
    boolean ok = mqtt_client.publish(topic.c_str(), payload.c_str(), true);
    if (!ok)
    {
      Serial.print("Failed to publish discovery message:");
    }
    else
    {
      Serial.print("Discovery subnet published: ");
    }
    Serial.print(topic);
    Serial.print(" -> ");
    Serial.println(payload);
  }

  for (const auto &h : config.static_hosts)
  {
    String objectId = "overwatch_host_" + sanitizeId(h.ip);
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
    dev["ids"] = "esp-overwatch";
    dev["name"] = "ESP32 Overwatch";
    dev["mdl"] = "XIAO ESP32C3";
    dev["mf"] = "Seeed";
    String payload;
    serializeJson(doc, payload);
    boolean ok = mqtt_client.publish(topic.c_str(), payload.c_str(), true);
    if (!ok)
    {
      Serial.print("Failed to publish discovery message: ");
    }
    else
    {
      Serial.print("Discovery host published: ");
    }
    Serial.print(topic);
    Serial.print(" -> ");
    Serial.println(payload);
  }
}

void publishOnlineCount(const Subnet &subnet, int count)
{
  String topic = "esp-overwatch/network/" + subnet.cidr + "/online_count";
  mqtt_client.publish(topic.c_str(), String(count).c_str(), true);
}

void publishHostStatus(const StaticHost &host, bool online)
{
  String topic = "esp-overwatch/host/" + host.ip + "/status";
  mqtt_client.publish(topic.c_str(), online ? "online" : "offline", true);
}

void publishHostStatusIp(const String &ip, bool online)
{
  String topic = "esp-overwatch/host/" + ip + "/status";
  mqtt_client.publish(topic.c_str(), online ? "online" : "offline", true);
}

void publishNewHost(const String &ip)
{
  String topic = "esp-overwatch/host/" + ip + "/discovered";
  mqtt_client.publish(topic.c_str(), "1", false);
}

void publishFoundCount(int count)
{
  mqtt_client.publish("esp-overwatch/hosts/found_count", String(count).c_str(), false);
}

void publishAvailability(const char *payload)
{
  mqtt_client.publish(AVAIL_TOPIC, payload, true);
}

void finishScan()
{
  scanState.active = false;
  prevOnlineHosts = scanState.currentOnlineHosts;
  lastScanCompletedMs = millis();
  Serial.println("Scan complete");
}

void finishSubnet()
{
  const Subnet &subnet = config.subnets[scanState.subnetIndex];
  SubnetScanResult r;
  r.cidr = subnet.cidr;
  r.online = scanState.currentOnline;
  lastSubnetResults.push_back(r);
  if (scanState.mqttReady)
    publishOnlineCount(subnet, scanState.currentOnline);
  scanState.subnetIndex++;
  if (scanState.subnetIndex < config.subnets.size())
  {
    scanState.subnetCursor = config.subnets[scanState.subnetIndex].firstHost;
    scanState.currentOnline = 0;
  }
  else
  {
    finishScan();
  }
}

void stepScan()
{
  if (!scanState.active)
    return;

  uint8_t budget = SCAN_STEP_BUDGET;
  while (budget-- && scanState.active)
  {
    if (scanState.hostIndex < config.static_hosts.size())
    {
      const auto &h = config.static_hosts[scanState.hostIndex];
      bool ok = false;
      if (h.port)
      {
        ok = portOpen(h.ip, h.port);
      }
      else
      {
        IPAddress parsed;
        if (parsed.fromString(h.ip))
          ok = pingHost(parsed);
      }
      Serial.print("scan host ");
      Serial.print(h.ip);
      if (h.port)
      {
        Serial.print(":");
        Serial.print(h.port);
        Serial.print(" tcp ");
      }
      else
      {
        Serial.print(" ping ");
      }
      Serial.println(ok ? "online" : "offline");
      HostScanResult hr;
      hr.ip = h.ip;
      hr.port = h.port;
      hr.name = h.name;
      hr.online = ok;
      lastHostResults.push_back(hr);
      if (ok)
      {
        scanState.currentOnlineHosts.insert(h.ip);
        bool newSincePrev = prevOnlineHosts.find(h.ip) == prevOnlineHosts.end();
        if (newSincePrev)
          scanState.foundOnlineCount++;
        if (scanState.mqttReady)
        {
          publishHostStatus(h, true);
          publishFoundCount(scanState.foundOnlineCount);
        }
      }
      else if (scanState.mqttReady)
      {
        publishHostStatus(h, false);
      }
      scanState.hostIndex++;
      continue;
    }

    if (scanState.subnetIndex >= config.subnets.size())
    {
      finishScan();
      break;
    }

    const Subnet &subnet = config.subnets[scanState.subnetIndex];
    IPAddress target = intToIp(scanState.subnetCursor);
    bool ok = pingHost(target);
    if (ok)
    {
      scanState.currentOnline++;
      String ipStr = target.toString();
      scanState.currentOnlineHosts.insert(ipStr);
      bool newSincePrev = prevOnlineHosts.find(ipStr) == prevOnlineHosts.end();
      if (newSincePrev)
        scanState.foundOnlineCount++;
      if (scanState.mqttReady)
      {
        if (newSincePrev && seenHosts.insert(ipStr).second)
          publishNewHost(ipStr);
        publishFoundCount(scanState.foundOnlineCount);
      }
      if (scanState.mqttReady)
        publishHostStatusIp(ipStr, true);
    }
    Serial.print("scan subnet ");
    Serial.print(subnet.cidr);
    Serial.print(" host ");
    Serial.print(target);
    Serial.print(" ping ");
    Serial.println(ok ? "online" : "offline");

    if (scanState.subnetCursor >= subnet.lastHost)
    {
      finishSubnet();
    }
    else
    {
      scanState.subnetCursor++;
    }
  }
}

bool startScan()
{
  if (scanState.active)
    return false;

  lastHostResults.clear();
  lastSubnetResults.clear();
  prevOnlineHosts = scanState.currentOnlineHosts;
  scanState.currentOnlineHosts.clear();
  scanState.foundOnlineCount = 0;
  scanState.active = true;
  scanState.mqttReady = mqtt_client.connected();
  scanState.hostIndex = 0;
  scanState.subnetIndex = 0;
  scanState.subnetCursor = config.subnets.empty() ? 0 : config.subnets[0].firstHost;
  scanState.currentOnline = 0;
  lastScanStartMs = millis();
  Serial.println("Scan started");
  return true;
}

bool connectWifi()
{
  if (!config.wifi_ssid.length())
    return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifi_ssid.c_str(), config.wifi_pass.c_str());
  for (uint8_t i = 0; i < MAX_WIFI_RETRIES; i++)
  {
    if (WiFi.status() == WL_CONNECTED)
      return true;
    delay(500);
  }
  return WiFi.status() == WL_CONNECTED;
}

void startCaptivePortal()
{
  captivePortal = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32NetMon", "esp32config");
  IPAddress apIP(192, 168, 4, 1);
  IPAddress net(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, net);
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println("Captive portal at http://192.168.4.1");
}

void ensureWifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!lastWifiConnected)
    {
      Serial.print("WiFi connected: ");
      Serial.println(WiFi.localIP());
    }
    lastWifiConnected = true;
    return;
  }

  lastWifiConnected = false;
  if (captivePortal)
    return;
  if (!connectWifi())
  {
    Serial.println("WiFi connect failed, starting captive portal");
    startCaptivePortal();
  }
  else
  {
    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());
    lastWifiConnected = true;
  }
}

void ensureMqtt()
{
  if (captivePortal)
  {
    mqttReason = "captive_portal";
    return;
  }
  if (!config.mqtt_host.length())
  {
    mqttReason = "no_host";
    return;
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    mqttReason = "wifi_offline";
    return;
  }
  if (mqtt_client.connected())
  {
    if (!lastMqttConnected)
      Serial.println("MQTT connected");
    lastMqttConnected = true;
    mqttReason = "connected";
    return;
  }

  mqtt_client.setServer(config.mqtt_host.c_str(), config.mqtt_port);
  bool ok;
  if (config.mqtt_user.length())
  {
    ok = mqtt_client.connect(
        "esp-overwatch",
        config.mqtt_user.c_str(),
        config.mqtt_pass.c_str(),
        AVAIL_TOPIC,
        0,
        true,
        AVAIL_OFF);
  }
  else
  {
    ok = mqtt_client.connect("esp-overwatch", AVAIL_TOPIC, 0, true, AVAIL_OFF);
  }

  if (ok)
  {
    Serial.println("MQTT connected");
    lastMqttConnected = true;
    mqttReason = "connected";
    publishAvailability(AVAIL_ON);
    publishDiscovery();
  }
  else
  {
    int state = mqtt_client.state();
    if (!lastMqttConnected || state != lastMqttState)
    {
      Serial.print("MQTT connect failed, state ");
      Serial.println(state);
    }
    lastMqttConnected = false;
    lastMqttState = state;
    mqttReason = String("connect_failed_") + state;
  }
}

String renderTextAreaList(const std::vector<Subnet> &subnets)
{
  String combined;
  for (const auto &s : subnets)
    combined += s.cidr + "\n";
  return combined;
}

String renderHosts(const std::vector<StaticHost> &hosts)
{
  String combined;
  for (const auto &h : hosts)
  {
    combined += h.ip;
    if (h.port)
      combined += ":" + String(h.port);
    if (h.name.length())
      combined += "|" + h.name;
    combined += "\n";
  }
  return combined;
}

void handleRoot()
{
  if (!ensureFsMounted())
  {
    server.send(500, "text/plain", "LittleFS mount failed");
    return;
  }
  File f = LittleFS.open("/index.html", "r");
  if (!f)
  {
    Serial.println("index.html missing on LittleFS");
    server.send(500, "text/plain", "index.html missing on LittleFS");
    return;
  }
  server.streamFile(f, "text/html");
  f.close();
}

void handleConfigJson()
{
  JsonDocument doc;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_pass"] = config.wifi_pass;
  doc["mqtt_host"] = config.mqtt_host;
  doc["mqtt_port"] = config.mqtt_port;
  doc["mqtt_user"] = config.mqtt_user;
  doc["mqtt_pass"] = config.mqtt_pass;
  doc["scan_interval_ms"] = config.scan_interval_ms;

  JsonArray subs = doc["subnets"].to<JsonArray>();
  for (const auto &s : config.subnets)
    subs.add(s.cidr);
  JsonArray hosts = doc["static_hosts"].to<JsonArray>();
  for (const auto &h : config.static_hosts)
  {
    JsonObject o = hosts.add<JsonObject>();
    o["ip"] = h.ip;
    o["port"] = h.port;
    o["name"] = h.name;
  }

  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);
}

void handleStatus()
{
  JsonDocument doc;
  bool wifiUp = WiFi.status() == WL_CONNECTED && !captivePortal;
  doc["wifi_connected"] = wifiUp;
  doc["wifi_ip"] = wifiUp ? WiFi.localIP().toString() : "";
  bool mqttUp = mqtt_client.connected() && wifiUp;
  doc["mqtt_connected"] = mqttUp;
  doc["mqtt_reason"] = mqttReason;

  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);
}

void handleScanResults()
{
  JsonDocument doc;
  doc["last_scan_ms"] = lastScanCompletedMs;
  doc["device_now_ms"] = millis();

  JsonArray subs = doc["subnets"].to<JsonArray>();
  for (const auto &s : lastSubnetResults)
  {
    JsonObject o = subs.add<JsonObject>();
    o["cidr"] = s.cidr;
    o["online"] = s.online;
  }

  JsonArray hosts = doc["hosts"].to<JsonArray>();
  for (const auto &h : lastHostResults)
  {
    JsonObject o = hosts.add<JsonObject>();
    o["ip"] = h.ip;
    o["port"] = h.port;
    o["name"] = h.name;
    o["online"] = h.online;
  }

  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);
}

void handleSave()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "text/plain", "Missing body");
    return;
  }

  if (!parseConfigPayload(server.arg("plain")))
  {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  if (!saveConfig())
  {
    server.send(500, "text/plain", "Save failed");
    return;
  }

  server.send(200, "text/plain", "Saved, rebooting");
  delay(500);
  ESP.restart();
}

void handleScan()
{
  if (startScan())
  {
    server.send(202, "text/plain", "Scan started");
  }
  else
  {
    server.send(202, "text/plain", "Scan already running");
  }
}

void handleNotFound()
{
  if (captivePortal)
  {
    server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
    server.send(302, "text/plain", "Redirect");
    return;
  }
  Serial.print("HTTP 404 for path: ");
  Serial.println(server.uri());
  server.send(404, "text/plain", "Not found");
}

void setupWeb()
{
  server.on("/", handleRoot);
  server.on("/index.html", handleRoot);
  server.on("/favicon.ico", HTTP_GET, []()
            { server.send(204); });
  server.on("/config", HTTP_GET, handleConfigJson);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/scan_results", HTTP_GET, handleScanResults);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/scan", HTTP_GET, handleScan);
  server.onNotFound(handleNotFound);
  server.begin();
}

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println("Booting ESP32 NetMon...");
  ensureFsMounted();
  loadConfig();
  ensureWifi();
  setupWeb();
  ensureMqtt();

  // Wait briefly for MQTT to connect before first scan
  unsigned long waitStart = millis();
  while (!mqtt_client.connected() && millis() - waitStart < 5000)
  {
    ensureWifi();
    ensureMqtt();
    mqtt_client.loop();
    delay(50);
  }

  if (mqtt_client.connected())
  {
    Serial.println("Initial scan after MQTT connect");
    startScan();
  }
  else
  {
    Serial.println("Initial scan skipped (MQTT offline)");
  }
  Serial.println("Setup done");
}

void loop()
{
  server.handleClient();
  if (captivePortal)
    dnsServer.processNextRequest();
  ensureWifi();
  ensureMqtt();
  mqtt_client.loop();

  stepScan();

  unsigned long now = millis();
  if (!scanState.active && (now - lastScanStartMs >= config.scan_interval_ms))
  {
    startScan();
  }
}
