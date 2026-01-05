import { useState } from 'preact/hooks';
import { Router } from 'preact-router';
import { useWebSocket } from './hooks/useWebSocket';
import { StatusPage } from './StatusPage';
import { SettingsPage } from './SettingsPage';
import { Navigation } from './Navigation';
import type { Config } from './types';

export function App() {
  const { status, config, scanResults, scanProgress, connected, saveConfig, triggerScan } =
    useWebSocket();
  const [localConfig, setLocalConfig] = useState<Config | null>(config);

  const handleConfigChange = (updates: Partial<Config>) => {
    if (!localConfig && !config) return;
    setLocalConfig((prev) => ({ ...(prev || config!), ...updates }));
  };

  const handleSave = () => {
    if (localConfig) {
      saveConfig(localConfig);
    }
  };

  return (
    <div>
      <Navigation />
      <Router>
        <StatusPage
          path="/"
          status={status}
          scanResults={scanResults}
          scanProgress={scanProgress}
          connected={connected}
        />
        <SettingsPage
          path="/settings"
          config={config}
          localConfig={localConfig}
          onConfigChange={handleConfigChange}
          onSave={handleSave}
          onScan={triggerScan}
        />
      </Router>
    </div>
  );
}
