# Windows Standalone: MIDI OUT Ignored by Hardware (sysexID Mismatch)

## Symptom

On a fresh Windows 11 standalone install: MIDI IN works (hardware SysEx received,
UI updates when hardware knobs move). MIDI OUT appears to send — both the physical
interface LED and the app's OUT LED flash when a parameter is changed in the app —
but the hardware ignores every outgoing SysEx and never changes.

## Root cause

All outgoing SysEx embeds a device-ID byte (`F0 10 [sysexID] …`). The Xpander/Matrix-12
only acts on SysEx whose device-ID byte matches its own configured ID. The receive path
does **not** filter by device ID (it accepts any Oberheim SysEx regardless of that byte),
which is why MIDI IN worked even with the wrong ID — masking the problem.

On a fresh install there are no saved settings. `EditorTabComponent::loadSettings()`
called `appProperties->getIntValue("SysexID", 0)`, which returns `0` when the key
doesn't exist yet — silently overriding the in-code default of `2`. Neither `0` nor `2`
is likely to match the hardware's actual configured ID (commonly `1`, or whatever the
user set), so every outgoing parameter/page-select SysEx was silently dropped by the
hardware.

A secondary issue: the standalone build had no visible UI control for the Synth ID —
`MidiSettingsPanel` (INPUT list / OUTPUT dropdown / SYNTH ID dropdown) was only created
in plugin builds, so standalone users had no way to see or correct a mismatched ID.

## Fixes

1. **Auto-detect sysexID from incoming hardware SysEx** (`MidiEngine`) — the first time
   an incoming Oberheim SysEx carries a device-ID byte different from our current
   `sysexID`, we adopt it and notify listeners via the new
   `MidiEngine::Listener::onSysexIDDetected(int id)` callback. `EditorTabComponent`
   updates the SYNTH ID selector and persists the learned ID.
2. **Stop overwriting sysexID with 0 on fresh install** (`EditorTabComponent::loadSettings`) —
   only apply the saved `SysexID` when the settings key actually exists
   (`appProperties->containsKey("SysexID")`); otherwise leave the in-code default of `2`
   in place until auto-detection fires.
3. **Expose SYNTH ID in the standalone build** — `MidiSettingsPanel` is now created in
   all build targets (was plugin-only), with its own nav button so standalone users can
   view and manually correct the device ID.

## Files changed

| File | Change |
|---|---|
| `XpandrLink/Source/Engine/MidiEngine.h` | Add `onSysexIDDetected(int)` to `Listener` |
| `XpandrLink/Source/Engine/MidiEngine.cpp` | Auto-detect sysexID from incoming SysEx in `handleIncomingMidiMessage` |
| `XpandrLink/Source/Tabs/EditorTabComponent.h` | Declare `onSysexIDDetected` override |
| `XpandrLink/Source/Tabs/EditorTabComponent.cpp` | Implement callback; fix `loadSettings` fallback; create `MidiSettingsPanel` + `btnMidi` pane in standalone |

## Verification

- macOS Debug build (Standalone) compiles clean.
- Hardware verification pending: delete settings file, launch standalone, move a
  hardware knob to trigger auto-detect, confirm outgoing parameter changes now reach
  the hardware, and confirm the MIDI pane's SYNTH ID dropdown reflects the learned ID.
