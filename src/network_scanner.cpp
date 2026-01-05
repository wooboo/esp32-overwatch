#include "network_scanner.h"

namespace {
  const uint8_t SCAN_STEP_BUDGET = 3;
}

NetworkScanner::NetworkScanner(Config& cfg, MqttManager& mqttMgr)
  : config(cfg), mqtt(mqttMgr) {}

bool NetworkScanner::pingHost(const IPAddress &ip)
{
  return Ping.ping(ip, 1);
}

bool NetworkScanner::portOpen(const String &ip, int port)
{
  WiFiClient c;
  return c.connect(ip.c_str(), port, PING_TIMEOUT_MS);
}

bool NetworkScanner::start()
{
  if (scanning) return false;
  lastHostResults.clear();
  lastSubnetResults.clear();
  prevOnlineHosts = currentOnlineHosts;
  currentOnlineHosts.clear();
  foundOnlineCount = 0;
  foundOnlineCountSubnet = 0;
  scanning = true;
  mqttReady = mqtt.isConnected();
  hostIndex = 0;
  subnetIndex = 0;
  subnetCursor = config.subnets.empty() ? 0 : config.subnets[0].firstHost;
  currentOnline = 0;
  lastScanStartMs = millis();
  Serial.println("Scan started");
  return true;
}

void NetworkScanner::finishScan()
{
  scanning = false;
  prevOnlineHosts = currentOnlineHosts;
  lastScanCompletedMs = millis();
  Serial.println("Scan complete");
}

void NetworkScanner::finishSubnet()
{
  const Subnet &subnet = config.subnets[subnetIndex];
  SubnetScanResult r;
  r.cidr = subnet.cidr;
  r.online = currentOnline;
  lastSubnetResults.push_back(r);
  if (mqttReady) mqtt.publishOnlineCount(subnet, currentOnline);
  if (mqttReady) mqtt.publishFoundCount(subnet, foundOnlineCountSubnet);
  subnetIndex++;
  if (subnetIndex < config.subnets.size()) {
    subnetCursor = config.subnets[subnetIndex].firstHost;
    foundOnlineCountSubnet = 0;
    currentOnline = 0;
  } else {
    finishScan();
  }
}

void NetworkScanner::step()
{
  if (!scanning) return;
  uint8_t budget = SCAN_STEP_BUDGET;
  while (budget-- && scanning) {
    // Keep MQTT alive during potentially slow scan work to avoid availability flaps
    mqtt.loop();

    if (hostIndex < config.static_hosts.size()) {
      const auto &h = config.static_hosts[hostIndex];
      bool ok = false;
      if (h.port) ok = portOpen(h.ip, h.port);
      else {
        IPAddress parsed; if (parsed.fromString(h.ip)) ok = pingHost(parsed);
      }
      Serial.print("scan host "); Serial.print(h.ip);
      if (h.port) { Serial.print(":"); Serial.print(h.port); Serial.print(" tcp "); }
      else { Serial.print(" ping "); }
      Serial.println(ok ? "online" : "offline");
      HostScanResult hr;
      hr.ip = h.ip;
      hr.port = h.port;
      hr.name = h.name;
      hr.online = ok;
      lastHostResults.push_back(hr);
      if (ok) {
        currentOnlineHosts.insert(h.ip);
        bool newSincePrev = prevOnlineHosts.find(h.ip) == prevOnlineHosts.end();
        if (newSincePrev) foundOnlineCount++;
        if (mqttReady) {
          mqtt.publishHostStatus(h, true);
        }
      } else if (mqttReady) {
        mqtt.publishHostStatus(h, false);
      }
      hostIndex++;
      continue;
    }

    if (subnetIndex >= config.subnets.size()) {
      finishScan();
      break;
    }

    const Subnet &subnet = config.subnets[subnetIndex];
    IPAddress target = IPAddress((subnetCursor >> 24) & 0xFF, (subnetCursor >> 16) & 0xFF, (subnetCursor >> 8) & 0xFF, subnetCursor & 0xFF);
    bool ok = pingHost(target);
    if (ok) {
      currentOnline++;
      String ipStr = target.toString();
      currentOnlineHosts.insert(ipStr);
      bool newSincePrev = prevOnlineHosts.find(ipStr) == prevOnlineHosts.end();
      if (newSincePrev) {
        foundOnlineCount++;
        foundOnlineCountSubnet++;
      }
      if (mqttReady) {
        if (newSincePrev) mqtt.publishNewHost(ipStr);
        mqtt.publishHostStatusIp(ipStr, true);
      }
    }
    Serial.print("scan subnet "); Serial.print(subnet.cidr); Serial.print(" host "); Serial.print(target); Serial.print(" ping "); Serial.println(ok ? "online" : "offline");

    if (subnetCursor >= subnet.lastHost) {
      finishSubnet();
    } else {
      subnetCursor++;
    }
  }
}

bool NetworkScanner::active() const { return scanning; }
unsigned long NetworkScanner::lastCompletedMs() const { return lastScanCompletedMs; }
const std::vector<SubnetScanResult>& NetworkScanner::subnetResults() const { return lastSubnetResults; }
const std::vector<HostScanResult>& NetworkScanner::hostResults() const { return lastHostResults; }
int NetworkScanner::foundCount() const { return foundOnlineCount; }
