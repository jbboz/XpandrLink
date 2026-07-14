# XpandrLink — Roadmap

Forward-looking work only. Closed work is recorded in commit history. For the product itself, see [SPEC.md](SPEC.md).

**Last reconciled:** 2026-07-14.

> **Release gate (v1.0.0 / XpandrLink launch):** ✅ **PASSED.** [`docs/TEST-PLAN.md`](docs/TEST-PLAN.md)
> Sessions **A** (Matrix-12 hardware), **B** (Xpander per-feature coverage), **C** (morphing),
> **D** (DAW CC automation), and **F** (G1/G2/G9 parity gaps) have all passed clean. Session
> **E** (Windows VST3-in-DAW) remains open and may stagger post-launch, with the README marking
> Windows VST3 as untested-in-hosts until it closes.

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
| **WIN-1 — Windows 10/11 build and smoke test** | Low (core paths + both synth types confirmed; only DAW/VST3 host untested) — TEST-PLAN Session E, deferred post-launch | ✅ Standalone build, MIDI I/O, patch load, param edits confirmed on Windows 11 against **both Matrix-12 and Xpander**. ✅ VST3 build-without-admin-rights fix confirmed compiling. Still open: install the built VST3 and load it in a Windows DAW (Ableton/Reaper); confirm `%APPDATA%\XpandrLink\` path in that context. |
| **Master-command buttons (nav bar)** | ✅ Hardware-validated (macOS) | **MUTE** (`0B 1B 00`) confirmed working — silences held notes and switches the synth to the Master/MIDI page. **Tune All** confirmed working — sends the MIDI Tune Request `F0 F6 F7` and triggers a full (multi-minute) VCO/VCF autocalibration; gated behind a Load/Cancel confirm dialog since it's long-running. The page-edit (`0A`) soft-button approach was tried and failed (a received page-select navigates the Tune page display but the `0A` press is ignored for Master-section command buttons), and **Tune VCOs was removed** (no remote MIDI command for VCO-only tune). Remaining: broader visual smoke (button order `… CC · TUNE ALL · MIDI · MUTE`, no-latch behavior, Init Load/Cancel dialog) and the same validation on Windows. |
| **Randomizer MOD scope produced an inaudible patch (routing itself is fine; four causes found, all fixed)** | Medium (musical correctness, not hardware-affecting) | MOD-only randomize no longer locks up hardware but could still, in earlier passes, produce a silent patch even after manually removing the auto-generated mod routings. Four independent causes were found and fixed one at a time in the same safety pass — an ENV1-audibility floor that only fired on an all-zero envelope; other mod-matrix entries able to land on and cancel the amplitude-safety destinations; a partial floor replaced with the C# reference's own full-archetype-overwrite approach (`PatchRandomizer.h`: `VcaEnvStyle`/`vcaEnvArchetype()`); and finally a missing VCO1/VCF audibility floor specific to MOD-only rolls, ported from the VCO1/VCF section-gating fix. TDD regression coverage added at every step (`PatchRandomizerSafetyTest.h`). Unvalidated on real hardware — needs a MOD-only randomize retest, holding a note through several rolls, starting from a patch with the oscillator muted or filter closed to specifically re-exercise the last cause. This is fix attempt 4 for this symptom — if it still reproduces, the next step is a hardware experiment (a minimal static-VCA1, no-modulation test patch, to confirm directly whether that alone stays audible on this unit) rather than a fifth logic guess. |

**Closed this cycle** (fixed, hardware-validated, folded into commit history): **TEST-PLAN Sessions A, B, C, D, and F all passed clean — release gate cleared.** Closed: **TASK-07** (randomizer stuck notes — VCF/ENV/RAMP scopes all hardware-confirmed no stuck notes across repeated rolls); **BUG-32** (scratchpad slot 99 redirect, incl. save-to-file preserving original slot); **BUG-31** (mod-dest LED bleed); **BUG-33** (LFO Sample S&H selector); **TASK-01**/**TASK-02** (PAGE 2 visual checks); **TASK-05** (patch# click-to-load, incl. BUG-36 nav-jump regression check); **TASK-10** (tone morphing musical result + Undo); patch-number display desync on load; **BUG-36** — prev/next nav jump after number-entry load. **Store to hardware slot**: implemented, UI confirm/reject flow fixed, hardware-validated ("Store tests validated"). Also closed, all hardware-validated: **SYNTH LED lit from persisted settings, not live state** (fixed via `MidiEngine::hasSeenSynthThisSession()`); **randomizer forced VCF cutoff/VCO1 wave+volume with no sections selected** (audibility floors now scoped per-section); **randomizer MOD scope hardware lockup** (mod-matrix destinations now capped at hardware's 6-sources-per-destination limit, matching the C# reference). **Mod-matrix live-edit hardware lockup** (user-confirmed "not able to get the hardware to lock up anymore"): three fixes — atomic CHANGESOURCE replacing delete+re-add for source changes, missing `decrementIdSourceAfterRemove()` calls added to the destination-change path, and a coalescing throttle preventing fast amount-knob drags from flooding the synth's MIDI IN. **Front-panel mod-matrix edit decode** (user-confirmed "was also able to add a new routing in hardware and see it in the editor"): replaced the disproven debounce-then-redump resync with direct decode of the cmd=0x0F `ModulationEdit` SysEx applied incrementally to both mod-matrix panels. **Embedded Init Patch mod-matrix slots** (user-confirmed "init patch only has the two default mod mappings"): `Resources/InitPatch.syx`'s 18 unused mod-matrix slots were zero-filled instead of using the hardware's actual empty-slot sentinel (`src=0x1F`/`dst=0x3F`), so the synth considered all 20 slots occupied the instant Init Patch loaded and rejected any further add. **Full smoke test against Xpander** (TEST-PLAN Session B): full per-feature coverage on Xpander hardware — parameter/mod-matrix/hardware-initiated changes, tone morphing, randomizer, and librarian audition flow all validated; bank send dropped from scope entirely (see P3) so it's no longer part of this session's coverage. **Synth-type toggle**: manual Xpander/Matrix-12 flag in the MIDI pane, pure UI state with no SysEx sent — no validation debt. **TASK-09 — CC automation table**: standalone CC-map-to-parameter, physical-controller drive, and no-echo-to-synth all confirmed; AU/VST3 CC-pane-hidden and DAW-automation-lane-reaches-synth both confirmed. **Hardware display banner**: text display, uppercasing, truncation, and re-display all confirmed correct on both synth models; the one open checklist item (a dedicated "OFF" display state) was found not to be a real thing to test — switching functions on the synth resets the display back to normal on its own. See [`docs/TEST-PLAN.md`](docs/TEST-PLAN.md) for the checked-off procedures.

**Remaining P0 items are all deferred-to-post-launch per the release gate** (TEST-PLAN Session E) or genuinely unvalidated new work (randomizer MOD-audibility) — none block v1.0.0.

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
| ~~**Synth-type flag (Xpander vs Matrix-12)**~~ ✅ DONE | `AllUsersSettings.SynthTypeIsMatrix12` | In the C# original this is a manual setting (the `rdMatrix12` radio button in [`MidiPage.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/View/Settings/MidiPage.cs); stored as a plain boolean in [`AllUsersSettings.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/Service/Settings/AllUsersSettings.cs)), never derived from SysEx. Traced every use in [`XpanderController.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/XpanderController.cs): it selects one differing byte in two outbound SysEx commands — (1) writing text to the synth's own front-panel display (`SendGreetingsToSynth`/`SendTypeWriterMessageToSynth`, command byte `0x05` Xpander / `0x06` Matrix-12), and (2) the all-data-dump (full memory backup) request (`0x00` Xpander / `0x01` Matrix-12). Implemented as `MidiEngine::setSynthTypeIsMatrix12`/`isSynthTypeMatrix12` (persisted) with a MODEL toggle in `MidiSettingsPanel` — feeds the display banner below. No validation debt: pure UI state, no SysEx sent. |
| **All-notes-off before single-patch load** | C# `XpanderController.LoadTone` | `midiOutput->sendMessageNow(MidiMessage::allNotesOff(ch))` before `sendPatchToSynth`. Bank send already does this (`sendAllNotesOff()`); single-patch load doesn't. |
| **Extract bank → individual `.syx` files** | `ExtractSingleToneForm.cs` | Inverse of `importBankFile`. Useful when a user wants to share a single patch from an imported bank. |
| ~~**Hardware display banner**~~ ✅ DONE, hardware-validated (both Xpander and Matrix-12) | `XpanderController` `DISPLAY_MESSAGE`; `SendGreetingsToSynth`/`SendTypeWriterMessageToSynth` | Send ≤80 uppercase-ASCII chars to the synth's front-panel display (optional typewriter scroll). Implemented as `MidiEngine::sendDisplayMessage`/`sendDisplayOff`/`sendDisplayOn` plus a DISPLAY field in `MidiSettingsPanel`. |
| ~~**Store to hardware slot**~~ ✅ DONE, hardware-validated | [`XpanderController.StoreSinglePatchToSynth`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/XpanderController.cs) | XplorerEditor's Patch → Store lets the user commit the currently-edited/loaded patch permanently to any real program slot (0–99): dump the patch bytes stamped with the target slot, then send `F0 10 [id] 07 [programNumber] F7`. Implemented as `MidiEngine::storePatchToSlot(int)` plus a Store footer button in `PatchBrowserPanel` with a slot picker (0–98; 99 rejected) behind a two-stage confirm dialog naming the permanent-overwrite risk. Not a command collision with mod-matrix "Set Sign" as originally suspected — Set Sign's top-level command byte is `0x0F`, not `0x07`. Hardware-validated on both synth types. |
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
- **TASK-08 — Full bank send.** Implemented, found broken (sent the *entire* library rather than a sequenced set of intended slots — not what the button promised), and removed rather than ship a confusing, blast-radius-high control. Dropped from the roadmap entirely, not planned for a future redesign. `MidiEngine::sendPatchBytesToSlot`/`sendAllNotesOff` remain in the codebase (still used by store-to-hardware-slot) but bank send itself has no UI entry point and none is planned.

---

## Process Notes

- All P0 items are validation, not coding. They should be batched into one hardware session, not interleaved with feature work.
- P1 items (header split + EditorTabComponent decomposition + tests) are a single PR's worth of architectural cleanup. Do them together so re-validation is one cycle.
- P2 items are independent — pick up individually as needed.
- Roadmap moves an item from open to closed when (a) committed AND (b) validated (for P0) or merged (for others). Closed items go to commit history; this file is not an archive.
