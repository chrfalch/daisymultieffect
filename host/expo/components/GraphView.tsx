import React from "react";
import { Pressable, ScrollView, StyleSheet, View } from "react-native";
import { PedalSlot } from "./PedalSlot";
import {
  buildRoutingGraphLayout,
  type GraphLayout,
  type GraphNode,
  type GraphNodeId,
} from "../utils/routingGraph";

type EffectSlotLike = {
  slotIndex: number;
  typeId: number;
  enabled: boolean;
  inputL: number;
  inputR: number;
};

export const GraphView: React.FC<{
  slots: EffectSlotLike[];
  numSlots: number;
  selectedSlotIndex?: number;
  getShortName: (typeId: number) => string;
  getName: (typeId: number) => string;
  onSelectSlot: (slotIndex: number) => void;
  onToggleSlotEnabled: (slotIndex: number, enabled: boolean) => void;
}> = ({
  slots,
  numSlots,
  selectedSlotIndex,
  getShortName,
  getName,
  onSelectSlot,
  onToggleSlotEnabled,
}) => {
  const [containerWidth, setContainerWidth] = React.useState<number | null>(
    null
  );

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
    // The graph layout includes an "IN" node as a visual source.
    // For the current UX, it reads better to omit it entirely.
    return layout.nodes.filter((n) => n.kind !== "IN");
  }, [layout.nodes]);

  const nodesById = React.useMemo(() => {
    const map = new Map<GraphNodeId, GraphNode>();
    for (const n of renderNodes) map.set(n.id, n);
    return map;
  }, [renderNodes]);

  const lineThickness = 2;

  const wrapped = layout.wrap != null;

  return (
    <View
      style={styles.root}
      onLayout={(e) => {
        const w = e.nativeEvent.layout.width;
        if (w > 0 && w !== containerWidth) setContainerWidth(w);
      }}
    >
      <ScrollView
        horizontal={!wrapped}
        showsHorizontalScrollIndicator={false}
        contentContainerStyle={[
          styles.scrollContent,
          { height: layout.height, width: layout.width },
        ]}
      >
        <View
          style={[
            styles.canvas,
            { width: layout.width, height: layout.height },
          ]}
        >
          {layout.edges.map((e) => {
            const from = nodesById.get(e.from);
            const to = nodesById.get(e.to);
            if (!from || !to) return null;

            const nodeW = layout.nodeWidth;
            const nodeH = layout.nodeHeight;

            const x1 = from.x + nodeW;
            const y1 = from.y + nodeH / 2;
            const x2 = to.x;
            const y2 = to.y + nodeH / 2;

            // Edge router:
            // - Same-row: simple 3-segment dogleg using a midpoint.
            // - Cross-row (wrapped layout): route out to the right edge, drop into the row gap,
            //   travel back to the left gutter, then drop to the destination row.
            const segments: Array<
              | { kind: "h"; left: number; top: number; width: number }
              | { kind: "v"; left: number; top: number; height: number }
            > = [];

            const addH = (left: number, y: number, width: number) => {
              const w = Math.max(0, width);
              if (w <= 0) return;
              segments.push({
                kind: "h",
                left,
                top: y - lineThickness / 2,
                width: w,
              });
            };
            const addV = (x: number, top: number, height: number) => {
              const h = Math.max(0, height);
              if (h <= 0) return;
              segments.push({
                kind: "v",
                left: x - lineThickness / 2,
                top,
                height: h,
              });
            };

            const sameWrapRow =
              !wrapped || from.wrapRow == null || to.wrapRow == null
                ? true
                : from.wrapRow === to.wrapRow;

            if (sameWrapRow) {
              const midX = Math.round((x1 + x2) / 2);
              addH(x1, y1, midX - x1);
              addV(midX, Math.min(y1, y2), Math.abs(y2 - y1));
              addH(midX, y2, x2 - midX);
            } else {
              // Wrapped routing: connect from the bottom-center of the source node
              // to the top-center of the destination node.
              const xFrom = from.x + nodeW / 2;
              const yFrom = from.y + nodeH;
              const xTo = to.x + nodeW / 2;
              const yTo = to.y;

              const rowOffsets = layout.wrap?.rowOffsets ?? [];
              const rowGap = layout.wrap?.rowGap ?? 0;
              const toRowStart =
                layout.padding + (rowOffsets[to.wrapRow!] ?? 0);
              const yBetween = toRowStart - rowGap / 2;

              // Wrapped routing: drop from the source bottom-center into the row gap,
              // run horizontally across the gap, then drop into the destination top-center.
              // The horizontal "gutter" segment should meet the two verticals at xFrom/xTo
              // (not at the far left/right edges).

              // Down from the bottom-center of the source pedal into the gutter.
              addV(
                xFrom,
                Math.min(yFrom, yBetween),
                Math.abs(yBetween - yFrom)
              );

              // Horizontal across the gutter, between the two verticals.
              addH(Math.min(xFrom, xTo), yBetween, Math.abs(xTo - xFrom));

              // Down into the destination top-center.
              addV(xTo, Math.min(yBetween, yTo), Math.abs(yTo - yBetween));
            }

            const opacity = e.enabled ? 1 : 0.3;

            return (
              <View key={e.id} pointerEvents="none" style={{ opacity }}>
                {segments.map((s, idx) => {
                  if (s.kind === "h") {
                    return (
                      <View
                        key={`${e.id}:h:${idx}`}
                        style={{
                          position: "absolute",
                          left: s.left,
                          top: s.top,
                          width: s.width,
                          height: lineThickness,
                          backgroundColor: "#1976D2",
                        }}
                      />
                    );
                  }
                  return (
                    <View
                      key={`${e.id}:v:${idx}`}
                      style={{
                        position: "absolute",
                        left: s.left,
                        top: s.top,
                        width: lineThickness,
                        height: s.height,
                        backgroundColor: "#1976D2",
                      }}
                    />
                  );
                })}
              </View>
            );
          })}

          {renderNodes.map((n) => {
            const isSelected =
              n.kind === "SLOT" && n.slotIndex === selectedSlotIndex;

            const body = (() => {
              const slot =
                n.slotIndex != null
                  ? slots.find((s) => s.slotIndex === n.slotIndex)
                  : undefined;

              const typeId = slot?.typeId ?? 0;
              const enabled = slot?.enabled ?? false;

              const shortName =
                typeId !== 0
                  ? getShortName(typeId) || `S${(n.slotIndex ?? 0) + 1}`
                  : "OFF";

              const name =
                typeId !== 0
                  ? getName(typeId) || `Slot ${(n.slotIndex ?? 0) + 1}`
                  : `Slot ${(n.slotIndex ?? 0) + 1}`;

              return (
                <PedalSlot
                  name={name}
                  shortName={shortName}
                  enabled={enabled}
                  selected={isSelected}
                  onPress={() => onSelectSlot(n.slotIndex!)}
                  onToggleEnabled={() =>
                    onToggleSlotEnabled(n.slotIndex, !enabled)
                  }
                />
              );
            })();

            return (
              <View
                key={n.id}
                style={{
                  position: "absolute",
                  left: n.x,
                  top: n.y,
                }}
              >
                {body}
              </View>
            );
          })}
        </View>
      </ScrollView>
    </View>
  );
};

const styles = StyleSheet.create({
  root: {
    width: "100%",
  },
  scrollContent: {
    paddingVertical: 8,
  },
  canvas: {
    position: "relative",
  },
});
