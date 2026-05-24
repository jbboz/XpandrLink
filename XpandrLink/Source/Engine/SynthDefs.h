#pragma once

namespace Matrix12 {

    // --- PAGE DEFINITIONS (SysEx byte 4 in page-select and param-change messages) ---
    enum Page {
        // Voice pages
        PAGE_VCO1       = 0x20, // 32 - VCO 1
        PAGE_VCO2       = 0x21, // 33 - VCO 2
        PAGE_VCF        = 0x22, // 34 - VCF / VCA
        PAGE_FM         = 0x23, // 35 - FM / Lag

        // Master command page (page-select byte; see OberheimXpanderMidiSpec page table)
        PAGE_MIDI_MUTE   = 0x1B, // 27 - Master -> MIDI -> Mute (numerically same as Ctrl::KNOB_COL_4, different enum/SysEx byte position — no collision)

        // Envelopes (5x)
        PAGE_ENV1       = 0x28, // 40
        PAGE_ENV2       = 0x29, // 41
        PAGE_ENV3       = 0x2A, // 42
        PAGE_ENV4       = 0x2B, // 43
        PAGE_ENV5       = 0x2C, // 44

        // LFOs (5x)
        PAGE_LFO1       = 0x30, // 48
        PAGE_LFO2       = 0x31, // 49
        PAGE_LFO3       = 0x32, // 50
        PAGE_LFO4       = 0x33, // 51
        PAGE_LFO5       = 0x34, // 52

        // Tracking generators (3x)
        PAGE_TRACK1     = 0x38, // 56
        PAGE_TRACK2     = 0x39, // 57
        PAGE_TRACK3     = 0x3A, // 58

        // Ramps (4x)
        PAGE_RAMP1      = 0x40, // 64
        PAGE_RAMP2      = 0x41, // 65
        PAGE_RAMP3      = 0x42, // 66
        PAGE_RAMP4      = 0x43, // 67
    };

    // --- SUBPAGE MODES (SysEx byte 5 in page-select message) ---
    // Mode 0 = rotary/slider subpage
    // Mode 1 = button subpage
    enum SubPage {
        SUBPAGE_ROTARY  = 0,
        SUBPAGE_BUTTON  = 1,
    };

    // --- CONTROL COLUMN IDs (SysEx byte 5 in parameter-change messages) ---
    // Rotary encoder columns (relative delta on VCO/VCF, absolute on ENV/LFO/RAMP/TRACK)
    enum Ctrl {
        // --- Rotary knob columns (used on all pages, subpage 0) ---
        KNOB_COL_1      = 0x18, // 24  - Rotary delta
        KNOB_COL_2      = 0x19, // 25
        KNOB_COL_3      = 0x1A, // 26
        KNOB_COL_4      = 0x1B, // 27
        KNOB_COL_5      = 0x1C, // 28
        KNOB_COL_6      = 0x1D, // 29

        // --- Absolute parameter columns (ENV, LFO, RAMP, TRACK use absolute values) ---
        PARAM_COL_1     = 0x08, // 8
        PARAM_COL_2     = 0x09, // 9
        PARAM_COL_3     = 0x0A, // 10
        PARAM_COL_4     = 0x0B, // 11
        PARAM_COL_5     = 0x0C, // 12
        PARAM_COL_6     = 0x0D, // 13

        // --- VCO subpage 0 (rotary absolute values) ---
        VCO_FREQ        = 0x08, // col 1 = Frequency
        VCO_DETUNE      = 0x09, // col 2 = Detune
        VCO_PW          = 0x0B, // col 4 = Pulse Width
        VCO_VOL         = 0x0D, // col 6 = Volume

        // --- VCO subpage 1 (buttons) ---
        VCO_MOD_KEYB    = 0x01, // Keyboard tracking mod flag
        VCO_MOD_LAG     = 0x02, // Lag mod flag
        VCO_MOD_LEV1    = 0x03, // Lever 1 mod flag
        VCO_MOD_VIB     = 0x04, // Vibrato mod flag
        VCO_WAVE_SYNC   = 0x05, // Sync (VCO 2 only)
        VCO_WAVE_TRI    = 0x09, // Triangle waveform
        VCO_WAVE_SAW    = 0x0A, // Sawtooth waveform
        VCO_WAVE_PULSE  = 0x0B, // Pulse waveform
        VCO_WAVE_NOISE  = 0x0C, // Noise (VCO 2 only)
        // Aliases for legacy EditorTabComponent code
        VCO_TRI_BTN     = 0x09,
        VCO_SAW_BTN     = 0x0A,
        VCO_PULSE_BTN   = 0x0B,

        // --- VCF subpage 0 ---
        VCF_FREQ        = 0x08, // Frequency
        VCF_RES         = 0x09, // Resonance
        VCF_MODE        = 0x0A, // Filter mode (0-14)
        VCF_VCA1_VOL    = 0x0C, // VCA 1 volume
        VCF_VCA2_VOL    = 0x0D, // VCA 2 volume

        // --- VCF subpage 1 (buttons) ---
        VCF_MOD_KEYB    = 0x01,
        VCF_MOD_LAG     = 0x02,
        VCF_MOD_LEV1    = 0x03,
        VCF_MOD_VIB     = 0x04,

        // --- FM/LAG subpage 0 ---
        FM_AMP          = 0x08, // FM amplitude
        FM_DEST         = 0x09, // FM destination (0=VCO, 1=VCF)
        LAG_IN          = 0x0B, // Lag input source
        LAG_RATE        = 0x0C, // Lag rate

        // --- FM/LAG subpage 1 (buttons) ---
        LAG_LEGATO      = 0x03,
        LAG_LIN_EXP     = 0x08, // Linear/Exponential timing
        LAG_EQUAL_TIME  = 0x0B, // Equal time mode

        // --- ENV subpage 0 ---
        ENV_DELAY       = 0x08,
        ENV_ATTACK      = 0x09,
        ENV_DECAY       = 0x0A,
        ENV_SUSTAIN     = 0x0B,
        ENV_RELEASE     = 0x0C,
        ENV_AMP         = 0x0D,

        // --- ENV subpage 1 (mode flags and trigger buttons) ---
        ENV_MODE_RESET  = 0x01,
        ENV_MODE_FREERUN= 0x02,
        ENV_MODE_DADR   = 0x03,
        ENV_TRIG_MULTI  = 0x09, // Single (off) / Multi (on)
        ENV_TRIG_EXTRIG = 0x0A,
        ENV_TRIG_LFO    = 0x0B, // LFO retrigger enable
        ENV_TRIG_LFOSRC = 0x0C, // LFO source selector (rotary, subpage 1)
        ENV_TRIG_GATED  = 0x0D,

        // --- LFO subpage 0 ---
        LFO_SPEED       = 0x08,
        LFO_WAVESHAPE   = 0x09,
        LFO_SAMPLE_IN   = 0x0A, // Sample-and-hold input source
        LFO_RETRIG      = 0x0B, // Retrigger point
        LFO_AMP         = 0x0D,

        // --- LFO subpage 1 ---
        LFO_LAG         = 0x00, // Lag flag (button)
        LFO_RETRIG_MODE = 0x1A, // 26 - Retrigger mode (rotary encoder only)

        // --- TRACK subpage 0 ---
        TRACK_INPUT     = 0x08, // Tracking generator input source
        TRACK_PT1       = 0x09, // Point 1
        TRACK_PT2       = 0x0A, // Point 2
        TRACK_PT3       = 0x0B, // Point 3
        TRACK_PT4       = 0x0C, // Point 4
        TRACK_PT5       = 0x0D, // Point 5

        // --- RAMP subpage 0 ---
        RAMP_RATE       = 0x09,

        // --- RAMP subpage 1 (trigger buttons) ---
        RAMP_TRIG_MULTI = 0x09, // Single / Multi
        RAMP_TRIG_EXT   = 0x0A,
        RAMP_TRIG_LFO   = 0x0B, // LFO retrigger enable
        RAMP_TRIG_LFOSRC= 0x0C, // LFO source selector
        RAMP_TRIG_GATED = 0x0D,
    };

} // namespace Matrix12
