import React from "react";
import {
  Skia,
  Path as SkiaPath,
} from "@shopify/react-native-skia";
import type {
  GraphLayout,
  GraphNode,
  GraphNodeId,
  GraphEdge,
} from "../../utils/routingGraph";

// ─── Path Helpers ────────────────────────────────────────────────────

/** Build a Skia path string for a smooth cubic-bezier routing edge. */
function buildEdgePath(
  x1: number,
  y1: number,
  x2: number,
  y2: number,
): string {
  const dx = Math.abs(x2 - x1);
  const controlPointOffset = Math.max(20, dx * 0.4);
  return `M ${x1} ${y1} C ${x1 + controlPointOffset} ${y1}, ${x2 - controlPointOffset} ${y2}, ${x2} ${y2}`;
}

/** Build a Skia path string for a wrapped (cross-row) routing edge. */
function buildWrappedEdgePath(
  xFrom: number,
  yFrom: number,
  xTo: number,
  yTo: number,
  yMid: number,
): string {
  const controlPointDistance = 16;
  // Down from source → horizontal across → down to destination
  return [
    `M ${xFrom} ${yFrom}`,
    `C ${xFrom} ${yFrom + controlPointDistance}, ${xFrom} ${yMid - controlPointDistance}, ${xFrom} ${yMid}`,
    `L ${xTo} ${yMid}`,
    `C ${xTo} ${yMid + controlPointDistance}, ${xTo} ${yTo - controlPointDistance}, ${xTo} ${yTo}`,
  ].join(" ");
}

// ─── Component ───────────────────────────────────────────────────────

export interface EdgeDrawProps {
  edge: GraphEdge;
  nodesById: Map<GraphNodeId, GraphNode>;
  nodeWidth: number;
  nodeHeight: number;
  layout: GraphLayout;
  /** Left-channel edge color */
  colorLeft: string;
  /** Right-channel edge color */
  colorRight: string;
}

export const SkiaEdge: React.FC<EdgeDrawProps> = React.memo(
  ({ edge, nodesById, nodeWidth, nodeHeight, layout, colorLeft, colorRight }) => {
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
    const color = edge.label === "R" ? colorRight : colorLeft;
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
