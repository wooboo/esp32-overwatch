# Config Backup/Restore Scripts

Scripts to preserve ESP32 settings during firmware deployment.

## Workflow Overview

**Key concept:** Put `config.json` in `data/` folder. This file gets uploaded to LittleFS via `pio run -t uploadfs`. The device boots with existing config - no captive portal needed.

## Scripts

### `backup_config.py`
Downloads current config from device to `backup/config.json` and copies to `data/config.json`.

**Usage:**
```bash
python scripts/backup_config.py [IP]
```

**IP sources (in order):**
1. Command line argument
2. `ESP32_IP` environment variable
3. Interactive prompt

**Options:**
- `--no-copy`: Download to `backup/` only, don't copy to `data/`

### `deploy.py`
Builds firmware and uploads it + filesystem (including `config.json` if present).

**Usage:**
```bash
python scripts/deploy.py
```

## Deployment Workflow

### First-time Setup (or after factory reset)

1. Connect to device via captive portal (ESP32NetMon / esp32config)
2. Configure WiFi, MQTT, and targets
3. Save & Reboot
4. Download config:
   ```bash
   python scripts/backup_config.py 192.168.1.100
   ```
   Config is now in `backup/config.json` and `data/config.json`
5. `data/config.json` is gitignored, so secrets won't leak

### Subsequent Updates

**Automatic deployment (preserves settings):**
```bash
python scripts/deploy.py
```

This:
1. Builds firmware
2. Uploads firmware via serial
3. Uploads filesystem (includes web UI + `data/config.json`)
4. Device boots with existing config - no captive portal needed!

**If config needs changes:**
```bash
# Option A: Download, edit, deploy
python scripts/backup_config.py 192.168.1.100
nano data/config.json  # Edit as needed
python scripts/deploy.py

# Option B: Use web UI, then re-download
# Edit via http://192.168.1.100/settings
python scripts/backup_config.py 192.168.1.100  # Save to data/
python scripts/deploy.py
```

## Files

- `backup/config.json` - Backup copy (gitignored)
- `data/config.json` - Active config uploaded to device (gitignored)
- `.gitignore` - Prevents committing secrets

## Config Formats

The project uses different config formats for different purposes:

### 1. LittleFS Format (`data/config.json`, `backup/config.json`)
**Used for**: File storage, filesystem upload via serial
```json
{
  "wifi": {
    "ssid": "NetworkName",
    "pass": "Password"
  },
  "mqtt": {
    "host": "192.168.1.10",
    "port": 1883,
    "user": "mqtt_user",
    "pass": "mqtt_pass"
  },
  "scan_interval_ms": 300000,
  "resolve_names": true,
  "subnets": [
    "10.11.12.0/24",
    "10.10.10.0/24"
  ],
  "static_hosts": [
    {"ip": "10.11.12.6", "port": 8123, "name": "Home Assistant"}
  ]
}
```

### 2. HTTP API Format (`/config` endpoint)
**Used for**: Web UI read/write, `backup_config.py` download

Supports both formats (backward compatible):
```json
{
  "subnets": [
    "10.11.12.0/24",
    {"cidr": "10.10.10.0/24", "name": "Home Network"}
  ],
  "static_hosts": [
    "10.11.12.6:8123 # Home Assistant",
    {"ip": "10.11.99.1", "port": 8123, "name": "Router"}
  ]
}
```

### Key Differences

| Field | LittleFS Format | HTTP API Format |
|--------|-----------------|-----------------|
| WiFi/MQTT | Nested objects | Flat keys |
| Subnets | Array of CIDR strings | Array of CIDR strings |
| Hosts | Array of objects (ip/port/name) | Array of strings ("ip:port # name") |

**Note**: `backup_config.py` automatically converts from HTTP API format to LittleFS format when copying to `data/config.json`.

### Limitations

None - all features are fully supported:
- Subnets with names ✓
- Static hosts with names ✓
- Hostnames (not just IPs) ✓
- Backward compatible with plain CIDR strings ✓

## Security Notes

- **WiFi/MQTT credentials**: Stored in `config.json` - treat as sensitive
- **Git**: Both `backup/config.json` and `data/config.json` are gitignored
- **Don't commit**: Never add `config.json` to version control

## `save_targets` Payload Format

The `save_targets` WebSocket message accepts subnet data in multiple formats:

1. **String format** (backward compatible): `"10.11.12.0/24 # Enterprise"`
2. **Object format** (new): `{"cidr": "10.11.12.0/24", "name": "Enterprise"}`

Both formats are supported and parsed correctly. Names are preserved in both cases.

## How It Works

1. `data/` folder contains files uploaded to LittleFS
2. `pio run -t uploadfs` packages all of `data/` and uploads via serial
3. Device loads `/config.json` from LittleFS on boot
4. If config exists → connects to WiFi/MQTT; if missing → captive portal

## Notes

- **WiFi credentials**: Saved in `config.json` - treat as sensitive
- **Git**: Both `backup/config.json` and `data/config.json` are gitignored
- **Don't commit**: Never add `config.json` to version control
- **Library dependencies**: PlatformIO installs libraries to `lib/` directory (gitignored)
- **Clean repo**: If you see library directories in repo root, they can be safely deleted
