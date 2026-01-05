#!/usr/bin/env python3
"""Download config from device to backup/ and optionally copy to data/."""

import argparse
import json
import os
import shutil
import sys
import urllib.request
import urllib.error
from pathlib import Path


def backup_config(ip: str, copy_to_data: bool = True) -> bool:
    """Download config.json from device and convert to LittleFS format."""
    try:
        url = f"http://{ip}/config"
        print(f"Downloading config from {url}...")

        with urllib.request.urlopen(url, timeout=10) as response:
            if response.status != 200:
                print(f"Error: HTTP {response.status}")
                return False

            config_data = response.read().decode('utf-8')
            flat_config = json.loads(config_data)

        backup_dir = Path("backup")
        backup_dir.mkdir(exist_ok=True)

        backup_file = backup_dir / "config.json"
        with open(backup_file, 'w') as f:
            json.dump(flat_config, f, indent=2)

        print(f"✓ Config backed up to {backup_file}")
        print(f"  WiFi SSID: {flat_config.get('wifi_ssid', 'N/A')}")
        print(f"  MQTT Host: {flat_config.get('mqtt_host', 'N/A')}")
        print(f"  Subnets: {len(flat_config.get('subnets', []))}")
        print(f"  Static hosts: {len(flat_config.get('static_hosts', []))}")

        if copy_to_data:
            data_dir = Path("data")
            data_dir.mkdir(exist_ok=True)
            data_file = data_dir / "config.json"

            nested_config = {
                "wifi": {
                    "ssid": flat_config.get("wifi_ssid", ""),
                    "pass": flat_config.get("wifi_pass", "")
                },
                "mqtt": {
                    "host": flat_config.get("mqtt_host", ""),
                    "port": flat_config.get("mqtt_port", 1883),
                    "user": flat_config.get("mqtt_user", ""),
                    "pass": flat_config.get("mqtt_pass", "")
                },
                "scan_interval_ms": flat_config.get("scan_interval_ms", 300000),
                "resolve_names": flat_config.get("resolve_names", True),
                "subnets": flat_config.get("subnets", []),
                "static_hosts": flat_config.get("static_hosts", [])
            }

            with open(data_file, 'w') as f:
                json.dump(nested_config, f, indent=2)

            print(f"✓ Converted to nested format and saved to {data_file}")
            print(f"  (ready for LittleFS deployment)")

        return True

    except urllib.error.URLError as e:
        print(f"Error: Could not connect to device at {ip}")
        print(f"  Reason: {e.reason}")
        return False
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON response from device")
        print(f"  {e}")
        return False
    except Exception as e:
        print(f"Error: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Download config from ESP32 device"
    )
    parser.add_argument("ip", nargs="?", help="ESP32 device IP")
    parser.add_argument("--no-copy", action="store_true", help="Don't copy to data/ folder")

    args = parser.parse_args()

    if args.ip:
        ip = args.ip
    elif "ESP32_IP" in os.environ:
        ip = os.environ["ESP32_IP"]
    else:
        print("ESP32 Overwatch - Config Backup")
        print("Enter IP address of your ESP32 device:")
        ip = input("> ").strip()

    if not ip:
        print("Error: IP address required")
        sys.exit(1)

    success = backup_config(ip, copy_to_data=not args.no_copy)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()