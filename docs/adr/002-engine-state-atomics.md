# ADR-002: Engine-state atomics instead of a single lock

**Status**: Accepted, in production since the session-50 code review (commit `6a4ebe9`, PR #16).

## Context

`MidiEngine` has scalar state read from the message thread (UI queries, e.g. "what page did we last
send?") and written from the MIDI input thread (e.g. "hardware just changed page"): `sysexID`, page/mode/
program tracking, `hardwareUpdateMode`, `lastModSentTime_`. A fresh code review (N-03 finding) identified
these as unsynchronized cross-thread reads/writes — a real, if usually benign, data race.

## Decision

Convert each of these scalars to `std::atomic<T>` individually, rather than introducing one broad mutex
covering all of `MidiEngine`'s state. `synthInputName` (a `juce::String`, not trivially atomic) is instead
guarded by the existing `listenerLock`, with call sites taking a per-message snapshot rather than holding
the lock across a callback.

## Consequences

- Cheap, uncontended reads/writes for the hot-path scalars (no lock acquisition on every SysEx byte
  processed).
- Correctness depends on each field being *independently* consistent being sufficient — this is true for
  simple state-tracking scalars, but would NOT be true for anything requiring multiple fields to update
  atomically as a group. `activeOutput` (a `unique_ptr`, not a scalar) is deliberately NOT part of this
  scheme — see [ADR-007](007-activeoutput-locking-invariant.md) for its separate, asymmetric-locking
  discipline, which must not be confused with or merged into this pattern.
- Any new piece of engine state shared across threads should default to `std::atomic` for scalars,
  `listenerLock` + snapshot for non-trivial types — matching the two patterns already established, not a
  third new one.
