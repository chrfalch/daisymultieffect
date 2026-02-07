
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
`host/apple/` inneholder Swift-filer du kan droppe inn i en ny SwiftUI-app i Xcode (iOS + macOS).

## Daisy MIDI Bridge (macOS) auto Network MIDI setup
Bridge-appen aktiverer nå macOS Network MIDI-sesjonen automatisk ved oppstart.

For å prøve automatisk iPad-tilkobling også, start bridge med disse env-variablene:

```sh
export DAISY_MIDI_IPAD_HOST=192.168.1.42
export DAISY_MIDI_IPAD_PORT=5004   # optional, default 5004
export DAISY_MIDI_IPAD_NAME="My iPad"  # optional
./bridge/run-bridge.sh
```

Hvis `DAISY_MIDI_IPAD_HOST` ikke er satt, blir kun Network MIDI-sesjonen aktivert
slik at du slipper å gjøre dette manuelt i Audio MIDI Setup.

## Neste steg
- Integrere ekte libDaisy audio + USB MIDI i `main.cpp`
- Definere endelig 7-bit-safe patch-serialisering
- Lage crossfade patch switching i audio callback
- Utvide iPad UI til å generere kontroller fra `EffectMeta`
