import React from "react";
import {
  RoundedRect,
  Text as SkiaText,
  Circle,
  Group,
} from "@shopify/react-native-skia";
import { getColorForType } from "../../utils/pedalColors";
import {
  PEDAL_W,
  PEDAL_H,
  STRIP_H,
  CORNER_R,
  FOOTSWITCH_R,
  FOOTSWITCH_INNER_R,
  MAX_DISPLAY_TEXT_LENGTH,
  FONT_BOLD_18,
  FONT_NORMAL_10,
  FONT_NORMAL_8,
  FONT_BOLD_9,
  textWidth,
} from "./constants";

export interface PedalDrawProps {
  x: number;
  y: number;
  shortName: string;
  name: string;
  subtitle?: string;
  enabled: boolean;
  selected: boolean;
  typeId: number;
  channelPolicy: number;
  sumToMono: boolean;
}

export const SkiaPedal: React.FC<PedalDrawProps> = React.memo(
  ({
    x,
    y,
    shortName,
    name,
    subtitle,
    enabled,
    selected,
    typeId,
    channelPolicy,
    sumToMono,
  }) => {
    const colors = getColorForType(typeId);
    const isEmpty = !name || name === "Empty" || shortName === "--";

    // ── Colors ──
    const borderColor = selected ? "#2196F3" : "#DDDDDD";
    const borderWidth = selected ? 2.5 : 1.5;
    const bodyBg = selected ? "#E3F2FD" : enabled ? colors.body : "#F5F5F5";
    const stripBg = !enabled
      ? "#E0E0E0"
      : selected
        ? "#2196F3"
        : colors.strip;
    const stripTextColor = !enabled
      ? "#999"
      : selected
        ? "#fff"
        : colors.stripText;

    const nameColor = selected ? "#1976D2" : !enabled ? "#999" : "#333";
    const subtitleColor = selected ? "#1976D2" : !enabled ? "#999" : "#888";

    // Footswitch colours
    const fsOuterBorder = enabled ? "#2196F3" : isEmpty ? "#E0E0E0" : "#D0D0D0";
    const fsInnerBorder = enabled ? "#1976D2" : isEmpty ? "#E0E0E0" : "#9E9E9E";
    const fsInnerFill = enabled ? "#4CAF50" : "#fff";

    // Channel indicator
    const policyLabel =
      channelPolicy === 1 ? "M" : channelPolicy === 2 ? "S" : "A";
    const indColor = selected ? "#1976D2" : !enabled ? "#999" : "#666";
    const indBg = selected ? "#E3F2FD" : !enabled ? "#F0F0F0" : "#F5F5F5";

    // ── Layout positions (relative to pedal origin) ──
    const fsCenterX = x + PEDAL_W / 2;
    const fsCenterY = y + PEDAL_H - 24;

    // Short name centred in strip
    const snW = textWidth(shortName, FONT_BOLD_18);
    const snX = x + (PEDAL_W - snW) / 2;
    const snY = y + STRIP_H / 2 + 7; // baseline offset

    // Name/subtitle centred below strip
    const displayText = subtitle || name;
    const displayFont = subtitle ? FONT_NORMAL_8 : FONT_NORMAL_10;
    const dtW = textWidth(displayText, displayFont);
    const dtX = x + (PEDAL_W - dtW) / 2;
    const dtY = y + STRIP_H + 18;

    // Channel indicator badge
    const fullLabel = sumToMono ? `Σ ${policyLabel}` : policyLabel;
    const badgeW = textWidth(fullLabel, FONT_BOLD_9) + 10;
    const badgeX = x + (PEDAL_W - badgeW) / 2;
    const badgeY = y + STRIP_H + 30;

    return (
      <Group>
        {/* Shadow / border */}
        <RoundedRect
          x={x - borderWidth / 2}
          y={y - borderWidth / 2}
          width={PEDAL_W + borderWidth}
          height={PEDAL_H + borderWidth}
          r={CORNER_R + 1}
          color={borderColor}
        />
        {/* Body */}
        <RoundedRect
          x={x}
          y={y}
          width={PEDAL_W}
          height={PEDAL_H}
          r={CORNER_R}
          color={bodyBg}
        />
        {/* Top strip */}
        <RoundedRect
          x={x}
          y={y}
          width={PEDAL_W}
          height={STRIP_H + CORNER_R}
          r={CORNER_R}
          color={stripBg}
        />
        {/* Flat bottom of strip (cover lower rounding) */}
        <RoundedRect
          x={x}
          y={y + STRIP_H - 2}
          width={PEDAL_W}
          height={CORNER_R + 2}
          r={0}
          color={stripBg}
        />

        {/* Short name text */}
        <SkiaText
          x={snX}
          y={snY}
          text={shortName}
          font={FONT_BOLD_18}
          color={stripTextColor}
        />

        {/* Name / subtitle */}
        <SkiaText
          x={Math.max(x + 2, dtX)}
          y={dtY}
          text={displayText.length > MAX_DISPLAY_TEXT_LENGTH ? displayText.slice(0, MAX_DISPLAY_TEXT_LENGTH - 1) + "…" : displayText}
          font={displayFont}
          color={subtitle ? subtitleColor : nameColor}
        />

        {/* Channel indicator badge */}
        <RoundedRect
          x={badgeX}
          y={badgeY}
          width={badgeW}
          height={16}
          r={4}
          color={indBg}
        />
        <SkiaText
          x={badgeX + 5}
          y={badgeY + 12}
          text={fullLabel}
          font={FONT_BOLD_9}
          color={indColor}
        />

        {/* Footswitch outer */}
        <Circle
          cx={fsCenterX}
          cy={fsCenterY}
          r={FOOTSWITCH_R}
          color="#fff"
          style="fill"
        />
        <Circle
          cx={fsCenterX}
          cy={fsCenterY}
          r={FOOTSWITCH_R}
          color={fsOuterBorder}
          style="stroke"
          strokeWidth={2}
        />
        {/* Footswitch inner */}
        <Circle
          cx={fsCenterX}
          cy={fsCenterY}
          r={FOOTSWITCH_INNER_R}
          color={fsInnerFill}
          style="fill"
        />
        <Circle
          cx={fsCenterX}
          cy={fsCenterY}
          r={FOOTSWITCH_INNER_R}
          color={fsInnerBorder}
          style="stroke"
          strokeWidth={2}
        />
      </Group>
    );
  },
);
