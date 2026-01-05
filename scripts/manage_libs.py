#!/usr/bin/env python3
"""Manage PlatformIO libraries - move them to lib/ directory and clean up."""

import os
import shutil
from pathlib import Path


def move_libs_to_lib_dir():
    """Move any library directories in root to lib/ directory."""
    root_dir = Path(".")
    lib_dir = root_dir / "lib"

    # Create lib directory if it doesn't exist
    lib_dir.mkdir(exist_ok=True)

    # Library directory patterns to look for
    lib_patterns = [
        "arduino-esp32",
        "ESP32-DNSServerAsync",
        "ESPAsync_WiFiManager",
        "esp-idf",
        "esp-idf-repo",
        "socket.io",
        "uWebSockets",
        "WebSocket-Node",
        "ws",
        # Add any other library names you see
    ]

    moved = False
    for item in root_dir.iterdir():
        if item.is_dir() and item.name in lib_patterns:
            target = lib_dir / item.name
            if target.exists():
                shutil.rmtree(target)
            shutil.move(str(item), str(lib_dir))
            print(f"Moved {item.name} to lib/")
            moved = True

    if moved:
        print("Library cleanup complete")
    else:
        print("No library directories to clean up")


if __name__ == "__main__":
    move_libs_to_lib_dir()