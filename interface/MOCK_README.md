# ESP32 Overwatch UI - Mock Server Development

## Running the UI with Mock Backend

To develop and test the UI without deploying to the ESP32, you can use the mock WebSocket server.

### Quick Start

Run both the Vite dev server and the mock WebSocket server:

```bash
npm run dev
```

This will start:
- Vite dev server at http://localhost:5173
- Mock WebSocket server at ws://localhost:3001

The Vite server is configured to proxy `/ws` requests to the mock server, so the UI will automatically connect to it.

### Running Components Separately

If you want to run the components separately:

```bash
# Start only the mock server
npm run dev:mock

# Start only the Vite dev server
npm run dev:vite
```

### Mock Server Features

The mock server simulates the ESP32 backend behavior:

1. **Status Messages** - Sends WiFi and MQTT connection status
2. **Config Messages** - Provides mock configuration data
3. **Scan Results** - Returns mock subnet and host scan results
4. **Save Config** - Simulates saving configuration and reboot
5. **Trigger Scan** - Simulates a network scan with progress updates

### Simulated Behavior

- Initial connection sends all current data (status, config, scan results)
- `trigger_scan` simulates scanning each subnet with 2-second intervals
- `save_config` simulates a 1-second reboot delay
- WebSocket automatically handles client disconnections and reconnections

### Building for Production

To build the UI for deployment to the ESP32:

```bash
npm run build
```

This creates a single HTML file at `../data/index.html` ready for the ESP32.

### Customizing Mock Data

You can modify the mock data in `mock-server.ts`:

```typescript
const mockStatus = { ... };     // WiFi and MQTT status
const mockConfig = { ... };     // Configuration values
const mockScanResults = { ... }; // Scan results
```
