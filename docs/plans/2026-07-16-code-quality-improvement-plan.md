# XpandrLink code-quality / architecture improvement plan

**Status (2026-07-16)**: Independently reviewed (see "Proposed plan" section below, revised in place with
that review's findings — the `activeOutput` locking invariant, Phase 2.5 split out, Phase 3 reframed
around TSan-replay value with explicit kill criteria, Phase 5 deferred indefinitely). **Phases 0-2 are
done** — JUCE pinned (`SPEC.md`), `docs/adr/` + `docs/NFR.md` written, CI hardened
(`.github/workflows/build.yml`: Windows build, `auval`, clang-tidy, ASan). **Phase 2.5 (characterization
tests) is done** (2026-07-16, this session) — see the Phase 2.5 section below for what's covered, what
isn't, and the one real bug it caught. **Phase 3 (`IMidiBackend` abstraction) coding is done; hardware
validation is partial** (2026-07-16, same session, after explicit user go-ahead) — see the Phase 3
section below for the exact (deliberately narrowed) scope, what was built, and the current validation
status (Standalone confirmed, AU/VST3 not yet tried — this refactor is specifically about output-backend
thread-affinity, which the plugin hosting model can exercise differently).
See `ROADMAP.md`'s "Code-quality / architecture improvement plan" entry for the running summary.

## Context

XpandrLink is a JUCE (C++) MIDI editor for Oberheim Xpander/Matrix-12 hardware, built over ~2 months
across many sessions, with heavy live-hardware validation (several genuine hardware-lockup-class bugs
found and fixed only via real synth testing — mod-matrix corruption, stuck notes, MIDI flood lockups).
It's a fork/alternative of an existing C# app (XplorerEditor). The original author of that app has,
independently and in parallel, started his own formal JUCE port (EARS requirements, ADRs, TDD, CI on
every push). A comparative audit this session found his process/tooling more rigorous in several
respects; his actual code quality (independently reviewed) is genuinely good — not spec-theater. We
want to close specific, verified gaps rather than chase parity for its own sake.

**Critical constraint that must shape sequencing**: `MidiEngine` is the most hardware-critical class in
the project. Its correctness has been earned through repeated real-hardware testing, not just logic
review — several past "fixes" that looked correct in code/tests still caused real hardware lockups or
corruption and were only caught by live testing. Any refactor touching `MidiEngine`'s message-send or
message-receive paths carries real regression risk to already-hardware-validated behavior. The project's
own standing rule: "MIDI protocol work is only confirmed working when tested against real
hardware, not just by reading the code."

## Verified current-state facts (not assumptions)

- `MidiEngine.h:312` owns `std::unique_ptr<juce::MidiOutput>` directly; class inherits
  `juce::MidiInputCallback` directly. No interface/abstraction between logic and JUCE I/O — can't unit
  test message handling with a mock backend.
- `MidiEngine.cpp` is 1203 lines, `EditorTabComponent.cpp` is 1084 lines — largest files in the project,
  doing many unrelated responsibilities each.
- Send-queue pacing uses `juce::Timer::timerCallback()` (5ms poll), not an event-driven worker thread.
- JUCE version is not pinned anywhere — `CMakeLists.txt` does `add_subdirectory(../JUCE ...)` against
  whatever's on disk, no tag/commit recorded.
- No ADR-equivalent documents exist anywhere in the repo (`find . -iname "*ADR*"` = empty).
- CI (just added this session) is macOS-only: Standalone + Tests build + ctest. No Windows CI, no
  `auval` check, no sanitizers, no coverage.
- 9 test files, ~62 assertions, all testing pure/extracted logic (e.g. `decodeSysexValue`,
  `buildChangeSourceMessage`, `shouldCoalesceModAmount`), none exercising `handleIncomingMidiMessage`
  end-to-end.

## Proposed plan (phased by risk, cheapest/safest first)

### Phase 0 — JUCE version pin (near-zero risk)
Document the exact currently-working JUCE version/commit in `SPEC.md`. Optionally switch
`add_subdirectory(../JUCE ...)` to `FetchContent` pinned to that tag, matching the sibling-directory
convention or replacing it — TBD, sibling-directory may be intentional for local dev iteration speed.
**Validation**: clean build succeeds, existing tests pass. No hardware involvement.

### Phase 1 — Structured documentation (zero code risk, pure documentation)
Three sub-parts, all zero-risk, no code touched:
1. **ADR folder** — `docs/adr/`, ~10-15 short decision records for load-bearing choices already made and
   proven in production: scratchpad-slot-99 hardware-safety model, engine-state atomics, Timer-based send
   pacing + coalescing throttle, hardware-update-guard pattern, subpage paramCol collision handling,
   CHANGESOURCE atomic mod-edit, the `activeOutput` asymmetric-locking invariant (see Phase 3 note below —
   this is a real existing invariant that deserves its own ADR regardless of whether Phase 3 proceeds).
   These are retroactive ADRs documenting *already-validated* decisions, not new decisions.
2. **Non-functional requirements doc** — `docs/NFR.md` (or fold into `SPEC.md`), formalizing what's
   currently scattered prose across code comments and docs: the
   60ms/150ms SysEx timing constraints, thread-safety guarantees (which fields are atomic, which are
   lock-guarded, which are message-thread-only by convention), and the hardware-safety invariants
   (scratchpad redirection, hardware-update guard, subpage collision matching). Give each a short stable
   ID (e.g. `NFR-TIMING-01`) so it can be cited from code comments and tests.
3. **Lightweight requirement-to-test traceability** — not a full EARS system, but enough to answer "is X
   actually verified" without re-deriving it from memory. For each existing test file, add a one-line
   header comment naming which NFR/behavior it verifies (cross-referencing the IDs from part 2 above where
   applicable). This directly closes the scope-mapping gap already identified this session — the goal is
   answering "what's covered" by reading a list, not by re-deriving it from session history.

### Phase 2 — CI hardening (low risk, additive only)
Add Windows CI build (mirror existing macOS job), add `auval -v aufx XpLk RpAu` as a CI step (macOS
only), add static analysis (clang-tidy, run as a non-blocking informational job initially — this project
has never run a linter, so an initial pass will likely surface a backlog of pre-existing findings that
need triage before it can be made blocking). Add ASan/TSan as a **gating** CI job (not optional/non-
blocking — per independent review, this project's specific, repeated history of subtle concurrency bugs
means sanitizer coverage should be treated as required, not nice-to-have).
**Validation**: CI jobs must actually pass on real runners (not just locally) before being called done.

### Phase 2.5 — Characterization tests ✅ DONE (2026-07-16)
Before any refactor: write `MockMidiBackend`-based(-precursor) tests that capture current `MidiEngine`
output for known inputs (page-select, param change, mod-matrix ops, patch dump) as a byte-for-byte
regression oracle. This has standalone value independent of whether Phase 3 proceeds — it's the first
real test coverage of the actual message-handling path rather than just extracted helper functions.

**What was actually done**: rather than a full `MockMidiBackend` (that's Phase 3's interface-injection
work), added one trivial pass-through method, `MidiEngine::processIncomingMessageForTest(source, message)`,
that just forwards to the existing private `handleIncomingMidiMessage` — matching the class's own
established "public for unit testing" convention already used for `decodeSysexValue`,
`buildChangeSourceMessage`, etc. This let a new test file,
`XpandrLink/Source/Tests/MidiEngineCharacterizationTest.h`, drive the *real* receive-path dispatch
end-to-end (not a hand-copied mirror of the decode rules, which is what one pre-existing test,
`BUG05_ProgramChangeSysexTest.h`, had been doing) and assert on `MidiEngine::Listener` callbacks. Covers:
page-select → parameter-change context resolution (button and rotary-delta cases, `NFR-HW-07`/`08`), the
LFO-retrig subtract exception, patch-dump decode (`NFR-HW-01`-adjacent — this is the first test to
actually call `handlePatchDump`, not just `loadPatchFromMemory`), mod-matrix edit destination resolution
from live page-select state (`NFR-HW-03`, `NFR-HW-05`), front-panel program nav and MIDI Program Change
reconciliation (`NFR-HW-09`), sysexID auto-learn (`NFR-HW-10`), and CC-automation interception. Four new
NFR IDs (`NFR-HW-07`-`10`) added to `docs/NFR.md` to close a gap Phase 1 left open — these were already
known to be load-bearing invariants, but they'd never been given stable IDs.

**Deliberately not covered** (documented in the test file's own header, not a silent gap): the
synth-input-auto-detect and MIDI-thru-forward branches both depend on `source->getName()` for a real,
named `juce::MidiInput`, which this seam (`source` may be `nullptr`) doesn't attempt to fabricate. Also not
covered: the mod-matrix echo-suppression window (`lastModSentTime_`) and any outgoing byte-for-byte
send-path capture — both need a live `Timer`/message-loop pump or an actual output seam, which is exactly
what Phase 3's `IMidiBackend` is for, not this precursor.

**One real bug found and fixed in passing**: this was the first test ever to exercise
`MidiEngine::handlePatchDump` directly, and it immediately hit a `jassert` in `juce_String.cpp` — a raw
UTF-8 arrow (`→`) embedded in an 8-bit C string literal at `MidiEngine.cpp:786` (`logMsg += " → PATCH OK
..."`), the same recurring bug class as BUG-29 (sessions 52/58, VFD display glyphs) just never previously
hit because no prior test reached this code path. Fixed to a plain ASCII `->`. This is exactly the kind of
finding Phase 2.5 was justified by — not "clean architecture," but catching a real defect the first time
the actual dispatch path got exercised outside live hardware.

**Verification**: `XpandrLink_Tests` — all 40 test groups pass, exit code 0, no assertions. Clean Debug
builds of Standalone/AU/VST3 (production change is a one-character string-literal fix plus one
zero-logic pass-through method — no send-path or locking changes). `auval -v aufx XpLk RpAu` passes.

### Phase 3 — MIDI I/O abstraction (HIGH RISK — reframed per independent review) ✅ DONE (2026-07-17)

**Scoped to output only, with explicit user sign-off before implementation**, given the stakes. The
interface does **not** cover the
"register receive callback" part of this section's original wording: Phase 2.5 already made the receive
path directly testable via `MidiEngine::processIncomingMessageForTest()`, and input device management
goes through `juce::AudioDeviceManager`'s own multi-device callback system — a separate, much larger
subsystem abstracting it would touch, for no remaining testability gap. The concrete motivating scenario
(burst-drag mod-amount flood) is a send-path concern anyway. Answers this section's own open question 1
("should Phase 3 be scoped down?") — yes, to output-only.

**What was built**: `IMidiBackend` interface + `JuceMidiBackend` production implementation
(`XpandrLink/Source/Engine/IMidiBackend.h`, new, header-only) wrapping exactly the `juce::MidiOutput` calls
`MidiEngine` used to make directly — `open(name)`/`close()`/`isOpen()`/`getName()`/`getIdentifier()`/
`send(message)`. `JuceMidiBackend::send()` remains a direct, synchronous `sendMessageNow` with no
buffering, matching the hard requirement above. `MidiEngine`'s `std::unique_ptr<juce::MidiOutput>
activeOutput` field became `std::unique_ptr<IMidiBackend> midiOutputBackend_`, constructor-injected
(defaulting to `JuceMidiBackend`, so the sole production construction site needed zero changes) — sole
production path, no dual-path/feature-flag, exactly as required. All ~6 call sites
(`setMidiOutput`/`getCurrentMidiOutputName`/`sendSysex`/`sendTuneAll`/`sendProgramChange`/
`sendAllNotesOff`/the MIDI-thread thru-forward path) were converted 1:1, preserving the exact same
lock/no-lock shape at each site — verified line-by-line per the hard requirement above; full reasoning
and the updated invariant description now live in [ADR-007](../adr/007-activeoutput-locking-invariant.md).
New test-only `MockMidiBackend` (`XpandrLink/Source/Tests/MockMidiBackend.h`) plus a new
`drainSendQueueForTest()` pass-through seam on `MidiEngine` (matches the existing "public for unit
testing" convention — `TestMain.cpp` runs no live `Timer`/`MessageManager` loop, so the real 5ms-poll
drain never fires during unit tests without this) enabled `XpandrLink/Source/Tests/
MidiEngineSendPathTest.h`: covers `sendAllNotesOff`/`sendTuneAll`/`sendProgramChange` (immediate,
unqueued sends), `sendParameterToSynth` (queued page-select + param byte sequence), `sendPatchToSynth`
(scratchpad-slot redirect, `NFR-HW-01`), and — the concrete motivating case — the burst-drag mod-amount
coalescing scenario: many rapid `changeModulationAmount()` calls with no sleep between them, followed by
a real sleep past the throttle window, asserting only ONE coalesced flush ever reaches the mock and it
carries the LAST value, not an intermediate one. This extends (doesn't replace) `ModAmountCoalesceTest.h`,
which only unit-tests the pure `shouldCoalesceModAmount` predicate.

**A real timing subtlety found while writing this test**: `Oberheim::kModCmdGapMs` (50ms) is reused as
*both* the page-select settle delay after a mod-matrix send *and* the coalescing throttle window, so an
initial test design that tried to assert "nothing sent yet, mid-burst" at a precise wall-clock offset was
inherently racy against real scheduling jitter (draining the first send's own queued messages takes
about as long as the coalescing window itself). Redesigned around a more robust invariant instead of a
precise timing window: fire the whole burst back-to-back with no sleep at all (matching how fast a real
drag actually is), then let the window fully pass once at the end and assert total-sends-stayed-at-2 and
last-value-won — this is the actual safety property the throttle exists for, and doesn't depend on
catching an exact intermediate window.

**TSan CI job added** (`.github/workflows/build.yml`, new `tsan` job) — there was no TSan job at all
before this phase; Phase 2's own validation criteria required one. Mirrors the existing `sanitizers`
(ASan) job's structure. **Local TSan attempt**: building `XpandrLink_Tests` with `-fsanitize=thread`
succeeds, but running it SEGFAULTs immediately with zero output — confirmed via the same methodology
used for the earlier ASan hang (a trivial two-line hello-world program built with identical flags
SEGFAULTs identically), conclusively ruling out an XpandrLink code bug. Same toolchain/OS-version
mismatch class of issue as the ASan hang (this dev machine: macOS 26.5, Xcode-bundled clang 17.0.0), just
a crash instead of a hang this time — `macos-14` is a different, stable, released combination this local
result says nothing definitive about. **Got a real green run on `macos-14` on the first PR (#29,
https://github.com/jbboz/XplorerMac/actions/runs/29528760147)**, so flipped from `continue-on-error: true`
to gating the same day, per the policy stated when the job was added.

**Verification**: all of Phase 2.5's existing tests still pass unchanged (confirms the send-path refactor
didn't disturb receive-side behavior). New `MidiEngineSendPathTest.h` — all 7 cases pass. Full suite:
86 test groups, exit code 0, no assertions. Clean Debug builds: Standalone, AU, VST3. `auval -v aufx XpLk
RpAu` passes.

**Not done / explicitly deferred**: input-side abstraction (see scope note above — not needed).

**Update 2026-07-16, same day — hardware validation partial, NOT closed.** User confirmed "testing
complete, working as expected" after hardware-testing PR #29 — but then flagged that this testing was
against the **Standalone build only**; AU (and VST3) were never built/loaded into a DAW during that pass.
This matters specifically for this phase: the whole point of the `IMidiBackend` refactor was preserving
`ADR-007`'s thread-affinity/locking discipline, and `MidiEngine` runs inside a host's own threading model
in AU/VST3 rather than Standalone's — the same source code, but not necessarily the same runtime
guarantees a plugin host provides around callback timing/threads. Validation criteria item 4 (full manual
hardware re-validation) is **not yet satisfied**. The P0 validation-debt row on ROADMAP.md was reopened
(previously closed in error) — narrowed to "AU/VST3 still need a real DAW-hosted hardware pass." Built a
fresh Debug AU (`cmake --build build/mac --target XpandrLink_AU --config Debug`) for the user to load
into a DAW next.

**Update 2026-07-17 — AU hardware-tested, Phase 3 fully closed.** User confirmed "testing passed on both
standalone and AU." Validation criteria item 4 is now satisfied for both formats sharing the `MidiEngine`/
`IMidiBackend` code path (VST3 not separately tried, same code path). ROADMAP.md's P0 row closed and
folded into "Closed this cycle." Phase 3 is done in full — coding, ASan/TSan CI gating, and hardware
validation across both formats that matter.

---

**Original phase text below (superseded in the "what was built" summary above, kept for the reasoning
that shaped it):**
**Reframed justification**: the original rationale ("unit-test message handling with a mock") is weak on
its own — a byte-recording mock would have caught *none* of this project's actual historical bugs (MIDI
flood lockup, mod-matrix slot corruption, stuck notes), which are hardware-timing/state bugs, not
encoding bugs. The real payoff is enabling **TSan-instrumented replay of recorded traffic** (e.g. the
2026-07-13 burst-drag flood scenario) through a deterministic harness — that's the justification, not
"clean architecture" for its own sake.

**Non-negotiable hard invariant, discovered by independent review, that must survive this refactor
unchanged**: `activeOutput` is currently accessed under *asymmetric* locking — `sendSysex`/
`sendProgramChange`/`sendAllNotesOff` touch it on the message thread *without* `listenerLock` (relying on
"only the message thread mutates it"), while the MIDI-thread thru-forward path *does* lock, and
`setMidiOutput` mutates under lock. An abstraction that changes ownership/lifetime of the output, or adds
any indirection, can silently break this exact discipline and pass every unit test while doing so — this
is precisely this project's documented failure signature (subtle race under concurrent port-change +
thru-forward, invisible to static review). The lock boundary and thread-affinity of every `activeOutput`
access must be explicitly re-verified line-by-line after the refactor, not assumed preserved.
`JuceMidiBackend::send` must remain a synchronous `sendMessageNow` with no buffering — the existing
pacing design assumes it blocks.

Define `IMidiBackend` interface covering exactly the JUCE calls `MidiEngine` currently makes directly
(open/close input+output by name, send message, register receive callback). Implement `JuceMidiBackend`
wrapping current behavior 1:1 — **as the sole production path, no dual-path/feature-flag** (a runtime
branch inside hardware-critical code is more risk, not less). Implement `MockMidiBackend` for tests.
Inject the backend into `MidiEngine` (constructor injection). Do NOT change any SysEx timing, pacing, or
protocol logic in this phase — pure extract-interface refactor, behavior must be byte-for-byte identical.

**Validation (non-negotiable given the stakes)**:
1. Phase 2.5's characterization tests must pass byte-for-byte after refactor.
2. Full existing test suite must still pass.
3. TSan-gated CI job (Phase 2) must pass against the new abstraction, including a replayed burst-drag/
   rapid mod-amount scenario (the `pendingModAmount_` coalescing path — the actual historical repro
   condition), not just the steady-state cases.
4. **Full manual hardware re-validation pass required before this is considered done** — no exceptions.
   Re-run TEST-PLAN Sessions A/B/C/F/G at minimum (parameter sync, mod-matrix edit, randomizer, display
   banner, mod-matrix live-edit) **plus explicit rapid-drag/burst mod-amount testing** (not just listed
   under Phase 5 — the drain path is touched here too).
5. **Explicit kill criteria, decided up front**: any stuck note, any dropped or duplicated SysEx message,
   any observable pacing change → revert immediately, do not attempt to patch forward.

### Phase 4 — File decomposition (MEDIUM RISK)
Only after Phase 3 lands and is hardware-validated. Split `MidiEngine.cpp` further by responsibility
(e.g. extract send-queue/pacing into its own class, now easier post-Phase-3 since the backend is already
separated). Split `EditorTabComponent.cpp` by the same TASK-13b precedent already used for
`TitleBarComponent`/`BottomPaneManager`. Mechanical, low logic-risk, but touches hot files — full build +
existing test suite must pass; no hardware re-validation strictly required if no logic changes, but a
smoke-test hardware session is still recommended given this project's track record of "looked safe,
wasn't."

### Phase 5 — Worker-thread modernization — **DEFERRED INDEFINITELY per independent review**
Originally proposed: replace `Timer`-based send-queue draining with `std::jthread` + `stop_token` +
`condition_variable`. Rejected for now: this rewrites the exact code path implicated in the mod-matrix-
flood hardware lockup saga, for a modernization nicety with no concrete problem currently driving it.
Revisit only if a specific, concrete issue with the Timer-based approach actually surfaces.

### Phase 6 — Code coverage tracking (low risk, tooling only)
Add coverage instrumentation (e.g. llvm-cov) to the test CI job, report percentage. No source changes.

### Phase 7 — Structured bug-report facility (G16, already on ROADMAP, low risk, additive)
New, self-contained feature: on unhandled exception, bundle MIDI device state + log snapshot into one
reportable artifact. No changes to existing hardware-critical paths.

## Open questions for the reviewer

1. Is Phase 3 (MIDI abstraction) actually worth the regression risk given the codebase's specific,
   documented history of "looked correct, broke hardware anyway"? Or should it be scoped down/skipped?
2. Is the phase ordering right, or should something be resequenced?
3. Is anything missing that a senior engineer would flag before greenlighting this plan?
4. Is the "full manual hardware re-validation after Phase 3" requirement sufficient, or does it need to
   be even more conservative (e.g. feature-flagged rollout, keep the old direct-JUCE path alongside the
   new abstraction for a full release cycle before removing it)?
