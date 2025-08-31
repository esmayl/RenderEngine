# Ants Attack — Game Overview

## Core Loop
- Guide an ant column between nest and food sources. Ants travel ToFood then ToNest, depositing food at the nest to score points.
- Meet the stage target before the timer expires. Between stages, choose an upgrade or switch to Endless mode.

## Controls
- Left-click: Select a food node under the cursor. Click the same node again to deselect (ants at nest stay idle; in‑flight ants finish their leg).
- Right-click: Set nest position.
- 1/2/3: Choose upgrade on Stage Clear (Speed +15%, Spawn Rate +25%, Max Ants +128).
- N: Advance to next stage after clearing.
- R: Restart from Stage 1.
- E: Toggle Endless mode (auto‑advance on timeout).

## Food & Selection
- Nodes spawn each stage (count increases with stage) with randomized amounts.
- Node shape is mixed: most are squares; ~30% are triangles.
- Node size scales with amount; hover shows a thin, faded outline around the node footprint.
- Active node is highlighted (yellow). No text is drawn over nodes to avoid selection interference.

## Ant Behavior
- Independent ants with slight lane offset and per‑ant speed variance reduce stacking.
- Ants don’t move until a node is selected. Deselecting doesn’t cancel in‑flight ants; they complete their leg.
- When no node is active, ants arriving at the nest remain there.

## Scoring, Stages, Upgrades
- Deposits at nest score points; quick successive deposits raise a combo up to x5.
- Each stage: higher target, more nodes, larger amounts, faster spawn limits.
- Upgrades on Stage Clear: Speed, Spawn Rate, or Max Ants.
- Endless mode: keeps escalating past timeouts.

## UI & Indicators
- ImGui HUD: FPS, Score+Combo, Mode, Stage/Target/Time, Ants, Event status.
- On‑field markers: Nest (red), Food nodes (green), Active food (yellow), Hover outline.

## Hazards & Events
- Occasional “Rain” slow event reduces speed briefly.

## Tech Notes
- DirectX 11 instanced/compute rendering; nodes drawn top‑left anchored; hit‑tests match on‑screen footprint.
