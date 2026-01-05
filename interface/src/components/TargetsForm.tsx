import type { Config } from '../types';

interface Props {
  config: Config | null;
  onChange: (updates: Partial<Config>) => void;
}

export function TargetsForm({ config, onChange }: Props) {
  if (!config) {
    return <Card title="Targets">Loading...</Card>;
  }

  const subnetsText = config.subnets
    .map((s) => (s.name ? `${s.cidr} # ${s.name}` : s.cidr))
    .join('\n');
  const hostsText = config.static_hosts
    .map((h) => {
      let line = h.ip;
      if (h.port) line += `:${h.port}`;
      if (h.name) line += ` # ${h.name}`;
      return line;
    })
    .join('\n');

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
      <Label text="Static hosts (ip[:port] per line)">
        <Textarea
          value={hostsText}
          onChange={handleHostsChange}
          placeholder="10.11.12.6:8123 # HA VM\n10.11.99.1 # OPNsense"
        />
      </Label>
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
