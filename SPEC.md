# XpandrLink — Product Specification

**Audience:** anyone who needs to understand what XpandrLink is, what it does, and how it's built — without reading the source.

For forward-looking work, see [ROADMAP.md](ROADMAP.md).

---

## 1. What XpandrLink Is

**XpandrLink** is a real-time, bi-directional editor for the **Oberheim Xpander** and **Matrix-12** synthesizers. It exposes all 226 parameters of a single patch and keeps the editor UI, hardware front panel, MIDI controllers, and DAW automation in sync.

It is a C++/JUCE port of the original Windows/.NET [XplorerEditor](https://github.com/xplorer2716/XplorerEditor), whose source remains the authoritative reference for hardware protocol nuances not covered below (see the quick-index in §9).

---

## 2. Supported Platforms & Plugin Formats

| Platform | Standalone | AU | VST3 | Verified |
|---|---|---|---|---|
| macOS (arm64 + x86_64, 13.0+) | ✅ | ✅ (`aufx`) | ✅ | Yes (auval passes; hardware-validated against Matrix-12 and Xpander) |
| Windows (x64, VS2022) | ✅ | — (Apple-only) | ✅ builds | Standalone hardware-validated on Windows 11 against both synths (2026-07-03). VST3 compiles but has not been loaded in a Windows DAW yet — see ROADMAP WIN-1 |

AU is gated `if(APPLE)` in `CMakeLists.txt` and excluded from the Windows formats list automatically. AU type is explicitly `kAudioUnitType_Effect` (aufx) — required by Ableton Live; JUCE's CMake defaults to `aumf` whenever `NEEDS_MIDI_INPUT` is `TRUE`, so the explicit setting must not be removed.

---

## 3. Feature Inventory

### Editor
- All 226 patch parameters across VCO1/2, VCF+VCA, FM, LAG, LFOs ×5, ENVs ×5, TRACKs ×3, RAMPs ×4.
- Real-time bi-directional sync: hardware front panel, MIDI controllers, DAW, and editor UI all stay coherent.
- Patch name (8-char, uppercase) — editable inline in the title bar.
- PAGE 2 advanced parameters (ENV/RAMP/LFO mode flags, KB Tracking).
- Filter graph + envelope graph visualisations (envelope has D/A/D/S/R drag handles).

### Modulation
- 20-slot mod matrix; per-slot source / destination / amount / quantize.
- Add/remove routings via click-to-assign (yellow-highlight destinations on a source click).
- IDSource tracking so multiple routings to the same destination don't collide.
- ModSummary panel (6 SRC slots) + FullModMatrix panel (5×4 grid). Mod-destination LEDs on every assignable parameter label.

### Librarian (SQLite-backed)
- Managed library root (default `<Documents>/OberheimPatches/Library` via JUCE's cross-platform `userDocumentsDirectory`; user-configurable, persisted in the DB `settings` table).
- Single-patch and bank-file import (auto-detects 399-byte blocks).
- Per-patch metadata: name, description, dateAdded, favorite, content-hash.
- Content-hash deduplication with amber `[D]` marker + "Rm Dupes" action.
- Sort by name / date-added / description.
- Multi-select removal.
- Audition prev/next (◀ ▶ or arrow keys; 150 ms debounce).
- ~~Send Bank~~ — removed 2026-07-06 (sent the whole library, not a sequenced set of
  intended slots as advertised); dropped from the roadmap entirely 2026-07-14, no
  redesign planned — see ROADMAP.md P3 (Out of Scope).

### Randomizer (RND pane)
- **Nudge** — ±8 % perturbation of continuous params only.
- **Randomize** — amount-scaled interpolation toward random target for continuous params; amount-probability change for discrete params; mod matrix can be re-rolled independently.
- Per-section toggles (VCO1, VCO2, VCF, FM/LAG, LFO, ENV, TRACK, RAMP, MOD) default off.
- **SAFE mode** — applies musical safety: VCO1 waveform non-zero, open VCF, VCO2 musical-interval tuning, and an ENV1→VCA1 routing when MOD is enabled so results stay audible.
- One-level Revert (snapshots the pre-roll patch).

### MIDI CC Automation (CC pane — standalone only)
- 128-entry CC map (`MidiEngine::ccMap_`); each CC can target any Xpander param.
- Incoming CC from non-synth inputs is scaled to the param range and broadcast as a hardware-parameter event — never echoed to the synth.
- Binary toggle special case: cc value > 63 → 1, else 0.
- Persisted via `juce::PropertiesFile` key `"ccMap"`.
- Hidden in AU/VST3 builds (`#if JucePlugin_Build_Standalone`) — DAWs supply CC routing in plugin contexts.

### Tone Morphing (MORPH pane)
- Snapshot the current patch as **A** when the pane opens; load a library patch as **B**.
- 0–100 slider linearly interpolates continuous params (A→B); discrete params take A's value; mod matrix bytes always copied from A.
- Live preview to scratchpad slot 99 on every slider tick; UNDO re-applies A, APPLY confirms.

### Plugin behaviour
- AU + VST3 host their own editor window (same `EditorTabComponent` UI).
- Session restore: `getStateInformation` saves the 399-byte SysEx cache (base64) plus the displayed slot as 99; `setStateInformation` restores both, calls `broadcastCachedPatch()` to repopulate the UI, and schedules 4 retry timers (2/5/10/20 s) to push the patch to hardware once MIDI output is open.

---

## 4. Hardware Protocol Summary

### SysEx Messages

| Message | Format |
|---|---|
| Patch request | `F0 10 [id] 00 00 [prog] F7` |
| Patch dump (send/receive) | `F0 10 [id] 01 00 [prog] [392 packed bytes] F7` — 399 bytes total |
| Page select | `F0 10 [id] 0B [page] [mode] F7` |
| Parameter change | `F0 10 [id] 0A 00 [paramCol] 00 00 00 [val_lo] [val_hi] F7` |
| Add mod source | `F0 10 [id] 0F 00 [destID] [srcID] [idSource] F7` |
| Set sign | `F0 10 [id] 07 00 [destID] [srcID] [idSource] [sign] F7` |
| Set unsigned value | `F0 10 [id] 03 00 [destID] [srcID] [idSource] [amount] F7` |
| Hardware nav (+/−) | `F0 10 [id] 0E 04/08 F7` — fired by the front-panel buttons; **not** a standard MIDI program change |

### The `[id]` byte is a MIDI unit/device number, **not** a model code

`[id]` is the Oberheim MIDI unit (device) number. It is **not** a model discriminator:
both a Matrix-12 **and** an Xpander were captured transmitting `[id] = 0x02` (2026-07-05)
— Matrix-12 `F0 10 02 0A 00 18 00 01 00 00 00 F7`, Xpander
`F0 10 02 0A 00 18 00 7E 01 00 00 F7`. Both models default to `0x02`, so the byte cannot
tell them apart.

> **Note:** an earlier theory held that `0x02` = Xpander / `0x04` = Matrix-12 as a
> firmware-fixed model code, inferred from the upstream C# app (which hardcodes `0x02`)
> and from a report where a Matrix-12 ignored `0x02`-stamped output — but that report's
> real cause was a fresh-install `SysexID` of `0`, not a model mismatch. The direct
> hardware capture above disproves the model-code theory. There is no known way to
> distinguish the two models from the SysEx stream today.

XpandrLink learns the value automatically: `MidiEngine::handleIncomingMidiMessage` adopts
`data[1]` from the first incoming Oberheim SysEx and persists it (`SysexID` setting). The
value is not surfaced in the UI (it carried no useful information — see above); the MIDI
pane instead shows a **SYNTH** presence LED that lights once any port sends Oberheim SysEx.
The receive path does **not** filter by ID (see ROADMAP MIDI-1), so with two synths
connected at once the last one to talk wins the outgoing ID.

### Encoding

Each raw byte is split into two SysEx bytes:
- Encode: `[v & 0x7F, (v >> 7) & 0x01]`
- Decode: `((hi & 1) << 7) | (lo & 0x7F)`

The 196-byte raw patch encodes into 392 SysEx bytes; with F0/header/F7 the on-wire dump is 399 bytes.

### Page Addresses

| Page | Hex |
|---|---|
| VCO1 / VCO2 | `0x20` / `0x21` |
| VCF | `0x22` |
| FM / LAG | `0x23` |
| ENV1..ENV5 | `0x28..0x2C` |
| LFO1..LFO5 | `0x30..0x34` |
| TRACK1..TRACK3 | `0x38..0x3A` |
| RAMP1..RAMP4 | `0x40..0x43` |

There is **no standalone VCA page** — VCA1 / VCA2 volumes are columns on `PAGE_VCF`.

### Timing Constants (in `OberheimDefs.h`)

| Constant | Value | When it applies |
|---|---|---|
| `kButtonPageSettleMs` | 150 ms | After page-select before any subpage-1 (button) param SysEx |
| `kSliderPageSettleMs` | 100 ms | After page-select before any subpage-0 (slider) param SysEx |
| `kModRoutingStepMs` | 50 ms | Between `ADDSOURCE` / `SETSIGN` / `SETUNSIGNEDVALUE` |
| `kProgramChangeToDumpMs` | 100 ms | Between PC and the patch-dump request |
| Bank send inter-patch | 150 ms | Per C# `DELAY_BETWEEN_ALL_DATA_DUMP_SEND_SINGLE_PATCH` |

### Modes

`MODE` byte in page-select: `0` = slider (subpage 0), `1` = button (subpage 1).

### Mod Matrix Storage (in raw patch bytes 128..187)

20 slots × 3 bytes each. Per-slot validity test (matches C# `XpanderTone.ModulationMatrix.cs`):

- Active iff `src ≤ 26` AND `dst ≤ 46` AND `amount ≠ 0`.
- Unused slots encode `src = 0x1F`, `dst = 0x3F` — correctly rejected by the validity test.
- Amount byte: bits 0–5 = magnitude, bit 6 = sign, bit 7 = quantize flag.

### Scratchpad Slot

All library/editor loads redirect the dump's program byte AND the activating program-change to `Oberheim::kScratchpadProgram = 99` so other hardware memory slots are never overwritten. Mutation is on a local copy only; `lastPatchSysexCache` and `savePatchToFile` retain the original program number.

---

## 5. Architecture

### Pattern

Listener-based event architecture with strict thread safety. `MidiEngine` is the central component; UI components register as `MidiEngine::Listener` and are notified on the MIDI thread.

### Component Hierarchy

```
XpandrLinkAudioProcessor (JUCE AudioProcessor; both standalone and AU/VST3 paths)
  └── MainComponent
        ├── MidiEngine        ← declared FIRST, constructed first, destroyed LAST
        │     ├── juce::AudioDeviceManager
        │     ├── juce::MidiOutput
        │     ├── ccMap_[128]
        │     ├── std::vector<Listener*> (recursive juce::CriticalSection)
        │     └── send queue (juce::Timer @ 5 ms tick, message-thread only)
        │
        └── EditorTabComponent  ← declared SECOND, destroyed FIRST
              ├── Title bar (XPLORER wordmark + patch name/number VFD + ◀ ▶)
              ├── VoiceArchitectureComponent (3-col grid: VCO1/VCO2/FM+LAG | VCF/MOD/RAMPS | ENV/TRACK/LFO)
              ├── 5 bottom panes (Mod / PAGE2 / RND / MORPH / CC) — CC hidden in plugin builds
              └── PatchBrowserPanel (library drawer)
```

**Declaration-order invariant:** `MidiEngine` MUST be declared first in `MainComponent` so it's constructed first and destroyed last (RAII). `EditorTabComponent` is declared second so it's destroyed *before* `MidiEngine` — outstanding listener callbacks have a valid engine to deregister from.

### Threading Model

- **MIDI thread** — JUCE provides this for `MidiInputCallback`. All `MidiEngine::Listener` callbacks fire here.
- **Message thread** — owns all UI mutation. Listener implementations use `juce::Component::SafePointer<EditorTabComponent>` and `juce::MessageManager::callAsync` to hop from the MIDI thread to the message thread.
- **Send queue** — a `juce::Timer` on the message thread drains an outgoing SysEx FIFO every 5 ms, applying page-settle delays. No lock needed (single-thread access).
- **Listener list** — protected by a recursive `juce::CriticalSection`; iterated *while holding the lock* (no snapshot-then-call), so re-entrant listener removals during a callback are safe.

### Key Components

| File | Role |
|---|---|
| `Engine/MidiEngine.{h,cpp}` | All MIDI I/O, SysEx formatting, send queue, listener fan-out, CC map. Splits incoming SysEx into 5 sub-handlers (`handlePageSelect`/`handleParameterChange`/`handlePatchDump`/`handleModRouting`/`handleProgramNav`). |
| `Plugin/XpandrLinkAudioProcessor.{h,cpp}` | JUCE plugin wrapper. `setStateInformation` populates the cached patch synchronously *before* the `callAsync`, eliminating an editor-open-vs-cache-populated race. |
| `Tabs/EditorTabComponent.{h,cpp}` | Main editor UI. Implements `MidiEngine::Listener`. Builds a `paramsByPage_` map at construction for O(~10) hardware→UI dispatch. Currently 1134 lines (decomposition tracked in [ROADMAP.md](ROADMAP.md)). |
| `Tabs/LibraryTabComponent.{h,cpp}` | Legacy file-browser tab. Most library UX moved to `PatchBrowserPanel` (drawer). |
| `Data/XpanderData.h` | The `XpanderParams[]` catalog — 226+ entries with `id`, `page`, `paramCol`, `name`, `group`, `min`, `max`, `choices`, `isButton`, `isBitmask`, `isRadioLED`. `inline const` to avoid ODR violations. |
| `Data/SynthDefs.h` + `Engine/OberheimDefs.h` | MIDI page/paramCol constants + timing/mode constants. |
| `Data/PatchCodec.h` | Pure-C++ `namespace PatchCodec { decode, encode }` — 196 raw bytes ↔ `{paramID → value}` map. No JUCE deps; unit-testable. |
| `Data/PatchRandomizer.h` | Pure-C++ randomizer logic — `nudge`, `randomize`, `randomizeModBytes`, `applyMusicalSafety`. |
| `Data/PatchMorpher.h` | Pure-C++ linear-interpolation morph between two param maps. |
| `Data/PatchLibrary.{h,cpp}` | SQLite-backed catalog. `isPatch399Block()` shared helper; `content_hash` column populated at import time (O(N) refresh after first migration). `isOpen()` lets callers detect DB-open failure. |
| `UI/HardwareComponents.h` | `HardwareKnob`, `VfdDropdown`, `VfdScrollList`, `WaveformButton`, `NumericEntry`, `VfdPopupList`. 838 lines (split tracked in ROADMAP). |
| `UI/ModulePanel.h`, `UI/GroupTabPanel.h`, `UI/FmLagPanel.h`, `UI/RampRatesPanel.h` | Per-module parameter panels. `ModulePanel` auto-creates the right widget based on the `XpanderParam` struct. |
| `UI/VoiceArchitectureComponent.h` | 3-column grid. Builds a `paramPanelMap_` at construction for O(1) `updateParameter`/`getParameterValue` dispatch (was 8-panel fan-out). |
| `UI/AdvancedParamsPanel.h` | PAGE 2 content (ENV/RAMP/LFO mode flags + KB Tracking). |
| `UI/ModSummaryPanel.h`, `UI/FullModMatrixPanel.h` | Mod matrix UI (compact vs full views). |
| `UI/RandomizerPanel.h`, `UI/MorphPanel.h`, `UI/CcMapPanel.h` | RND / MORPH / CC bottom panes. |
| `UI/PatchBrowserPanel.h` | Library drawer (multi-select, sort, dedup, send-bank). 809 lines (split tracked in ROADMAP). |
| `UI/ThemeData.h` | All colours (hardware VFD theme). `vfdGhost` (dark) for `~` backing segments; `vfdDim` (brighter) for off-state text inside a VFD box. |
| `UI/OberheimLookAndFeel.h`, `UI/HardwareButtonLookAndFeel.h` | Custom JUCE LookAndFeels. |
| `Tests/*.h` | 3 `juce::UnitTestRunner` tests (KNOB_COL decode, program-change SysEx, IDSource tracking). Currently only compiled under `XPLORER_RUN_TESTS=1` in standalone — wiring into CMake tracked in ROADMAP. |

---

## 6. Critical Implementation Invariants

These are non-obvious rules that have caused bugs in the past; future changes must preserve them.

### Hardware-Update Guard (no MIDI feedback loop)

When hardware sends a parameter change, the UI must update without echoing the new value back to the synth. `MidiEngine::hardwareUpdateMode` is the guard:

```cpp
void EditorTabComponent::onParameterChangedFromHardware(...) {
    MessageManager::callAsync([...] {
        safeThis->midiEngine.setHardwareUpdateMode(true);
        // update UI control values…
        safeThis->midiEngine.setHardwareUpdateMode(false);
    });
}
```

`sendParameterToSynth` returns immediately when `hardwareUpdateMode` is true, so the UI's `onValueChange` callback can't echo.

### Subpage paramCol Collision

Hardware uses the same `paramCol` byte for different parameters on subpage 0 (sliders) vs subpage 1 (buttons). Example: `col=0x08` on subpage 0 = `FM_AMP`; on subpage 1 = `LAG_LIN_EXP`. The `onParameterChangedFromHardware` callback carries an `isButton` flag (`currentRxSubPage == 1`); dispatch must match BOTH `page+paramCol` AND `def.isButton == isButton`.

### Hardware Front-Panel Nav

The Matrix-12's front-panel `+`/`−` buttons send `F0 10 [id] 0E 04/08 F7`, **not** a standard MIDI program change. The `cmd == 0x0E` handler increments/decrements `currentProgram` and requests a dump. Standard MIDI PC is also handled (for DAW-driven changes) — both paths are intentional.

### Knob-Col Detection

Hardware rotary-knob (delta) messages use `paramCol ≥ 0x18`. Decode: subtract `0x10`, read signed delta from `data[6]/data[7]`, pass `isDelta = true` through the callback. `EditorTabComponent` applies as a clamped offset. (LFO retrig at col `0x1A` on LFO pages / subpage 1 is special-cased and does NOT subtract.)

### Hardware Knob → No Page Select

The Matrix-12 emits rotary-knob param changes (cmd `0x0A`, KNOB_COL) **without** a preceding page-select unless the user pressed a page button. When `currentRxPage == -1`, fall back to `lastSentPage` (the last page XpandrLink sent). After the first editor parameter interaction, `lastSentPage` is set and hardware knob turns route correctly. A `[ParamChange DROPPED]` log appears only for true cold start.

### Mod-Dest LED Reconciliation

`VoiceArchitectureComponent::updateModSummary` must NOT union the freshly-decoded routed set with the previous patch's set — that bleeds stale LEDs across patch loads. Use `ModAssignmentLogic::reconcileWithDecodedSet()`: the decoded set REPLACES the active set; only optimistic inserts younger than `kOptimisticRouteWindowMs` (2500 ms) merge back. `addOptimisticRoute()` / `removeRoute()` timestamp pending inserts so in-flight UI assignments survive a stale dump.

### Listener Lifecycle

`MidiEngine::Listener` holds a private `juce::WeakReference<MidiEngine> midiEngineRef_`. `addListener` sets it (with `jassert` on double-registration in Debug); `removeListener` clears it; `~Listener()` auto-removes from the engine if still registered. Subclasses are safe to destroy without explicitly calling `removeListener()`.

### Typeface Cache

`XpandrLinkAudioProcessor`'s destructor calls `juce::Typeface::clearTypefaceCache()`. Without it, the CoreText typeface cache is destroyed during `__cxa_finalize` after the ObjC runtime has torn down, causing `std::terminate()` on app close.

### Binary-Data Symbol Names

`juce_add_binary_data` strips the hyphen from `DSEG14Classic-BoldItalic.ttf` → `BinaryData::DSEG14ClassicBoldItalic_ttf` (same as Projucer; **not** hyphen→underscore). Do not assume underscore — it will break the build.

---

## 7. Settings & Persistence

| What | Where |
|---|---|
| MIDI device selection, SysEx ID, CC map, displayed slot # | `juce::PropertiesFile` — `~/Library/Application Support/XpandrLink/XpandrLink.settings` (macOS) / `%APPDATA%\XpandrLink\XpandrLink.settings` (Windows) |
| Patch library | SQLite — `~/Library/Application Support/XpandrLink/PatchLibrary.db` (macOS) / `%APPDATA%\XpandrLink\PatchLibrary.db` (Windows) |
| Library root folder | Library DB `settings` table, key `libraryRoot` (default `~/Documents/OberheimPatches/Library`) |
| Plugin session state | Host-supplied via `getStateInformation` / `setStateInformation` — 399-byte SysEx cache, base64-encoded |

---

## 8. Build System

CMake (JUCE 8 `juce_add_plugin` API). `CMakeLists.txt` at the repo root is the source of truth. JUCE is expected as a sibling directory (`../JUCE`). Build artefacts land in `build/<plat>/XpandrLink_artefacts/<Config>/`.

### macOS prerequisites
- Xcode (for the compiler and SDK)
- CMake 3.22+ (`brew install cmake`)
- JUCE 8 cloned as a sibling: `git clone https://github.com/juce-framework/JUCE ../JUCE`

### Windows prerequisites
- **C++ compiler**: [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) (scroll to "Tools for Visual Studio") — install the **"Desktop development with C++"** workload. Visual Studio Community 2022 also works.
- **CMake 3.22+**: `winget install Kitware.CMake` or download from cmake.org
- **JUCE 8** cloned as a sibling: `git clone https://github.com/juce-framework/JUCE ..\JUCE`

Directory layout expected on both platforms:
```
<parent>/
  JUCE/           ← JUCE repo
  XpandrLink/     ← this repo
```

```bash
# macOS — configure once, then build
cmake -B build/mac -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build/mac --config Debug                                  # all targets
cmake --build build/mac --config Debug --target XpandrLink_Standalone   # one target
cmake --build build/mac --config Release

# Validate the AU
auval -v aufx XpLk RpAu

# Windows — run in Developer PowerShell or Developer Command Prompt
cmake -B build/win -G "Visual Studio 17 2022" -A x64
cmake --build build/win --config Debug --target XpandrLink_Standalone   # fastest first build
cmake --build build/win --config Release
```

To add or remove source files, edit the `target_sources(XpandrLink …)` list in `CMakeLists.txt`. Header-only files don't need to be listed. Binary resources are declared in `juce_add_binary_data(XpandrLinkBinaryData …)`.

CMake replaces the legacy Projucer flow entirely. `XplorerMac.jucer`, `Builds/MacOSX/XplorerMac.xcodeproj`, and `post_projucer_patch.sh` are archived (kept, not deleted, not the active path). The CMake config encodes every fix from `post_projucer_patch.sh` directly:
- `CMAKE_OSX_DEPLOYMENT_TARGET=13.0` (set before `project()`)
- `HARDENED_RUNTIME_ENABLED TRUE`
- `JUCE_VST3_CAN_REPLACE_VST2=0`
- `MICROPHONE_PERMISSION_ENABLED TRUE`
- Icon embedding (juceaide generates `.icns` / `.ico` from `ICON_BIG` / `ICON_SMALL`)
- `AU_MAIN_TYPE kAudioUnitType_Effect` (the JUCE CMake default of `aumf` would break Ableton)

---

## 9. References

| Resource | Purpose |
|---|---|
| [github.com/xplorer2716/XplorerEditor](https://github.com/xplorer2716/XplorerEditor) | Upstream C# project — original protocol source |
| [github.com/xplorer2716/OberheimXpanderMidiSpec](https://github.com/xplorer2716/OberheimXpanderMidiSpec) | Oberheim SysEx specification — also vendored at [`Reference/`](Reference/) for archival purposes |
| [juce.com](https://juce.com/) | JUCE framework |
| `XpandrLink-User-Guide.md` | End-user feature guide |
| `DESIGN.md` | UI/visual spec (hardware VFD theme tokens) |

### C# Reference Quick-Index

Links point at the upstream `XplorerEditor` repo (`Xplorer/` subdirectory), not a local checkout.

| File | Use for |
|---|---|
| [`Common/XpanderConstants.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Common/XpanderConstants.cs) | Flag bitmask values (`VCOWAVEFLAG_*`, `ENVMODE_*`, `RAMPF_*`, `MODFLAG_*`), `EnumPages`, source/dest enums |
| [`Model/Tone/XPanderSinglePatch.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Model/Tone/XPanderSinglePatch.cs) | Binary layout (`FromStream` / `ToStream`), 399-byte packet length |
| [`Model/Tone/XpanderTone.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Model/Tone/XpanderTone.cs) | `FromSinglePatch` — authoritative field→parameter mapping |
| [`Model/Tone/XpanderTone.ModulationMatrix.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Model/Tone/XpanderTone.ModulationMatrix.cs) | Mod matrix field layout, `GetNextAvailableModIDSourceForDest` |
| [`Controller/XpanderController.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/XpanderController.cs) | `SendProgramDumpRequestToSynth`, `LoadTone`, `SaveTone` |
| [`Controller/XpanderController.ModulationMatrix.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/XpanderController.ModulationMatrix.cs) | Mod matrix SysEx send/receive |
| [`Controller/XpanderController.MIDIEvents.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/XpanderController.MIDIEvents.cs) | CC scaling formula |
| [`Controller/SinglePatchIterator.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/SinglePatchIterator.cs) | `IsSinglePatch` — 399-byte validation |
| [`Controller/SysexFileType.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/SysexFileType.cs) | Single tone vs AllDataDump classification |
