import React from "react";
import { View, StyleSheet } from "react-native";
import {
  Canvas,
  RoundedRect,
  Text as SkiaText,
  Circle,
  Path as SkiaPath,
  Skia,
  Group,
  matchFont,
} from "@shopify/react-native-skia";
import {
  Gesture,
  GestureDetector,
} from "react-native-gesture-handler";
import {
  useSharedValue,
  runOnJS,
} from "react-native-reanimated";
import {
  buildRoutingGraphLayout,
  type GraphLayout,
  type GraphNode,
  type GraphNodeId,
  type GraphEdge,
} from "../utils/routingGraph";
import { getColorForType } from "../utils/pedalColors";

/** Pedal slot dimensions — kept in sync with routingGraph layout */
const PEDAL_W = 90;
const PEDAL_H = 120;
const STRIP_H = 28;
const CORNER_R = 8;
const FOOTSWITCH_R = 19;
const FOOTSWITCH_INNER_R = 9;
const MAX_DISPLAY_TEXT_LENGTH = 12;
const LONG_PRESS_DURATION_MS = 300;

// ─── Fonts ───────────────────────────────────────────────────────────
const FONT_BOLD_18 = matchFont({
  fontFamily: "System",
  fontSize: 18,
  fontWeight: "bold",
});
const FONT_NORMAL_10 = matchFont({
  fontFamily: "System",
  fontSize: 10,
});
const FONT_NORMAL_8 = matchFont({
  fontFamily: "System",
  fontSize: 8,
});
const FONT_BOLD_9 = matchFont({
  fontFamily: "System",
  fontSize: 9,
  fontWeight: "bold",
});

// ─── Types ───────────────────────────────────────────────────────────

type EffectSlotLike = {
  slotIndex: number;
  typeId: number;
  enabled: boolean;
  inputL: number;
  inputR: number;
  sumToMono: boolean;
  channelPolicy: number;
  params: Record<number, number>;
};

export interface SkiaGraphViewProps {
  slots: EffectSlotLike[];
  numSlots: number;
  selectedSlotIndex?: number;
  getShortName: (typeId: number) => string;
  getName: (typeId: number) => string;
  onSelectSlot: (slotIndex: number) => void;
  onToggleSlotEnabled: (slotIndex: number, enabled: boolean) => void;
  onReorderSlot?: (fromIndex: number, toIndex: number) => void;
  getDisplayLabel?: (
    typeId: number,
    params: Record<number, number>,
  ) => string | undefined;
}

// ─── Helpers ─────────────────────────────────────────────────────────

/** Measure text width using the skia font (approximate via char count when unavailable). */
function textWidth(text: string, font: ReturnType<typeof matchFont>): number {
  // matchFont returns an SkFont with measureText
  if (font && typeof font.measureText === "function") {
    return font.measureText(text).width;
  }
  return text.length * 7; // rough fallback
}

/** Build a Skia path string for a smooth cubic-bezier routing edge. */
function buildEdgePath(
  x1: number,
  y1: number,
  x2: number,
  y2: number,
): string {
  const dx = Math.abs(x2 - x1);
  const cpOffset = Math.max(20, dx * 0.4);
  return `M ${x1} ${y1} C ${x1 + cpOffset} ${y1}, ${x2 - cpOffset} ${y2}, ${x2} ${y2}`;
}

/** Build a Skia path string for a wrapped (cross-row) routing edge. */
function buildWrappedEdgePath(
  xFrom: number,
  yFrom: number,
  xTo: number,
  yTo: number,
  yMid: number,
): string {
  const cp = 16;
  // Down from source → horizontal across → down to destination
  return [
    `M ${xFrom} ${yFrom}`,
    `C ${xFrom} ${yFrom + cp}, ${xFrom} ${yMid - cp}, ${xFrom} ${yMid}`,
    `L ${xTo} ${yMid}`,
    `C ${xTo} ${yMid + cp}, ${xTo} ${yTo - cp}, ${xTo} ${yTo}`,
  ].join(" ");
}

// ─── Single Pedal (pure Skia drawing) ────────────────────────────────

interface PedalDrawProps {
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

const SkiaPedal: React.FC<PedalDrawProps> = React.memo(
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

// ─── Routing Edge (Skia Path) ────────────────────────────────────────

interface EdgeDrawProps {
  edge: GraphEdge;
  nodesById: Map<GraphNodeId, GraphNode>;
  nodeWidth: number;
  nodeHeight: number;
  layout: GraphLayout;
}

const SkiaEdge: React.FC<EdgeDrawProps> = React.memo(
  ({ edge, nodesById, nodeWidth, nodeHeight, layout }) => {
    const from = nodesById.get(edge.from);
    const to = nodesById.get(edge.to);
    if (!from || !to) return null;

    const wrapped = layout.wrap != null;
    const sameWrapRow =
      !wrapped || from.wrapRow == null || to.wrapRow == null
        ? true
        : from.wrapRow === to.wrapRow;

    let pathStr: string;

    if (sameWrapRow) {
      const x1 = from.x + nodeWidth;
      const y1 = from.y + nodeHeight / 2;
      const x2 = to.x;
      const y2 = to.y + nodeHeight / 2;
      pathStr = buildEdgePath(x1, y1, x2, y2);
    } else {
      const xFrom = from.x + nodeWidth / 2;
      const yFrom = from.y + nodeHeight;
      const xTo = to.x + nodeWidth / 2;
      const yTo = to.y;
      const rowOffsets = layout.wrap?.rowOffsets ?? [];
      const rowGap = layout.wrap?.rowGap ?? 0;
      const toRowStart = layout.padding + (rowOffsets[to.wrapRow!] ?? 0);
      const yBetween = toRowStart - rowGap / 2;
      pathStr = buildWrappedEdgePath(xFrom, yFrom, xTo, yTo, yBetween);
    }

    const path = Skia.Path.MakeFromSVGString(pathStr);
    if (!path) return null;

    const opacity = edge.enabled ? 0.8 : 0.25;
    const color = edge.label === "L" ? "#1976D2" : edge.label === "R" ? "#D32F2F" : "#1976D2";
    const strokeWidth = edge.label === "LR" ? 2.5 : 2;

    return (
      <SkiaPath
        path={path}
        color={color}
        style="stroke"
        strokeWidth={strokeWidth}
        strokeCap="round"
        opacity={opacity}
      />
    );
  },
);

// ─── Main SkiaGraphView ──────────────────────────────────────────────

export const SkiaGraphView: React.FC<SkiaGraphViewProps> = ({
  slots,
  numSlots,
  selectedSlotIndex,
  getShortName,
  getName,
  onSelectSlot,
  onToggleSlotEnabled,
  onReorderSlot,
  getDisplayLabel,
}) => {
  const [containerWidth, setContainerWidth] = React.useState<number | null>(
    null,
  );

  // ── Graph layout ──
  const layout = React.useMemo<GraphLayout>(() => {
    return buildRoutingGraphLayout({
      slots,
      numSlots,
      getShortName,
      selectedSlotIndex,
      maxWidth: containerWidth ?? undefined,
    });
  }, [slots, numSlots, getShortName, selectedSlotIndex, containerWidth]);

  const renderNodes = React.useMemo(() => {
    return layout.nodes.filter((n) => n.kind !== "IN");
  }, [layout.nodes]);

  const nodesById = React.useMemo(() => {
    const map = new Map<GraphNodeId, GraphNode>();
    for (const n of renderNodes) map.set(n.id, n);
    return map;
  }, [renderNodes]);

  // ── Drag state ──
  const dragSlotIndex = useSharedValue<number>(-1);
  const dragOffsetX = useSharedValue(0);
  const dragOffsetY = useSharedValue(0);
  const dragStartX = useSharedValue(0);
  const dragStartY = useSharedValue(0);

  // ── Gesture handlers ──
  // We use a single gesture detector over the whole canvas.  Touch position
  // determines which pedal (if any) was tapped/dragged.

  const findNodeAt = React.useCallback(
    (x: number, y: number): GraphNode | undefined => {
      return renderNodes.find(
        (n) =>
          x >= n.x &&
          x <= n.x + PEDAL_W &&
          y >= n.y &&
          y <= n.y + PEDAL_H,
      );
    },
    [renderNodes],
  );

  const isFootswitchHit = React.useCallback(
    (node: GraphNode, x: number, y: number): boolean => {
      const cx = node.x + PEDAL_W / 2;
      const cy = node.y + PEDAL_H - 24;
      const dist = Math.sqrt((x - cx) ** 2 + (y - cy) ** 2);
      return dist <= FOOTSWITCH_R;
    },
    [],
  );

  // Tap gesture — select pedal or toggle footswitch
  const tapGesture = React.useMemo(
    () =>
      Gesture.Tap().onEnd((e) => {
        const node = findNodeAt(e.x, e.y);
        if (!node || node.slotIndex == null) return;

        if (isFootswitchHit(node, e.x, e.y)) {
          const slot = slots.find((s) => s.slotIndex === node.slotIndex);
          if (slot && slot.typeId !== 0) {
            runOnJS(onToggleSlotEnabled)(node.slotIndex, !slot.enabled);
          }
        } else {
          runOnJS(onSelectSlot)(node.slotIndex);
        }
      }),
    [findNodeAt, isFootswitchHit, slots, onSelectSlot, onToggleSlotEnabled],
  );

  // Long-press + pan gesture for drag-to-reorder
  const dragGesture = React.useMemo(
    () =>
      Gesture.Pan()
        .activateAfterLongPress(LONG_PRESS_DURATION_MS)
        .onStart((e) => {
          const node = findNodeAt(e.x, e.y);
          if (!node || node.slotIndex == null) return;
          dragSlotIndex.value = node.slotIndex;
          dragStartX.value = node.x;
          dragStartY.value = node.y;
          dragOffsetX.value = 0;
          dragOffsetY.value = 0;
        })
        .onUpdate((e) => {
          if (dragSlotIndex.value < 0) return;
          dragOffsetX.value = e.translationX;
          dragOffsetY.value = e.translationY;
        })
        .onEnd((e) => {
          if (dragSlotIndex.value < 0) return;

          // Find the target slot under the drop position
          const dropX = dragStartX.value + e.translationX + PEDAL_W / 2;
          const dropY = dragStartY.value + e.translationY + PEDAL_H / 2;
          const target = findNodeAt(dropX, dropY);

          if (
            target &&
            target.slotIndex != null &&
            target.slotIndex !== dragSlotIndex.value &&
            onReorderSlot
          ) {
            runOnJS(onReorderSlot)(dragSlotIndex.value, target.slotIndex);
          }

          dragSlotIndex.value = -1;
          dragOffsetX.value = 0;
          dragOffsetY.value = 0;
        }),
    [findNodeAt, onReorderSlot, dragSlotIndex, dragOffsetX, dragOffsetY, dragStartX, dragStartY],
  );

  const composed = React.useMemo(
    () => Gesture.Exclusive(dragGesture, tapGesture),
    [tapGesture, dragGesture],
  );

  // ── Render ──
  return (
    <View
      style={styles.root}
      onLayout={(e) => {
        const w = e.nativeEvent.layout.width;
        if (w > 0 && w !== containerWidth) setContainerWidth(w);
      }}
    >
      <GestureDetector gesture={composed}>
        <Canvas
          style={{
            width: layout.width,
            height: layout.height,
            alignSelf: "center",
          }}
        >
          {/* Routing edges (drawn below pedals) */}
          {layout.edges.map((edge) => (
            <SkiaEdge
              key={edge.id}
              edge={edge}
              nodesById={nodesById}
              nodeWidth={layout.nodeWidth}
              nodeHeight={layout.nodeHeight}
              layout={layout}
            />
          ))}

          {/* Pedal nodes */}
          {renderNodes.map((n) => {
            const slot = slots.find((s) => s.slotIndex === n.slotIndex);
            const typeId = slot?.typeId ?? 0;
            const enabled = slot?.enabled ?? false;
            const sumToMono = slot?.sumToMono ?? false;
            const channelPolicy = slot?.channelPolicy ?? 0;
            const isSelected =
              n.kind === "SLOT" && n.slotIndex === selectedSlotIndex;

            const shortName =
              typeId !== 0
                ? getShortName(typeId) || `S${(n.slotIndex ?? 0) + 1}`
                : "--";
            const name =
              typeId !== 0
                ? getName(typeId) || `Slot ${(n.slotIndex ?? 0) + 1}`
                : `Slot ${(n.slotIndex ?? 0) + 1}`;
            const subtitle =
              typeId !== 0 && slot && getDisplayLabel
                ? getDisplayLabel(typeId, slot.params)
                : undefined;

            return (
              <SkiaPedal
                key={n.id}
                x={n.x}
                y={n.y}
                shortName={shortName}
                name={name}
                subtitle={subtitle}
                enabled={enabled}
                selected={isSelected}
                typeId={typeId}
                channelPolicy={channelPolicy}
                sumToMono={sumToMono}
              />
            );
          })}
        </Canvas>
      </GestureDetector>
    </View>
  );
};

const styles = StyleSheet.create({
  root: {
    width: "100%",
  },
});
