export type GraphViewMode = "grid" | "graph";

export type GraphNodeId = "IN" | `S${number}`;

export type GraphNode = {
  id: GraphNodeId;
  kind: "IN" | "SLOT";
  slotIndex?: number;
  label: string;
  x: number;
  y: number;
  lane: number;
  wrapRow?: number;
  wrapCol?: number;
};

export type GraphEdge = {
  id: string;
  from: GraphNodeId;
  to: GraphNodeId;
  label: "L" | "R" | "LR";
  enabled: boolean;
};

export type GraphLayout = {
  nodes: GraphNode[];
  edges: GraphEdge[];
  width: number;
  height: number;
  nodeWidth: number;
  nodeHeight: number;
  padding: number;
  wrap?: {
    maxWidth: number;
    columns: number;
    rowGap: number;
    rowOffsets: number[];
  };
};

type SlotLike = {
  slotIndex: number;
  typeId: number;
  enabled: boolean;
  inputL: number; // 0-11 or 255
  inputR: number; // 0-11 or 255
};

export function buildRoutingGraphLayout(args: {
  slots: SlotLike[];
  numSlots: number;
  getShortName: (typeId: number) => string;
  selectedSlotIndex?: number;
  maxWidth?: number;
}): GraphLayout {
  const { slots, numSlots, getShortName } = args;

  // Match PedalSlot sizing so we can render slots using that component.
  const nodeWidth = 90;
  const nodeHeight = 120;
  const gapX = 28;
  const gapY = 36;
  const padding = 16;
  const rowGap = 24;

  const wrapMaxWidth = args.maxWidth;
  const wrapColumns =
    wrapMaxWidth != null && wrapMaxWidth > padding * 2 + nodeWidth
      ? Math.max(
          1,
          Math.floor((wrapMaxWidth - padding * 2 + gapX) / (nodeWidth + gapX))
        )
      : undefined;

  const normalizedSlots = slots
    .filter((s) => s.slotIndex >= 0 && s.slotIndex < numSlots)
    .sort((a, b) => a.slotIndex - b.slotIndex);

  const nodeIdForSlot = (slotIndex: number): GraphNodeId => `S${slotIndex}`;

  // Treat the input as an implicit source node.
  // By placing it at depth -1, the first real slot column becomes 0,
  // removing the visual "hole" where an explicit IN node would sit.
  const inputDepth = -1;

  const safeSource = (
    source: number,
    destSlotIndex: number
  ): {
    id: GraphNodeId;
    enabled: boolean;
  } => {
    if (source === 255) return { id: "IN", enabled: true };
    if (source < 0 || source >= numSlots) return { id: "IN", enabled: false };
    // forward references are not valid in the DSP (read as 0), treat as disabled
    if (source >= destSlotIndex) return { id: "IN", enabled: false };
    return { id: nodeIdForSlot(source), enabled: true };
  };

  // Compute a simple DAG depth based on routing (distance from IN).
  // Since valid routes only reference earlier slots (or IN), iterating in slotIndex order works.
  const depthByNode = new Map<GraphNodeId, number>();
  depthByNode.set("IN", inputDepth);
  for (const s of normalizedSlots) {
    const srcL = safeSource(s.inputL, s.slotIndex);
    const srcR = safeSource(s.inputR, s.slotIndex);

    const dL = depthByNode.get(srcL.id) ?? 0;
    const dR = depthByNode.get(srcR.id) ?? 0;
    const depth = Math.max(dL, dR) + 1;
    depthByNode.set(nodeIdForSlot(s.slotIndex), depth);
  }

  // Lane assignment (simple, stable heuristic)
  const laneByNode = new Map<GraphNodeId, number>();
  laneByNode.set("IN", 0);

  const branchCountBySource = new Map<GraphNodeId, number>();
  const occupiedByColumn = new Map<number, Set<number>>();
  const placementByNode = new Map<
    GraphNodeId,
    { colKey: number; wrapRow: number; wrapCol: number }
  >();
  const maxLaneByWrapRow = new Map<number, number>();

  const placeInColumn = (colKey: number): number => {
    const used = occupiedByColumn.get(colKey) ?? new Set<number>();
    let lane = 0;
    while (used.has(lane)) lane += 1;
    used.add(lane);
    occupiedByColumn.set(colKey, used);
    return lane;
  };

  const toPlacement = (col: number) => {
    if (wrapColumns == null) {
      return { colKey: col, wrapRow: 0, wrapCol: col };
    }
    const wrapRow = Math.floor(col / wrapColumns);
    const wrapCol = col % wrapColumns;
    const colKey = wrapRow * wrapColumns + wrapCol;
    return { colKey, wrapRow, wrapCol };
  };

  const advancePlacement = (p: { wrapRow: number; wrapCol: number }) => {
    if (wrapColumns == null) {
      return toPlacement(p.wrapCol + 1);
    }
    let wrapRow = p.wrapRow;
    let wrapCol = p.wrapCol + 1;
    if (wrapCol >= wrapColumns) {
      wrapRow += 1;
      wrapCol = 0;
    }
    const colKey = wrapRow * wrapColumns + wrapCol;
    return { colKey, wrapRow, wrapCol };
  };

  for (const s of normalizedSlots) {
    const baseCol = depthByNode.get(nodeIdForSlot(s.slotIndex)) ?? 0;
    let placed = toPlacement(baseCol);

    // Keep OFF slots packed into a single row when possible: rather than creating
    // extra vertical lanes for many OFF nodes that share a depth, slide them right
    // until lane 0 is free in that column.
    if (s.typeId === 0) {
      while (
        (occupiedByColumn.get(placed.colKey) ?? new Set<number>()).has(0)
      ) {
        placed = advancePlacement(placed);
      }
    }

    const srcL = safeSource(s.inputL, s.slotIndex);
    const srcR = safeSource(s.inputR, s.slotIndex);

    const laneL = laneByNode.get(srcL.id) ?? 0;
    const laneR = laneByNode.get(srcR.id) ?? 0;

    let proposedLane = 0;

    if (srcL.id !== srcR.id) {
      proposedLane = Math.min(laneL, laneR);
    } else {
      const count = branchCountBySource.get(srcL.id) ?? 0;
      proposedLane = laneL + count;
      branchCountBySource.set(srcL.id, count + 1);
    }

    const lane = placeInColumn(placed.colKey);
    laneByNode.set(nodeIdForSlot(s.slotIndex), lane);
    placementByNode.set(nodeIdForSlot(s.slotIndex), {
      colKey: placed.colKey,
      wrapRow: placed.wrapRow,
      wrapCol: placed.wrapCol,
    });

    const prevMax = maxLaneByWrapRow.get(placed.wrapRow) ?? 0;
    if (lane > prevMax) maxLaneByWrapRow.set(placed.wrapRow, lane);
  }

  const maxWrapRow =
    wrapColumns != null
      ? Array.from(placementByNode.values()).reduce(
          (m, p) => Math.max(m, p.wrapRow),
          0
        )
      : 0;

  const rowOffsets: number[] = [];
  rowOffsets[0] = 0;
  for (let r = 1; r <= maxWrapRow; r++) {
    const prevMaxLane = maxLaneByWrapRow.get(r - 1) ?? 0;
    const prevRowHeight = (prevMaxLane + 1) * (nodeHeight + gapY) - gapY;
    rowOffsets[r] = rowOffsets[r - 1] + prevRowHeight + rowGap;
  }

  const nodes: GraphNode[] = [];

  for (const s of normalizedSlots) {
    const lane = laneByNode.get(nodeIdForSlot(s.slotIndex)) ?? 0;
    const short = getShortName(s.typeId) || `S${s.slotIndex + 1}`;
    const depth = depthByNode.get(nodeIdForSlot(s.slotIndex)) ?? s.slotIndex;

    const placed = placementByNode.get(nodeIdForSlot(s.slotIndex));
    const wrapRow =
      placed?.wrapRow ??
      (wrapColumns != null ? Math.floor(depth / wrapColumns) : 0);
    const wrapCol =
      placed?.wrapCol ?? (wrapColumns != null ? depth % wrapColumns : depth);
    const yBase = padding + (rowOffsets[wrapRow] ?? 0);

    nodes.push({
      id: nodeIdForSlot(s.slotIndex),
      kind: "SLOT",
      slotIndex: s.slotIndex,
      label: `${s.slotIndex + 1}:${short}`,
      x: padding + wrapCol * (nodeWidth + gapX),
      y: yBase + lane * (nodeHeight + gapY),
      lane,
      wrapRow,
      wrapCol,
    });
  }

  const edges: GraphEdge[] = [];
  for (const s of normalizedSlots) {
    const toId = nodeIdForSlot(s.slotIndex);

    const srcL = safeSource(s.inputL, s.slotIndex);
    const srcR = safeSource(s.inputR, s.slotIndex);

    if (srcL.id === srcR.id) {
      edges.push({
        id: `${srcL.id}->${toId}:LR`,
        from: srcL.id,
        to: toId,
        label: "LR",
        enabled: srcL.enabled && srcR.enabled,
      });
    } else {
      edges.push({
        id: `${srcL.id}->${toId}:L`,
        from: srcL.id,
        to: toId,
        label: "L",
        enabled: srcL.enabled,
      });
      edges.push({
        id: `${srcR.id}->${toId}:R`,
        from: srcR.id,
        to: toId,
        label: "R",
        enabled: srcR.enabled,
      });
    }
  }

  const maxX = nodes.reduce((m, n) => Math.max(m, n.x), 0);
  const maxY = nodes.reduce((m, n) => Math.max(m, n.y), 0);

  const width =
    wrapColumns != null && wrapMaxWidth != null
      ? Math.min(
          wrapMaxWidth,
          padding * 2 + wrapColumns * (nodeWidth + gapX) - gapX
        )
      : maxX + nodeWidth + padding;

  const height = maxY + nodeHeight + padding;

  return {
    nodes,
    edges,
    width,
    height,
    nodeWidth,
    nodeHeight,
    padding,
    ...(wrapColumns != null && wrapMaxWidth != null
      ? {
          wrap: {
            maxWidth: wrapMaxWidth,
            columns: wrapColumns,
            rowGap,
            rowOffsets,
          },
        }
      : null),
  };
}
