# XpandrLink — Hardware & DAW Test Plan

Step-by-step verification procedures for every item in [ROADMAP.md](../ROADMAP.md) P0
("Validation Debt"). **Sessions A, B, and C — the v1.0.0 release gate — have all passed
clean.** Sessions D and E remain open and may stagger post-launch.

Check off each item as it's completed and record the result (pass / fail / notes) back into
ROADMAP.md — when an item is fully validated, remove its row from the P0 table.

---

## Session A — Matrix-12 hardware (highest priority; ~1–2 hours) — ✅ PASSED

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

**Removed 2026-07-06, dropped from the roadmap entirely 2026-07-14.** The "Send Bank" button
sent the entire library rather than a sequenced set of intended slots, not what it advertised —
pulled from the librarian footer rather than shipped broken. No redesign planned; see
ROADMAP.md P3 for detail. No test steps.

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

## Session B — Xpander hardware — ✅ PASSED

Core parameter/mod-matrix/hardware-initiated-change paths validated on both macOS and Windows;
tone morphing, randomizer, and librarian audition flow validated specifically on Xpander since.
Bank send (TASK-08) dropped from scope entirely (see ROADMAP.md P3) — it's no longer part of
this session's coverage.

- [x] Tone morphing (TASK-10) specifically on Xpander.
- [x] Randomizer (TASK-07) specifically on Xpander.
- [x] Librarian audition flow (◀/▶ + arrow-key auto-load) specifically on Xpander.

## Session C — Tone morphing (either synth) — ✅ PASSED

### TASK-10 — Tone morphing musical result

- [x] Snapshot a patch as A (e.g. a bright lead).
- [x] Load a contrasting patch as B (e.g. a dark pad) via "Load B".
- [x] Sweep the morph slider A→B and confirm the hardware audibly interpolates rather than
  ```
  producing corrupted/glitchy intermediate states.
  ```
- [x] Confirm Undo restores patch A exactly (not a partially-morphed state).

## Session D — DAW / CC automation (standalone + AU/VST3) — ✅ PASSED

### TASK-09 — CC automation table

Standalone:

- [x] Map a few CCs to parameters in the CC pane.
- [x] Drive those CCs from a physical MIDI controller.
- [x] Confirm the UI updates and there's no feedback echo back to the synth.

AU/VST3 (in a DAW):

- [x] Confirm the CC pane is hidden (DAW supplies CC routing instead, per
  ```
  `#if JucePlugin_Build_Standalone` gating).
  ```
- [x] Draw/record a CC automation lane in the DAW targeting a mapped parameter; confirm it reaches
  ```
  the synth.
  ```

## Session E — Windows VST3 in a DAW (last open piece of WIN-1) — ✅ PASSED

The Windows standalone build (MIDI I/O, patch load, param edits) is already validated on both
Matrix-12 and Xpander. User-confirmed: "Windows build tested and validated, both standalone
and VST3" — a Release build of both formats was built and run, closing the last open piece
of WIN-1.

- [x] `cmake --build build/win --config Release --target XpandrLink_VST3` (no admin rights).
- [x] Copy/confirm the VST3 lands in `%LOCALAPPDATA%\Programs\Common\VST3`.
- [x] Open a Windows DAW that scans that location on launch.
- [x] Confirm the plugin loads, shows the same UI as standalone, and reaches the hardware over its
  ```
  own internal MIDI I/O (per the architecture — it doesn't rely on host MIDI routing).
  ```
- [x] Confirm `%APPDATA%\XpandrLink\` is the settings/library path used in this context.

## Session F — G1/G2/G9 hardware validation (parity gaps) — ✅ PASSED

Newest work, not yet touched by real hardware. **G2 is the highest-stakes item in this whole
document — it writes permanent, non-volatile patch memory with no undo.** Run G9 first (it's
just a UI toggle, but G1/G2 both depend on sending the correct command byte for the connected
model). Test on both a Matrix-12 and an Xpander if both are available — the model byte differs
between them and a wrong byte on one model but not the other would otherwise go unnoticed.

### 1. G9 — Synth-type model toggle

- [x] Open the MIDI pane. Confirm the MODEL toggle button exists and switches between
  XPANDER / MATRIX-12.
- [x] Set it to match the synth actually connected. Confirm the setting persists across an app
  restart.
- [x] No SysEx is sent by this toggle alone — it only feeds the command bytes used by G1/G2/G3.
  Nothing to observe on the hardware yet.

### 2. G1 — Hardware display banner

- [x] With G9 set correctly for the connected synth, enter text (≤80 chars) in the DISPLAY field
  in the MIDI pane and press SEND.
- [x] **Expected:** the synth's front-panel VFD shows the text, uppercased, correctly spaced —
  not garbled, truncated wrong, or shifted.
- [x] Confirm text longer than 80 characters truncates cleanly (no wraparound garbage).
- [x] ~~Send the OFF control~~ — confirmed unnecessary: there is no dedicated OFF/blank state
  to test on real hardware; switching functions on the synth resets the display back to normal
  as expected on its own.
- [x] Send the ON control (redisplay) and confirm it re-shows correctly.
- [x] Repeat against the other synth model (Xpander cmd `0x05` vs Matrix-12 cmd `0x06`) if
  available — a wrong command byte would only show up on one of the two models.

### 3. G2 — Store to hardware slot (⚠️ permanent, non-volatile write)

**Do this on a spare/scratch slot you don't mind overwriting, not a slot with a patch you care
about, until the first pass is confirmed clean.**

- [x] Pick a known, distinctive patch (e.g. one already in the library) and a spare target slot
  (0–98, not 99).
- [x] Use the **Store** button in the patch browser footer; confirm the slot picker rejects 99
  and confirms before committing (warning `AlertWindow`).
- [x] Commit the store. Power-cycle the synth (fully off/on, not just a patch reload) to rule out
  a write that only "stuck" in volatile memory.
- [x] Re-get that slot from hardware (patch # click-to-load or normal program change) and confirm
  the bytes match the source patch exactly — same name, same every parameter, same mod routing.
- [x] Confirm no *other* slot was disturbed — spot check the slot immediately before/after the
  target, and whatever was the synth's currently-active slot before the store.
- [x] Repeat once more on a second spare slot to rule out an off-by-one in slot addressing.
- [x] Repeat the whole check against the other synth model if available.

## Session G — Mod-matrix live-edit hardware lockup fix — ✅ PASSED (user: "not able to get the hardware to lock up anymore — consider this validated for now")

**This is a real hardware-lockup-class bug. Do this on a Init Patch / scratch slot, not a patch
you care about, and be ready to power-cycle the synth if anything looks wrong.** Found from a
user report that manually editing mod-matrix routings (not the randomizer) reproducibly locked
up the hardware. Root cause + fix detail in [ROADMAP.md](../ROADMAP.md). The user's confirmation
was general (no longer able to reproduce a lockup), not an item-by-item walkthrough of every box
below — treat as validated but keep an eye out if any of these specific scenarios recur.

### 1. Source change on an existing entry (the exact repro reported)

- [x] Load the Init Patch. Open the Mod Matrix pane.
- [x] Add two mod-matrix entries targeting the **same** destination (e.g. ENV1→VCA1 and
  ENV2→VCA1), so the destination has 2 active slots.
- [x] Change the **first** (lower-numbered) entry's source (e.g. ENV1 → ENV3). Confirm no
  lockup, and confirm the **second, untouched** entry (ENV2→VCA1) still shows the correct
  source/amount in both the Mod Summary panel and the Full Mod Matrix panel.
- [x] Change the amount (positive and negative) on that second, untouched entry. Confirm it
  actually changes the routing you expect — not some other, unrelated slot.
- [x] Repeat the source change 5-10 times in a row on different entries. Confirm no lockup and
  no "Voice Processor Malfunction"-class error across repeated edits.

### 2. Destination change on an existing entry, with a sibling entry at the same source destination

- [x] With 2+ entries still at one destination, change the **first** entry's destination to a
  different one (e.g. ENV1→VCA1 becomes ENV1→VCF FREQ). Confirm no lockup.
- [x] Confirm the **second, untouched** entry (still at the old destination) still shows/behaves
  correctly — edit its amount and source, confirm it targets the right routing.
- [x] Repeat several times, mixing source changes and destination changes on entries at shared
  destinations, to specifically re-exercise the idSource-renumbering fix.

### 3. General regression check

- [x] Right-click remove still works correctly (unchanged code path) — remove an entry, confirm
  the destination LED clears and no other entries are disturbed.
- [x] Adding a brand-new source to an empty slot still works (unchanged code path).

### 4. Fast amount-drag flooding (found 2026-07-13, separate mechanism from #1/#2)

- [x] Add a mod-matrix entry targeting **VCA1 Vol** (or VCA2 Vol — the specific destinations
  reported). Drag its amount knob/slider from max (+63) to min (-63) as fast as possible,
  repeatedly, several times in a row.
- [x] **Expected**: no lockup at all. If a lockup does still occur, it should be brief and
  self-recovering (matches the pre-fix symptom, which self-recovered but still shouldn't happen
  post-fix) — a lockup that does NOT self-recover is a regression, stop and report immediately.
- [x] Confirm the knob/slider still feels responsive during the drag (the throttle should not be
  perceptible as UI lag — only the outgoing SysEx rate is capped, not the on-screen value).
- [x] Confirm the FINAL value you drag to is the one that actually lands on the synth (check via
  Get Patch / re-dump) — the coalescing must never drop the last, settled value.
- [x] Repeat the same fast-drag test on an ordinary (non-mod-matrix) knob, e.g. VCF Freq, to
  confirm regular parameter drags are unaffected by this change (they were never routed through
  `changeModulationAmount`).

---

## Session H — Front-panel mod-matrix edit decode — ✅ PASSED (user: "was also able to add a new routing in hardware and see it in the editor")

**Not a hardware-lockup risk** (this only changes how the editor *displays* what the hardware
already did — it never sends new SysEx back), but it lives in the same mod-matrix subsystem that
has had several real bugs (Sessions F/G above), so treat the display results with the same
care. Root cause: editing a mod-matrix routing on the hardware's own front panel was invisible
in the editor no matter how long you waited afterward — a "Get Patch" dump request never
reflects live front-panel mod-matrix edits on this hardware. Fixed by decoding the edit SysEx
directly instead of re-requesting a dump. See [ROADMAP.md](../ROADMAP.md) for detail. Full
item-by-item walkthrough completed and validated — every command type (add/dial-amount/quantize/
change-source/delete) and every destination class (VCO/LFO/ENV/FM-LAG/VCF-VCA) confirmed correct,
plus the editor-initiated-edit and library/randomizer-load regression checks.

### 1. Add a new routing from the front panel

- [x] On the hardware, navigate to a destination not currently focused in the editor (e.g. VCF
  page → VCA1 Vol) and add a new modulation source (e.g. LFO1) via the front panel.
- [x] Confirm the editor's **Mod** pane (full 20-slot matrix) shows the new entry within about a
  second, with the correct source and destination.
- [ ] Click the destination's parameter in the main editor so the compact MODULATION summary
  focuses on it; confirm it shows the same entry.

### 2. Edit an existing front-panel-added entry

- [x] Turn the AMOUNT dial on the entry from #1. Confirm the editor's displayed amount tracks it
  (both positive and negative directions).
- [x] Toggle quantize on that entry from the front panel (if accessible) and confirm the editor
  reflects it.
- [x] Change the entry's source from the front panel (e.g. LFO1 → LFO2). Confirm the editor updates
  the source without creating a duplicate or losing the slot.
- [x] Delete the entry from the front panel. Confirm it disappears from both mod-matrix panels and
  the destination's LED (if applicable) clears.

### 3. Cross-check several destination classes

The page/subpage→destination table (`kModDestTable`) was only live-verified at one destination
(VCF page, VCA1/VCA2) this session — the rest are transcribed from the C# reference and the
existing outgoing-send path, which only gives indirect validation. Repeat #1 briefly at a few
other destination classes to confirm the table is correct end-to-end:

- [x] A VCO destination (e.g. VCO1 Freq).
- [x] An LFO destination (e.g. LFO1 Amp).
- [x] An ENV destination (e.g. Env1 Amp).
- [x] The FM/LAG destinations (FM Amp, Lag Rate).

### 4. No regression on editor-initiated edits or hardware round-trips

- [x] Add/remove/change a mod-matrix entry from the **editor** itself (not the front panel) —
  confirm this still works exactly as before (unchanged code path, just checking nothing in the
  hardware-edit path was accidentally shared).
- [x] Load a patch from the library / randomizer while some front-panel mod edits are pending —
  confirm no crash or corrupted display.

---

## Session I — Init Patch mod-matrix slot fix — ✅ PASSED (user: "init patch only has the two default mod mappings")

Root cause: loading Init Patch from the editor, then trying to add any
modulation routing, failed with "maximum of 20 modulations per voice" even though only 2 routings
were visible. The embedded `Resources/InitPatch.syx` had its 18 unused mod-matrix slots zero-filled
instead of using the hardware's actual "empty slot" sentinel (`src=0x1F`, `dst=0x3F`), so the synth
believed all 20 slots were already occupied the moment Init Patch was sent. Fixed by rewriting the
asset. See [ROADMAP.md](../ROADMAP.md) for detail.

- [x] Load Init Patch (editor's INIT PATCH button). Add a new modulation routing from the editor's
  Mod Matrix pane. Confirm it succeeds (no "maximum of 20" error).
- [x] Load Init Patch again. Add a new modulation routing from the hardware's own front panel.
  Confirm it succeeds.
- [x] Confirm Init Patch still sounds/behaves the same as before (only the *unused* mod-matrix
  slots changed — the two real routings, VCO1-ish env→VCF and env→VCA2, are untouched).
- [x] Load the internal "Oberheim" factory patch (the one that already worked in the original bug
  report) and confirm it's unaffected (this fix only touches `InitPatch.syx`).

---

## Session J — `midi-output-autoselect` branch — ✅ PASSED

Everything on the local `midi-output-autoselect` branch, tested before merging to `main`.
All six items confirmed clean, including the item 3 retest after fixing the incomplete
test instructions (a fresh install needs a manually-enabled input before anything
downstream can auto-detect).

### 1. Mod-matrix unused-slot sentinel fix — HIGH PRIORITY, same bug class as the Init

Patch corruption fix (Session I above)

`getCurrentModBytes()` was zero-filling unused mod-matrix slots instead of using the
hardware's sentinel (`src=0x1F`, `dst=0x3F`) on every Save Patch / Store-to-hardware-slot
from the live editor — the same mechanism that caused "maximum of 20 modulations per
voice" errors in Session I, but live in an ongoing code path, not one static file.

- [x] Load a patch with only 1-2 active mod-matrix routings (e.g. Init Patch).
- [x] Save it to a file, or Store it to a spare hardware slot.
- [x] Reload that saved/stored patch. Confirm it still shows only the original 1-2
  routings — no phantom extras.
- [x] With that reloaded patch active, try adding a new mod-matrix routing from the
  editor. Confirm it succeeds (no "maximum of 20 modulations" error). This is the
  direct repro for the bug class — if it fails here, the fix didn't take.
- [x] Repeat once more after a second save/reload round-trip, to rule out the corruption
  compounding across multiple saves.

### 2. Mod-matrix grid restacking (the gap-closing feature)

- [x] Add 3-4 mod-matrix routings to different destinations from the editor's Mod Matrix
  pane. Note their grid positions.
- [x] Remove one from the middle (not the first or last). Confirm the entries after it
  shift down to close the gap, rather than leaving a blank cell.
- [x] Repeat the same removal from the hardware's own front panel instead of the editor.
  Confirm the editor's grid still compacts (mirrors the hardware-initiated delete).
- [x] After compacting, add a new routing and confirm it fills the front of the grid
  correctly, and edit/remove a couple of the remaining ones to confirm nothing's
  addressing the wrong hardware slot (the actual hazard class from the session-55–61
  mod-matrix sagas, even though this change was designed not to touch idSource at all).

### 3. MIDI output auto-select

**Note (found during testing): a fresh install with no saved settings has zero MIDI
inputs enabled by default, and that's deliberately unchanged by this branch (no
auto-enable-all-inputs) -- the app has to actually hear the synth's SysEx before it can
auto-detect anything, on the input OR output side.** Enabling an input is a required
manual step here, not optional -- without it, nothing downstream (SYNTH LED, output
auto-select, the greeting) can ever fire.

- [x] Delete `~/Library/Application Support/XpandrLink/XpandrLink.settings` (or just clear
  the saved MIDI output) to simulate a fresh install. Launch the app.
- [x] In the MIDI pane, manually check the box for your synth's input port (this step is
  required -- a fresh install starts with no inputs enabled).
- [x] Turn a knob or interact with the synth's front panel so it sends some SysEx. Confirm
  the SYNTH LED lights up.
- [x] With the LED lit, confirm the output auto-selected (no manual pick needed) --
  assuming your interface exposes a single output, or one that shares the synth input's
  port name.
- [x] If you have multiple unrelated MIDI outputs available, confirm it does NOT guess —
  output should stay unset until you pick one manually.
- [x] Confirm a previously manually-chosen output is never silently overridden by this.

### 4. Welcome greeting on connect

- [x] Launch the app with the synth connected (or connect it after launch). Confirm the
  synth's own front-panel VFD shows `XPANDRLINK V1.0.0-BETA.1` once, shortly after
  connecting.
- [x] Confirm it only fires once per session — reconnecting/re-triggering SysEx later in
  the same session shouldn't re-send it.
- [x] Confirm the display returns to normal (patch name, etc.) as soon as you interact
  with a function on the synth — no dedicated "OFF" needed, matching the G1 finding.

### 5. Version label

- [x] Confirm the nav bar shows `v1.0.0-beta.1` (not truncated, not just `1.0.0`).
- [x] If convenient, check the AU/VST3 as listed in a DAW's plugin browser — should still
  show a clean `1.0.0` (the plugin-metadata version is deliberately unaffected).

### 6. Removed/changed UI (quick visual pass)

- [x] MIDI pane: confirm the DISPLAY text field + SEND button are gone, and the panel
  layout still looks right without that row.
- [x] Morph pane: confirm the slider shows a `%` value, and only one button (UNDO)
  remains in the action row, centered, not stretched oddly.

---

## Recording results

After each session, update [ROADMAP.md](../ROADMAP.md) — remove the row for anything fully
validated; narrow the "what needs to happen" text for anything partially validated.