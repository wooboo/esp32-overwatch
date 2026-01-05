import { ConfigForm } from './components/ConfigForm';
import { TargetsForm } from './components/TargetsForm';
import type { Config } from './types';

interface Props {
  path?: string;
  config: Config | null;
  localConfig: Config | null;
  onConfigChange: (updates: Partial<Config>) => void;
  onSave: () => void;
  onSaveTargets: (targets: { subnets: string[]; hosts: string[] }) => void;
}

export function SettingsPage({ config, localConfig, onConfigChange, onSave, onSaveTargets }: Props) {
  const handleConfigChange = (updates: Partial<Config>) => {
    if (!localConfig && !config) return;
    onConfigChange({ ...(localConfig || config!), ...updates });
  };

  const effectiveConfig = localConfig || config;

  return (
    <div class="max-w-[1100px] mx-auto p-5">
      <h1 class="text-2xl font-bold mb-3">ESP32 Network Monitor - Settings</h1>

      <div class="grid grid-cols-1 md:grid-cols-2 gap-3.5 mt-4">
        <ConfigForm
          config={effectiveConfig}
          onChange={handleConfigChange}
          onSave={onSave}
        />
        <TargetsForm
          config={effectiveConfig}
          onChange={handleConfigChange}
          onSave={onSaveTargets}
        />
      </div>
    </div>
  );
}
