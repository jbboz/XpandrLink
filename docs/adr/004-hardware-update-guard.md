# ADR-004: Hardware-update guard pattern

**Status**: Accepted, long-standing project invariant.

## Context

UI controls (`HardwareKnob`, `ModulePanel` widgets) call `sendParameterToSynth` on `onValueChange`. When
a value change originates from the *hardware* (an incoming SysEx parameter-change decoded and applied to
a control), setting that control's value programmatically would normally re-trigger the same
`onValueChange` callback, which would send the value right back to the synth — an echo. Depending on
timing, this can manifest as a double-increment or a feedback loop.

## Decision

`MidiEngine::setHardwareUpdateMode(true)` is called immediately before any hardware-initiated UI
mutation, and `setHardwareUpdateMode(false)` immediately after. Control callbacks check this flag and
skip sending to the synth when it's set. The flag lives on `MidiEngine`, not on `EditorTabComponent` —
this was itself a fix (see git history / `feedback_hardware_update_guard` memory): if the guard lived on
the tab component, `ModulePanel`'s callbacks (which only hold a reference to `MidiEngine`, not the tab
component) couldn't see it.

## Consequences

- Any new code path that mutates a control's value in response to hardware input MUST wrap the mutation
  in `setHardwareUpdateMode(true)`/`(false)`, matching the existing pattern, or it will silently
  reintroduce the double-increment/echo class of bug.
- The guard is a single global flag, not scoped per-control — a hardware update touching multiple
  controls in sequence relies on the guard staying set for the whole batch, not being toggled off between
  individual control updates.
