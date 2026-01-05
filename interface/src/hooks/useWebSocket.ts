import { useState, useEffect, useCallback, useRef } from 'preact/hooks';
import type { Status, Config, ScanResults, ScanProgress, WsMessage } from '../types';

interface WebSocketState {
  status: Status | null;
  config: Config | null;
  scanResults: ScanResults | null;
  scanProgress: ScanProgress;
  connected: boolean;
}

export function useWebSocket() {
  const [state, setState] = useState<WebSocketState>({
    status: null,
    config: null,
    scanResults: null,
    scanProgress: { scanning: false, completedSubnets: [] },
    connected: false,
  });
  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimeoutRef = useRef<number>();

  const connect = useCallback(() => {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const ws = new WebSocket(`${protocol}//${window.location.host}/ws`);

    ws.onopen = () => {
      setState((s) => ({ ...s, connected: true }));
      ws.send(JSON.stringify({ type: 'get_all' }));
    };

    ws.onclose = () => {
      setState((s) => ({ ...s, connected: false }));
      reconnectTimeoutRef.current = window.setTimeout(connect, 2000);
    };

    ws.onmessage = (event) => {
      try {
        const msg: WsMessage = JSON.parse(event.data);
        setState((s) => {
          switch (msg.type) {
            case 'status':
              return { ...s, status: msg.data as Status };
            case 'config':
              return { ...s, config: msg.data as Config };
            case 'scan_results':
              return {
                ...s,
                scanResults: msg.data as ScanResults,
                scanProgress: { scanning: false, completedSubnets: [] },
              };
            case 'scan_started':
              return {
                ...s,
                scanProgress: (msg.data as ScanProgress) || { scanning: true, completedSubnets: [] },
              };
            default:
              return s;
          }
        });
      } catch {
      }
    };

    wsRef.current = ws;
  }, []);

  useEffect(() => {
    connect();
    return () => {
      if (reconnectTimeoutRef.current) {
        clearTimeout(reconnectTimeoutRef.current);
      }
      wsRef.current?.close();
    };
  }, [connect]);

  const send = useCallback((type: string, data?: unknown) => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({ type, data }));
    }
  }, []);

  const saveConfig = useCallback(
    (config: Partial<Config>) => {
      send('save_config', config);
    },
    [send]
  );

  const triggerScan = useCallback(() => {
    send('trigger_scan');
  }, [send]);

  return {
    ...state,
    saveConfig,
    triggerScan,
  };
}
