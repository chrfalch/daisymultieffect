import React from "react";
import { View, StyleSheet } from "react-native";
import { Canvas } from "@shopify/react-native-skia";
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
} from "../../utils/routingGraph";
import { SkiaPedal } from "./SkiaPedal";
import { SkiaEdge } from "./SkiaEdge";
import {
  PEDAL_W,
  PEDAL_H,
  FOOTSWITCH_R,
  LONG_PRESS_DURATION_MS,
} from "./constants";

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
