# Changelog

## v1.0.0 — (unreleased)

First public release. A complete C++/JUCE rebuild of
[XplorerEditor](https://github.com/xplorer2716/XplorerEditor) for the Oberheim
Xpander and Matrix-12, following the original project's recommendation to port
to JUCE — extended well beyond the original's scope.

### Highlights

- **Cross-platform**: macOS (universal, 13+) and Windows x64; Standalone, Audio
  Unit (`aufx`), and VST3 from one CMake build.
- **Full editor** — all 226 parameters, bi-directional with the hardware front
  panel, PAGE 2 advanced parameters, filter and DADSR envelope visualizers,
  hardware-faithful VFD interface.
- **20-slot mod matrix** with click-to-assign routing, destination LEDs, and two
  synchronized views.
- **SQLite patch librarian** — bank import, content-hash duplicate detection, instant
  arrow-key audition, favorites, search/sort.
- **Hardware patch safety** — every load/audition/preview lands on scratchpad
  slot 99; memory slots 0–98 are never overwritten by browsing or editing.
- **Smart randomizer** with musical-safety guardrails and one-level undo.
- **Tone morphing** — continuous live interpolation between two patches.
- **DAW automation** — all parameters exposed to the host; direct MIDI to the
  synth bypasses host SysEx filtering (works in Ableton Live); patch state
  restores with the DAW project.
- **MIDI conveniences** — CC-to-parameter automation table (standalone),
  multi-input with gated MIDI thru, auto-detection of the synth port and its
  SysEx device ID, a hardware display banner (send text to the synth's own
  front-panel VFD), and store-to-hardware-slot for permanently committing a
  patch (with a two-stage confirm dialog, since it's the one path that
  writes non-volatile hardware memory).
- **Mod-matrix stability** — front-panel routing edits (add/change source/
  change amount/quantize/delete) are decoded directly from the hardware's
  own edit SysEx and mirrored live in both matrix views, instead of
  re-requesting a patch dump (which never reflected those edits). Fast
  amount-knob drags are coalesced so they can't flood the synth's MIDI IN.
  Removing a routing now correctly restacks the remaining entries instead
  of leaving a gap, and saving/storing a patch no longer risks writing a
  corrupt silent routing into an unused mod-matrix slot.
- **MIDI output auto-select** — the editor picks the matching MIDI output
  automatically once it detects the synth on an input port, and shows a
  greeting on the synth's own front-panel VFD the first time it's detected
  each session.
- **`IMidiBackend` abstraction** — the MIDI engine's send path is injectable
  and independently unit-tested, with CI-gated ASan/TSan sanitizer runs.

See [FEATURES.md](FEATURES.md) for the complete inventory.
