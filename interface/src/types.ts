export interface Status {
  wifi_connected: boolean;
  wifi_ip: string;
  mqtt_connected: boolean;
  mqtt_reason: string;
}

export interface Subnet {
  cidr: string;
  name?: string;
}

export interface Config {
  wifi_ssid: string;
  wifi_pass: string;
  mqtt_host: string;
  mqtt_port: number;
  mqtt_user: string;
  mqtt_pass: string;
  scan_interval_ms: number;
  subnets: Subnet[];
  static_hosts: StaticHost[];
}

export interface StaticHost {
  ip: string;
  port?: number;
  name?: string;
}

export interface SubnetResult {
  cidr: string;
  name?: string;
  online: number;
}

export interface HostResult {
  ip: string;
  port?: number;
  name?: string;
  online: boolean;
}

export interface ScanResults {
  last_scan_ms: number;
  device_now_ms: number;
  found_count: number;
  subnets: SubnetResult[];
  hosts: HostResult[];
}

export type WsMessageType = 'status' | 'config' | 'scan_results' | 'scan_started' | 'targets_saved' | 'config_saved';

export interface ScanProgress {
  scanning: boolean;
  currentSubnet?: string;
  completedSubnets: string[];
}

export interface WsMessage {
  type: WsMessageType;
  data?: Status | Config | ScanResults | ScanProgress;
}
