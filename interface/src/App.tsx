import { useState, useEffect } from 'preact/hooks';
import { Router } from 'preact-router';
import { useWebSocket } from './hooks/useWebSocket';
import { StatusPage } from './StatusPage';
import { SettingsPage } from './SettingsPage';
import { Navigation } from './Navigation';
import type { Config } from './types';

export function App() {
  const { status, config, scanResults, scanProgress, connected, saveConfig, saveTargets, triggerScan } =
    useWebSocket();
  const [localConfig, setLocalConfig] = useState<Config | null>(config);
  const [toast, setToast] = useState<string | null>(null);

  // Auto-clear toast after 3 seconds
  useEffect(() => {
    if (toast) {
      const timer = setTimeout(() => setToast(null), 3000);
      return () => clearTimeout(timer);
    }
  }, [toast]);

  const handleConfigChange = (updates: Partial<Config>) => {
    if (!localConfig && !config) return;
    setLocalConfig((prev) => ({ ...(prev || config!), ...updates }));
  };

  const handleSave = () => {
    if (localConfig) {
      saveConfig(localConfig);
      setToast('Settings saved successfully!');
    }
  };

  const handleSaveTargets = (targets: { subnets: string[]; hosts: string[] }) => {
    saveTargets(targets);
    setToast('Targets saved successfully!');
  };

  return (
    <div>
      {toast && (
        <div class="fixed top-4 right-4 bg-green-500 text-white px-4 py-2 rounded-lg shadow-lg z-50 animate-pulse">
          {toast}
        </div>
      )}
      <Navigation />
      <Router>
        <StatusPage
          path="/"
          status={status}
          scanResults={scanResults}
          scanProgress={scanProgress}
          connected={connected}
          onScan={triggerScan}
        />
        <SettingsPage
          path="/settings"
          config={config}
          localConfig={localConfig}
          onConfigChange={handleConfigChange}
          onSave={handleSave}
          onSaveTargets={handleSaveTargets}
        />
      </Router>
    </div>
  );
}
