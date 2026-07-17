# Architecture Decision Records

Short, retroactive records of load-bearing design decisions already made and validated in production.
These document *why*, not *what* — the code itself already covers *what*.

Written after the fact (2026-07) to make the reasoning behind hard-won, hardware-validated decisions
legible without excavating the project's full history. New ADRs should be added for future decisions
of similar weight — anything that, if silently violated, would reintroduce a previously-fixed class
of bug.

| ADR | Title |
|---|---|
| [001](001-scratchpad-slot-safety.md) | Scratchpad-slot-99 hardware-safety model |
| [002](002-engine-state-atomics.md) | Engine-state atomics instead of a single lock |
| [003](003-timer-pacing-coalescing.md) | Timer-based send pacing + amount-coalescing throttle |
| [004](004-hardware-update-guard.md) | Hardware-update guard pattern |
| [005](005-subpage-paramcol-collision.md) | Subpage paramCol collision dispatch |
| [006](006-changesource-atomic-edit.md) | CHANGESOURCE atomic mod-matrix source edit |
| [007](007-activeoutput-locking-invariant.md) | `activeOutput` asymmetric locking invariant |
