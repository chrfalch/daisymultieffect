# SwiftUI demo (iOS + macOS)

Dette kan være én og samme kodebase på iOS og macOS.
CoreMIDI finnes på begge plattformer, og MidiController.swift er allerede skrevet uten iOS-spesifikke avhengigheter.

## Anbefalt: Multiplatform-app i Xcode

1. Xcode → File → New → Project…
2. Velg Multiplatform App (SwiftUI)
3. Legg inn disse filene i prosjektet (f.eks. i en "Shared" gruppe) og huk av at de legges til både iOS- og macOS-target:
   - ContentView.swift
   - DaisyMultiFXApp.swift
   - MidiController.swift
4. Kjør macOS-targetet for rask testing mens Daisy Seed er koblet til USB.

## Alternativ: Legg til macOS-target på eksisterende iOS-app

1. Xcode → File → New → Target…
2. Velg macOS App (SwiftUI)
3. Sørg for at de samme Swift-filene er inkludert i begge targets (File Inspector → Target Membership).

## Tips for testing

- På macOS kan du sjekke at enheten dukker opp i Audio MIDI Setup.
- Appen forsøker å auto-velge et MIDI-endepunkt som inneholder "daisy"/"seed" i navnet, ellers velger den første tilgjengelige.
