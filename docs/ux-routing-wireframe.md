# Routing UX sketch (NoiseGate → Comp → split Chorus/Sweep → Mixer)

This app currently has:
- A **slot grid** (small cards) for selection + enable
- A **bottom panel** showing params for the selected slot

---

## Implemented (current app)

The **Selected Slot** bottom panel now has a third tab: **Routing**.

### Routing tab controls

- **Inputs**
	- `Left input`: pill selector (Badges) for `IN` or an earlier slot (`1:NGT`, `2:CMP`, ...)
	- `Right input`: same
	- **Constraint**: “future” slots (later in the chain) are disabled to avoid forward-references.

- **Channel**
	- `Sum to mono`: pill toggle (`ON`/`OFF`)
	- `Channel policy`: segmented pills: `Auto`, `Mono`, `Stereo`

- **Mix**
	- `Dry` slider (0–127)
	- `Wet` slider (0–127)

These controls update the patch live via SysEx (same mechanism as params/type/enabled).

### Practical notes

- For typical “mono guitar into a stereo effect”, set **Left** and **Right** inputs to the same mono source slot (e.g. `2:CMP`) and leave `Sum to mono` OFF.
- The **Mixer** effect interprets **Left input as A** and **Right input as B**.

---

Below are two lightweight ways to represent and edit routing without changing the overall UX structure.

---

## Option A (minimal): “Patch Graph” card + per-slot Routing controls

### 1) Patch Graph (readable split/merge)

Add a new card above the slot grid:

```
┌───────────────────────────────────────────────────────────────────────────┐
│ Patch Graph                                                               │
│                                                                           │
│  IN (mono)                                                                │
│    │                                                                      │
│   [NGT] ──▶ [CMP] ──┬──▶ [CHO] ──┐                                        │
│                     │           │                                         │
│                     └──▶ [SWP] ─┴──▶ [MIX] ──▶ OUT (stereo)               │
│                                                                           │
│  Legend: solid arrows = audio taps, [..] = slot short-name                │
└───────────────────────────────────────────────────────────────────────────┘
```

Interaction:
- Tapping a node selects the corresponding slot (same as tapping the slot card).
- Selected node is highlighted (blue outline like the slot cards).

### 2) Slot grid (unchanged)

Keep the existing slot cards exactly as-is.

#### Slot rendering suggestions (when routing splits/merges)

Even if you keep the grid interaction the same, you can make split/merge patches much easier to understand by adding *small, text-only hints* inside each slot card (no new icons required).

**A. Add compact “routing badges” inside each slot card**

Goal: make it obvious where a slot is reading from (and whether it’s mono-summed) without opening the details panel.

Example slot card layout (fits the current 70×80-ish card, conceptually):

```
┌──────────────┐
│ CHO          │
│              │
│  IN: 2 / 2   │  (Left / Right tap)
│  Σ: OFF  P:S │  (Sum-to-mono + Policy)
│      ● ON    │
└──────────────┘
```

Suggested abbreviations:
- `IN: IN/IN` or `IN: 2/2` (slot index) or `IN: CMP/CMP` (short-name)
- `Σ: ON|OFF` for `sumToMono`
- `P: A|M|S` for ChannelPolicy (`Auto`, `Mono`, `Stereo`)

These can be rendered as 1–2 tiny lines under the slot short-name.

**B. Show branch membership (A/B) for split patches**

Goal: in the grid, users should immediately see that two effects are siblings fed from the same source.

For the example patch, you could annotate:
- `CHO` as `A`
- `SWP` as `B`
- `MIX` as `A+B`

Example:

```
CHO  (A)
SWP  (B)
MIX  (A+B)
```

This works well with the existing “short name” pattern (`NGT`, `CMP`, `CHO`, `SWP`, `MIX`) and doesn’t require drawing lines in the grid.

**C. Optional: lightweight grouping in the grid (no new screens)**

Goal: visually imply the graph shape while staying inside a card grid.

One simple approach is to render “branch rows” after the split point:

```
Row 1 (main):  [NGT] → [CMP]
Row 2 (branch):        [CHO]    [SWP]
Row 3 (merge):                 [MIX]
```

The selection behavior stays the same (tap any slot card). The Patch Graph card still provides the most accurate picture; this is just a helpful hint.

**D. Selection-based highlights (helps editing)**

When a slot is selected, highlight related cards in the grid:
- its immediate inputs (upstream taps)
- its immediate consumers (slots that read from it)

This makes “what does this feed / where does it come from” clear without drawing connector lines.

### 3) Selected Slot panel: add Routing section

Extend the bottom panel (currently title + enable switch + parameter sliders) with a small “Routing” section.

**Example: when selecting the Chorus slot**

```
┌───────────────────────────────────────────────────────────────────────────┐
│ Chorus                                                        [ ON  ⏽ ]   │
├───────────────────────────────────────────────────────────────────────────┤
│ Routing                                                                   │
│  Left in :  [ Slot 2: CMP ▼ ]                                             │
│  Right in:  [ Slot 2: CMP ▼ ]                                             │
│  Sum to mono: [ OFF ]                                                     │
│  Channel policy: [ Auto ] [ Mono ] [ Stereo ]                             │
├───────────────────────────────────────────────────────────────────────────┤
│ Parameters                                                                │
│  Rate  ────────────────▮▮▮▮▯▯▯▯▯▯▯▯  0.8                                 │
│  Depth ────────────────▮▮▮▮▮▯▯▯▯▯▯▯  0.6                                 │
│  ...                                                                      │
└───────────────────────────────────────────────────────────────────────────┘
```

Input dropdown values:
- `IN` (ROUTE_INPUT)
- `Slot 1 (NGT)`
- `Slot 2 (CMP)`
- … up to `Slot 12`

Notes:
- For typical “mono guitar into stereo effect”, you’d set Left in = CMP, Right in = CMP.
- For the mixer, Left in should point to Chorus, Right in should point to Sweep.

---

## Option B (guided): a “Routing Mode” tap-to-wire UI

This keeps Option A’s layout, but makes editing wiring faster/safer.

### Patch Graph with “Route” affordance

When a slot is selected, show a small inline action row:

```
┌───────────────────────────────────────────────────────────────────────────┐
│ Patch Graph                                                               │
│  IN → [NGT] → [CMP] → [CHO]                                               │
│                    ↘ [SWP] → [MIX] → OUT                                  │
│                                                                           │
│  Selected: [CHO]    Actions:  [ Set Inputs… ]  [ Set Outputs… ]           │
└───────────────────────────────────────────────────────────────────────────┘
```

Tapping `Set Inputs…` opens a simple sheet:

```
Set inputs for CHO
- Left input  : (•) CMP   ( ) IN   ( ) Slot…
- Right input : (•) CMP   ( ) IN   ( ) Slot…
- Sum to mono : [ ]
- Channel policy: Auto / Mono / Stereo
[Done]
```

This avoids needing to explain L/R taps in the main UI.

---

## Concrete mapping for your example patch

Assuming slots:
- Slot 1: NGT
- Slot 2: CMP
- Slot 3: CHO
- Slot 4: SWP
- Slot 5: MIX

Routing fields to show/edit:
- NGT: L=IN, R=IN, sumToMono=ON, policy=Mono
- CMP: L=NGT, R=NGT, sumToMono=ON, policy=Mono
- CHO: L=CMP, R=CMP, sumToMono=OFF, policy=Stereo
- SWP: L=CMP, R=CMP, sumToMono=OFF, policy=Stereo
- MIX: L=CHO, R=SWP, sumToMono=OFF, policy=Stereo

### Test mode: Chorus in one ear, Delay in the other

To quickly verify split/merge wiring with headphones, configure the **Mixer** like this:

- Mixer Routing:
	- `Left input` = `CHO`
	- `Right input` = `SWP`
- Mixer Params:
	- `A level (mixA)` = 127
	- `B level (mixB)` = 127
	- `Cross` = 0

Result:
- Left ear = Chorus
- Right ear = Sweep Delay

---

## Why this fits your current UX

- It reuses the existing mental model: “select a slot → edit details in the bottom panel”.
- It introduces routing without adding new pages or heavy UI.
- The Patch Graph card makes the *split/merge* obvious at a glance, which the current slot grid can’t show.
