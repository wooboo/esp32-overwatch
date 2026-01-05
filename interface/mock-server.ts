import { WebSocketServer } from 'ws';

const PORT = 3001;

const mockStatus = {
  wifi_connected: true,
  wifi_ip: '192.168.1.100',
  mqtt_connected: true,
  mqtt_reason: '',
};

const mockConfig = {
  wifi_ssid: 'MyWiFi',
  wifi_pass: '********',
  mqtt_host: '192.168.1.10',
  mqtt_port: 1883,
  mqtt_user: 'admin',
  mqtt_pass: '********',
  scan_interval_ms: 300000,
  subnets: [
    { cidr: '10.11.12.0/22', name: 'Home Network' },
    { cidr: '10.11.16.0/24', name: 'Office Network' },
  ],
  static_hosts: [
    { ip: '10.11.12.6', port: 8123, name: 'HA VM' },
    { ip: '10.11.99.1', name: 'OPNsense' },
  ],
};

const mockScanResults = {
  last_scan_ms: Date.now() - 120000,
  device_now_ms: Date.now(),
  found_count: 5,
  subnets: [
    { cidr: '10.11.12.0/22', name: 'Home Network', online: 3 },
    { cidr: '10.11.16.0/24', name: 'Office Network', online: 2 },
  ],
  hosts: [
    { ip: '10.11.12.6', port: 8123, name: 'HA VM', online: true },
    { ip: '10.11.12.10', name: 'Home Assistant', online: true },
    { ip: '10.11.12.20', name: 'Desktop', online: true },
    { ip: '10.11.99.1', name: 'OPNsense', online: true },
    { ip: '10.11.99.10', name: 'NAS', online: true },
  ],
};

const wss = new WebSocketServer({ port: PORT });

console.log(`Mock WebSocket server running on ws://localhost:${PORT}`);

wss.on('connection', (ws) => {
  console.log('Client connected');

  ws.send(JSON.stringify({ type: 'status', data: mockStatus }));
  ws.send(JSON.stringify({ type: 'config', data: mockConfig }));
  ws.send(JSON.stringify({ type: 'scan_results', data: mockScanResults }));

  ws.on('message', (data) => {
    try {
      const message = JSON.parse(data.toString());
      console.log('Received:', message.type);

      switch (message.type) {
        case 'get_all':
          ws.send(JSON.stringify({ type: 'status', data: mockStatus }));
          ws.send(JSON.stringify({ type: 'config', data: mockConfig }));
          ws.send(JSON.stringify({ type: 'scan_results', data: mockScanResults }));
          break;

        case 'save_config':
          console.log('Saving config:', message.data);
          Object.assign(mockConfig, message.data);
          setTimeout(() => {
            console.log('Reboot complete, reconnected');
            ws.send(JSON.stringify({ type: 'status', data: mockStatus }));
            ws.send(JSON.stringify({ type: 'config', data: mockConfig }));
          }, 1000);
          break;

        case 'trigger_scan':
          console.log('Triggering scan...');
          simulateScan(ws);
          break;

        default:
          console.log('Unknown message type:', message.type);
      }
    } catch (error) {
      console.error('Error parsing message:', error);
    }
  });

  ws.on('close', () => {
    console.log('Client disconnected');
  });

  ws.on('error', (error) => {
    console.error('WebSocket error:', error);
  });
});

function simulateScan(ws: any) {
  const subnets = [...mockConfig.subnets];
  const completedSubnets: string[] = [];
  let currentIndex = 0;

  ws.send(
    JSON.stringify({
      type: 'scan_started',
      data: {
        scanning: true,
        currentSubnet: subnets[0].cidr,
        completedSubnets: [],
      },
    })
  );

  const interval = setInterval(() => {
    if (currentIndex >= subnets.length) {
      clearInterval(interval);
      mockScanResults.last_scan_ms = Date.now();
      mockScanResults.device_now_ms = Date.now();
      ws.send(JSON.stringify({ type: 'scan_results', data: mockScanResults }));
      console.log('Scan complete');
      return;
    }

    completedSubnets.push(subnets[currentIndex].cidr);
    currentIndex++;

    if (currentIndex < subnets.length) {
      ws.send(
        JSON.stringify({
          type: 'scan_started',
          data: {
            scanning: true,
            currentSubnet: subnets[currentIndex].cidr,
            completedSubnets,
          },
        })
      );
      console.log(`Scanning: ${subnets[currentIndex].cidr}`);
    }
  }, 2000);
}
