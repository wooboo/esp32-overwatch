import type { Config } from '../types';

interface Props {
  config: Config | null;
  onChange: (updates: Partial<Config>) => void;
  onSave: () => void;
  onScan: () => void;
}

export function ConfigForm({ config, onChange, onSave, onScan }: Props) {
  if (!config) {
    return <Card title="Connectivity">Loading...</Card>;
  }

  return (
    <Card title="Connectivity">
      <Label text="WiFi SSID">
        <Input
          value={config.wifi_ssid}
          onChange={(v) => onChange({ wifi_ssid: v })}
          placeholder="Network name"
        />
      </Label>
      <Label text="WiFi Password">
        <Input
          type="password"
          value={config.wifi_pass}
          onChange={(v) => onChange({ wifi_pass: v })}
          placeholder="Password"
        />
      </Label>
      <Label text="MQTT Host">
        <Input
          value={config.mqtt_host}
          onChange={(v) => onChange({ mqtt_host: v })}
          placeholder="192.168.1.10"
        />
      </Label>
      <Label text="MQTT Port">
        <Input
          type="number"
          value={String(config.mqtt_port)}
          onChange={(v) => onChange({ mqtt_port: parseInt(v) || 1883 })}
          placeholder="1883"
        />
      </Label>
      <Label text="MQTT User">
        <Input
          value={config.mqtt_user}
          onChange={(v) => onChange({ mqtt_user: v })}
        />
      </Label>
      <Label text="MQTT Password">
        <Input
          type="password"
          value={config.mqtt_pass}
          onChange={(v) => onChange({ mqtt_pass: v })}
        />
      </Label>
      <Label text="Scan interval (ms)">
        <Input
          type="number"
          value={String(config.scan_interval_ms)}
          onChange={(v) => onChange({ scan_interval_ms: parseInt(v) || 300000 })}
          placeholder="300000"
        />
      </Label>
      <div class="flex flex-wrap gap-2 mt-3">
        <Button onClick={onSave}>Save & Reboot</Button>
        <Button secondary onClick={onScan}>
          Run Scan
        </Button>
      </div>
    </Card>
  );
}

function Card({ title, children }: { title: string; children: preact.ComponentChildren }) {
  return (
    <div class="bg-card border border-border rounded-xl p-4 shadow-lg">
      <h3 class="font-bold mb-2">{title}</h3>
      {children}
    </div>
  );
}

function Label({ text, children }: { text: string; children: preact.ComponentChildren }) {
  return (
    <label class="block mt-2">
      <span class="text-sm font-bold block mb-1">{text}</span>
      {children}
    </label>
  );
}

function Input({
  value,
  onChange,
  type = 'text',
  placeholder,
}: {
  value: string;
  onChange: (v: string) => void;
  type?: string;
  placeholder?: string;
}) {
  return (
    <input
      type={type}
      value={value}
      onInput={(e) => onChange((e.target as HTMLInputElement).value)}
      placeholder={placeholder}
      class="w-full p-2.5 rounded-lg border border-border bg-input-bg text-text text-sm"
    />
  );
}

function Button({
  children,
  onClick,
  secondary,
}: {
  children: preact.ComponentChildren;
  onClick: () => void;
  secondary?: boolean;
}) {
  const base = 'px-3.5 py-2.5 rounded-xl font-extrabold cursor-pointer';
  const variant = secondary
    ? 'bg-chip-bg text-text'
    : 'bg-accent text-bg shadow-md hover:bg-accent-hover';

  return (
    <button class={`${base} ${variant}`} onClick={onClick}>
      {children}
    </button>
  );
}
