#include "web_app.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

WebApp::WebApp(ConfigStore& st, NetworkScanner& sc, MqttManager& mq)
  : store(st), scanner(sc), mqtt(mq) {}

void WebApp::setWifiStatusProvider(std::function<bool()> wifiUpFn, std::function<String()> wifiIpFn, std::function<bool()> captiveFn) {
  wifiUp = std::move(wifiUpFn);
  wifiIp = std::move(wifiIpFn);
  isCaptive = std::move(captiveFn);
}

void WebApp::begin() {
  setupWebSocket();
  setupRoutes();
  server.begin();
}

void WebApp::setupWebSocket() {
  ws.onEvent([this](AsyncWebSocket* s, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      client->text("{\"type\":\"connected\"}");
    } else if (type == WS_EVT_DATA) {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        String msg;
        msg.reserve(len + 1);
        for (size_t i = 0; i < len; i++) {
          msg += (char)data[i];
        }
        handleWsMessage(client, msg);
      }
    }
  });
  server.addHandler(&ws);
}

void WebApp::handleWsMessage(AsyncWebSocketClient* client, const String& message) {
  JsonDocument doc;
  if (deserializeJson(doc, message)) return;

  const char* type = doc["type"];
  if (!type) return;

  if (strcmp(type, "get_all") == 0) {
    client->text("{\"type\":\"status\",\"data\":" + buildStatusJson() + "}");
    client->text("{\"type\":\"config\",\"data\":" + buildConfigJson() + "}");
    client->text("{\"type\":\"scan_results\",\"data\":" + buildScanResultsJson() + "}");
  } else if (strcmp(type, "trigger_scan") == 0) {
    triggerScan();
    ws.textAll("{\"type\":\"scan_started\"}");
  } else if (strcmp(type, "save_config") == 0) {
    JsonObject data = doc["data"];
    if (data) {
      String payload;
      serializeJson(data, payload);
      if (store.parseConfigPayload(payload) && store.save()) {
        ws.textAll("{\"type\":\"config_saved\"}");
        delay(500);
        ESP.restart();
      }
    }
  }
}

void WebApp::setupRoutes() {
  // Handle root with gzip support
  server.on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
    if (req->hasHeader("Accept-Encoding")) {
      String encoding = req->getHeader("Accept-Encoding")->value();
      if (encoding.indexOf("gzip") >= 0 && LittleFS.exists("/index.html.gz")) {
        AsyncWebServerResponse* response = req->beginResponse(LittleFS, "/index.html.gz", "text/html");
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "no-cache");
        req->send(response);
        return;
      }
    }
    req->send(LittleFS, "/index.html", "text/html");
  });

  // Handle captive portal detection files
  server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/plain", "Success");
  });

  server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", "<html><body>Success</body></html>");
  });

  server.on("/config", HTTP_GET, [this](AsyncWebServerRequest* req) {
    req->send(200, "application/json", buildConfigJson());
  });

  server.on("/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
    req->send(200, "application/json", buildStatusJson());
  });

  server.on("/scan_results", HTTP_GET, [this](AsyncWebServerRequest* req) {
    req->send(200, "application/json", buildScanResultsJson());
  });

  server.on("/scan", HTTP_GET, [this](AsyncWebServerRequest* req) {
    triggerScan();
    req->send(202, "text/plain", "Scan started");
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest* req) {
    req->send(400, "text/plain", "Use WebSocket");
  }, NULL, [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    if (index == 0) {
      req->_tempObject = new String();
    }
    String* body = (String*)req->_tempObject;
    for (size_t i = 0; i < len; i++) {
      *body += (char)data[i];
    }
    if (index + len == total) {
      if (store.parseConfigPayload(*body) && store.save()) {
        req->send(200, "text/plain", "Saved, rebooting");
        delete body;
        delay(500);
        ESP.restart();
      } else {
        req->send(400, "text/plain", "Invalid config");
        delete body;
      }
    }
  });

  server.onNotFound([this](AsyncWebServerRequest* req) {
    if (isCaptive && isCaptive()) {
      req->redirect("http://" + req->client()->localIP().toString());
      return;
    }
    req->send(404, "text/plain", "Not found");
  });
}

String WebApp::buildStatusJson() {
  JsonDocument doc;
  bool wifi_ok = wifiUp ? wifiUp() : false;
  doc["wifi_connected"] = wifi_ok;
  doc["wifi_ip"] = wifi_ok && wifiIp ? wifiIp() : "";
  bool mqtt_ok = mqtt.isConnected() && wifi_ok;
  doc["mqtt_connected"] = mqtt_ok;
  doc["mqtt_reason"] = mqtt.reason();
  String out;
  serializeJson(doc, out);
  return out;
}

String WebApp::buildConfigJson() {
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
  for (const auto& s : cfg.subnets) subs.add(s.cidr);

  JsonArray hosts = doc["static_hosts"].to<JsonArray>();
  for (const auto& h : cfg.static_hosts) {
    JsonObject o = hosts.add<JsonObject>();
    o["ip"] = h.ip;
    o["port"] = h.port;
    o["name"] = h.name;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

String WebApp::buildScanResultsJson() {
  JsonDocument doc;
  doc["last_scan_ms"] = scanner.lastCompletedMs();
  doc["device_now_ms"] = millis();
  doc["found_count"] = scanner.foundCount();

  JsonArray subs = doc["subnets"].to<JsonArray>();
  for (const auto& s : scanner.subnetResults()) {
    JsonObject o = subs.add<JsonObject>();
    o["cidr"] = s.cidr;
    o["online"] = s.online;
  }

  JsonArray hosts = doc["hosts"].to<JsonArray>();
  for (const auto& h : scanner.hostResults()) {
    JsonObject o = hosts.add<JsonObject>();
    o["ip"] = h.ip;
    o["port"] = h.port;
    o["name"] = h.name;
    o["online"] = h.online;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

void WebApp::broadcastJson(const String& type, const String& data) {
  ws.textAll("{\"type\":\"" + type + "\",\"data\":" + data + "}");
}

void WebApp::broadcastStatus() {
  broadcastJson("status", buildStatusJson());
}

void WebApp::broadcastScanResults() {
  broadcastJson("scan_results", buildScanResultsJson());
}

void WebApp::triggerScan() {
  scanner.start();
}
