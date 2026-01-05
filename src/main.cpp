#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <functional>
#include <esp_task_wdt.h>

#include "config_store.h"
#include "mqtt_manager.h"
#include "network_scanner.h"
#include "web_app.h"
#include "wifi_manager.h"

ConfigStore configStore;
WifiManager wifi(configStore.data());
MqttManager mqttManager(configStore.data());
NetworkScanner scanner(configStore.data(), mqttManager);
WebApp web(configStore, scanner, mqttManager);

unsigned long lastScanKickMs = 0;
unsigned long lastStatusBroadcastMs = 0;
bool discoverySent = false;
bool lastScanActive = false;

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println("Booting ESP32 Overwatch...");

  // Initialize watchdog timer with 30 second timeout
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(nullptr);

  configStore.ensureFsMounted();
  configStore.load();
  wifi.begin();

  web.setWifiStatusProvider(
      []()
      { return wifi.isWifiUp(); },
      []()
      { return wifi.ip(); },
      []()
      { return wifi.isCaptive(); });
  web.begin();

  mqttManager.ensureConnected(wifi.isWifiUp(), wifi.isCaptive());
  mqttManager.publishDiscovery(configStore.data().subnets, configStore.data().static_hosts);

  unsigned long waitStart = millis();
  while (!mqttManager.isConnected() && millis() - waitStart < 5000)
  {
    wifi.loop();
    mqttManager.ensureConnected(wifi.isWifiUp(), wifi.isCaptive());
    mqttManager.loop();
    delay(50);
  }

  if (mqttManager.isConnected())
  {
    mqttManager.publishDiscovery(configStore.data().subnets, configStore.data().static_hosts);
    scanner.start();
  }
  else
  {
    Serial.println("Initial scan skipped (MQTT offline)");
  }

  lastScanKickMs = millis();
  Serial.println("Setup done");
}

void loop()
{
  // Feed watchdog timer at start of loop
  esp_task_wdt_reset();
  
  wifi.loop();
  mqttManager.ensureConnected(wifi.isWifiUp(), wifi.isCaptive());
  mqttManager.loop();

  if (!mqttManager.isConnected()) {
    discoverySent = false;
  } else if (!discoverySent) {
    mqttManager.publishDiscovery(configStore.data().subnets, configStore.data().static_hosts);
    discoverySent = true;
  }

  scanner.step();

  unsigned long now = millis();

  if (lastScanActive && !scanner.active()) {
    web.broadcastScanResults();
  }
  lastScanActive = scanner.active();

  if (now - lastStatusBroadcastMs >= 5000) {
    web.broadcastStatus();
    lastStatusBroadcastMs = now;
  }

  if (!scanner.active() && (now - lastScanKickMs >= configStore.data().scan_interval_ms)) {
    scanner.start();
    lastScanKickMs = now;
  }
}
