#!/usr/bin/env python3
"""Deploy firmware with config from data/ folder."""

import json
import subprocess
import sys
from pathlib import Path


def main():
    data_config = Path("data/config.json")

    print("=" * 50)
    print("ESP32 Overwatch - Deploy")
    print("=" * 50)

    if data_config.exists():
        with open(data_config, 'r') as f:
            config = json.load(f)

        print(f"\n✓ Found config at {data_config}")
        print("  This will be uploaded to device filesystem.")
        print("  Device will boot with existing settings (no captive portal).")
        print(f"  WiFi: {config.get('wifi', {}).get('ssid', 'N/A')}")
        print(f"  Subnets: {len(config.get('subnets', []))}")
        print(f"  Hosts: {len(config.get('static_hosts', []))}")
    else:
        print(f"\n⚠ No config at {data_config}")
        print("  Device will enter captive portal mode on first boot.")
        print("  Connect to ESP32NetMon / esp32config to configure.")
        print("\nTo use existing settings:")
        print("  1. Connect to your running device via HTTP")
        print("  2. Run: python scripts/backup_config.py <device-ip>")
        print("  3. Config will be saved to data/config.json")
        response = input("\nContinue anyway? (y/N): ").strip().lower()
        if response != 'y':
            print("Aborted.")
            sys.exit(0)

    print("\n" + "=" * 50)
    print("Step 1: Building firmware...")
    print("=" * 50)
    result = subprocess.run(["pio", "run"])
    if result.returncode != 0:
        print("Error: Build failed")
        sys.exit(1)

    print("\n" + "=" * 50)
    print("Step 2: Uploading firmware...")
    print("=" * 50)
    result = subprocess.run(["pio", "run", "-t", "upload"])
    if result.returncode != 0:
        print("Error: Upload failed")
        sys.exit(1)

    print("\n" + "=" * 50)
    print("Step 3: Uploading filesystem...")
    print("=" * 50)
    print("  Includes web UI (index.html)")
    if data_config.exists():
        print("  Includes config.json (preserves settings)")
    result = subprocess.run(["pio", "run", "-t", "uploadfs"])
    if result.returncode != 0:
        print("Error: Filesystem upload failed")
        sys.exit(1)

    print("\n" + "=" * 50)
    print("✓ Deployment complete!")
    print("=" * 50)


if __name__ == "__main__":
    main()