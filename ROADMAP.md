# XpandrLink — Roadmap

Forward-looking work only. Closed work is recorded in commit history. For the product itself, see [SPEC.md](SPEC.md).

**Last reconciled:** 2026-07-06, branch `main`.

> **Release gate (v1.0.0 / XpandrLink launch):** ✅ **PASSED.** [`docs/TEST-PLAN.md`](docs/TEST-PLAN.md)
> Sessions **A** (Matrix-12 hardware) and **C** (morphing) both confirmed clean, 2026-07-06.
> Sessions B (Xpander per-feature
> coverage), D (DAW CC automation), and E (Windows VST3-in-DAW) remain open and may stagger
> post-launch, with the README marking Windows VST3 as untested-in-hosts until E closes.

---

## Priority Tiers

- **P0 — Validation debt.** Code has shipped; needs to be confirmed against real hardware / real DAW / real Windows host before being trusted.
- **P1 — Maintainability.** Existing code works but is approaching unmaintainable.
- **P2 — Feature gaps.** C# parity items the port hasn't picked up yet.
- **P3 — Nice-to-have.** Low value or out of scope today.

---

## P0 — Validation Debt

These items have committed implementations but no hardware/visual confirmation. Some are hardware-affecting; if they regress they corrupt user state silently.

| Item | Risk | What needs to happen |
|---|---|---|
| **TASK-08 — Full bank send** | High (writes 100 slots) — **known broken, feature disabled** | `PatchBrowserPanel::doSendBank()` sent the *entire* library to hardware rather than a sequenced set of intended slots — not what the button promised. Removed the "Send Bank" button and its wiring (2026-07-06) rather than ship a confusing, blast-radius-high control; not critical for launch. `MidiEngine::sendPatchBytesToSlot`/`sendAllNotesOff` are left in place for a future reimplementation. Needs a redesign (explicit slot-range selection, dry-run/confirm-per-patch, or scoping to a user-chosen subset) before re-adding a UI entry point. |
| **TASK-09 — CC automation table** | Medium (UI never tested in DAW) — TEST-PLAN Session D, deferred post-launch | Standalone: map a few CCs to params; drive from a controller; verify UI updates and no echo to synth. AU/VST3: confirm CC pane is hidden and host CC routing reaches params instead. |
| **XPANDER-1 — Full smoke test against Xpander** | Medium (all validation so far is Matrix-12 only) — TEST-PLAN Session B, deferred post-launch | ~~Connect an Xpander~~ ✅ Validated (2026-07-03) on both macOS and Windows: parameter changes, mod matrix, and hardware-initiated changes all worked as expected. Not every function was exercised — remaining gap is exhaustive per-feature coverage (bank send, morph, randomizer, librarian) specifically on Xpander hardware. |
| **WIN-1 — Windows 10/11 build and smoke test** | Low (core paths + both synth types confirmed; only DAW/VST3 host untested) — TEST-PLAN Session E, deferred post-launch | ✅ Standalone build, MIDI I/O, patch load, param edits confirmed on Windows 11 against **both Matrix-12 and Xpander** (2026-07-03; see XPANDER-1 row). ✅ VST3 build-without-admin-rights fix confirmed compiling. Still open: install the built VST3 and load it in a Windows DAW (Ableton/Reaper); confirm `%APPDATA%\XpandrLink\` path in that context. |
| **Master-command buttons (nav bar)** | ✅ Hardware-validated (macOS) | **MUTE** (`0B 1B 00`) confirmed working — silences held notes and switches the synth to the Master/MIDI page. **Tune All** confirmed working — sends the MIDI Tune Request `F0 F6 F7` and triggers a full (multi-minute) VCO/VCF autocalibration; gated behind a Load/Cancel confirm dialog since it's long-running. The page-edit (`0A`) soft-button approach was tried and failed (a received page-select navigates the Tune page display but the `0A` press is ignored for Master-section command buttons), and **Tune VCOs was removed** (no remote MIDI command for VCO-only tune). Remaining: broader visual smoke (button order `… CC · TUNE ALL · MIDI · MUTE`, no-latch behavior, Init Load/Cancel dialog) and the same validation on Windows. |

**Closed this cycle** (fixed, hardware-validated, folded into commit history): **TEST-PLAN Sessions A + C passed clean, 2026-07-06 — release gate cleared.** Closed: **TASK-07** (randomizer stuck notes — VCF/ENV/RAMP scopes all hardware-confirmed no stuck notes across repeated rolls); **BUG-32** (scratchpad slot 99 redirect, incl. save-to-file preserving original slot); **BUG-31** (mod-dest LED bleed); **BUG-33** (LFO Sample S&H selector); **TASK-01**/**TASK-02** (PAGE 2 visual checks); **TASK-05** (patch# click-to-load, incl. BUG-36 nav-jump regression check); **TASK-10** (tone morphing musical result + Undo). Also previously closed: patch-number display desync on load; **BUG-36** — prev/next nav jump after number-entry load. See [`docs/TEST-PLAN.md`](docs/TEST-PLAN.md) Sessions A/C for the checked-off procedures.

**Remaining P0 items are all deferred-to-post-launch per the release gate** (TEST-PLAN Sessions B/D/E) or a disabled feature (TASK-08) — none block v1.0.0.

---

## P1 — Maintainability

### ~~TASK-13a — Split header-only implementation files~~ ✅ DONE (2026-06-16)

All five files split into `.h` + `.cpp` pairs.

| File | Header before | Header after | New `.cpp` |
|---|---:|---:|---:|
| `HardwareComponents` | 838 | 194 | 722 |
| `PatchBrowserPanel`  | 809 |  95 | 720 |
| `ModSummaryPanel`    | 504 | 100 | 410 |
| `FullModMatrixPanel` | 468 |  80 | 390 |
| `ModulePanel`        | 575 |  75 | 480 |

### ~~TASK-13b — Decompose `EditorTabComponent.cpp`~~ ✅ DONE (2026-06-19)

Was 1134 lines → now ~817 lines.

- **Step 1 — TitleBarComponent**: `TitleBarComponent.h` + `.cpp`. Push-in/callback-out: `setPatchName`, `setProgramNumber`, `flashMidiLed`; callbacks `onPrevClicked`, `onNextClicked`, `onLibraryClicked`, `onPatchNumberEdited`. `MidiActivityIndicator` moved here.
- **Step 2 — BottomPaneManager**: `BottomPaneManager.h` (header-only). Owns pane open/close state, nav-button layout, per-pane `onOpen` callbacks. `#if JucePlugin_Build_Standalone` blocks reduced from 11 → 6.
- **Step 3 — PatchOrchestrator**: deferred/optional. Would extract the encode/cache/broadcast/send pattern from randomizer + morph + library load. Not started.

### ~~Wire unit tests into CMake~~ ✅ DONE (2026-06-28)

`XpandrLink_Tests` console target added to `CMakeLists.txt`. Runs BUG01, BUG05, and IDSourceTracking tests via `juce::UnitTestRunner`. Registered with `add_test()` so `ctest` picks it up.

```bash
cmake --build build/mac --target XpandrLink_Tests
ctest --test-dir build/mac -R XpandrLinkTests -V
```

### ~~Windows build verification~~ ✅ Mostly done (2026-07-02–2026-07-03)

Standalone builds and runs on Windows 11, hardware-validated against both synths; VST3 compiles without admin rights. The only remaining gap is loading the built VST3 in a Windows DAW — tracked as the open tail of **WIN-1** in the P0 table above (TEST-PLAN Session E).

---

## P2 — Feature Gaps vs C# Original

| Item | Source | Sketch |
|---|---|---|
| **Synth-type flag (Xpander vs Matrix-12)** | `AllUsersSettings.SynthTypeIsMatrix12` | In the C# original this is a manual setting (the `rdMatrix12` radio button in [`MidiPage.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/View/Settings/MidiPage.cs); stored as a plain boolean in [`AllUsersSettings.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/Service/Settings/AllUsersSettings.cs)), never derived from SysEx. Traced every use in [`XpanderController.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/XpanderController.cs): it selects one differing byte in two outbound SysEx commands — (1) writing text to the synth's own front-panel display (`SendGreetingsToSynth`/`SendTypeWriterMessageToSynth`, command byte `0x05` Xpander / `0x06` Matrix-12), and (2) the all-data-dump (full memory backup) request (`0x00` Xpander / `0x01` Matrix-12). **XpandrLink doesn't implement either feature yet** — no display-message writer, and the librarian only imports all-data-dump *files* from disk rather than requesting a live dump — so the missing flag has no functional impact today. It would need adding (as a manual toggle, same as the original — see [SPEC.md §4](SPEC.md#4-hardware-protocol-summary) for why it can't be auto-detected from `sysexID`) only if either feature is ported. |
| **All-notes-off before single-patch load** | C# `XpanderController.LoadTone` | `midiOutput->sendMessageNow(MidiMessage::allNotesOff(ch))` before `sendPatchToSynth`. Bank send already does this (`sendAllNotesOff()`); single-patch load doesn't. |
| **Extract bank → individual `.syx` files** | `ExtractSingleToneForm.cs` | Inverse of `importBankFile`. Useful when a user wants to share a single patch from an imported bank. |
| **Store to hardware slot** — High priority, no equivalent today | [`XpanderController.StoreSinglePatchToSynth`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/XpanderController.cs) | XplorerEditor's Patch → Store lets the user commit the currently-edited/loaded patch permanently to any real program slot (0–99): dump the patch bytes stamped with the target slot, then send `F0 10 [id] 07 [programNumber] F7`. XpandrLink has no equivalent — every load/preview is redirected to scratchpad slot 99 (by design, see Hardware Patch Safety in FEATURES.md), so there is currently **no way to permanently save an edited patch back onto the synth** short of using the synth's own front panel. Note: SysEx cmd `0x07` is already used elsewhere in this project's protocol table for mod-matrix "Set Sign" (`F0 10 [id] 07 00 [destID] [srcID] [idSource] [sign] F7`) — the two are presumably disambiguated by message length (6 bytes vs 8), but confirm against hardware before implementing. |
| **All Data Dump — Backup & Restore (whole memory)** — High priority, no equivalent today | [`AllDataDumpRequestState.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/AllDataDumpRequestState.cs), [architecture-dynamic.md §4](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/docs/architecture-dynamic.md) | Requests the synth's entire memory (100 single + 100 multi patches) in one pass with a progress bar, writes it to one file; restore replays every SysEx message back to the synth with a 150ms gap between patches, then resyncs the current patch. This is the real feature the removed "Send Bank" was reaching for but implemented wrong (it sent the whole *imported library*, not a synth-requested full-memory dump) — nothing currently fills this gap. Multi (split/layer) patches ride along as opaque blobs here; XplorerEditor itself never edits multis, only preserves them during backup/restore, so there's no separate "multi editing" gap to track. |
| **Get All Single Patches from synth → folder** — Medium priority | `FileOperationsManager.GetSinglePatchesFromSynth` | Iterates all 100 program slots, requesting and saving each as its own `.syx` file. Not implemented. |

### Multi-instance / multi-synth safety (no guardrails today)

Goal: support multiple Matrix-12 / Xpander units on different MIDI ports or device IDs, and avoid two app instances (e.g. standalone + plugin) stepping on each other when pointed at the same port.

Current state (as of 2026-06-21):
- **Outbound is already per-instance.** Each `MidiEngine` picks its own port and stamps the device-ID byte with `sysexID` (`MidiEngine.cpp` TX paths). Two synths on **different ports** with **different IDs** already work for sending.
- **Inbound is NOT filtered by device ID.** `handleIncomingMidiMessage` only checks `data[0] == 0x10` (Oberheim manufacturer); `data[1]` (device ID) is never compared to `sysexID`. So two synths sharing one port cross-talk on receive — instance A absorbs the other synth's dumps.
- **The ID byte is a MIDI unit number, not a model code** (corrected 2026-07-05; see SPEC §4). A Matrix-12 was captured sending `[id] = 0x02`, same as the Xpander default, so it cannot distinguish the models and both units commonly share `0x02`. With one app instance and both synths connected: auto-detect flip-flops `sysexID` to whichever synth spoke last (outgoing edits reach only that synth), both synths' dumps/param changes merge into the one editor, program changes from either synth trigger dump requests, and the non-designated synth's port is treated as a *controller* — its notes/CC/PC are MIDI-thru-forwarded to the output, risking a feedback loop if the output chain reaches it. Supported setup until MIDI-1/2 land: **one synth per app instance, separate ports**, configured one at a time (standalone instances share one settings file — last writer wins).
- **No MIDI channel filtering.** Program changes are hardcoded to channel 1 (`MidiEngine.cpp:695, 710`); inbound PCs aren't channel-filtered.
- **No cross-process conflict detection.** Standalone uses its own `AudioDeviceManager`; the plugin shares the host's. On macOS CoreMIDI both can open the same source/destination. Same port + same `sysexID` → duplicate outbound sends and both UIs reacting to the same dump. Nothing warns.

| Item | Risk | Sketch |
|---|---|---|
| **MIDI-1 — Inbound device-ID filter** | Medium — **hardware-affecting; lands as P0 validation debt once implemented** | In `handleIncomingMidiMessage`, drop SysEx where `data[1] != sysexID`. This partitions synths that have been given **distinct unit IDs**; it does **not** partition by model (a Matrix-12 and Xpander both default to `0x02` — SPEC §4), so the user must first set different unit numbers on each synth. Two units sharing an ID still can't share a port. Interacts with auto-detect: once an ID is learned, the filter must not block re-detection after the user swaps synths — define that rule when implementing. Must be tested against real hardware (verify nothing legitimate is dropped) before being trusted. |
| **MIDI-2 — Same-port/same-ID conflict detection** | Low (warning only) | Warn when the selected input/output + `sysexID` is already claimed by another instance. Needs a lightweight cross-process registry (standalone and plugin are separate processes), e.g. a lockfile under App Support. Most involved of the three. |
| **MIDI-3 — Per-instance MIDI channel** | Low | Replace the hardcoded channel 1 in the program-change paths with a per-instance setting. Minor. |

Recommended order: MIDI-1 first (highest value, lowest risk), then MIDI-2, then MIDI-3. MIDI-1 is independent of the other two.

---

## P3 — Out of Scope / Low Priority

- **Zone management (Matrix-12 only).** `XpanderConstants.cs` defines ZONE page addresses (`0x50–0x65`); even the C# original doesn't implement editing.
- **CC Learn mode.** Alternative UX for TASK-09 — click "Learn" on any knob, move a physical controller, capture. Lower priority than the explicit CC map.

---

## Process Notes

- All P0 items are validation, not coding. They should be batched into one hardware session, not interleaved with feature work.
- P1 items (header split + EditorTabComponent decomposition + tests) are a single PR's worth of architectural cleanup. Do them together so re-validation is one cycle.
- P2 items are independent — pick up individually as needed.
- Roadmap moves an item from open to closed when (a) committed AND (b) validated (for P0) or merged (for others). Closed items go to commit history; this file is not an archive.
