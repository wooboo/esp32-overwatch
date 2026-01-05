# ESP32 Overwatch32 - Improvement Roadmap

This document outlines identified improvements for the ESP32 network monitoring solution, organized by priority and category.

---

## High Priority (Stability & Reliability)

### 1. WiFi Reconnection from Captive Mode
**Location:** `wifi_manager.cpp:47`

**Problem:** Once the device enters captive portal mode (`captive=true`), it never attempts to reconnect to the configured WiFi network. The device stays in AP mode forever until manually rebooted.

**Solution:**
- Add periodic retry timer (e.g., every 5 minutes) to attempt STA connection
- After successful STA connection, disable AP mode
- Configurable retry interval via web UI

**Effort:** Medium

---

### 2. MQTT Reconnect Backoff
**Location:** `mqtt_manager.cpp:25-56`

**Problem:** On MQTT connection failure, the device immediately retries on the next loop iteration, potentially spamming the broker with connection attempts.

**Solution:**
- Implement exponential backoff (1s → 2s → 4s → ... → 60s max)
- Reset backoff timer on successful connection
- Add connection attempt counter for diagnostics

**Effort:** Low

---

### 3. Watchdog Timer
**Location:** `main.cpp`

**Problem:** If a network scan hangs (e.g., unresponsive host with long timeout), the device becomes unresponsive with no recovery mechanism.

**Solution:**
- Initialize ESP32 task watchdog (`esp_task_wdt_init()`)
- Feed watchdog in main loop
- Configure appropriate timeout (e.g., 30 seconds)

**Effort:** Low

---

### 4. Reduce String Heap Fragmentation
**Location:** Throughout codebase

**Problem:** Heavy use of Arduino `String` class causes heap fragmentation on long-running ESP32 devices, eventually leading to crashes.

**Solution:**
- Replace `String` with `char[]` buffers in MQTT publish paths
- Use stack-allocated buffers for topic/payload construction
- Consider `StaticJsonDocument` instead of `JsonDocument` where size is known

**Affected files:**
- `mqtt_manager.cpp` (topic and payload construction)
- `network_scanner.cpp` (IP string handling)
- `config_store.cpp` (JSON serialization)

**Effort:** High

---

## Medium Priority (Functionality & Features)

### 5. Configurable Hostname & Unique Device ID
**Location:** `main.cpp:44-51`, `mqtt_manager.cpp:29,38`

**Problem:** 
- MQTT client ID is hardcoded to `"esp-overwatch"`
- Multiple devices on same network will conflict
- Device hostname generation is basic

**Solution:**
- Use configurable hostname in MQTT client ID
- Add device ID to Home Assistant discovery payloads
- Generate unique default from full MAC address

**Effort:** Low

---

### 6. Scan Timeout & Abort Mechanism
**Location:** `network_scanner.cpp`

**Problem:** Scanning large subnets (e.g., /16) or subnets with many unresponsive hosts can take extremely long with no abort option.

**Solution:**
- Add configurable scan timeout (default: 10 minutes)
- Add `/scan/abort` HTTP endpoint
- Track scan duration and report in status
- Consider per-host timeout configuration

**Effort:** Medium

---

### 7. Consolidate Discovery Publishing
**Location:** `main.cpp:64,77,100`

**Problem:** `publishDiscovery()` is called up to 3 times during startup, causing duplicate MQTT traffic.

**Solution:**
- Remove duplicate calls in `setup()`
- Keep only the `discoverySent` flag pattern in `loop()`
- Ensure discovery is sent once after MQTT connects

**Effort:** Low

---

### 8. Non-Blocking Operations
**Location:** `wifi_manager.cpp:19`, `web_app.cpp:143,202`

**Problem:** `delay()` calls block the main loop, affecting responsiveness and MQTT keepalive.

**Solution:**
- Replace `delay()` with state machine patterns
- Use `millis()` for timing
- Queue reboot requests instead of immediate `ESP.restart()`

**Effort:** Medium

---

### 9. Offline Host Tracking
**Location:** `network_scanner.cpp`

**Problem:** Only current online/offline state is reported. No historical data about when hosts went offline.

**Solution:**
- Track last-seen timestamp per host
- Publish `last_seen` attribute in MQTT
- Optional: Store recent history in SPIFFS

**Effort:** Medium

---

### 10. Unused Variable Cleanup
**Location:** `network_scanner.h:41`

**Problem:** `seenHosts` set is declared but never used in the implementation.

**Solution:**
- Either implement new-host-ever-seen tracking using this set
- Or remove the unused variable

**Effort:** Low

---

## Low Priority (Polish & Optimization)

### 11. Async/Parallel Scanning
**Location:** `network_scanner.cpp`

**Problem:** Current scanning is synchronous with budget of 3 hosts per step. Large subnets take very long to scan.

**Solution:**
- Increase `SCAN_STEP_BUDGET` for faster scanning
- Consider async ping library
- Implement parallel TCP port checking
- Add scan progress reporting to web UI

**Effort:** High

---

### 12. Security Improvements

#### 12a. Encrypted Credentials Storage
**Location:** `config_store.cpp`, `/config.json`

**Problem:** WiFi and MQTT passwords stored in plaintext on filesystem.

**Solution:**
- Encrypt sensitive fields using ESP32 secure storage
- Or use NVS with encryption enabled
- Consider hardware secure element if available

**Effort:** High

#### 12b. Configurable OTA Password
**Location:** `main.cpp:53`

**Problem:** OTA password hardcoded to `"esp32config"`.

**Solution:**
- Move OTA password to config file
- Add to web UI configuration
- Consider per-device unique password

**Effort:** Low

#### 12c. HTTPS for Web UI
**Location:** `web_app.cpp`

**Problem:** Configuration portal serves over HTTP. Credentials transmitted in cleartext.

**Solution:**
- Add self-signed certificate support
- Use `WebServerSecure` or similar
- Generate cert on first boot

**Effort:** High

---

### 13. Configurable Debug Logging
**Location:** Throughout codebase

**Problem:** Serial logging on every scanned host adds latency and clutters output.

**Solution:**
- Add log level configuration (ERROR, WARN, INFO, DEBUG)
- Make per-host logging optional
- Consider remote syslog support

**Effort:** Medium

---

### 14. Code Quality Improvements

#### 14a. Consistent Formatting
**Problem:** Mixed tabs/spaces, inconsistent indentation across files.

**Solution:**
- Add `.clang-format` configuration
- Run formatter on all source files
- Add pre-commit hook

#### 14b. Const Correctness
**Location:** `mqtt_manager.h:13`

**Problem:** `isConnected()` modifies no state but isn't marked `const`.

**Solution:**
- Add `const` qualifier to appropriate methods
- Review all getters for const correctness

#### 14c. Header Constants
**Location:** `config_store.h:7-11`

**Problem:** `static const` in header can cause multiple definitions.

**Solution:**
- Use `constexpr` for compile-time constants
- Move to anonymous namespace in `.cpp` where appropriate

**Effort:** Low (all)

---

### 15. Additional Detection Methods
**Location:** `network_scanner.cpp`

**Problem:** Only ICMP ping and TCP connect are supported.

**Solution:**
- Add ARP cache inspection for passive discovery
- Add mDNS/DNS-SD service discovery
- Add UDP port probing option
- Consider SNMP support for network devices

**Effort:** High

---

### 16. Bounded Memory for Host Tracking
**Location:** `network_scanner.h:39-41`

**Problem:** `std::set<String>` for online hosts can grow unbounded on large subnets.

**Solution:**
- Add maximum tracked hosts limit
- Use LRU eviction for oldest entries
- Or use bloom filter for seen-hosts (memory efficient, probabilistic)

**Effort:** Medium

---

## Implementation Order Recommendation

### Phase 1: Stability (Week 1-2)
1. [ ] Watchdog timer
2. [ ] MQTT reconnect backoff
3. [ ] WiFi reconnection from captive mode
4. [ ] Consolidate discovery publishing

### Phase 2: Core Improvements (Week 3-4)
5. [ ] Configurable hostname & unique device ID
6. [ ] Scan timeout & abort mechanism
7. [ ] Non-blocking operations
8. [ ] Unused variable cleanup

### Phase 3: Memory & Performance (Week 5-6)
9. [ ] Reduce String heap fragmentation
10. [ ] Configurable debug logging
11. [ ] Bounded memory for host tracking

### Phase 4: Features (Week 7-8)
12. [ ] Offline host tracking
13. [ ] Configurable OTA password
14. [ ] Code quality improvements

### Phase 5: Advanced (Future)
15. [ ] Async/parallel scanning
16. [ ] Security improvements (encryption, HTTPS)
17. [ ] Additional detection methods

---

## Version Targets

| Version | Focus | Key Deliverables |
|---------|-------|------------------|
| v1.1.0 | Stability | Watchdog, MQTT backoff, WiFi reconnect |
| v1.2.0 | Reliability | Scan timeout, non-blocking ops, memory fixes |
| v1.3.0 | Features | Host tracking, configurable hostname |
| v2.0.0 | Security | Encrypted storage, HTTPS, secure OTA |

---

## Contributing

When implementing improvements:
1. Create feature branch from `main`
2. Follow existing code style (pending formatter setup)
3. Test on actual hardware before PR
4. Update this roadmap when items are completed

---

*Last updated: January 2026*
