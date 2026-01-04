# Copilot Instructions

- Project: ESP32 network monitor for Seeed XIAO ESP32C3 (Arduino framework) that pings/scans subnets and static hosts, reports to MQTT/Home Assistant, and serves a captive-portal config UI. Core logic in [src/main.cpp](src/main.cpp).
- Build/flash: use PlatformIO (`pio run`), upload (`pio run -t upload`), serial monitor at 115200 (`pio device monitor -b 115200`). LittleFS config is written at runtime; if you pre-provision, upload with `pio run -t uploadfs`.
- Dependencies declared in [platformio.ini](platformio.ini): PubSubClient, ArduinoJson 7.x, ESP32Ping, LittleFS (built-in). Board: `seeed_xiao_esp32c3`; framework: Arduino.
- Configuration storage: [src/main.cpp](src/main.cpp) mounts LittleFS and reads `/config.json` into `Config` (wifi, mqtt, scan interval, subnets, static_hosts). Save path uses the same file.
- Config JSON shape (load): `{ wifi: { ssid, pass }, mqtt: { host, port, user, pass }, scan_interval_ms, subnets: ["10.0.0.0/24"...], static_hosts: [{ ip, port, name }] }`. Cap at 8192 bytes.
- Config JSON shape (save endpoint): expects `{ wifi_ssid, wifi_pass, mqtt_host, mqtt_port, mqtt_user, mqtt_pass, scan_interval_ms, subnets: [cidr...], hosts: ["ip[:port][|name]"...] }`; after save it reboots. Host lines parse `ip[:port]|name`.
- Captive portal flow: if STA WiFi fails, starts AP `ESP32NetMon`/`esp32config`, DNS 53 wildcard to 192.168.4.1, serves the config UI at `/`, redirects unknown paths during captive mode.
- HTTP endpoints: `/` HTML config page (inline JS fetches config/save/scan), `/config` GET returns current config JSON, `/save` POST saves and restarts, `/scan` GET triggers immediate scan and replies 200/202 based on MQTT availability.
- Scanning loop: every `config.scan_interval_ms` (default 300000 ms) runs `doScan()`; pings each host in each configured subnet (prefix 1..30, excludes broadcast); 5 ms delay per host; reports MQTT `network/<cidr>/online_count`. Static hosts: if `port` set, tries TCP connect; otherwise pings IP.
- MQTT topics: Home Assistant discovery for subnets `homeassistant/sensor/espnetmon_subnet_<cidr_sanitized>/config` and hosts `homeassistant/binary_sensor/espnetmon_host_<ip_sanitized>/config`. State topics: `network/<cidr>/online_count`, `network/host/<ip>/status` (online/offline), and new-host events `network/host/<ip>/discovered` once per IP (tracked in `seenHosts`). Client ID is `esp-netmon`.
- Web UI data bindings: textareas for `subnets` and `hosts`; JS builds payload aligning with `/save` contract; keep field names consistent when extending UI.
- Loop hygiene: main loop calls `server.handleClient()`, `dnsServer.processNextRequest()` when captive, `ensureWifi()`, `ensureMqtt()`, `mqtt_client.loop()`. Avoid long blocking operations; keep new work within scan cadence.
- File safety: call `LittleFS.begin(true)` before file IO; `saveConfig()` overwrites `/config.json`. Validate subnets via `parseSubnet()` (prefix 1-30 only) and host lines via `parseHostLine()`.
- When adding MQTT messages, remember QoS 0 and retained flags: discovery and status use retained=true except new-host discovery which is not retained.
- Serial output is minimal (mount/load/save messages, setup done); add logging sparingly to avoid timing issues on low-power boards.
- Testing: no automated tests present; quick sanity check is to start with empty FS, connect via captive portal, configure WiFi/MQTT/subnets, observe MQTT topics and serial logs.

Please let me know if any sections are unclear or missing project specifics you'd like captured.
