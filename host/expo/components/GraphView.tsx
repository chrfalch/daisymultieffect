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
  sumToMono: boolean;
  channelPolicy: number;
  params: Record<number, number>;
};

export const GraphView: React.FC<{
  slots: EffectSlotLike[];
  numSlots: number;
  selectedSlotIndex?: number;
  getShortName: (typeId: number) => string;
  getName: (typeId: number) => string;
  onSelectSlot: (slotIndex: number) => void;
  onToggleSlotEnabled: (slotIndex: number, enabled: boolean) => void;
  getDisplayLabel?: (typeId: number, params: Record<number, number>) => string | undefined;
}> = ({
  slots,
  numSlots,
  selectedSlotIndex,
  getShortName,
  getName,
  onSelectSlot,
  onToggleSlotEnabled,
  getDisplayLabel,
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
              | {
                  kind: "corner";
                  x: number;
                  y: number;
                  corner: "tl" | "tr" | "bl" | "br";
                }
            > = [];

            const cornerRadius = 4;

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
            const addCorner = (
              x: number,
              y: number,
              corner: "tl" | "tr" | "bl" | "br"
            ) => {
              segments.push({ kind: "corner", x, y, corner });
            };

            const sameWrapRow =
              !wrapped || from.wrapRow == null || to.wrapRow == null
                ? true
                : from.wrapRow === to.wrapRow;

            if (sameWrapRow) {
              const midX = Math.round((x1 + x2) / 2);
              const goingDown = y2 > y1;
              const goingUp = y2 < y1;

              if (goingDown) {
                // H right → corner ┐ (tr) → V down → corner └ (bl) → H right
                addH(x1, y1, midX - x1 - cornerRadius);
                addCorner(midX, y1, "tr");
                addV(
                  midX,
                  y1 + cornerRadius,
                  Math.abs(y2 - y1) - cornerRadius * 2
                );
                addCorner(midX, y2, "bl");
                addH(midX + cornerRadius, y2, x2 - midX - cornerRadius);
              } else if (goingUp) {
                // H right → corner ┘ (br) → V up → corner ┌ (tl) → H right
                addH(x1, y1, midX - x1 - cornerRadius);
                addCorner(midX, y1, "br");
                addV(
                  midX,
                  y2 + cornerRadius,
                  Math.abs(y2 - y1) - cornerRadius * 2
                );
                addCorner(midX, y2, "tl");
                addH(midX + cornerRadius, y2, x2 - midX - cornerRadius);
              } else {
                // Straight horizontal
                addH(x1, y1, x2 - x1);
              }
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

              const goingRight = xTo > xFrom;

              if (goingRight) {
                // V down → corner └ (bl) → H right → corner ┐ (tr) → V down
                addV(xFrom, yFrom, yBetween - yFrom - cornerRadius);
                addCorner(xFrom, yBetween, "bl");
                addH(
                  xFrom + cornerRadius,
                  yBetween,
                  xTo - xFrom - cornerRadius * 2
                );
                addCorner(xTo, yBetween, "tr");
                addV(
                  xTo,
                  yBetween + cornerRadius,
                  yTo - yBetween - cornerRadius
                );
              } else {
                // V down → corner ┘ (br) → H left → corner ┌ (tl) → V down
                addV(xFrom, yFrom, yBetween - yFrom - cornerRadius);
                addCorner(xFrom, yBetween, "br");
                addH(
                  xTo + cornerRadius,
                  yBetween,
                  xFrom - xTo - cornerRadius * 2
                );
                addCorner(xTo, yBetween, "tl");
                addV(
                  xTo,
                  yBetween + cornerRadius,
                  yTo - yBetween - cornerRadius
                );
              }
            }

            const opacity = e.enabled ? 1 : 0.3;
            const color = "#1976D2";

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
                          backgroundColor: color,
                        }}
                      />
                    );
                  }
                  if (s.kind === "v") {
                    return (
                      <View
                        key={`${e.id}:v:${idx}`}
                        style={{
                          position: "absolute",
                          left: s.left,
                          top: s.top,
                          width: lineThickness,
                          height: s.height,
                          backgroundColor: color,
                        }}
                      />
                    );
                  }
                  // Corner piece - uses border to draw quarter-circle
                  // The corner position (s.x, s.y) is where the two line segments meet
                  const r = cornerRadius;
                  const t = lineThickness;
                  const borderStyle = {
                    // tl: horizontal comes from right, turns up
                    tl: {
                      borderTopWidth: t,
                      borderLeftWidth: t,
                      borderTopLeftRadius: r,
                    },
                    // tr: horizontal comes from left, turns up
                    tr: {
                      borderTopWidth: t,
                      borderRightWidth: t,
                      borderTopRightRadius: r,
                    },
                    // bl: vertical comes from top, turns left (or horizontal from right turns down)
                    bl: {
                      borderBottomWidth: t,
                      borderLeftWidth: t,
                      borderBottomLeftRadius: r,
                    },
                    // br: vertical comes from top, turns right (or horizontal from left turns down)
                    br: {
                      borderBottomWidth: t,
                      borderRightWidth: t,
                      borderBottomRightRadius: r,
                    },
                  }[s.corner];
                  // Position the corner box so the curved border aligns with the line segments
                  // The corner point (s.x, s.y) is where the centerlines meet
                  // Each corner type has its curved border in a different quadrant
                  const half = t / 2;
                  const offset = {
                    // ┌ curve in top-left quadrant of the box
                    tl: { left: s.x - half, top: s.y - half },
                    // ┐ curve in top-right quadrant
                    tr: { left: s.x + half - r * 2, top: s.y - half },
                    // └ curve in bottom-left quadrant
                    bl: { left: s.x - half, top: s.y + half - r * 2 },
                    // ┘ curve in bottom-right quadrant
                    br: { left: s.x + half - r * 2, top: s.y + half - r * 2 },
                  }[s.corner];
                  return (
                    <View
                      key={`${e.id}:c:${idx}`}
                      style={{
                        position: "absolute",
                        left: offset.left,
                        top: offset.top,
                        width: r * 2,
                        height: r * 2,
                        borderColor: color,
                        ...borderStyle,
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
              const sumToMono = slot?.sumToMono ?? false;
              const channelPolicy = slot?.channelPolicy ?? 0;

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
                <PedalSlot
                  name={name}
                  shortName={shortName}
                  subtitle={subtitle}
                  enabled={enabled}
                  selected={isSelected}
                  sumToMono={sumToMono}
                  channelPolicy={channelPolicy}
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
    alignSelf: "center",
  },
  canvas: {
    position: "relative",
  },
});
