# XpandrLink

**A real-time editor, librarian, and DAW plugin for the Oberheim Xpander and Matrix-12.**

Standalone app, Audio Unit, and VST3 for macOS (Apple Silicon + Intel) and Windows.

![XpandrLink main editor](docs/images/editor-main.png)

## What it does

XpandrLink gives you hands-on visual control of every one of the 226 parameters in an
Xpander or Matrix-12 patch — five LFOs, five envelopes, three tracking generators, four
ramps, the 15-mode filter, and the full 20-slot modulation matrix — with everything kept
in sync bi-directionally: turn a knob on the synth and the editor follows; drag in the
editor and the synth updates instantly.

**Your patches are safe.** Every patch load, library audition, morph preview, and
randomizer roll is redirected to the hardware's slot 99 scratchpad. Memory slots 0–98
are never overwritten by browsing or editing.

### Highlights

- **Full editor** — all parameters live, with filter-response and draggable DADSR
  envelope visualizers, PAGE 2 advanced flags, and a hardware-faithful VFD interface
- **Mod matrix** — click-to-assign 20-slot routing with destination LEDs
- **Patch librarian** — import decades of stray `.syx` files and bank dumps, flag and
  remove duplicates automatically (content-hash), and audition patches with the arrow keys
- **Smart randomizer** — musical-safety guardrails keep every roll audible and playable
- **Tone morphing** — continuously interpolate between two patches, live on the synth
- **DAW automation** — record and draw automation lanes for any parameter; the plugin
  streams changes to the synth over its own direct MIDI connection, bypassing DAW SysEx
  filtering (yes, it works in Ableton Live); the current patch restores with your project
- **Zero-config MIDI** — the synth's port and SysEx device ID are auto-detected

## Download

Get the latest build from the [Releases page](../../releases) — macOS universal
(Standalone / AU / VST3) and Windows x64 (Standalone / VST3).

| Platform | Formats | Requirements |
|---|---|---|
| macOS 13+ | Standalone, AU (`aufx`), VST3 | Apple Silicon or Intel |
| Windows 10/11 x64 | Standalone, VST3 | — |

Install plugins to the usual locations (`/Library/Audio/Plug-Ins/Components` for AU,
`.../VST3` for VST3). Connect MIDI IN **and** OUT between your interface and the synth —
the editor auto-detects the port and device ID from the first SysEx it receives.

Full instructions: **[User Guide](XpandrLink-User-Guide.md)**.

## Building from source

Requires CMake 3.22+, a C++17 toolchain, and [JUCE 8](https://github.com/juce-framework/JUCE)
checked out as a sibling directory (`../JUCE`).

```bash
# macOS
cmake -B build/mac -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build/mac --config Release

# Windows
cmake -B build/win -G "Visual Studio 17 2022" -A x64
cmake --build build/win --config Release
```

Documentation for contributors: [SPEC.md](SPEC.md) (architecture, hardware protocol,
implementation invariants), [FEATURES.md](FEATURES.md), [DESIGN.md](DESIGN.md) (UI visual
spec), [ROADMAP.md](ROADMAP.md).

## Credits & lineage

- Built on the research and protocol implementation of
  **[XplorerEditor](https://github.com/xplorer2716/XplorerEditor)** —
  the original Windows/.NET editor whose curated Xpander/Matrix-12 SysEx documentation
  made this project possible. This project follows its author's recommendation to rebuild
  the editor on JUCE, ported to C++ and extended (librarian, randomizer, morphing, plugin
  formats, DAW automation).
- **[JUCE](https://juce.com)** framework (GPLv3 open-source license).
- **[DSEG](https://github.com/keshikan/DSEG)** 14-segment font by keshikan (SIL OFL 1.1) —
  the VFD display face.
- Oberheim is a trademark of its respective owner; this project is not affiliated with or
  endorsed by Oberheim.

## License

[GPL-3.0](LICENSE) — same as the upstream XplorerEditor.
