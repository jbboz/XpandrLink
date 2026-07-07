# XpandrLink — Hardware & DAW Test Plan

Step-by-step verification procedures for every item in [ROADMAP.md](../ROADMAP.md) P0
("Validation Debt"). **Sessions A and C — the v1.0.0 release gate — passed clean 2026-07-06.**
Sessions B, D, and E remain open and may stagger post-launch.

Check off each item as it's completed and record the result (pass / fail / notes) back into
ROADMAP.md — when an item is fully validated, remove its row from the P0 table.

---

## Session A — Matrix-12 hardware (highest priority; ~1–2 hours) — ✅ PASSED 2026-07-06

Connect the Matrix-12. Close any open DAW (Ableton Live) before testing — an open DAW can echo
MIDI back to the synth and cause false double-increment symptoms.

### 1. BUG-32 — Save-to-file uses original slot, not 99

The scratchpad-99 redirect itself is validated (patches load to slot 99 on hardware, other slots
untouched). The remaining gap is the save path.

- [x] Load a library patch (lands on hardware slot 99, per BUG-32 fix).
- [x] Edit a parameter, then use "Save Patch" / "Save As" in the library.
- [x] Inspect the saved `.syx` file's program byte (or reload it into the library and check the
  ```
  displayed slot). **Expected:** the file preserves the patch's original slot number, not 99.
  ```

### 2. TASK-08 — Full bank send

**Removed 2026-07-06.** The "Send Bank" button sent the entire library rather than a
sequenced set of intended slots, not what it advertised — pulled from the librarian
footer rather than shipped broken. Not critical for launch. See ROADMAP.md P0 table
for the redesign needed before a replacement ships. No test steps until it's rebuilt.

### 3. TASK-07 — Randomizer musical safety

**Bug found across five passes, 2026-07-06** (user report during this test-plan pass):
repeated randomize rolls while holding a note left stuck/hung notes on the hardware, and
the user reports this class of bug can trigger a hardware "Voice Processor Malfunction"
error requiring a reset, with potential for permanent damage — **treat this whole section
as high severity**, not just a musical nuisance. Passes 1–2 fixed a real but secondary
patch-content issue (`VCA1_VOL`/`VCA2_VOL` opening to a static, unmodulated level with VCF
scope on and MOD off). Pass 3 found the VCF-scope stuck-note mechanism:
`sendPatchToSynth()` swaps the hardware's active patch on every roll with no All Notes
Off, orphaning a held note's gate — hardware-validated. Pass 4: the
same symptom reappeared with ENV scope, a different mechanism (each envelope's FREE RUN /
LFO-triggered mode cycles it forever independent of the note gate; fixed by zeroing both
flags whenever ENV is in scope). **Given the hardware-damage severity, pass 5 was a
proactive full review of every remaining scope** (not a new user report) — found RAMP
generators have the identical LFO-triggered self-cycling risk (`RAMP_TRIG_LFO`), previously
completely unguarded; fixed the same way. VCO1/2, FM/LAG, LFO, TRACK, and the mod-matrix
(MOD) encoding were reviewed and found to have no equivalent hazard. See ROADMAP.md
TASK-07 row for the full writeup. Passes 1, 2, 4, and 5 are covered by
`PatchRandomizerSafetyTest.h`; pass 3 has no automated test (pure MIDI/hardware voice
behavior). **Re-test below is required, with a note actually held through several rolls,
for VCF, ENV, and RAMP scopes separately** — don't treat this as closed until confirmed on
real hardware for all three.

- [x] Open the RND pane. With SAFE on, roll several randomizations.
- [x] For each roll, confirm: VCO1 waveform is non-zero (not silent), VCF is open enough to be
  ```
  audible, VCA1 produces sound, and a re-roll of MOD routing stays musically usable (not just
  noise).
  ```
- [x] **Regression check**: with only the VCF scope button enabled (all others off), roll several
  times. Play and release a note after each roll — confirm the sound decays/stops normally on
  note-off (does not sustain indefinitely or stack stuck voices across rolls).
- [x] **Stuck-note regression check (VCF)**: hold a note down on the keyboard, then click
  Randomize several times in a row while still holding it, with only VCF scope enabled. Confirm
  no notes hang/stick — release the key and everything should go silent.
- [x] **Stuck-note regression check (ENV)**: same as above, but with only ENV scope enabled.
  Confirm no envelope keeps cycling/sounding after all keys are released, across several rolls.
  **Confirmed clean 2026-07-06 — no stuck/cycling envelopes.**
- [x] **Stuck-note regression check (RAMP)**: same as above, but with only RAMP scope enabled.
  Confirm no ramp keeps cycling/re-triggering after all keys are released, across several rolls.
  **Confirmed clean 2026-07-06 — no stuck/re-triggering ramps.**
- [x] Try Nudge (small perturbation) and confirm the result stays close to the source patch.
- [x] Confirm Revert/Undo restores the pre-randomize patch exactly.

### 4. BUG-31 — Mod-dest LED bleed (UI only, low risk)

- [x] Load patch "S GENVIV" (or any patch that does NOT route VCF Res). Confirm no VCF Res LED
  ```
  lights up in the Mod Summary / Full Mod Matrix.
  ```
- [x] Load a patch that DOES route VCF Res, confirm the LED lights.
- [x] Load "S GENVIV" again immediately after. **Expected:** the VCF Res LED clears — no bleed
  ```
  from the previous patch's decoded set.
  ```

### 5. BUG-33 — LFO Sample S&H source selector (UI only, low risk)

- [x] Open PAGE 2 → an LFO Adv tab.
- [x] Confirm the "S&H In" dropdown is enabled/openable regardless of the LFO Wave selection
  ```
  (previously it was wrongly gated and stayed disabled).
  ```
- [x] Set Wave = Sample and confirm S&H In behaves as expected (selection routes to hardware).

### 6. TASK-05 — Patch# click-to-load

- [x] Click the VFD **number** field in the title bar (not the name field).
- [x] Type a 1- or 2-digit slot number and press Enter.
- [x] Confirm the hardware loads that slot (program change + dump request fires).
- [x] Confirm subsequent programmatic title-bar updates (e.g. from ▶/◀ or another load) don't
  ```
  retrigger a spurious load.
  ```
- [x] While here, re-confirm BUG-36 stays fixed: after a number-entry load, click ▶ and confirm it
  ```
  steps to the correct adjacent patch (not a jump to a stale `programSelector` value).
  ```

### 7. MIDI pane as sole MIDI I/O UI (Options dialog removed)

- [x] Confirm the standalone window shows no "Options" button (the JUCE Audio/MIDI Settings
  ```
  dialog is retired; the MIDI nav-bar pane is the only MIDI I/O UI).
  ```
- [x] In the MIDI pane, tick the synth input and pick the output; confirm both work and survive
  ```
  an app restart (settings persistence).
  ```
- [x] After picking the output in the pane, toggle an input on/off. **Expected:** the output stays
  ```
  connected (regression check for the device-manager default-output sync fix).
  ```
- [x] Confirm the SYNTH readout shows the detected model (XPANDER / MATRIX-12) after the first
  ```
  incoming SysEx (turn a front-panel knob), and that "MATRIX-12" fits its VFD box.
  ```

### 8. Visual checks (TASK-01, TASK-02)

- [x] **TASK-01**: On any PAGE 2 button (ENV/RAMP/LFO/KB TRACKING), confirm the active-state
  ```
  underscore sits close under the label text but never overlaps a glyph.
  ```
- [x] **TASK-02**: Compare the VFD box background color against `DESIGN.md`'s reference dark
  ```
  green; confirm inactive (`vfdDim`) text is still readable against it.
  ```

---

## Session B — Xpander hardware (medium priority)

Core parameter/mod-matrix/hardware-initiated-change paths are already validated on Xpander
(both macOS and Windows). Remaining gap is feature-specific coverage:

- [ ] Tone morphing (TASK-10) specifically on Xpander.
- [ ] Randomizer (TASK-07) specifically on Xpander.
- [ ] Librarian audition flow (◀/▶ + arrow-key auto-load) specifically on Xpander.

## Session C — Tone morphing (either synth) — ✅ PASSED 2026-07-06

### TASK-10 — Tone morphing musical result

- [x] Snapshot a patch as A (e.g. a bright lead).
- [x] Load a contrasting patch as B (e.g. a dark pad) via "Load B".
- [x] Sweep the morph slider A→B and confirm the hardware audibly interpolates rather than
  ```
  producing corrupted/glitchy intermediate states.
  ```
- [x] Confirm Undo restores patch A exactly (not a partially-morphed state).

## Session D — DAW / CC automation (standalone + AU/VST3)

### TASK-09 — CC automation table

Standalone:

- [ ] Map a few CCs to parameters in the CC pane.
- [ ] Drive those CCs from a physical MIDI controller.
- [ ] Confirm the UI updates and there's no feedback echo back to the synth.

AU/VST3 (in a DAW):

- [ ] Confirm the CC pane is hidden (DAW supplies CC routing instead, per
  ```
  `#if JucePlugin_Build_Standalone` gating).
  ```
- [ ] Draw/record a CC automation lane in the DAW targeting a mapped parameter; confirm it reaches
  ```
  the synth.
  ```

## Session E — Windows VST3 in a DAW (last open piece of WIN-1)

The Windows standalone build (MIDI I/O, patch load, param edits) is already validated on both
Matrix-12 and Xpander. The VST3-without-admin-rights build fix is
confirmed compiling. What's never been done: actually loading the built VST3 inside a Windows DAW.

- [ ] `cmake --build build/win --config Release --target XpandrLink_VST3` (no admin rights).
- [ ] Copy/confirm the VST3 lands in `%LOCALAPPDATA%\Programs\Common\VST3`.
- [ ] Open a Windows DAW that scans that location on launch (Ableton Live or Reaper for Windows).
- [ ] Confirm the plugin loads, shows the same UI as standalone, and reaches the hardware over its
  ```
  own internal MIDI I/O (per the architecture — it doesn't rely on host MIDI routing).
  ```
- [ ] Confirm `%APPDATA%\XpandrLink\` is the settings/library path used in this context.

---

## Recording results

After each session, update:

1. [ROADMAP.md](../ROADMAP.md) — remove the row for anything fully validated; narrow the "what
  needs to happen" text for anything partially validated.

