import React from "react";
import { StatusMeter } from "./StatusMeter";
import { useDeviceStatus } from "../hooks/useDeviceStatus";

/**
 * Container component that connects StatusMeter to the device status hook.
 *
 * This isolates the high-frequency (~30Hz) device status updates
 * to this component tree only, preventing unnecessary re-renders
 * of the parent EditorScreen.
 */
export const DeviceStatusMeter: React.FC = () => {
  const deviceStatus = useDeviceStatus();
  return <StatusMeter deviceStatus={deviceStatus} />;
};
