# ADR-007: `activeOutput` asymmetric locking invariant

**Status**: Documented as-found 2026-07-16 (Phase 1 of the code-quality plan). **Updated 2026-07-16
(Phase 3)**: the invariant has been preserved through the `IMidiBackend` refactor — see "Post-Phase-3
shape" below. Not yet hardware-validated (see ROADMAP.md P0).

## Context

`activeOutput` (`std::unique_ptr<juce::MidiOutput>`, `MidiEngine.h:312`) is accessed from two different
threads with two different locking disciplines, verified directly against `MidiEngine.cpp`:

- **Message-thread send paths do NOT lock**: `sendSysex` (lines 224, 233), tune request (367-368),
  program change (937, 942), all-notes-off (1023-1024) all check/dereference `activeOutput` with no
  `listenerLock` held, relying on the implicit invariant "only the message thread mutates `activeOutput`,
  so message-thread reads are inherently safe from message-thread writes."
- **The MIDI-thread thru-forward path DOES lock** (line 1111, `const juce::ScopedLock sl(listenerLock)`),
  with an explanatory comment: "setMidiOutput (message thread) may reset activeOutput concurrently."
- **`setMidiOutput` mutates under lock** (line 147-148, with the comment "the MIDI thread dereferences
  activeOutput in the thru-forward path").

This is a real, deliberate-looking but *implicit* invariant: the message-thread send paths are safe only
because `setMidiOutput` — the sole mutator — is documented (by comment, not by any compiler-enforced
mechanism) to also only run on the message thread. Nothing prevents a future call site from invoking
`setMidiOutput` off the message thread, or from adding a new send path that races with it.

## Decision (as originally authored — not yet acted on)

No code change made at this ADR's authoring time. This is documented specifically because it is the
single highest-risk invariant identified for the pending MIDI I/O abstraction refactor (ROADMAP.md /
code-quality plan, Phase 3): an `IMidiBackend` abstraction that changes ownership or adds any indirection
around `activeOutput` could silently break this exact discipline, pass every unit test (the failure is a
data race under concurrent port-change + thru-forward, not a logic error), and reintroduce the precise
failure signature this project has been burned by before.

## Post-Phase-3 shape (2026-07-16)

`activeOutput` (`std::unique_ptr<juce::MidiOutput>`) has been replaced with `std::unique_ptr<IMidiBackend>
midiOutputBackend_` (`XpandrLink/Source/Engine/IMidiBackend.h`, new). **The invariant itself is
unchanged — only where the mutable state physically lives moved one level of indirection deeper.**

- `midiOutputBackend_` (the outer pointer on `MidiEngine`) is set once at construction (constructor
  injection, defaulting to `JuceMidiBackend`) and is **never reassigned afterward** — there is no more
  "outer pointer race" to reason about at all.
- The state that actually changes at runtime is `JuceMidiBackend`'s own internal `std::unique_ptr<juce::
  MidiOutput> output` member, mutated by its `open()`/`close()` methods and read by `isOpen()`/`getName()`/
  `send()`.
- Every call site that used to touch `activeOutput` directly now calls the equivalent `midiOutputBackend_
  ->...()` method, **at exactly the same call sites, under exactly the same lock/no-lock discipline as
  before**: `setMidiOutput` still mutates (`close()`+`open()`) under `listenerLock`; the MIDI-thread thru-
  forward path in `handleIncomingMidiMessage` still reads (`isOpen()`+`send()`) under the same lock; every
  message-thread send path (`sendSysex`, `sendTuneAll`, `sendProgramChange`, `sendAllNotesOff`) still reads
  unlocked, relying on the same "only the message thread mutates the backend" invariant as before.
- **`JuceMidiBackend` is deliberately NOT internally thread-safe.** It has no locking of its own — it
  relies entirely on `MidiEngine`'s call-site locking (above) being preserved exactly, the same way the
  raw `juce::MidiOutput` pointer did before this refactor. The lock is acquired around the *call into the
  backend*, not inside the backend, so this holds regardless of how many indirection layers separate
  `MidiEngine` from the real `juce::MidiOutput`.
- `JuceMidiBackend::send()` remains a direct, synchronous `sendMessageNow` call with no buffering — the
  existing send-queue pacing design in `MidiEngine` assumes it blocks.
- New test seam: `MidiEngine::drainSendQueueForTest()` (forwards to the private `timerCallback()`) lets
  `MidiEngineSendPathTest.h` drive the real send/drain path through a `MockMidiBackend`
  (`XpandrLink/Source/Tests/MockMidiBackend.h`) with no live `Timer`/`MessageManager` loop and no real MIDI
  device — including the burst-drag mod-amount coalescing scenario that originally motivated this ADR.

## Post-Phase-4 shape (2026-07-21)

Phase 4 extracted the generic send-queue/pacing machinery into a new `MidiSendQueue` class
(`XpandrLink/Source/Engine/MidiSendQueue.{h,cpp}`; see ADR-003's post-Phase-4 note). **This did not
touch the output-backend locking invariant at all.** `MidiSendQueue` never sees `midiOutputBackend_`:
it holds only protocol-agnostic queue state and calls back into `MidiEngine` via `onSendSysex`, whose
body still funnels through `MidiEngine::sendSysex()` — the same message-thread-only, unlocked send path
as before. No new send path was introduced (only where the queue's *storage* lives moved), so the "any
new send path must follow the convention" clause below is not engaged. Every `midiOutputBackend_` access
listed in the Post-Phase-3 shape remains at its exact prior call site with its exact prior lock/no-lock
discipline.

## Consequences

- Any refactor touching `MidiEngine`'s output ownership must explicitly re-verify, line-by-line, that
  every `midiOutputBackend_` access retains its current thread-affinity/locking discipline — not assume
  it's preserved by construction. This was done for the Phase 3 change above; do it again for any future
  change to these call sites.
- Any new send path added to `MidiEngine` must follow the existing convention (message-thread-only,
  unlocked) or explicitly lock if it might run elsewhere — mixing the two without care is exactly how this
  class of bug hides until a real concurrent-traffic scenario exercises it.
- **Not yet hardware-validated.** Phase 3's own validation criteria require a full manual hardware
  re-validation pass (TEST-PLAN Sessions A/B/C/F/G plus explicit rapid mod-amount drag testing) before this
  refactor is considered done — tracked as new P0 validation debt on ROADMAP.md. The characterization
  tests above prove the send-path *encoding* is unchanged; they cannot prove real-hardware timing/gate
  behavior, which is exactly this project's documented failure class.
