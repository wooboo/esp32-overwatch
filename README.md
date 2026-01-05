# ESP32 Overwatch

A network monitoring solution for the Seeed XIAO ESP32C3 that scans network subnets and static hosts, reports status to MQTT/Home Assistant, and provides a web-based configuration interface via captive portal.

![ESP32 Overwatch](https://img.shields.io/badge/ESP32-Overwatch-blue)
![Platform](https://img.shields.io/badge/Platform-PlatformIO-green)
![Framework](https://img.shields.io/badge/Framework-Arduino-red)
![UI](https://img.shields.io/badge/UI-Preact%2FTypeScript-cyan)

## Features

- **Network Scanning**: Automatically scans configured subnets and static hosts at configurable intervals
- **Captive Portal**: Web-based configuration interface that activates when WiFi fails to connect
- **MQTT Integration**: Publishes network status to MQTT broker with Home Assistant auto-discovery
- **Multi-Subnet Support**: Monitor multiple network subnets simultaneously with custom naming
- **Static Host Monitoring**: Track specific hosts with optional TCP port checking
- **Real-time Status**: View network status, scan results, and configuration via responsive web UI
- **Persistent Configuration**: Stores settings in LittleFS (up to 8KB config size)
- **Auto-Recovery**: Automatic WiFi and MQTT reconnection logic

## Hardware Requirements

- **Board**: Seeed XIAO ESP32C3
- **Minimum**: 4MB Flash, 400KB SRAM
- **Connectivity**: WiFi (2.4GHz only)
- **Power**: USB-C or 5V DC

## Quick Start

### 1. Clone and Install Dependencies

```bash
git clone <repository-url>
cd esp32-overwatch
```

Firmware dependencies are managed by PlatformIO. Web UI dependencies:

```bash
cd interface
npm install
```

### 2. Build and Flash Firmware

```bash
# Build firmware (auto-builds web UI if changed)
pio run

# Upload to device
pio run -t upload

# Upload pre-provisioned configuration (optional)
pio run -t uploadfs

# Monitor serial output
pio device monitor -b 115200
```

### 3. Initial Configuration

1. Power on the device
2. Connect to WiFi network `ESP32NetMon` (password: `esp32config`)
3. Open browser - it should redirect to configuration portal at `http://192.168.4.1`
4. Configure:
   - WiFi network credentials
   - MQTT broker settings
   - Network subnets to scan (CIDR format)
   - Static hosts to monitor (IP:port|name format)
5. Save and wait for device to reboot

### 4. Verify Operation

- Check serial monitor for "Booting ESP32 Overwatch..." and successful connection messages
- Verify MQTT topics appear on your broker:
  - `network/<cidr>/online_count` - Host count per subnet
  - `network/host/<ip>/status` - Individual host status (online/offline)
- Home Assistant sensors should auto-discover:
  - `sensor.espnetmon_subnet_<cidr_sanitized>` - Subnet device count
  - `binary_sensor.espnetmon_host_<ip_sanitized>` - Host availability

## Configuration

Configuration is stored in `/data/config.json` on the device (LittleFS). Example:

```json
{
  "wifi": {
    "ssid": "YourWiFiSSID",
    "pass": "YourWiFiPassword"
  },
  "mqtt": {
    "host": "192.168.1.100",
    "port": 1883,
    "user": "mqtt_user",
    "pass": "mqtt_password"
  },
  "scan_interval_ms": 300000,
  "resolve_names": true,
  "subnets": [
    {
      "cidr": "192.168.1.0/24",
      "name": "Home Network"
    },
    {
      "cidr": "10.0.0.0/24",
      "name": "IoT Network"
    }
  ],
  "static_hosts": [
    {
      "ip": "192.168.1.10",
      "port": 80,
      "name": "Web Server"
    },
    {
      "ip": "192.168.1.20",
      "name": "NAS Device"
    }
  ]
}
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `wifi.ssid` | string | - | WiFi network SSID |
| `wifi.pass` | string | - | WiFi network password |
| `mqtt.host` | string | - | MQTT broker hostname/IP |
| `mqtt.port` | number | 1883 | MQTT broker port |
| `mqtt.user` | string | - | MQTT username (optional) |
| `mqtt.pass` | string | - | MQTT password (optional) |
| `scan_interval_ms` | number | 300000 | Time between scans in milliseconds |
| `resolve_names` | boolean | true | Attempt DNS resolution for discovered hosts |
| `subnets` | array | - | Array of subnet objects with `cidr` and `name` |
| `static_hosts` | array | - | Array of host objects with `ip`, optional `port`, and `name` |

### Captive Portal Behavior

- **AP SSID**: `ESP32NetMon`
- **AP Password**: `esp32config`
- **AP IP**: `192.168.4.1`
- **DNS**: Wildcard redirect to 192.168.4.1 for captive portal detection

The captive portal activates when:
- No valid configuration exists on first boot
- WiFi connection fails after configured credentials are saved

## Web Interface

The web UI provides real-time monitoring and configuration:

- **Status Page**: View WiFi connection, MQTT status, scan progress, and recent scan results
- **Settings Page**: Modify configuration (WiFi, MQTT, subnets, hosts)
- **Scan Results**: Table showing discovered hosts with status, hostname, and response time
- **Navigation**: Simple tab-based navigation between pages

### Web UI Development

For UI development with a mock server:

```bash
cd interface
npm run dev          # Start Vite dev server + mock WebSocket server
npm run build        # Build for production (outputs to ../data/index.html)
npx tsc --noEmit     # Type checking
```

The UI is built automatically when running `pio run` (uses hash-based caching to skip unnecessary rebuilds).

## MQTT & Home Assistant Integration

### MQTT Topics

**Discovery (retained = true)**:
```
homeassistant/sensor/espnetmon_subnet_<cidr>/config
homeassistant/binary_sensor/espnetmon_host_<ip>/config
```

**State Topics**:
```
network/<cidr>/online_count          # Number of online hosts in subnet
network/host/<ip>/status             # "online" or "offline"
network/host/<ip>/discovered         # Emitted once when new host found (not retained)
```

### Home Assistant Configuration

Sensors auto-discover via MQTT Discovery. Manual configuration example:

```yaml
sensor:
  - platform: mqtt
    name: "Home Network Devices"
    state_topic: "network/192.168.1.0/24/online_count"
    unique_id: "espnetmon_subnet_192.168.1.0"

binary_sensor:
  - platform: mqtt
    name: "Web Server"
    state_topic: "network/host/192.168.1.10/status"
    payload_on: "online"
    payload_off: "offline"
    unique_id: "espnetmon_host_192.168.1.10"
```

## Development

### Project Structure

```
esp32-overwatch/
├── interface/              # Preact/TypeScript web UI
│   ├── src/
│   │   ├── components/    # Reusable UI components
│   │   ├── hooks/         # Custom Preact hooks
│   │   ├── types.ts       # TypeScript type definitions
│   │   └── *.tsx          # Page components
│   ├── mock-server.ts     # Mock WebSocket for development
│   └── package.json       # Node.js dependencies
├── src/                   # ESP32 firmware (C++)
│   ├── main.cpp           # Application entry point
│   ├── config_store.cpp   # Configuration persistence
│   ├── mqtt_manager.cpp   # MQTT communication
│   ├── network_scanner.cpp # Network scanning logic
│   ├── web_app.cpp        # HTTP server & captive portal
│   └── wifi_manager.cpp   # WiFi management
├── include/               # C++ header files
├── data/                  # LittleFS filesystem
│   ├── config.json        # Runtime configuration
│   └── index.html        # Built web UI (gzipped)
├── scripts/               # Build and utility scripts
└── platformio.ini         # PlatformIO configuration
```

### Build System

**Firmware (PlatformIO)**:
- Platform: ESP32
- Board: seeed_xiao_esp32c3
- Framework: Arduino
- Filesystem: LittleFS

**Dependencies**:
- `knolleary/PubSubClient@^2.8` - MQTT client
- `bblanchon/ArduinoJson@^7.4` - JSON parsing
- `marian-craciunescu/ESP32Ping@^1.7` - ICMP ping
- `ESP32Async/ESPAsyncWebServer` - Async HTTP server
- `ESP32Async/AsyncTCP` - Async TCP

**Web Interface (Node.js)**:
- Framework: Preact with Vite
- Language: TypeScript
- Styling: TailwindCSS
- Routing: preact-router

### Code Style

**TypeScript**:
- Strict mode enabled
- No `any` types - use `unknown` with guards
- Components: PascalCase, functions: camelCase
- Imports: external → internal → types

**C++**:
- Arduino framework conventions
- `setup()` / `loop()` structure
- Call `LittleFS.begin(true)` before file operations
- Minimal serial logging (timing-sensitive)

### Testing

No automated tests are currently configured. Manual testing:

1. **Firmware**: Flash to device, connect via serial monitor at 115200 baud
2. **Web UI**: Use `npm run dev` with mock server in `interface/`
3. **Integration**: Configure with actual WiFi/MQTT broker, observe topics

## Roadmap

See [ROADMAP.md](ROADMAP.md) for planned improvements including:

- WiFi reconnection from captive mode
- MQTT reconnect backoff
- Watchdog timer for scan hangs
- Configurable hostname and device ID
- Scan timeout & abort mechanism
- Security improvements (encrypted storage, HTTPS)
- And more...

## HTTP Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web interface HTML |
| `/config` | GET | Current configuration JSON |
| `/save` | POST | Save configuration and reboot |
| `/scan` | GET | Trigger immediate scan (returns 200 if idle, 202 if scanning) |

## WebSocket Protocol

The web UI communicates with the firmware via WebSocket:

**Client → Server**:
```json
{ "type": "get_all" }
{ "type": "save_config", "data": { ... } }
{ "type": "trigger_scan" }
```

**Server → Client**:
```json
{ "type": "status", "data": { ... } }
{ "type": "config", "data": { ... } }
{ "type": "scan_results", "data": { ... } }
{ "type": "scan_started", "data": { "scanning": true, "currentSubnet": "...", "completedSubnets": [...] } }
```

## Troubleshooting

### Device stays in captive portal mode

- Check WiFi credentials are correct
- Verify 2.4GHz network (ESP32 doesn't support 5GHz)
- Check if AP isolation is enabled on router
- Monitor serial output for connection errors

### MQTT not connecting

- Verify broker hostname/IP is reachable from ESP32 network
- Check broker allows connections from new clients
- Verify MQTT credentials and port (default: 1883)
- Check firewall rules

### Scans are slow or incomplete

- Reduce subnet size (use /24 instead of /16)
- Increase `scan_interval_ms` to reduce frequency
- Check network latency - slow networks affect ping performance
- Monitor serial output for scan progress

### Config not saving

- Verify LittleFS is mounted (check serial logs)
- Ensure config size is under 8192 bytes
- Check filesystem corruption - try `pio run -t uploadfs` to reflash

## Contributing

1. Fork the repository
2. Create a feature branch
3. Follow existing code style (see [AGENTS.md](AGENTS.md) for guidelines)
4. Test on actual hardware before submitting PR
5. Update relevant documentation

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- PlatformIO for the excellent build system
- Preact for the lightweight UI framework
- The ESP32 and Arduino communities
