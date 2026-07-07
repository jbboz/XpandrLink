# Features

The complete capability inventory as of v1.0.0. For technical detail (protocol, architecture,
invariants) see [SPEC.md](SPEC.md); for end-user workflows see the
[User Guide](XpandrLink-User-Guide.md); for open work see [ROADMAP.md](ROADMAP.md).

## Editor

- **All 226 patch parameters**, live and bi-directional: VCO1/2, VCF + VCA1/2, FM, LAG,
  5 LFOs, 5 envelopes, 3 tracking generators, 4 ramps.
- **Four-way sync** — the editor UI, the synth's front panel, MIDI controllers, and DAW
  automation all stay coherent. Hardware knob turns (delta encoders), page buttons, and
  front-panel ±patch navigation are decoded and reflected in the UI in real time.
- **PAGE 2 advanced parameters** — ENV mode/trigger flags, RAMP triggers, LFO retrig and
  S&H source, keyboard-tracking mod flags for VCO1/VCO2/VCF — in a dedicated bottom pane.
- **Real-time visualizers**: filter response graph (all 15 filter modes) and a draggable
  five-stage DADSR envelope graph per envelope.
- **Hardware-faithful UI** — DSEG14 "starburst" VFD font, green-on-dark VFD boxes with
  ghost segments, LED toggles, cyan knob arcs (see [DESIGN.md](DESIGN.md)). Uniform-scale
  window resize (16:10 aspect-locked).
- **Direct numeric entry** — double-click any VFD value box and type the value; click the
  patch-number VFD and type a slot to load it from hardware.

## Modulation Matrix

- Full **20-slot mod matrix**: per-slot source, destination, signed amount, quantize flag.
- **Click-to-assign routing** — click a source, valid destinations highlight, click one to
  route. Green destination LEDs on every assignable parameter label.
- Two synchronized views: the in-editor MODULATION summary (6 source slots per destination)
  and the full 5×4 matrix pane. Amount editing in either view; right-click to remove.
- Correct hardware slot (IDSource) tracking so multiple routings to one destination never
  collide, including after removals.

## Patch Librarian (SQLite-backed)

- Managed library folder (user-configurable) with per-patch name, description, date added,
  and favorites.
- **Import anything**: single `.syx` patches or full bank dumps (auto-detects the 399-byte
  patch blocks and extracts every patch).
- **Content-hash deduplication**: duplicates flagged amber `[D]`, removable
  in one click.
- **Instant audition** — scroll the list with arrow keys or ◀/▶ and each patch loads to the
  synth's scratchpad slot as you move (150 ms debounce).
- Search, favorites filter, sort by name / date / description, multi-select removal,
  rename (updates the name embedded in the SysEx), Save As from the editor.
- ~~Send Bank~~ — removed (2026-07-06): sent the entire library rather than a sequenced
  set of intended slots. See [ROADMAP.md](ROADMAP.md) P0 table (TASK-08) for the redesign
  needed before this ships again.

## Hardware Patch Safety

- **Scratchpad-slot protection**: every load, audition, morph preview, and randomizer roll
  is redirected to hardware slot 99. Memory slots 0–98 are never overwritten by editing or
  browsing. Saved files keep the patch's original slot number.
- **Different from the original XplorerEditor**: the C# original sends patches straight to
  their own embedded program slot with no scratchpad, so loading a patch there can overwrite
  whatever was already stored at that slot number. Back up or move anything you have stored
  in slot 99 before using XpandrLink for the first time — see the User Guide's
  [Getting Started](XpandrLink-User-Guide.md#1-getting-started) section.

## Smart Randomizer

- **Randomize** with per-section scope toggles (VCO1, VCO2, VCF, FM/LAG, LFO, ENV, TRACK,
  RAMP, MOD) and a 1–100 amount control; **Nudge** for ±8 % perturbation of continuous
  parameters only.
- **Musical safety (SAFE)** — rolls stay audible: VCO1 forced to a waveform, filter opened,
  VCO2 tuned to a musical interval of VCO1, and an ENV1→VCA1 routing injected so the amp
  envelope actually shapes the output.
- One-level UNDO restores the pre-roll patch.

## Tone Morphing

- Load any library patch as **B** and continuously interpolate the current patch (**A**)
  toward it with a slider — live to the hardware scratchpad on every tick.
- Continuous parameters interpolate; discrete parameters (waveforms, modes) switch at the
  midpoint; the mod matrix stays on A's routing (morphing two matrices produces nonsense).
- APPLY confirms, UNDO restores A exactly.

## MIDI

- **MIDI CC automation table** (standalone): map any of 128 CCs to any parameter; incoming
  CC from controller inputs drives the editor and synth, scaled to the parameter range.
- **Multi-input MIDI** with a designated synth port; **MIDI thru** from controller inputs
  to the synth, gated so the synth's own port is never echoed back (no feedback lockups).
- **Auto-detection**: the first port that sends Oberheim SysEx becomes the synth input, and
  the hardware's device ID is auto-learned so outgoing SysEx is never silently ignored.
- Correct vintage-CPU pacing throughout: 150 ms button-page settle, 100 ms slider-page
  settle, 50 ms mod-command gaps, queued sends.

## Plugin / DAW Integration

- **Standalone, Audio Unit (`aufx`), and VST3** from one CMake build; native Apple Silicon
  + Intel universal binary, macOS 13+; Windows x64 standalone + VST3.
- All 226 parameters exposed to the host — draw or record standard automation lanes and
  the plugin streams the changes to the synth over its own direct MIDI connection,
  bypassing host SysEx filtering (works in Ableton Live, which strips SysEx).
- **Session restore**: the current patch is saved in the DAW project and re-sent to the
  hardware automatically when the project reopens.

## Quality of Life

- Settings persistence (MIDI devices, SysEx ID, CC map, program number, library root).
- MIDI IN/OUT activity LEDs (standalone) and a full hex log of all SysEx traffic with
  decoded annotations for debugging.
- Unit tests for the protocol-critical decode paths (`ctest` target `XpandrLink_Tests`).
