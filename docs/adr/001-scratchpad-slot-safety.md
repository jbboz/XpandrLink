# ADR-001: Scratchpad-slot-99 hardware-safety model

**Status**: Accepted, hardware-validated (BUG-32; re-confirmed alongside G2).

## Context

Every patch load, audition, morph preview, and randomizer roll needs to send a full 399-byte patch dump
to the synth so the user can hear it. The naive approach — dumping with the patch's own embedded program
number — silently overwrites whatever the user has stored in that hardware slot the moment the dump is
received, with no confirmation and no way back. This was a real, reported bug (BUG-32): browsing the
patch library was overwriting arbitrary memory slots on the synth.

## Decision

`MidiEngine::sendPatchToSynth()` unconditionally redirects the outgoing dump's program byte (and the
activating program-change) to `Oberheim::kScratchpadProgram` (99), regardless of what program number the
source patch was embedded with. The redirect happens on a *local copy* of the SysEx bytes — the original
patch's stored program number is preserved in `lastPatchSysexCache` and in any file written via
`savePatchToFile`. Only the deliberate, explicitly-confirmed "Store" flow
([ADR — see G2 in ROADMAP.md](../../ROADMAP.md)) writes to a real, permanent slot, gated by a two-step
confirm dialog naming the exact target slot.

## Consequences

- Browsing, previewing, morphing, and randomizing are all non-destructive by construction — there is no
  code path where a passive action (viewing a patch) can silently corrupt hardware memory.
- The displayed patch number and the actually-transmitted slot number can diverge (the patch might be
  "program 54" in the library but always plays on slot 99) — this required a follow-on fix
  (`onPatchSentToSynth` listener callback) to keep the UI's displayed number honest about
  where the sound is actually coming from.
- Every load path must remember to call through `sendPatchToSynth`, not send raw SysEx directly — a
  future load path that bypasses this redirect would reintroduce BUG-32.
