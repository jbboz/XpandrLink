# XpandrLink — Roadmap

Forward-looking work only. Closed work is recorded in commit history. For the product itself, see [SPEC.md](SPEC.md).

**Last reconciled:** 2026-07-25, branch `main`.

> **Release gate (v1.0.0 / XpandrLink launch):** ✅ **PASSED.** [`docs/TEST-PLAN.md`](docs/TEST-PLAN.md)
> Sessions **A** (Matrix-12 hardware), **B** (Xpander per-feature coverage), **C** (morphing),
> **D** (DAW CC automation), **E** (Windows VST3-in-DAW), and **F** (G1/G2/G9 parity gaps) have
> all passed clean.

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
| **Master-command buttons (nav bar)** | ✅ Hardware-validated (macOS) | **MUTE** (`0B 1B 00`) confirmed working — silences held notes and switches the synth to the Master/MIDI page. **Tune All** confirmed working — sends the MIDI Tune Request `F0 F6 F7` and triggers a full (multi-minute) VCO/VCF autocalibration; gated behind a Load/Cancel confirm dialog since it's long-running. The page-edit (`0A`) soft-button approach was tried and failed (a received page-select navigates the Tune page display but the `0A` press is ignored for Master-section command buttons), and **Tune VCOs was removed** (no remote MIDI command for VCO-only tune). Remaining: broader visual smoke (button order `… CC · TUNE ALL · MIDI · MUTE`, no-latch behavior, Init Load/Cancel dialog) and the same validation on Windows. |

**Closed this cycle:** everything else that was P0 validation debt through 2026-07-17 — the TASK-07/BUG-31/32/33/36, TASK-01/02/05/09/10 hardware checks, G1/G2 store-and-display validation, the mod-matrix live-edit lockup and front-panel edit-decode fixes, the Init Patch mod-matrix sentinel fix, the Xpander/Windows smoke tests (Sessions B/D/E/F), the randomizer MOD-scope audibility fix, and the `IMidiBackend` (Phase 3) AU/VST3 validation — is fixed, hardware-confirmed, and closed. Full narrative detail for each is in git history (search commit messages for the item name/ID) and [`docs/TEST-PLAN.md`](docs/TEST-PLAN.md) Sessions A–I, which has the checked-off procedures.

Master-command buttons is already hardware-validated on macOS — what's left there is minor visual polish and a Windows repeat, not a functional gap. No other open P0 items remain.

**Timbre Space (feature/timbre-space)** — closed 2026-07-22, user-confirmed hardware-validated ("I tested the TS function and its operating as expected", see `docs/TEST-PLAN.md` Session K). The 2-D PCA patch-blending map (scratchpad 99, 300 ms throttle, mod matrix + discrete params always from the dominant patch) works correctly on real hardware. The self-contained-layout decision this closure unblocked is now recorded in [ADR-008](docs/adr/008-timbre-space-self-contained-layout.md).

---

## P1 — Maintainability

**No open P1 items remain (2026-07-25).** Every item this section enumerates — TASK-13a, TASK-13b
(all three steps, including the previously-deferred `PatchOrchestrator`), the CMake/`ctest` test
wiring, Windows build verification, and all four Phase 4 file-decomposition candidates — is done.
The only not-yet-started pieces of the original 8-phase code-quality plan are Phase 5 (worker-thread
modernization, deferred indefinitely per independent review — no concrete problem currently driving
it) and Phase 6 (code-coverage tooling, low priority) — see
[`docs/plans/2026-07-16-code-quality-improvement-plan.md`](docs/plans/2026-07-16-code-quality-improvement-plan.md)
for both; neither has been promoted to a tracked ROADMAP item. `EditorTabComponent.cpp` remains one
of the largest files in the tree (barely shrank in the Phase 4 pass — see below) and is the most
likely candidate for a future P1 item if one gets opened.

### Code-quality / architecture improvement plan (2026-07-16) — ✅ DONE

Phased plan drafted after a comparative audit against `xplorer2716/XplorerEditor`'s independent JUCE
port, then independently reviewed/reframed by a separate senior-engineer pass before any
hardware-critical code was touched. **Full detail — context, verified-fact baseline, all 8 phases
including Phase 3's exact hard invariants and kill criteria — lives in
[`docs/plans/2026-07-16-code-quality-improvement-plan.md`](docs/plans/2026-07-16-code-quality-improvement-plan.md);
this is a summary only.**

| Phase | Status | Summary |
|---|---|---|
| 0–2 | ✅ Done | JUCE pin, [`docs/adr/`](docs/adr/README.md) + [`docs/NFR.md`](docs/NFR.md), CI hardening ([`.github/workflows/build.yml`](.github/workflows/build.yml)). |
| 2.5 — Characterization tests | ✅ Done (2026-07-16) | `MidiEngineCharacterizationTest.h` drives `MidiEngine`'s real receive-path dispatch end-to-end via one new zero-logic test seam (`processIncomingMessageForTest`). Caught and fixed one real pre-existing bug in passing (a raw non-ASCII arrow in a log string, `MidiEngine.cpp:786`, same bug class as BUG-29). Four new NFR IDs (`NFR-HW-07`–`10`). |
| 3 — `IMidiBackend` abstraction | ✅ Done, hardware-validated 2026-07-17 | Output-side only — `activeOutput` replaced with an injected `IMidiBackend`/`JuceMidiBackend` ([`IMidiBackend.h`](XpandrLink/Source/Engine/IMidiBackend.h)), preserving ADR-007's locking discipline. New `MidiEngineSendPathTest.h` covers the send path via `MockMidiBackend`, including the mod-amount coalescing scenario that motivated the phase. Added a gating TSan CI job (green on GitHub's `macos-14` runner). User-confirmed working on Standalone and AU (VST3 shares the same code path, not separately tried). |
| 4 — File decomposition | ✅ Done (2026-07-24), hardware-validated | Extracted `MidiSendQueue` from `MidiEngine` ([PR #3](https://github.com/jbboz/XpandrLink/pull/3)); split `PatchLibrary.cpp` (754→628 lines) → `PatchSysexFile.h/.cpp`; split `PatchBrowserPanel.cpp` (808→454 lines) → `PatchBrowserPanelActions.cpp`; deleted `HardwareComponents.cpp` (735 lines) → six per-class `.h`/`.cpp` pairs. `EditorTabComponent.cpp` barely shrank (1197→~1144 lines, ~4%) — its real payoff was testability via `PatchOrchestrator` (TASK-13b step 3 below), not line count. Remains one of the largest files in the tree and the top candidate for further decomposition. |

**ASan hang** — root-caused as environmental, not a code bug (2026-07-16). `XpandrLink_Tests` built with
`-fsanitize=address` spun at ~100% CPU inside ASan's own init while probing the dyld shared cache; a
trivial hello-world program repro'd the identical hang, ruling out an XpandrLink bug. Cause: this dev
machine's macOS 26.5 + Xcode-bundled clang 17.0.0 ASan runtime doesn't yet handle this OS's dyld
shared-cache format; no code fix applies. The `sanitizers` CI job now runs green on GitHub's `macos-14`
runner and is gating.

### ~~TASK-13a — Split header-only implementation files~~ ✅ DONE (2026-06-16)

All five files split into `.h` + `.cpp` pairs. Present on `features-backlog`.

| File | Header before | Header after | New `.cpp` |
|---|---:|---:|---:|
| `HardwareComponents` | 838 | 194 | 722 |
| `PatchBrowserPanel`  | 809 |  95 | 720 |
| `ModSummaryPanel`    | 504 | 100 | 410 |
| `FullModMatrixPanel` | 468 |  80 | 390 |
| `ModulePanel`        | 575 |  75 | 480 |

### ~~TASK-13b — Decompose `EditorTabComponent.cpp`~~ ✅ DONE (present on `features-backlog`)

Was 1134 lines → now ~817 lines.

- **Step 1 — TitleBarComponent**: `TitleBarComponent.h` + `.cpp`. Push-in/callback-out: `setPatchName`, `setProgramNumber`, `flashMidiLed`; callbacks `onPrevClicked`, `onNextClicked`, `onLibraryClicked`, `onPatchNumberEdited`. `MidiActivityIndicator` moved here.
- **Step 2 — BottomPaneManager**: `BottomPaneManager.h` (header-only). Owns pane open/close state, nav-button layout, per-pane `onOpen` callbacks. `#if JucePlugin_Build_Standalone` blocks reduced from 11 → 6.
- **Step 3 — PatchOrchestrator**: ✅ DONE (2026-07-24). Extracted the encode/cache/broadcast/send pattern shared by the randomizer, morph, and library-load one-level-undo into [`PatchOrchestrator.h`](XpandrLink/Source/Tabs/PatchOrchestrator.h)/`.cpp`. Live UI state (param map, mod bytes, patch name) is read through injected callbacks, so the class has zero `juce::Component` dependencies and is fully unit-tested directly (`XpandrLink/Source/Tests/PatchOrchestratorTest.h`), including `MockMidiBackend`-based send-path coverage confirming the TASK-07 All-Notes-Off-before-dump ordering and the BUG-32 scratchpad-slot redirect on both `applyPatch()` and `revert()`.

### ~~Wire unit tests into CMake~~ ✅ DONE (2026-06-28)

`XpandrLink_Tests` console target added to `CMakeLists.txt`. Runs BUG01, BUG05, and IDSourceTracking tests via `juce::UnitTestRunner`. Registered with `add_test()` so `ctest` picks it up.

```bash
cmake --build build/mac --target XpandrLink_Tests
ctest --test-dir build/mac -R XpandrLinkTests -V
```

### ~~Windows build verification~~ ✅ DONE

Standalone builds and runs on Windows 11, hardware-validated against both synths; VST3 compiles without admin rights (`b6be85c`) and is now built-and-loaded-in-a-DAW validated too (TEST-PLAN Session E, closed).

---

## P2 — Feature Gaps vs C# Original

Gaps below (`G#` ids) were found by tracing every menu item and controller command in the C#
original against XpandrLink. C# source is on disk at `~/Documents/Development/XplorerEditor`
(also on GitHub at `xplorer2716/XplorerEditor`) — always re-check against that live source, not
this table, before implementing any of these. **The three highest-value gaps (G1 display banner,
G2 store-to-slot, G3 backup/restore) are the ones prior audits missed.** G2 and G3-restore write
permanent hardware memory — they become P0 validation debt the moment they're coded.

Suggested order: **G9 → G1 → G2 → G3+G4 → G6 → rest.** (G9 is tiny and unblocks the correct
model-dependent command bytes that G1 and G3 need.)

| Gap | Priority / risk | Source | Sketch |
|---|---|---|---|
| ~~**G1 — Hardware display banner (text to VFD)**~~ ✅ DONE, hardware-validated (both Xpander and Matrix-12) | P2, high value, low risk | `XpanderController` `DISPLAY_MESSAGE`; `SendGreetingsToSynth` / `SendTypeWriterMessageToSynth` | Send ≤80 uppercase-ASCII chars to the synth's front-panel display (optional typewriter scroll). OFF `F0 10 [id] [cmd] 00 F7`; ON `F0 10 [id] [cmd] 02 F7`; text `F0 10 [id] [cmd] 01 <80 bytes> F7` (86 bytes total: 5-byte header + 80-byte body + F7) — the 80-byte body is **always exactly 80 bytes**, space-padded and force-uppercased, never variable length; input over 80 chars truncates. Command byte `[cmd]` = `0x05` (Xpander) / `0x06` (Matrix-12), fed by G9. Implemented: `MidiEngine::sendDisplayMessage(String, bool typewriter)` (+ `sendDisplayOff`/`sendDisplayOn`) and a DISPLAY text field + SEND button in `MidiSettingsPanel`. No greeting-on-connect (deferred — optional, matches the original's About-box trigger). |
| ~~**G2 — Store to hardware slot (permanent)**~~ ✅ DONE, hardware-validated | P2, high value, **HIGH risk** | [`XpanderController.StoreSinglePatchToSynth`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/XpanderController.cs) | The one thing XpandrLink structurally couldn't do — every load/preview is redirected to scratchpad slot 99 (Hardware Patch Safety), so there was no way to permanently save an edited patch back to the synth except via the synth's own front panel. Impl: dump the patch stamped with the target slot (`sendPatchBytesToSlot` already does the stamp), then commit `F0 10 [id] 07 [programNumber] F7` (exactly 6 bytes), then re-get to confirm. Not actually a command collision with mod-matrix **SETSIGN** (a prior version of this note claimed one) — SETSIGN's top-level command byte is `0x0F` (`F0 10 [id] 0F 00 [idSrc] 00 07 00 [val] 00 F7`; its `0x07` is a sub-byte at data position 7, not the command byte), so store's `0x07` command byte is unambiguous on its own. Implemented: `MidiEngine::storePatchToSlot(int)` + a **Store** footer button in `PatchBrowserPanel` with a slot picker (0-98; 99 rejected) and a warning `AlertWindow` naming the permanent-overwrite risk. |
| **G3 — All Data Dump: Backup & Restore (whole memory)** | P2, high value, **HIGH risk on restore** | [`AllDataDumpRequestState.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/AllDataDumpRequestState.cs), `RestoreAllDataDumpToSynth`, `ProgressForm` | Backup: request `F0 10 [id] 02 <0x00 Xpander | 0x01 Matrix-12> F7` (**model byte → G9**), collect the streamed 100 single + 100 multi patches, write to one file. Restore: replay every SysEx from the file with ~150 ms inter-patch gap, then re-sync current patch. This is what the removed "Send Bank" was reaching for but got wrong (it sent the whole imported *library*, not a synth-requested full-memory dump). Multi (split/layer) patches ride along as opaque blobs — the original never edits them, only preserves them; no separate multi-editing gap. Needs a receive-side accumulator + completion heuristic in `MidiEngine`, a `.syx` writer, a paced re-sender, and a JUCE progress bar. Backup is read-only (low risk); **restore rewrites all 200 slots — confirm-gate hard + hardware-validate.** |
| **G4 — Get all single patches from synth → folder** | P2, medium | `FileOperationsManager.GetSinglePatchesFromSynth` | Bulk-export every program slot as its own `.syx` file (loose-file bank backup). Shares G3's all-data-dump receive machinery in `SinglePatch` mode; on each patch received, write a file (and optionally auto-import into the librarian — a natural XpandrLink bonus). Read-only. Build on top of G3. |
| **G5 — Extract bank file → individual `.syx` files** | P2/P3, low | [`ExtractSingleToneForm.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/View/ExtractSingleToneForm.cs) | Inverse of `importBankFile`: write every patch in a bank file out as its own file to a folder (for sharing one patch). `PatchLibrary` already parses 399-byte blocks — add an export-to-folder variant. Local file I/O only, no protocol. |
| **G6 — All-notes-off before single-patch load** | P2, low risk | C# `SendProgramChangeAndGetSinglePatchFromSynth` (guarded by `SmartAllNotesOff`) | The original sends Smart-ANO before *any* patch load; XpandrLink only does ANO before the randomizer send. Call `sendAllNotesOff()` at the top of the shared load path, ideally behind a settings toggle mirroring `SmartAllNotesOff`. Prevents stuck notes when swapping patches while holding keys. Wire-level — hardware-validate. |
| **G7 — CC automation parity (dedicated port, 1→many, thru-forward)** | P2, medium | `XpanderController.MIDIEvents.cs` `AutomationInputDeviceChannelMessageReceived` | The original uses a *dedicated* automation input device, maps one CC to *multiple* params (auto-scaled to each range), and forwards unmapped CC/notes/PC to the synth. XpandrLink's CC map is 1→1 over the shared inputs. Decide how far to match — at minimum allow one CC→several params. Lower urgency for plugin users (the AU/VST3 automation path already covers the core case). Tie into TASK-09 CC validation. |
| **G8 — Copy Page / Paste Page** | P3, low | [`XpanderController.Clipboard.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/Controller/XpanderController.Clipboard.cs) | Copy all parameters of one page and paste onto a compatible page. Per-panel context menu + a param-block clipboard keyed by page group; re-sends pasted params through the normal send path. No protocol. |
| ~~**G9 — Synth-type flag (Xpander vs Matrix-12)**~~ ✅ DONE | P2, unblocks G1/G3 | `AllUsersSettings.SynthTypeIsMatrix12` (the `rdMatrix12` radio in [`MidiPage.cs`](https://github.com/xplorer2716/XplorerEditor/blob/main/Xplorer/View/Settings/MidiPage.cs)) | Several commands differ by model: display cmd `0x05`/`0x06` (G1) and all-data-dump request byte `0x00`/`0x01` (G3). In the C# original it's a **manual** toggle, never derived from SysEx. **Cannot** be auto-detected — a Matrix-12 was captured sending `[id]=0x02`, identical to the Xpander default (see [SPEC §4](SPEC.md#4-hardware-protocol-summary)). Implemented: `MidiEngine::setSynthTypeIsMatrix12`/`isSynthTypeMatrix12` (atomic bool, persisted as `SynthTypeIsMatrix12`), a MODEL toggle button in `MidiSettingsPanel`. No functional impact until G1/G3 consume it — feeds those next. No validation debt: pure UI state, no SysEx sent. |
| **G10 — Configurable SysEx transmit delay** | P3, low | `AllUsersSettings.SysexTransmitDelay` | The original exposes the inter-message delay as a user setting (for slower units / busy MIDI chains); XpandrLink hardcodes the pacing constants. Surface them as a setting. Low effort. |
| **G11 — MIDI Learn for CC mapping** | P2, medium — **not a C# parity item**, an original UX idea (upstream has no learn gesture; see G7) | User request, 2026-07-12 — how most other softsynths handle CC assignment, vs. XpandrLink's flat 128-row list (`CcMapPanel`) | Right-click a control (knob/dropdown/button) → engine enters a "waiting for next CC" state → next incoming CC from a non-synth input binds to that param, no dropdown-hunting required. Backend: small, self-contained addition to `MidiEngine::handleIncomingMidiMessage`'s existing CC-interception branch (a `startCcLearn(paramId)` state + capture-on-next-CC; `ccMap_`/persistence unchanged, only how an entry gets created). UI: `HardwareKnob` already stores `paramID` and has a `mouseDown()` override with no existing right-click behavior — `if (e.mods.isRightButtonDown())` there covers the common case (continuous knobs) immediately; `VfdDropdown`/`WaveformButton` have no shared base so extending to them means repeating the same small addition per class. Keep the existing flat `CcMapPanel` as the manage/audit/clear-all view rather than replacing it — add learn as a faster way to *create* a mapping, not a replacement. |

**G12–G16** surfaced 2026-07-15 by cross-checking the prior feature-parity audit above against `xplorer2716/XplorerEditor` PR #8's own EARS requirements catalog (`process/1.requirements/RQ-*.md`, 136 requirements — an independent, in-flight JUCE port of the same reference app). Every existing G-item above matched something real in that catalog with no contradictions; these five are the residual items that more exhaustive extraction caught that the prior manual menu-tree read didn't.

| Gap | Priority / risk | Source | Sketch |
|---|---|---|---|
| **G12 — Multi-patch (split/layer) dump safety** | P2, low risk but correctness-sensitive | `xplorer2716/XplorerEditor` PR #8 `RQ-CTL-026` (requires the reference controller to ignore a multi-patch dump *without corrupting the edited tone*) | Unverified in XpandrLink: does the incoming-dump handler in `MidiEngine` (`handlePatchDump`) correctly detect and discard a multi-patch (split/layer) SysEx if one ever arrives — e.g. a stray dump from front-panel activity — rather than assuming every dump is a 399-byte single patch and misdecoding it into `lastPatchSysexCache`? Distinct from G3's "multi patches ride along as opaque blobs during backup" note — this is about an *unsolicited* multi-patch dump outside the all-data-dump flow. Worth a length/type guard and a regression test; no protocol work needed if the existing length check already rejects non-399-byte blocks safely — needs verification, not necessarily new code. |
| **G13 — Drag-and-drop `.syx` onto the main window** | P2/P3, low risk, moderate value | `xplorer2716/XplorerEditor` PR #8 `RQ-GUI-029`; reference behavior in `MainForm.Overrides.cs` (`OnDragEnter`/`OnDragDrop`) | Not implemented in XpandrLink. Add `juce::FileDragAndDropTarget` to the main editor component; classify the dropped file (single patch vs bank) reusing `PatchLibrary`'s existing 399-byte-block detection; a single patch loads to scratchpad per the existing Hardware Patch Safety model, a bank file routes through the existing bank-import flow. Local UI convenience, no new protocol surface. |
| **G14 — Virtual piano-keyboard window** | P3, low | `xplorer2716/XplorerEditor` PR #8 `RQ-GUI-028` / `RQ-MID-010` | Not implemented in XpandrLink. A `juce::MidiKeyboardComponent` in a small window (opened from the nav bar or a menu), sending Note On/Off to the synth output — lets a user audition a patch without a physical MIDI keyboard attached. Self-contained, no protocol risk. |
| **G15 — Mod-matrix hover-highlight** | P3, low, polish | `xplorer2716/XplorerEditor` PR #8 `RQ-GUI-018` | Not implemented in XpandrLink. Hovering a page-family selector or knob that is a modulation source/destination highlights the matching row(s) in `ModSummaryPanel`/`FullModMatrixPanel`, using the existing LED/knob accent color. Pure UI polish, no protocol involvement. |
| **G16 — Structured bug-report/diagnostic capture** | P3, low | `xplorer2716/XplorerEditor` PR #8 `RQ-FMW-071` | Not implemented in XpandrLink (the hex SysEx log panel is the closest equivalent today). On an unhandled exception, bundle MIDI device state + a log snapshot into one reportable artifact instead of relying on the user to manually copy from the log panel. Nice-to-have, not blocking. |
| **G17 — Export current patch to an arbitrary folder** | P2/P3, low | User report, GitHub issue #7 (2026-08-04) | Every existing disk-write path is fixed-destination: Save/Save As ([`PatchBrowserPanelActions.cpp`](XpandrLink/Source/UI/PatchBrowserPanelActions.cpp) → `PatchLibrary::saveCurrentPatch`) always writes into the one DB-tracked library root, never a location the user picks per-action. Issue #7's reporter expected a `.syx` to land in a chosen PC folder after working with a patch and was confused when only the library got it. Add an **Export…** footer action (distinct from Save) that opens a `juce::FileChooser` in save mode and writes the currently-loaded patch's 399-byte SysEx (reusing `PatchSysexFile`'s buffer-build + `embedNameIntoSysex`) straight to the chosen path — no DB row, no library involvement, pure file I/O. Natural companion to G5 (bank → folder extraction), which wants the same "write single patch to arbitrary folder" primitive. |

### Multi-instance / multi-synth safety (no guardrails today)

Goal: support multiple Matrix-12 / Xpander units on different MIDI ports or device IDs, and avoid two app instances (e.g. standalone + plugin) stepping on each other when pointed at the same port.

Current state (audited 2026-06-21):
- **Outbound is already per-instance.** Each `MidiEngine` picks its own port and stamps the device-ID byte with `sysexID` (`MidiEngine.cpp` TX paths). Two synths on **different ports** with **different IDs** already work for sending.
- **Inbound is NOT filtered by device ID.** `handleIncomingMidiMessage` only checks `data[0] == 0x10` (Oberheim manufacturer); `data[1]` (device ID) is never compared to `sysexID`. So two synths sharing one port cross-talk on receive — instance A absorbs the other synth's dumps.
- **The ID byte is a MIDI unit number, not a model code** (corrected 2026-07-05; see SPEC §4 — supersedes an earlier model-code claim). A Matrix-12 was captured sending `[id] = 0x02`, same as the Xpander default, so it cannot distinguish the models and both units commonly share `0x02`. With one app instance and both synths connected: auto-detect flip-flops `sysexID` to whichever synth spoke last (outgoing edits reach only that synth), both synths' dumps/param changes merge into the one editor, program changes from either synth trigger dump requests, and the non-designated synth's port is treated as a *controller* — its notes/CC/PC are MIDI-thru-forwarded to the output, risking a feedback loop if the output chain reaches it. Supported setup until MIDI-1/2 land: **one synth per app instance, separate ports**, configured one at a time (standalone instances share one settings file — last writer wins).
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
- **NRPN / RPN support.** Neither version implements it.
- **MIDI clock / LFO tempo sync.** Out of scope.
- **VCO2 KEYB flag.** `decodePatch` overwrites param[25] with SYNC; KEYB has no storage slot. Low priority; would need protocol re-investigation.
- **CC Learn mode.** Alternative UX for TASK-09 — click "Learn" on any knob, move a physical controller, capture. Lower priority than the explicit CC map.
- **TASK-08 — Full bank send.** Implemented, found broken (`PatchBrowserPanel::doSendBank()` sent the *entire* library rather than a sequenced set of intended slots — not what the button promised), and removed (2026-07-06) rather than ship a confusing, blast-radius-high control. **Dropped from the roadmap entirely 2026-07-14** — not planned for a future redesign. `MidiEngine::sendPatchBytesToSlot`/`sendAllNotesOff` remain in the codebase (still used by `storePatchToSlot`/G2) but bank send itself has no UI entry point and none is planned.

---

## Process Notes

- All P0 items are validation, not coding. They should be batched into one hardware session, not interleaved with feature work.
- P1 is currently empty (2026-07-25) — see the note at the top of that section. If a new P1 item opens, prefer batching same-shaped mechanical cleanups together (as Phase 4's four file-decomposition tasks were) so re-validation is one cycle.
- P2 items are independent — pick up individually as needed.
- Roadmap moves an item from open to closed when (a) committed AND (b) validated (for P0) or merged (for others). Closed items go to commit history; this file is not an archive.
