#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <functional>

#include "config_store.h"
#include "mqtt_manager.h"
#include "network_scanner.h"
#include "web_app.h"
#include "wifi_manager.h"

ConfigStore configStore;
WifiManager wifi(configStore.data());
MqttManager mqttManager(wifi.client(), configStore.data());
NetworkScanner scanner(configStore.data(), mqttManager);
WebApp web(configStore, scanner, mqttManager);

unsigned long lastScanKickMs = 0;

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println("Booting ESP32 Overwatch...");

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
  web.handle();
  wifi.loop();
  mqttManager.ensureConnected(wifi.isWifiUp(), wifi.isCaptive());
  mqttManager.loop();

  if (mqttManager.isConnected())
  {
    static bool discoverySent = false;
    if (!discoverySent)
    {
      mqttManager.publishDiscovery(configStore.data().subnets, configStore.data().static_hosts);
      discoverySent = true;
    }
  }

  scanner.step();

  unsigned long now = millis();
  if (!scanner.active() && (now - lastScanKickMs >= configStore.data().scan_interval_ms))
  {
    scanner.start();
    lastScanKickMs = now;
  }
}
