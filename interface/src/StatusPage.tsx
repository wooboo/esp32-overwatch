import { StatusChips } from './components/StatusChips';
import { ScanResults } from './components/ScanResults';
import type { ScanResults as ScanResultsType, ScanProgress } from './types';

interface Props {
  path?: string;
  status: { wifi_connected: boolean; wifi_ip: string; mqtt_connected: boolean; mqtt_reason: string } | null;
  scanResults: ScanResultsType | null;
  scanProgress: ScanProgress;
  connected: boolean;
  onScan: () => void;
}

export function StatusPage({ status, scanResults, scanProgress, connected, onScan }: Props) {
  return (
    <div class="max-w-[1100px] mx-auto p-5">
      <div class="flex justify-between items-center mb-3">
        <h1 class="text-2xl font-bold">ESP32 Network Monitor</h1>
        <Button onClick={onScan}>Run Scan</Button>
      </div>

      <StatusChips
        status={status}
        scanResults={scanResults}
        connected={connected}
      />

      {scanProgress.scanning && (
        <div class="bg-accent/10 border border-accent rounded-xl p-4 mt-4">
          <div class="flex items-center gap-2">
            <div class="animate-spin h-5 w-5 border-2 border-accent border-t-transparent rounded-full"></div>
            <span class="font-bold text-accent">Scanning in progress...</span>
          </div>
          {scanProgress.currentSubnet && (
            <p class="mt-2 text-sm text-muted ml-7">
              Current subnet: {scanProgress.currentSubnet}
            </p>
          )}
          {scanProgress.completedSubnets.length > 0 && (
            <p class="mt-1 text-sm text-muted ml-7">
              Completed: {scanProgress.completedSubnets.join(', ')}
            </p>
          )}
        </div>
      )}

      <div class="grid grid-cols-1 md:grid-cols-2 gap-3.5 mt-3.5">
        <ScanResults
          title="Subnets"
          type="subnets"
          data={scanResults?.subnets}
        />
        <ScanResults title="Static Hosts" type="hosts" data={scanResults?.hosts} />
      </div>
    </div>
  );
}

function Button({
  children,
  onClick,
}: {
  children: preact.ComponentChildren;
  onClick: () => void;
}) {
  const base = 'px-3.5 py-2.5 rounded-xl font-extrabold cursor-pointer bg-accent text-bg shadow-md hover:bg-accent-hover';

  return (
    <button class={base} onClick={onClick}>
      {children}
    </button>
  );
}
