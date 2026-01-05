#include "web_app.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

WebApp::WebApp(ConfigStore& st, NetworkScanner& sc, MqttManager& mq)
  : store(st), scanner(sc), mqtt(mq) {}

void WebApp::setWifiStatusProvider(std::function<bool()> wifiUpFn, std::function<String()> wifiIpFn, std::function<bool()> captiveFn)
{
  wifiUp = std::move(wifiUpFn);
  wifiIp = std::move(wifiIpFn);
  isCaptive = std::move(captiveFn);
}

void WebApp::begin()
{
  server.on("/", [this]() { handleRoot(); });
  server.on("/index.html", [this]() { handleRoot(); });
  server.on("/favicon.ico", HTTP_GET, [this](){ server.send(204); });
  server.on("/config", HTTP_GET, [this]() { handleConfig(); });
  server.on("/status", HTTP_GET, [this]() { handleStatus(); });
  server.on("/scan_results", HTTP_GET, [this]() { handleScanResults(); });
  server.on("/save", HTTP_POST, [this]() { handleSave(); });
  server.on("/scan", HTTP_GET, [this]() { handleScan(); });
  server.onNotFound([this]() { handleNotFound(); });
  server.begin();
}

void WebApp::handle()
{
  server.handleClient();
}

void WebApp::handleRoot()
{
  if (!store.ensureFsMounted()) {
    server.send(500, "text/plain", "LittleFS mount failed");
    return;
  }
  File f = LittleFS.open("/index.html", "r");
  if (!f) {
    Serial.println("index.html missing on LittleFS");
    server.send(500, "text/plain", "index.html missing on LittleFS");
    return;
  }
  server.streamFile(f, "text/html");
  f.close();
}

void WebApp::handleConfig()
{
  JsonDocument doc;
  const Config& cfg = store.data();
  doc["wifi_ssid"] = cfg.wifi_ssid;
  doc["wifi_pass"] = cfg.wifi_pass;
  doc["mqtt_host"] = cfg.mqtt_host;
  doc["mqtt_port"] = cfg.mqtt_port;
  doc["mqtt_user"] = cfg.mqtt_user;
  doc["mqtt_pass"] = cfg.mqtt_pass;
  doc["scan_interval_ms"] = cfg.scan_interval_ms;

  JsonArray subs = doc["subnets"].to<JsonArray>();
  for (const auto &s : cfg.subnets) subs.add(s.cidr);
  JsonArray hosts = doc["static_hosts"].to<JsonArray>();
  for (const auto &h : cfg.static_hosts) {
    JsonObject o = hosts.add<JsonObject>();
    o["ip"] = h.ip;
    o["port"] = h.port;
    o["name"] = h.name;
  }

  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);
}

void WebApp::handleStatus()
{
  JsonDocument doc;
  bool wifi_ok = wifiUp ? wifiUp() : false;
  doc["wifi_connected"] = wifi_ok;
  doc["wifi_ip"] = wifi_ok && wifiIp ? wifiIp() : "";
  bool mqtt_ok = mqtt.isConnected() && wifi_ok;
  doc["mqtt_connected"] = mqtt_ok;
  doc["mqtt_reason"] = mqtt.reason();

  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);
}

void WebApp::handleScanResults()
{
  JsonDocument doc;
  doc["last_scan_ms"] = scanner.lastCompletedMs();
  doc["device_now_ms"] = millis();
  doc["found_count"] = scanner.foundCount();

  JsonArray subs = doc["subnets"].to<JsonArray>();
  for (const auto &s : scanner.subnetResults()) {
    JsonObject o = subs.add<JsonObject>();
    o["cidr"] = s.cidr;
    o["online"] = s.online;
  }

  JsonArray hosts = doc["hosts"].to<JsonArray>();
  for (const auto &h : scanner.hostResults()) {
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

void WebApp::handleSave()
{
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Missing body");
    return;
  }
  if (!store.parseConfigPayload(server.arg("plain"))) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  if (!store.save()) {
    server.send(500, "text/plain", "Save failed");
    return;
  }
  server.send(200, "text/plain", "Saved, rebooting");
  delay(500);
  ESP.restart();
}

void WebApp::handleScan()
{
  triggerScan();
  server.send(202, "text/plain", "Scan started");
}

void WebApp::handleNotFound()
{
  if (isCaptive && isCaptive()) {
    server.sendHeader("Location", String("http://") + server.client().localIP().toString(), true);
    server.send(302, "text/plain", "Redirect");
    return;
  }
  Serial.print("HTTP 404 for path: ");
  Serial.println(server.uri());
  server.send(404, "text/plain", "Not found");
}

void WebApp::triggerScan()
{
  scanner.start();
}
