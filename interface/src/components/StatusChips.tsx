import type { Status, ScanResults } from '../types';

interface Props {
  status: Status | null;
  scanResults: ScanResults | null;
  connected: boolean;
}

export function StatusChips({ status, scanResults, connected }: Props) {
  const wifiOk = status?.wifi_connected ?? false;
  const mqttOk = status?.mqtt_connected ?? false;

  const wifiText = wifiOk
    ? status?.wifi_ip
      ? `connected (${status.wifi_ip})`
      : 'connected'
    : 'offline';

  const mqttText = mqttOk
    ? 'connected'
    : `offline${status?.mqtt_reason ? ` (${status.mqtt_reason})` : ''}`;

  const scanAge = scanResults?.last_scan_ms
    ? Math.floor((scanResults.device_now_ms - scanResults.last_scan_ms) / 1000)
    : null;

  return (
    <div class="flex flex-wrap gap-2 mb-4">
      <Chip ok={connected} label={connected ? 'WS: connected' : 'WS: disconnected'} />
      <Chip ok={wifiOk} label={`WiFi: ${wifiText}`} />
      <Chip ok={mqttOk} label={`MQTT: ${mqttText}`} />
      <Chip
        muted={scanAge === null}
        label={scanAge !== null ? `Last scan: ${scanAge}s ago` : 'Last scan: --'}
      />
    </div>
  );
}

function Chip({
  ok,
  muted,
  label,
}: {
  ok?: boolean;
  muted?: boolean;
  label: string;
}) {
  const base = 'px-2.5 py-1.5 rounded-full font-semibold text-sm border';
  const variant = muted
    ? 'bg-chip-bg border-border text-muted'
    : ok
      ? 'bg-chip-ok border-accent'
      : 'bg-chip-bad border-danger';

  return <span class={`${base} ${variant}`}>{label}</span>;
}
