# Design Goals

> **Implementation status (2026-07):** everything below has shipped. This file is kept as the
> visual-language reference for contributors; where the implementation deliberately diverged
> from the original design notes, the differences are:
>
> - **Layout is a 3-column grid**, not the two-row layout described in the early design
>   notes: col 0 = VCO1/VCO2/FM+LAG, col 1 = FILTER/MODULATION/RAMPS,
>   col 2 = tabbed ENV/TRACK/LFO. Bottom panes (Mod / PAGE2 / RND+MORPH / CC / MIDI)
>   toggle below the always-visible editor instead of full page swaps.
> - **Parameter toggles use one unified style** (9px LED + VFD box + underscore active
>   indicator) rather than a plain LED+label-only style.
> - **Authoritative colour tokens live in `XpandrLink/Source/UI/ThemeData.h`** (e.g.
>   `moduleBackground` 64,64,64 — lighter than the window; `vfdBackground` 10,45,22;
>   `vfdDim` vs `vfdGhost` split). Values below are the original targets; ThemeData wins
>   where they differ.

## UI Goals

The goal is a software editor that reads like the Xpander/Matrix-12's own front panel: a
16-segment "starburst" VFD look for parameter values and patch names, module panels grouped
by voice-architecture section, and knobs with a simple arc indicator — the same visual
vocabulary used across the vintage hardware-synth-editor genre generally, not modeled on any
single third-party product.

Some specific definitions to recreate are:

1. Module layout should follow the Xpander/Matrix-12's own voice architecture (VCO/VCF/ENV/LFO/etc. grouped by function)

2. VFD style text fonts used for the parameters on each of the modules. 
    -The segmented VFD text looks nearly identical to the original hardware (16-segment "starburst" type VFD display). 
    -VFD text background is a darker shade of green to contrast the brighter VFD text
    -Active toggles are underlined with five dashes and a red "LED" indicator
    -The VFD font text is used for both parameters and the patch name
    -VFD text is sRGB 191 255 215

3. Title bar of the modules sections
    -Blue title bar is sRGB 44 95 190
    -Title Text is sRGB 217 216 209 and sits on the left side slightly indented into the blue title bar with the dark background

4. UI background sRGB 58 58 58

5. Module Parameters
    -Knobs: Have light blue arcs to indicate relative value positions
    -Values: Parameters have text boxes with VFD text font to display active value
    -Parameter display names: Are selectable buttons that have an indicator LED (sRGB 0 252 46) when actively routed in the mod matrix
    -When parameter button is active, that routing will be displayed as active in the mod matrix
    -Tabbed modules:
        -Envelope (ENV), Track and LFO all have tabs (ENV 1-5, Track 1-3, LFO 1-5)
        -ENV
            -ENV section gets text box for current value, a parameter knob with arc and a envelope visualization with draggable handle points for each value (DADSR)

6. Librarian
    -Current patch display is in the upper middle section
    -Patch display shows the currently loaded patch name in VFD text 
    -Patch display shows the current patch number
        -Selecting Patch number opens a menu to choose any available patch number
        -has buttons to increment or decrement patch value
        -can type a patch number directly as a two digit value, single digit values followed by enter will be accepted and input at 0+(1-9) 
    -Clicking or double clicking the patch name allows it to be edited
    -Patch name is constrained to 8 characters and is forced uppercase (Hardware limit)
    -Library panel 
        -place button to the left of the patch name to drop down a cascading library panel
        -shows all available patches in the currently selected folder
        -patches in Library can be navigated with arrow keys or mouse
        -selecting a patch loads it into the editor instantly
        -include buttons to save and load patches to and from disk

---

## Early Layout Notes (superseded)

The paragraphs above capture the shipped design goals. An earlier working draft of this file
also included a detailed screen-by-screen layout breakdown (module header bands, VFD box
proportions, knob arc angles, tab styling, modulation-matrix grid, etc.) written while surveying
the broader vintage-hardware-synth-editor genre for layout ideas. That breakdown described a
two-row layout, which the shipped 3-column grid superseded (see the implementation-status note
at the top of this file) — it added no information beyond what's already captured in the
"UI Goals" section above, so it's been removed rather than kept as dead weight.
