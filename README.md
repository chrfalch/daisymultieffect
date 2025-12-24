
# DaisyMultiFX (Monorepo starter)

Dette er et start-monorepo for en **Daisy Seed** multi‑FX pedal + en **iPad/iOS SwiftUI demo**
som konfigurerer “patches” via **MIDI SysEx**.

## Arkitektur (kort)
- **Audio engine**: 12 “slots”. Hver slot kan lese input fra:
  - fysisk input (`ROUTE_INPUT = 255`), eller
  - en annen slot sin output (gir serie/parallel routing)
- Hver slot har: `typeId`, `enabled`, `inputL/inputR`, `sumToMono`, `dry/wet`, `channelPolicy`, params.
- **Patch** (`PatchWireDesc`): liste av slots + 2 knapp‑bindings:
  - `ToggleBypass`: toggler `enabled` for en slot
  - `TapTempo`: beregner BPM fra tap og oppdaterer global `TempoSource` (brukt av tempo‑synced effekter)
- **Self-describing effects**: hver effekt eksponerer `EffectMeta` (navn/desc + paramliste).
  Parametre kan være:
  - `Number` (min/max/step)
  - `Enum` (valg som “spring/plate” osv)

## SysEx meldinger (vi bruker 0x7D som manuf id)
- `SET_PATCH`  : `F0 7D 10 <payload> F7`  (payload definerer du videre)
- `GET_PATCH`  : `F0 7D 12 F7`
- `PATCH_DUMP` : `F0 7D 11 <payload> F7`
- `BUTTON_STATE_CHANGE`: `F0 7D 40 <btn> <slot> <enabled> F7`
- `TEMPO_UPDATE`: `F0 7D 41 <bpm Q16.16 packed 5x7bit> F7`
- `GET_EFFECT_META`: `F0 7D 30 <typeId> F7`
- `GET_ALL_EFFECT_META`: `F0 7D 32 F7`
- `EFFECT_META_RESPONSE`: `F0 7D 31 <payload> F7`

## Bygg/flash firmware
Du trenger `libDaisy` og `DaisySP` ved siden av repoet (eller sett path i Makefile):

```sh
cd firmware
make
make flash
```

## iOS demo
`host/ios/` inneholder Swift-filer du kan droppe inn i en ny SwiftUI-app i Xcode.

## Neste steg
- Integrere ekte libDaisy audio + USB MIDI i `main.cpp`
- Definere endelig 7-bit-safe patch-serialisering
- Lage crossfade patch switching i audio callback
- Utvide iPad UI til å generere kontroller fra `EffectMeta`
