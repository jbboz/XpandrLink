# XpandrLink User Guide

XpandrLink is an editor for macOS and Windows for the Oberheim Xpander and Matrix-12 synthesizers. It provides real-time, bi-directional MIDI control for all 226 patch parameters. You can edit patches simultaneously from hardware knobs, the computer interface, a DAW, or any MIDI controller — all with instant visual feedback.

---

## Table of Contents

1. [Getting Started](#1-getting-started)
2. [Title Bar & Patch Navigation](#2-title-bar--patch-navigation)
3. [Voice Architecture — Main Editor](#3-voice-architecture--main-editor)
4. [Bottom Panes](#4-bottom-panes)
   - [Mod Matrix](#41-mod-matrix)
   - [Page 2 — Advanced Parameters](#42-page-2--advanced-parameters)
   - [RND — Randomizer](#43-rnd--randomizer)
   - [MORPH — Tone Morphing](#44-morph--tone-morphing)
   - [CC — MIDI CC Automation](#45-cc--midi-cc-automation)
5. [Patch Library](#5-patch-library)
6. [MIDI Setup](#6-midi-setup)
7. [Plugin Formats](#7-plugin-formats)
8. [Tips & Workflow Notes](#8-tips--workflow-notes)

---

## 1. Getting Started

### Connecting Your Synth

1. Connect the Xpander or Matrix-12 MIDI Out to a MIDI interface on your Mac.
2. Connect the synth MIDI In from the same interface.
3. Open XpandrLink. The app automatically detects the synth by listening for its SysEx signature — when valid Oberheim SysEx is received, the source port is designated as the synth input.
4. Click **Get Patch** (or use the ▶ / ◀ buttons) to load the current patch from hardware.

### MIDI Device Selection

Click **MIDI** in the bottom nav bar to open the MIDI pane. Tick the MIDI input port(s) to listen on and pick the output port that reaches the synth. The app remembers your selections across sessions. The same pane is used in standalone and plugin modes — the plugin manages its own MIDI I/O directly and does not rely on host MIDI routing.

### Synth Detection

The MIDI pane has a **SYNTH** indicator LED. It lights green once the editor has received SysEx from your hardware — confirming an Xpander or Matrix-12 is connected and talking. If it stays unlit, turn any knob on the synth's front panel to make it send something.

The editor also auto-learns and persists the synth's MIDI unit (device) number from that first message, and stamps your outgoing edits with it so the hardware accepts them — this happens behind the scenes with no setting to touch. Note the LED confirms a synth is present but **not which model**: an Xpander and a Matrix-12 both report unit ID `2` by default, so they can't be told apart from MIDI.

### Limitation: One Synth at a Time

Connect **one synth per running copy of XpandrLink**. The editor keeps a single synth connection — one detected ID, one designated synth input, one patch display. If an Xpander and a Matrix-12 are both connected to the same instance:

- Edits are sent to whichever synth the editor heard from last; the other silently ignores them.
- Patch dumps and knob movements from *both* synths land in the same editor display and mix together.
- The second synth's port is treated as a MIDI controller, so its notes and controllers are forwarded back out the MIDI output — if your cabling routes that back to the synth, you can create a MIDI feedback loop.

To work with two synths, run two instances (e.g. the standalone app and a plugin instance, or two plugin instances) with each pointed at its own MIDI ports, and configure them one at a time.

---

## 2. Title Bar & Patch Navigation

The title bar shows the current patch name and program number in a VFD-style display.

| Control | Action |
|---|---|
| **◀** (left arrow) | Load previous patch (program − 1) |
| **▶** (right arrow) | Load next patch (program + 1) |
| **Patch name area** (click) | Open the Patch Library browser |
| **Patch number field** (click) | Type a patch number 0–99 and press Enter to load that patch from hardware |
| **MIDI IN / OUT LEDs** | Flash when MIDI activity is received or sent |

Patch loads send a program change to the synth followed by a patch dump request. The current patch is loaded into scratchpad slot 99 so no other hardware memory slots are overwritten during editing.

---

## 3. Voice Architecture — Main Editor

The main editor is a three-column grid showing all synthesis modules. Every knob, button, and dropdown updates the hardware in real time.

### Column Layout

| Column 1 | Column 2 | Column 3 |
|---|---|---|
| VCO 1 | FILTER | ENV 1–5 (tabbed) |
| VCO 2 | MODULATION | TRACK 1–3 (tabbed) |
| FM + LAG | RAMPS | LFO 1–5 (tabbed) |

### VCO 1 and VCO 2

- **Freq** — oscillator pitch (continuous knob, drag up/down or double-click to type a value)
- **Detune** — fine-tune offset
- **PW** — pulse width
- **Vol** — oscillator level
- **Waveform buttons** — TRI / SAW / PULSE / NOISE (VCO 1); TRI / SAW / PULSE / NOISE / SYNC (VCO 2)

Waveform buttons are exclusive. The active waveform shows a lit LED and bright VFD text.

### FILTER (VCF)

- **Freq** — cutoff frequency
- **Resonance** — filter resonance
- **Modulation** — mod depth
- **FM** — FM amount from oscillators
- Filter mode (15 modes) displayed as a scrollable VFD list (LOWPASS 1–4, BANDPASS, HIGHPASS, NOTCH, etc.)
- A real-time **Filter Graph** shows the approximate frequency response curve below the controls.

### FM + LAG

The FM and LAG sections share one panel.

**FM:**
- **AMP** — FM amplitude
- **DEST** — FM destination (radio select)

**LAG:**
- **IN** — lag input source (scrollable list of 27 sources)
- **RATE** — lag rate
- **=TIME** — equal time toggle (only available when Linear mode is active)
- **LINEAR / EXPO** — lag shape radio pair
- **LEGATO** — legato toggle

### ENVs 1–5

Five independent envelope generators (tabbed). Each has:
- **DELAY, ATK, DEC, SUS, REL** — standard ADSR plus delay
- **AMP** — envelope output level
- An **Envelope Graph** displays the shape in real time.

### MODULATION (summary panel)

Shows the six active modulation routings for the currently focused destination parameter. Each slot displays:
- Source name (4-character abbreviated label)
- Amount VFD readout
- Mini amount knob (drag or double-click to type)

Click any parameter in the editor to focus its modulation destination in this panel. To assign a new routing, click a source slot's source name to enter routing mode, then click any valid destination parameter (highlighted in the editor).

### TRACK 1–3

Three tracking generators (tabbed). Each has:
- **INPUT** — source (scrollable list of 27 sources)
- Tracking breakpoints and levels

### RAMPS 1–4

Four ramp generators shown side-by-side. Each has a **RATE** knob. Trigger and mode flags for each ramp are in **Page 2**.

### LFOs 1–5

Five LFOs (tabbed). Each has:
- **FREQ** — LFO rate
- **WAVE** — waveform (scrollable: Triangle, Sine, Saw Up, Saw Down, Square, Random, Sample)
- **S&H In** — sample-and-hold source (scrollable, 27 sources; available for all LFO waveforms)
- **DELAY** — LFO onset delay
- **RETRIG** — retrigger mode dropdown (Off, Single, Multi, Extrig)

---

## 4. Bottom Panes

Click the buttons in the navigation bar at the bottom of the editor window to open bottom panes. Clicking the same button again closes the pane. The window grows or shrinks automatically.

Navigation bar buttons (left to right): **Mod · PAGE2 · RND · MORPH · CC**

---

### 4.1 Mod Matrix

Button: **Mod**

A full 5 × 4 modulation routing grid showing all 20 modulation slots. Each row shows:
- **Source** — modulation source (dropdown from all 27 sources)
- **Destination** — modulation destination (dropdown from all 47 destinations)
- **Amount** — modulation amount (mini knob + VFD readout)
- **Q** — quantize flag

Click any cell to edit inline. Amount edits are sent to hardware immediately via the Oberheim SysEx amount-change command.

---

### 4.2 Page 2 — Advanced Parameters

Button: **PAGE2**

Three-column layout with synthesis control flags not shown in the main editor:

**KB Tracking:**
VCO 1, VCO 2, and FILTER modulation flags (Keyb / Lag / Lev1 / Vib) — each is an independent toggle.

**ENV 1–5 (tabbed):**
For each envelope:
- **Mode** — Free/Dadr/Reset
- **EXTRIG** — external trigger enable
- **LFOTRIG** — LFO trigger enable
- **GATED** — gate mode (only active when EXTRIG or LFOTRIG is on)
- **LFO Src** — which LFO triggers this envelope (only active when LFOTRIG is on)

**RAMP 1–4 (tabbed):**
For each ramp:
- **Single / Multi** — radio toggle
- **EXTRIG** — external trigger
- **LFOTRG** — LFO trigger
- **GATED** — gate mode
- **LFO Src** — trigger LFO source

**LFO 1–5 (tabbed):**
Advanced LFO trigger flags.

All Page 2 buttons render in VFD style (DSEG14 font) without LEDs; active state is shown by an underscore beneath the label.

---

### 4.3 RND — Randomizer

Button: **RND**

A smart patch randomizer with per-section control.

#### Scope Toggles

Select which synthesis sections to randomize. Toggle any combination of:
**VCO1 · VCO2 · VCF · FM/LAG · ENV · LFO · TRK · RAMP · MOD**

All scopes are off by default — choose which sections you want to change.

#### Amount (1–100)

Controls how far parameters move from their current values:
- **Low amounts (5–20)**: subtle variation, adjacent values
- **Medium amounts (30–50)**: moderate reshaping within the section
- **High amounts (70–100)**: radical changes, near-random values

For discrete parameters (waveforms, modes, toggles), Amount controls the probability of a change.

#### SAFE Toggle

When on (default), applies musical safety rules after randomization:
- VCO 1 waveform is always set to a non-silent state
- VCF is opened to an audible cutoff
- VCO 2 tuning is constrained to musical intervals relative to VCO 1
- When MOD scope is enabled, ENV 1 → VCA 1 routing is preserved with a usable amp envelope shape so the result stays audible

#### Buttons

| Button | Action |
|---|---|
| **NUDGE** | Small ±8% perturbation of all continuous parameters in selected sections. Leaves discrete parameters and the mod matrix unchanged. |
| **RANDOMIZE** | Full randomization of selected sections at the current Amount. |
| **REVERT** | Restore the patch to its state before the last Nudge or Randomize. One level of undo. |

Results are sent to scratchpad slot 99 on hardware immediately.

---

### 4.4 MORPH — Tone Morphing

Button: **MORPH**

Interpolate continuously between two patches.

#### Workflow

1. Open the MORPH pane. The current patch is automatically captured as **Patch A**.
2. Click **Load B** and choose a `.syx` file to set the target patch.
3. Drag the **factor slider** from 0 to 100:
   - **0** = Patch A (no change)
   - **100** = Patch B (full target)
   - Values in between are a weighted interpolation of all continuous parameters
4. Click **APPLY** to commit the morphed state as the active patch, or **CANCEL** to revert to Patch A.

#### How Morphing Works

- Only continuous parameters (knob values) are interpolated.
- Discrete parameters (waveforms, modes, toggles) always use Patch A's values.
- The modulation matrix is always copied from Patch A.
- Morphed results are sent to scratchpad slot 99 in real time as you move the slider.

---

### 4.5 CC — MIDI CC Automation

Button: **CC**

Map any MIDI CC number (0–127) to any Xpander parameter. Incoming CC from non-synth inputs (e.g. a DAW automation lane or MIDI controller) is scaled to the parameter's native range and applied to the editor in real time — without echoing to the synth output.

#### Using the CC Table

1. Open the CC pane.
2. Find the CC number you want to map (e.g. CC 74 for a filter cutoff knob on your controller).
3. Use the dropdown in that row to select the Xpander parameter.
4. The mapping is saved automatically and persists across sessions.
5. Click **Clear All** to remove all mappings.

#### CC Scaling

Values are linearly scaled: CC 0 → parameter minimum, CC 127 → parameter maximum. For binary (on/off) parameters, CC > 63 = on, CC ≤ 63 = off.

#### Which MIDI Input Sends CC?

CC automation applies to any active MIDI input that is **not** designated as the synth input. Connect your DAW or MIDI controller to a separate port from the Matrix-12 / Xpander hardware.

---

## 5. Patch Library

Click the **patch name area** in the title bar to open the patch library browser. The library is a SQLite-backed catalog; all imported patches are copied to a managed folder (`~/Documents/OberheimPatches/Library` by default).

### Browsing

- **Search box** — filter by name or description (live as you type)
- **ALL / ★** — toggle between all patches and favorites only
- **Name / Date / Desc** — cycle through sort orders (sort by name alphabetically, by date added newest-first, or by description)
- **◀ / ▶** — step to the previous or next patch; stepping auto-loads after a short debounce
- **Arrow keys** — navigate the list; patches load automatically after a brief pause

### Loading a Patch

Click a patch row to select it. The patch loads to the editor and is sent to scratchpad slot 99 on the hardware.

### Importing Patches

Click **Import** and choose a `.syx` file. XpandrLink automatically detects single patches (399 bytes) and bank files (multiple patches):
- **Single patch**: prompted for a description, then added to the library.
- **Bank file**: each patch is extracted individually; you are prompted for a description that applies to all patches in the bank.

### Footer Actions

| Button | Action |
|---|---|
| **Import** | Import one or more `.syx` files (single or bank) |
| **Save** | Overwrite the selected library patch with the current editor state |
| **Save As** | Fork the current editor patch as a new named library entry |
| **Rename** | Edit the 8-character patch name (single selection only) |
| **Remove** | Delete selected patch(es) from the library (file stays on disk) |
| **Dedupe** | Remove all duplicate patches (later imports with identical content) |

### Favorites

Click the **★** star icon on any row to toggle favorite status. Use the ★ filter button to show only favorites.

### Duplicates

The library checks content hashes on import. Duplicate patches are shown in amber with a `[D]` suffix. Use **Dedupe** to remove them all at once, or remove individually with the Remove button.

### Changing the Library Folder

Click **Folder…** at the bottom of the browser to choose a different root folder. The path is saved to the database and persists across sessions.

---

## 6. MIDI Setup

### Auto-Detection

XpandrLink listens on all active MIDI inputs. The first port that delivers a valid Oberheim SysEx message is automatically designated as the synth input. A status message confirms detection: "Auto-detected synth on: [port name]". Only one synth is supported per instance — see [Limitation: One Synth at a Time](#limitation-one-synth-at-a-time).

### Manual Selection

In the MIDI pane (bottom nav bar) you can manually select:
- **MIDI Input** — the port(s) connected to the synth MIDI Out (tick to enable)
- **MIDI Output** — the port connected to the synth MIDI In
- **Synth** — read-only model readout (XPANDER / MATRIX-12); auto-detected from incoming hardware SysEx

### MIDI Thru

MIDI from non-synth inputs (controllers, DAW) is forwarded to the synth output as MIDI thru, enabling note playback through XpandrLink without additional routing. CC messages that match the CC automation table are intercepted and used to update editor parameters instead of being forwarded.

### Hardware Feedback Loop Prevention

When the hardware sends a parameter change (e.g. from a front-panel knob), XpandrLink updates the editor UI without echoing the value back to the synth. This prevents double-increment and feedback loops.

---

## 7. Plugin Formats

XpandrLink is available as:

- **Standalone App** — runs independently; manages its own MIDI I/O via the MIDI pane
- **AU (Audio Unit)** — loads in Logic, Ableton Live, and other AU hosts
- **VST3** — loads in compatible DAWs

### Using in a DAW

In **plugin mode**, the host provides MIDI routing. Wire the Matrix-12 / Xpander hardware to a MIDI track and route that track's output through the XpandrLink plugin to get bidirectional communication.

The plugin saves and restores the active patch with the project — when you reopen a session the last patch is automatically sent back to hardware.

### Ableton Live

XpandrLink uses the `aufx` (audio effect) plugin type in AU hosts. Load it as an audio effect on a MIDI-routed audio track. Use a separate MIDI input track (not routed through the plugin) for the CC automation feature.

---

## 8. Tips & Workflow Notes

### Editing Parameters

- **Drag** any knob up or down to change values (100px per full range).
- **Double-click** any VFD value display to type a value directly.
- **Scroll wheel** works on knobs and dropdown lists.

### Patch Number Entry

Click the **patch number** field (the two-digit VFD display in the title bar) to type a program number 0–99. Press Enter to send a program change and request that patch from hardware.

### Avoiding Accidental Overwrites

All patch loads (library, ◀/▶ navigation, Get Patch) use scratchpad **slot 99** on the synth. Your other 99 program slots are never overwritten during normal editing.

> **Keep slot 99 free on the hardware.** XpandrLink uses program slot 99 as a dedicated scratchpad for all patch audition and editing. Any patch stored there will be overwritten whenever XpandrLink loads or previews a patch — including on DAW project reopen. Store your patches in slots 0–98.

### DAW MIDI Routing Conflicts

If you notice that turning a hardware knob increments a parameter twice, check whether your DAW has a MIDI track that routes the synth's MIDI back to itself. Close or mute that track. XpandrLink internally prevents its own echo; the double-increment comes from external routing.

### Patch State in AU/VST3

The plugin saves the complete patch as part of the DAW project. On reopen, the patch is sent to hardware in scratchpad **slot 99** — the same slot used during normal editing — so your other hardware memory slots are never touched by a project load. If the synth isn't connected at project load time, XpandrLink retries automatically at 2, 5, 10, and 20 seconds after startup.

### Popup Dropdowns

Dropdown lists (waveforms, LFO sources, filter modes) close automatically when XpandrLink loses focus, preventing orphaned windows from floating over other apps.
