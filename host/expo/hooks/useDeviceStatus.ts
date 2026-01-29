import { useEffect, useState } from "react";
import {
  addStatusUpdateListener,
  getConnectionStatus,
} from "../modules/daisy-multi-fx";
import type { DeviceStatus } from "../modules/daisy-multi-fx";

/**
 * Separate hook for device status (audio levels + CPU load).
 *
 * This is extracted from useDaisyMultiFX to prevent the main hook
 * from causing re-renders at ~30Hz when status messages arrive.
 * Components that need device status should use this hook directly.
 */
export function useDeviceStatus(): DeviceStatus | null {
  const [deviceStatus, setDeviceStatus] = useState<DeviceStatus | null>(null);

  useEffect(() => {
    const subscription = addStatusUpdateListener((event) => {
      setDeviceStatus(event.status);
    });

    return () => {
      subscription.remove();
    };
  }, []);

  return deviceStatus;
}
