import type { SubnetResult, HostResult } from '../types';

interface SubnetProps {
  title: string;
  type: 'subnets';
  data: SubnetResult[] | undefined;
}

interface HostProps {
  title: string;
  type: 'hosts';
  data: HostResult[] | undefined;
}

type Props = SubnetProps | HostProps;

export function ScanResults({ title, type, data }: Props) {
  return (
    <div class="bg-card border border-border rounded-xl p-4 shadow-lg">
      <h3 class="font-bold mb-2">{title}</h3>
      {!data || data.length === 0 ? (
        <p class="text-muted">No scan data yet.</p>
      ) : type === 'subnets' ? (
        <SubnetsTable data={data as SubnetResult[]} />
      ) : (
        <HostsTable data={data as HostResult[]} />
      )}
    </div>
  );
}

function SubnetsTable({ data }: { data: SubnetResult[] }) {
  return (
    <table class="w-full border-collapse text-sm">
      <thead>
        <tr>
          <Th>CIDR</Th>
          <Th>Name</Th>
          <Th>Online</Th>
        </tr>
      </thead>
      <tbody>
        {data.map((row) => (
          <tr key={row.cidr}>
            <Td>{row.cidr}</Td>
            <Td>{row.name || ''}</Td>
            <Td>{row.online}</Td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}

function HostsTable({ data }: { data: HostResult[] }) {
  return (
    <table class="w-full border-collapse text-sm">
      <thead>
        <tr>
          <Th>Host</Th>
          <Th>Name</Th>
          <Th>Status</Th>
        </tr>
      </thead>
      <tbody>
        {data.map((row) => (
          <tr key={row.ip}>
            <Td>{row.port ? `${row.ip}:${row.port}` : row.ip}</Td>
            <Td>{row.name || ''}</Td>
            <Td>
              <Pill online={row.online} />
            </Td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}

function Th({ children }: { children: preact.ComponentChildren }) {
  return (
    <th class="p-2 border-b border-border text-left text-muted font-bold text-xs tracking-wide">
      {children}
    </th>
  );
}

function Td({ children }: { children: preact.ComponentChildren }) {
  return <td class="p-2 border-b border-border">{children}</td>;
}

function Pill({ online }: { online: boolean }) {
  const base = 'px-2 py-1 rounded-full font-bold text-xs border inline-block';
  const variant = online
    ? 'bg-chip-ok border-accent'
    : 'bg-chip-bad border-danger';

  return <span class={`${base} ${variant}`}>{online ? 'online' : 'offline'}</span>;
}
