# ADR-003: Timer-based send pacing + amount-coalescing throttle

**Status**: Accepted, hardware-validated (2026-07-13/14 — mod-matrix live-edit lockup investigation).

## Context

`MidiEngine::timerCallback()` drains a send queue on a `juce::Timer` tick (5ms), rather than a dedicated
worker thread. A prior comment in the code explains a nonzero gap between SETSIGN/SETUNSIGNEDVALUE
sub-commands was deliberately removed after it caused rapid-drag backlog buildup. Separately, fast
mod-matrix amount-knob drags (50-100+ mouse-move ticks/second, each enqueuing a message pair with zero
inter-message delay) could flood the send queue faster than the Oberheim's 31.25kbaud UART and vintage
firmware parse loop could keep up — a classic receive-buffer-overrun causing a temporary, self-recovering
hardware lockup.

## Decision

Keep the `Timer`-based drain (rejected switching to an event-driven worker thread as part of this fix —
see [ADR discussion / plan Phase 5](../../ROADMAP.md), deferred indefinitely as of 2026-07-16 pending a
concrete reason to revisit it). Instead, add `MidiEngine::shouldCoalesceModAmount()` — a throttle that
detects rapid repeated amount-changes to the same mod-matrix slot within `Oberheim::kModCmdGapMs` (50ms)
and coalesces them into a single pending value (`PendingModAmount`), flushed once the window passes,
rather than enqueuing every intermediate drag tick.

## Consequences

- Bounds the mod-matrix amount send rate to ~1 pair per 50ms regardless of drag speed, with no
  perceptible lag for the common case (a single deliberate click outside any burst).
- The `Timer`-based drain itself is unchanged and still a polling design, not an event-driven one — this
  is an accepted, deliberate tradeoff, not an oversight. Any future change to the drain mechanism
  (e.g. moving to `std::jthread`/`condition_variable`) touches this exact historically-fragile code path
  and requires a dedicated hardware validation pass specifically targeting rapid-drag/burst scenarios
  before being considered safe.
