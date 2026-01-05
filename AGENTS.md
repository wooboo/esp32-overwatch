# ESP32 Overwatch - Agent Guidelines

ESP32 network monitor for Seeed XIAO ESP32C3 (Arduino framework) with Preact/TypeScript web UI.

## Build Commands

**Firmware (PlatformIO):**
```bash
pio run              # Build firmware (includes UI)
pio run -t upload      # Upload to device
pio run -t uploadfs    # Upload filesystem (pre-provisioned config)
pio device monitor -b 115200  # Serial monitor
```

**Web Interface (Node.js):**
```bash
cd interface
npm run dev            # Development with mock WebSocket server
npm run build          # Build for production (outputs to ../data/index.html)
npx tsc --noEmit      # Type checking
```

**Note:** `pio run` auto-builds UI via `scripts/build_interface.py` (hash-based caching).

## Project Structure

```
interface/              # Preact/TypeScript web UI
  ├── src/
  │   ├── components/  # Reusable UI components
  │   ├── hooks/      # Custom React/Preact hooks
  │   ├── types.ts    # TypeScript type definitions
  │   └── *.tsx       # Page components
  └── mock-server.ts  # Mock WebSocket for dev
src/                   # ESP32 firmware (C++)
data/                  # Build output: UI for LittleFS
platformio.ini          # PlatformIO configuration
```

## Code Style Guidelines

### TypeScript/JavaScript

**Imports:** Order: external libraries → internal modules → types
```typescript
import { useState, useEffect } from 'preact/hooks';
import { useWebSocket } from './hooks/useWebSocket';
import type { ScanResults } from './types';
```

**Component Organization:**
- Props interface first (include `path?` for route components)
- Export function immediately after interface
- Helper functions below main component (if file-local)
- Prefer functional components with hooks over class components

**Naming Conventions:**
- Components: PascalCase (`StatusPage`, `ConfigForm`)
- Functions: camelCase (`handleConfigChange`, `triggerScan`)
- Constants: UPPER_SNAKE_CASE (mock data only)
- Type names: PascalCase (`ScanResults`, `Config`)
- Event handlers: `handle*` prefix
- Boolean flags: `is*`, `has*`, or descriptive words (`scanning`, `connected`)

**TypeScript:**
- Strict mode enabled (`strict: true`)
- Always type props and function parameters
- Use `type` over `interface` for simple object shapes
- Use `interface` for component props with methods
- Explicit `any` forbidden; use `unknown` with proper guards
- No `@ts-ignore`, `@ts-expect-error`, or `as any`
- Mark optional props with `?` (`path?: string`)

**Formatting:**
- Use Preact JSX: `class` (not `className`)
- Self-closing tags: `<Component />`
- 2-space indentation
- Prefer template literals for multiline strings

**Error Handling:**
- Validate props: early returns with loading/empty states
- Parse JSON in try/catch with silent failures in production
- WebSocket error handlers log but don't crash
- Input validation in forms: fallback to defaults

**C++ (Firmware):**
- Arduino framework conventions: `setup()`, `loop()`
- Use PlatformIO libraries (see platformio.ini deps)
- File I/O: call `LittleFS.begin(true)` before operations
- Serial logging: minimal, use for debugging only (timing-sensitive)
- Avoid blocking operations in main loop

## Configuration

**TypeScript:** Target ES2020, strict type checking, JSX: `react-jsx` (import source: `preact`)

**Dependencies (interface):** preact, preact-router, tailwindcss, vite

**Dependencies (firmware):** PubSubClient, ArduinoJson 7.x, ESP32Ping, ESPAsyncWebServer, AsyncTCP

## Testing

**No automated tests currently configured.**
- Quick sanity check: build firmware, flash, connect via captive portal
- For UI: use mock server (`npm run dev`) to verify WebSocket flow

## WebSocket Protocol

**Client → Server:**
```json
{ "type": "get_all" }
{ "type": "save_config", "data": { ... } }
{ "type": "trigger_scan" }
```

**Server → Client:**
```json
{ "type": "status", "data": { ... } }
{ "type": "config", "data": { ... } }
{ "type": "scan_results", "data": { ... } }
{ "type": "scan_started", "data": { scanning, currentSubnet, completedSubnets } }
```

## Development Workflow

1. For UI changes: work in `interface/`, use `npm run dev`
2. For firmware changes: modify `src/*.cpp`, use `pio run`
3. Both changed: use `pio run` (auto-builds UI if needed)
4. Type check UI: `cd interface && npx tsc --noEmit`
5. Build firmware: `pio run -t upload`

## Filesystem Constraints

- LittleFS partition for configuration (`/config.json`)
- Config cap: 8192 bytes
- UI build output: `../data/index.html` (gzipped + plain)
- Caching: `.interface/.build_hash` to skip unnecessary UI rebuilds

## MQTT Integration

- Client ID: `esp-netmon`
- Home Assistant discovery on `homeassistant/sensor/` and `homeassistant/binary_sensor/`
- State topics: `network/<cidr>/online_count`, `network/host/<ip>/status`
- Discovery: retained=true; events: not retained
- QoS: 0 for all messages
