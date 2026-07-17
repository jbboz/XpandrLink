# ADR-006: CHANGESOURCE atomic mod-matrix source edit

**Status**: Accepted, hardware-validated (2026-07-14, mod-matrix live-edit lockup saga, cause A).

## Context

Editing a mod-matrix entry's *source* in the live UI originally did `removeModulation` (DELETESOURCE)
immediately, then `addModulation` (ADDSOURCE, wire `idSource` always `0x00`, hardware auto-appends) 250ms
later — reusing the just-freed `idSource` as a *guess* at which hardware slot the auto-append would land
the new entry in. This guess is only correct when the changed entry happens to be the sole/highest-
numbered slot at that destination; otherwise it silently corrupts an unrelated routing. The upstream C#
reference has a dedicated atomic command for exactly this case
(`F0 10 [id] 0F 00 [idSource] 00 02 00 [newSource] 00 F7`, cmd=`0x02`) that this project's live-edit path
had never implemented, defaulting instead to the more destructive delete-then-add sequence.

## Decision

Added `MidiEngine::changeModulationSource(destIndex, idSource, newSourceIndex)` +
`buildChangeSourceMessage()`, mirroring the reference's CHANGESOURCE command — an atomic, in-place swap
that never touches the entry's assigned hardware slot. `EditorTabComponent`'s source-changed callbacks
call this directly instead of delete+delayed-add, falling back to the old add-fresh path only when
`idSource < 0` (destination already at its 6-source cap, no real slot to swap).

## Consequences

- Source edits at a shared destination (2+ entries) no longer risk corrupting a sibling entry's routing.
- Any future mod-matrix edit operation must be checked against the C# reference's own command set first
  (`ModulationMatrix.cs`) rather than assumed reconstructable from DELETESOURCE/ADDSOURCE alone — this
  project's own history shows that assumption is exactly what caused this bug in the first place.
- Paired with `decrementIdSourceAfterRemove` being wired into the destination-change path (a second,
  related cause found in the same investigation) — both fixes address different gaps in the same
  underlying pattern: client-side `idSource` tracking going stale relative to what the hardware actually
  did.
