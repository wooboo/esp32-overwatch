import { useState } from 'preact/hooks';
import type { Config } from '../types';

interface Props {
  config: Config | null;
  onChange: (updates: Partial<Config>) => void;
  onSave: (targets: { subnets: string[]; hosts: string[] }) => void;
}

export function TargetsForm({ config, onChange, onSave }: Props) {
  if (!config) {
    return <Card title="Targets">Loading...</Card>;
  }
  const [subnetsText, setSubnetText] = useState(() =>
    config.subnets
      .map((s) => (s.name ? `${s.cidr} # ${s.name}` : s.cidr))
      .join('\n')
  );
  const [hostsText, setHostsText] = useState(() =>
    config.static_hosts
      .map((h) => {
        let line = h.ip;
        if (h.port) line += `:${h.port}`;
        if (h.name) line += ` # ${h.name}`;
        return line;
      })
      .join('\n')
  );

  const handleSubnetsChange = (text: string) => {
    const subnets = text
      .split('\n')
      .filter(Boolean)
      .map((line) => {
        const [cidrPart, namePart] = line.split('#');
        const cidr = cidrPart.trim();
        const name = namePart ? namePart.trim() : undefined;
        return { cidr, name };
      });
    onChange({ subnets });
    setSubnetText(text);
  };

  const handleHostsChange = (text: string) => {
    const hosts = text
      .split('\n')
      .filter(Boolean)
      .map((line) => {
        const [hostPart, namePart] = line.split('#');
        const name = namePart ? namePart.trim() : undefined;
        const [ip, portStr] = hostPart.split(':');
        return {
          ip: ip.trim(),
          port: portStr ? parseInt(portStr.trim()) : undefined,
          name,
        };
      });
    onChange({ static_hosts: hosts });
    setHostsText(text);
  };

  const handleSave = () => {
    const subnets = config.subnets.map((s) => {
      let line = s.cidr;
      if (s.name) line += ` # ${s.name}`;
      return line;
    });
    const hosts = config.static_hosts.map((h) => {
      let line = h.ip;
      if (h.port) line += `:${h.port}`;
      if (h.name) line += ` # ${h.name}`;
      return line;
    });
    onSave({ subnets, hosts });
  };

  return (
    <Card title="Targets">
      <Label text="Subnets (one per line, CIDR)">
        <Textarea
          value={subnetsText}
          onChange={handleSubnetsChange}
          placeholder="10.11.12.0/22\n10.11.16.0/24 # Office Network"
        />
      </Label>
      <Label text="Static hosts (hostname or ip[:port] per line)">
        <Textarea
          value={hostsText}
          onChange={handleHostsChange}
          placeholder="10.11.12.6:8123 # HA VM\n10.11.99.1 # OPNsense\nmyserver.local:80 # My Server"
        />
      </Label>
      <div class="mt-3">
        <Button onClick={handleSave}>Save Targets</Button>
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

function Textarea({
  value,
  onChange,
  placeholder,
}: {
  value: string;
  onChange: (v: string) => void;
  placeholder?: string;
}) {
  return (
    <textarea
      value={value}
      onInput={(e) => onChange((e.target as HTMLTextAreaElement).value)}
      placeholder={placeholder}
      class="w-full p-2.5 rounded-lg border border-border bg-input-bg text-text text-sm min-h-[120px] resize-y"
    />
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
