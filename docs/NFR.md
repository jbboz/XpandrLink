# Non-Functional Requirements

Stable IDs for the project's load-bearing timing, thread-safety, and hardware-safety invariants —
previously scattered across scattered comments and `SPEC.md`'s timing table. This doc doesn't
replace those (`SPEC.md`'s timing table remains the source of truth for exact constant values) —
it exists so a specific requirement can be *cited* by ID from code comments and test headers,
closing the gap where "is X actually verified" required re-deriving the answer from history.

Each ID is referenced from the test file(s) that verify it where a test exists — see the header comment
in each `XpandrLink/Source/Tests/*.h` file.

## Timing

| ID | Requirement | Detail |
|---|---|---|
| `NFR-TIMING-01` | Button-page settle | 150ms after page-select before any subpage-1 (button) param SysEx. `SPEC.md` §"Timing Constants", `kButtonPageSettleMs`. |
| `NFR-TIMING-02` | Slider-page settle | 100ms after page-select before any subpage-0 (slider) param SysEx. `kSliderPageSettleMs`. |
| `NFR-TIMING-03` | Mod-routing step gap | 50ms between ADDSOURCE/SETSIGN/SETUNSIGNEDVALUE sub-commands. `kModRoutingStepMs`. Zero-gap between SETSIGN/SETUNSIGNEDVALUE pairs is deliberate — see [ADR-003](adr/003-timer-pacing-coalescing.md). |
| `NFR-TIMING-04` | Program-change-to-dump gap | 100ms between PC and patch-dump request. `kProgramChangeToDumpMs`. |
| `NFR-TIMING-05` | Mod-amount coalescing window | 50ms (`Oberheim::kModCmdGapMs`) — rapid amount-knob drags coalesce to one send per window. See [ADR-003](adr/003-timer-pacing-coalescing.md). |
| `NFR-TIMING-06` | Audition debounce | 150ms — arrow-key/◀▶ patch audition debounces before auto-loading, to avoid flooding sends while a user is rapidly stepping through a list. |

## Thread safety

| ID | Requirement | Detail |
|---|---|---|
| `NFR-THREAD-01` | Engine scalar state is atomic | `sysexID`, page/mode/program tracking, `hardwareUpdateMode`, `lastModSentTime_` are `std::atomic`. See [ADR-002](adr/002-engine-state-atomics.md). |
| `NFR-THREAD-02` | `synthInputName` lock discipline | Guarded by `listenerLock`; call sites take a per-message snapshot rather than holding the lock across a callback. See [ADR-002](adr/002-engine-state-atomics.md). |
| `NFR-THREAD-03` | Output backend asymmetric locking | Message-thread send paths (`sendSysex`, tune request, program change, all-notes-off) read `MidiEngine::midiOutputBackend_` (`IMidiBackend`, see `IMidiBackend.h` — replaced the raw `activeOutput` field in the Phase 3 code-quality-plan refactor) unlocked, relying on `setMidiOutput` being message-thread-only; the MIDI-thread thru-forward path explicitly locks. **This is a fragile, implicit invariant — see [ADR-007](adr/007-activeoutput-locking-invariant.md) before touching any code that owns or mutates `midiOutputBackend_`.** `JuceMidiBackend` itself is intentionally not internally thread-safe; it depends entirely on this call-site discipline. |
| `NFR-THREAD-04` | `MidiEngine::Listener` lifecycle | Auto-removes via `juce::WeakReference` on destruction; `addListener` jasserts on double-registration. Subclasses must not call `removeListener()` explicitly. |

## Hardware safety

| ID | Requirement | Detail |
|---|---|---|
| `NFR-HW-01` | Scratchpad-slot redirect | Every load/audition/morph/randomizer-roll dump is redirected to hardware slot 99; only the explicit, confirmed Store flow writes a real slot. See [ADR-001](adr/001-scratchpad-slot-safety.md). |
| `NFR-HW-02` | Hardware-update guard | Any UI mutation driven by an incoming hardware SysEx must be wrapped in `setHardwareUpdateMode(true)`/`(false)` to prevent echoing the change back to the synth. Guard lives on `MidiEngine`, not the tab component. See [ADR-004](adr/004-hardware-update-guard.md). |
| `NFR-HW-03` | Subpage paramCol dispatch | Hardware-initiated parameter dispatch matches `page + paramCol` AND `isButton`, not `page + paramCol` alone — the same byte means a different parameter on subpage 0 vs 1. See [ADR-005](adr/005-subpage-paramcol-collision.md). |
| `NFR-HW-04` | Mod-matrix 6-source cap | At most 6 simultaneous modulation sources may route to the same destination — enforced in `PatchRandomizer` and in live-edit validation, matching hardware firmware's fixed-size per-destination table. |
| `NFR-HW-05` | Mod-matrix source-edit atomicity | Changing a mod-matrix entry's source uses the atomic CHANGESOURCE command, not delete+re-add, to avoid corrupting a sibling entry sharing the same destination. See [ADR-006](adr/006-changesource-atomic-edit.md). |
| `NFR-HW-06` | MIDI thru safety | Thru-forwarding is only active when a synth input is explicitly designated (safe default: no forwarding), to avoid a feedback loop that can lock up hardware. |
| `NFR-HW-07` | Rotary knob delta decode | `paramCol >= 0x18` means a delta knob: subtract `0x10` from the column and read a signed delta from `data[6]`/`data[7]`, except `col=0x1A` on an LFO page (`0x30`-`0x34`) in button subpage, which is `LFO_RETRIG_MODE` and skips the subtract. |
| `NFR-HW-08` | Page/subpage fallback to last-sent | If hardware sends a parameter change with no prior page-select in this session (`currentRxPage`/`currentRxSubPage` still `-1`), fall back to `lastSentPage`/`lastSentMode` (the page XpandrLink itself last sent); if neither is known, the change is dropped rather than misattributed to page 0. |
| `NFR-HW-09` | Front-panel program nav decode | Matrix-12/Xpander `+`/`-` front-panel buttons send `F0 10 [id] 0E 04/08 F7`, not a MIDI Program Change; `currentProgram` is adjusted ±1 and clamped to `[0, 99]`. |
| `NFR-HW-10` | SysexID auto-learn | The device-ID byte (`data[1]`) of the first incoming Oberheim SysEx with a different, in-range (`0`-`15`) ID than the current `sysexID` is adopted automatically and notified once via `onSysexIDDetected`, so outgoing SysEx isn't silently ignored by a mismatched device. |
