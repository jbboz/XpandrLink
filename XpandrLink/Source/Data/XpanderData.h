/*
  XpanderData.h
  Complete Oberheim Xpander / Matrix-12 parameter catalog.

  Each XpanderParam entry maps a named parameter to its hardware MIDI page,
  param-column byte, display range, and UI representation.

  isButton = true  → sent on subpage 1 (button mode)
  isBitmask = true → N toggle buttons sharing one XpanderParam ID; the first
                     button uses paramCol, the next uses paramCol+1, etc.
*/
#pragma once
#include <JuceHeader.h>
#include "../Engine/SynthDefs.h"

struct XpanderParam
{
    int             id;
    int             page;       // Matrix12::Page value
    int             paramCol;   // MIDI param-column byte (Matrix12::Ctrl value)
    juce::String    name;
    juce::String    group;
    int             min;
    int             max;
    juce::StringArray choices;
    bool            isBitmask  = false; // toggle buttons; subsequent bits use paramCol+1, +2 …
    bool            isButton   = false; // single on/off toggle; sent on subpage 1
    // Render choices as mutually-exclusive LED radio buttons (subpage 1); value = selected index.
    bool            isRadioLED = false;
};

// ---------------------------------------------------------------------------
// Shared choice lists
// ---------------------------------------------------------------------------

// 27 modulation sources (indices must match hardware EnumModulationSources)
inline const juce::StringArray ModSources = {
    "Kbd", "Lag", "Vel", "Rel Vel", "Pressure",
    "Trk 1", "Trk 2", "Trk 3",
    "Rmp 1", "Rmp 2", "Rmp 3", "Rmp 4",
    "Env 1", "Env 2", "Env 3", "Env 4", "Env 5",
    "Ped 1", "Ped 2",
    "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5",
    "Vib", "Lev 1", "Lev 2"
};

// Abbreviated source names for Track/Lag VFD displays (max 5 chars)
// Index matches ModSources exactly; "Pressure"→"Press", "Rel Vel"→"R Vel"
inline const juce::StringArray TrackLagSources = {
    "Kbd", "Lag", "Vel", "R Vel", "Press",
    "Trk 1", "Trk 2", "Trk 3",
    "Rmp 1", "Rmp 2", "Rmp 3", "Rmp 4",
    "Env 1", "Env 2", "Env 3", "Env 4", "Env 5",
    "Ped 1", "Ped 2",
    "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5",
    "Vib", "Lev 1", "Lev 2"
};

// Compact 4-char (max) source names for the cramped mod-matrix slot VFDs, where the
// spaced names ("Rmp 2") truncate to 3 glyphs and DSEG14 makes "RMP2" read as "AMP12"
// (TASK-04). Index matches ModSources exactly.
inline const juce::StringArray ModSourcesShort = {
    "KBD", "LAG", "VEL", "RVEL", "PRES",
    "TRK1", "TRK2", "TRK3",
    "RMP1", "RMP2", "RMP3", "RMP4",
    "ENV1", "ENV2", "ENV3", "ENV4", "ENV5",
    "PED1", "PED2",
    "LFO1", "LFO2", "LFO3", "LFO4", "LFO5",
    "VIB", "LEV1", "LEV2"
};

// LFO trigger sources (indices match hardware EnumLFOTriggerSources)
inline const juce::StringArray LfoTrigSources = {
    "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "Vib"
};

// Mod-matrix destination names (matches hardware EnumModulationDestinations)
inline const juce::StringArray ModDestinations = {
    "VCO1 Freq", "VCO1 PW", "VCO1 Vol",
    "VCO2 Freq", "VCO2 PW", "VCO2 Vol",
    "VCF Freq",  "VCF Res",
    "VCA1 Vol",  "VCA2 Vol",
    "LFO1 Spd",  "LFO1 Amp",
    "LFO2 Spd",  "LFO2 Amp",
    "LFO3 Spd",  "LFO3 Amp",
    "LFO4 Spd",  "LFO4 Amp",
    "LFO5 Spd",  "LFO5 Amp",
    "Env1 Dly",  "Env1 Atk", "Env1 Dcy", "Env1 Rel", "Env1 Amp",
    "Env2 Dly",  "Env2 Atk", "Env2 Dcy", "Env2 Rel", "Env2 Amp",
    "Env3 Dly",  "Env3 Atk", "Env3 Dcy", "Env3 Rel", "Env3 Amp",
    "Env4 Dly",  "Env4 Atk", "Env4 Dcy", "Env4 Rel", "Env4 Amp",
    "Env5 Dly",  "Env5 Atk", "Env5 Dcy", "Env5 Rel", "Env5 Amp",
    "FM Amp",    "Lag Rate"
};

// VCF filter modes (15 modes; index 0-14 matches hardware EnumVCFFilterModes)
inline const juce::StringArray VcfFilterModes = {
    "1P LP", "2P LP", "3P LP", "4P LP",
    "1P HP", "2P HP", "3P HP",
    "2P BP", "4P BP",
    "2P Notch", "3P Phase",
    "2H+1L", "3H+1L",
    "2N+1L", "3Ph+1L"
};

// LFO waveshapes (7 shapes)
inline const juce::StringArray LfoShapes = {
    "Tri", "Up Saw", "Dn Saw", "Square", "Random", "Noise", "Sample"
};

// LFO retrigger modes (4)
inline const juce::StringArray LfoRetrigModes = {
    "Off", "Single", "Multi", "Extrig"
};

// FM destination
inline const juce::StringArray FmDestinations = { "VCO", "VCF" };

// ---------------------------------------------------------------------------
// Full parameter catalog
// ---------------------------------------------------------------------------
inline const std::vector<XpanderParam> XpanderParams = {

    // -----------------------------------------------------------------------
    // VCO 1  (page 0x20)   IDs 0-10
    // -----------------------------------------------------------------------
    { 0,  Matrix12::PAGE_VCO1, Matrix12::VCO_FREQ,     "Freq",    "VCO 1",  0,  63, {} },
    { 1,  Matrix12::PAGE_VCO1, Matrix12::VCO_DETUNE,   "Detune",  "VCO 1", -31, 31, {} },
    { 2,  Matrix12::PAGE_VCO1, Matrix12::VCO_PW,       "PW",      "VCO 1",  0,  63, {} },
    // Waveform buttons BEFORE VOL so they appear between PW and VOL in the flex row
    { 4,  Matrix12::PAGE_VCO1, Matrix12::VCO_WAVE_TRI, "Wave",    "VCO 1",  0,  7,
          { "TRI", "SAW", "PULSE" }, /*isBitmask*/true },
    { 3,  Matrix12::PAGE_VCO1, Matrix12::VCO_VOL,      "Vol",     "VCO 1",  0,  63, {} },
    // Modulation flags
    { 5,  Matrix12::PAGE_VCO1, Matrix12::VCO_MOD_KEYB, "Keyb",   "VCO 1 Mod", 0, 1, {}, false, true },
    { 6,  Matrix12::PAGE_VCO1, Matrix12::VCO_MOD_LAG,  "Lag",    "VCO 1 Mod", 0, 1, {}, false, true },
    { 7,  Matrix12::PAGE_VCO1, Matrix12::VCO_MOD_LEV1, "Lev 1",  "VCO 1 Mod", 0, 1, {}, false, true },
    { 8,  Matrix12::PAGE_VCO1, Matrix12::VCO_MOD_VIB,  "Vib",    "VCO 1 Mod", 0, 1, {}, false, true },

    // -----------------------------------------------------------------------
    // VCO 2  (page 0x21)   IDs 20-32
    // -----------------------------------------------------------------------
    { 20, Matrix12::PAGE_VCO2, Matrix12::VCO_FREQ,     "Freq",   "VCO 2",  0,  63, {} },
    { 21, Matrix12::PAGE_VCO2, Matrix12::VCO_DETUNE,   "Detune", "VCO 2", -31, 31, {} },
    { 22, Matrix12::PAGE_VCO2, Matrix12::VCO_PW,       "PW",     "VCO 2",  0,  63, {} },
    // Waveform buttons BEFORE VOL; SYNC after VOL wraps to row 2 (below FREQ)
    { 24, Matrix12::PAGE_VCO2, Matrix12::VCO_WAVE_TRI, "Wave",   "VCO 2",  0, 15,
          { "TRI", "SAW", "PULSE", "NOISE" }, /*isBitmask*/true },
    { 23, Matrix12::PAGE_VCO2, Matrix12::VCO_VOL,      "Vol",    "VCO 2",  0,  63, {} },
    // Sync — wraps to row 2 and appears below FREQ
    { 25, Matrix12::PAGE_VCO2, Matrix12::VCO_WAVE_SYNC,"Sync",   "VCO 2",  0,  1, {}, false, true },
    // Modulation flags
    { 26, Matrix12::PAGE_VCO2, Matrix12::VCO_MOD_KEYB, "Keyb",  "VCO 2 Mod", 0, 1, {}, false, true },
    { 27, Matrix12::PAGE_VCO2, Matrix12::VCO_MOD_LAG,  "Lag",   "VCO 2 Mod", 0, 1, {}, false, true },
    { 28, Matrix12::PAGE_VCO2, Matrix12::VCO_MOD_LEV1, "Lev 1", "VCO 2 Mod", 0, 1, {}, false, true },
    { 29, Matrix12::PAGE_VCO2, Matrix12::VCO_MOD_VIB,  "Vib",   "VCO 2 Mod", 0, 1, {}, false, true },

    // -----------------------------------------------------------------------
    // VCF / VCA  (page 0x22)   IDs 40-48
    // -----------------------------------------------------------------------
    { 40, Matrix12::PAGE_VCF, Matrix12::VCF_FREQ,     "Freq",    "VCF",  0, 127, {} },
    { 41, Matrix12::PAGE_VCF, Matrix12::VCF_RES,      "Res",     "VCF",  0,  63, {} },
    { 42, Matrix12::PAGE_VCF, Matrix12::VCF_MODE,     "Mode",    "VCF",  0,  14, VcfFilterModes },
    { 43, Matrix12::PAGE_VCF, Matrix12::VCF_VCA1_VOL, "VCA1",    "VCF",  0,  63, {} },
    { 44, Matrix12::PAGE_VCF, Matrix12::VCF_VCA2_VOL, "VCA2",    "VCF",  0,  63, {} },
    // Modulation flags
    { 45, Matrix12::PAGE_VCF, Matrix12::VCF_MOD_KEYB, "Keyb",   "VCF Mod", 0, 1, {}, false, true },
    { 46, Matrix12::PAGE_VCF, Matrix12::VCF_MOD_LAG,  "Lag",    "VCF Mod", 0, 1, {}, false, true },
    { 47, Matrix12::PAGE_VCF, Matrix12::VCF_MOD_LEV1, "Lev 1",  "VCF Mod", 0, 1, {}, false, true },
    { 48, Matrix12::PAGE_VCF, Matrix12::VCF_MOD_VIB,  "Vib",    "VCF Mod", 0, 1, {}, false, true },

    // -----------------------------------------------------------------------
    // FM / LAG  (page 0x23)   IDs 60-66
    // -----------------------------------------------------------------------
    { 60, Matrix12::PAGE_FM, Matrix12::FM_AMP,       "FM Amp",  "FM",  0,  63, {} },
    { 61, Matrix12::PAGE_FM, Matrix12::FM_DEST,      "FM Dest", "FM",  0,   1, FmDestinations },
    { 62, Matrix12::PAGE_FM, Matrix12::LAG_IN,       "Lag In",  "LAG",  0,  26, TrackLagSources },
    { 63, Matrix12::PAGE_FM, Matrix12::LAG_RATE,     "Lag Rate","LAG",  0,  63, {} },
    // Lag mode buttons (subpage 1)
    { 64, Matrix12::PAGE_FM, Matrix12::LAG_LEGATO,    "Legato",    "LAG", 0, 1, {}, false, true },
    { 65, Matrix12::PAGE_FM, Matrix12::LAG_LIN_EXP,   "Timing",    "LAG", 0, 1, {"Linear","Expo"}, false, true,  true },
    { 66, Matrix12::PAGE_FM, Matrix12::LAG_EQUAL_TIME, "Eq Time",  "LAG", 0, 1, {}, false, true },

    // -----------------------------------------------------------------------
    // LFO 1  (page 0x30)   IDs 70-76
    // -----------------------------------------------------------------------
    { 70, Matrix12::PAGE_LFO1, Matrix12::LFO_SPEED,    "Speed", "LFO 1", 0, 63, {} },
    { 71, Matrix12::PAGE_LFO1, Matrix12::LFO_WAVESHAPE,"Wave", "LFO 1", 0,  6, LfoShapes },
    { 72, Matrix12::PAGE_LFO1, Matrix12::LFO_SAMPLE_IN,"S&H In","LFO 1", 0, 26, ModSources },
    { 73, Matrix12::PAGE_LFO1, Matrix12::LFO_RETRIG,   "Retrig","LFO 1", 0, 63, {} },
    { 74, Matrix12::PAGE_LFO1, Matrix12::LFO_AMP,      "Amp",   "LFO 1", 0, 63, {} },
    { 75, Matrix12::PAGE_LFO1, Matrix12::LFO_LAG,      "Lag",   "LFO 1 Adv", 0,  1, {}, false, true },
    { 76, Matrix12::PAGE_LFO1, Matrix12::LFO_RETRIG_MODE,"Retrig","LFO 1 Adv", 0, 3, LfoRetrigModes },

    // -----------------------------------------------------------------------
    // LFO 2  (page 0x31)   IDs 80-86
    // -----------------------------------------------------------------------
    { 80, Matrix12::PAGE_LFO2, Matrix12::LFO_SPEED,    "Speed", "LFO 2", 0, 63, {} },
    { 81, Matrix12::PAGE_LFO2, Matrix12::LFO_WAVESHAPE,"Wave", "LFO 2", 0,  6, LfoShapes },
    { 82, Matrix12::PAGE_LFO2, Matrix12::LFO_SAMPLE_IN,"S&H In","LFO 2", 0, 26, ModSources },
    { 83, Matrix12::PAGE_LFO2, Matrix12::LFO_RETRIG,   "Retrig","LFO 2", 0, 63, {} },
    { 84, Matrix12::PAGE_LFO2, Matrix12::LFO_AMP,      "Amp",   "LFO 2", 0, 63, {} },
    { 85, Matrix12::PAGE_LFO2, Matrix12::LFO_LAG,      "Lag",   "LFO 2 Adv", 0,  1, {}, false, true },
    { 86, Matrix12::PAGE_LFO2, Matrix12::LFO_RETRIG_MODE,"Retrig","LFO 2 Adv", 0, 3, LfoRetrigModes },

    // -----------------------------------------------------------------------
    // LFO 3  (page 0x32)   IDs 90-96
    // -----------------------------------------------------------------------
    { 90, Matrix12::PAGE_LFO3, Matrix12::LFO_SPEED,    "Speed", "LFO 3", 0, 63, {} },
    { 91, Matrix12::PAGE_LFO3, Matrix12::LFO_WAVESHAPE,"Wave", "LFO 3", 0,  6, LfoShapes },
    { 92, Matrix12::PAGE_LFO3, Matrix12::LFO_SAMPLE_IN,"S&H In","LFO 3", 0, 26, ModSources },
    { 93, Matrix12::PAGE_LFO3, Matrix12::LFO_RETRIG,   "Retrig","LFO 3", 0, 63, {} },
    { 94, Matrix12::PAGE_LFO3, Matrix12::LFO_AMP,      "Amp",   "LFO 3", 0, 63, {} },
    { 95, Matrix12::PAGE_LFO3, Matrix12::LFO_LAG,      "Lag",   "LFO 3 Adv", 0,  1, {}, false, true },
    { 96, Matrix12::PAGE_LFO3, Matrix12::LFO_RETRIG_MODE,"Retrig","LFO 3 Adv", 0, 3, LfoRetrigModes },

    // -----------------------------------------------------------------------
    // LFO 4  (page 0x33)   IDs 100-106
    // -----------------------------------------------------------------------
    { 100, Matrix12::PAGE_LFO4, Matrix12::LFO_SPEED,    "Speed", "LFO 4", 0, 63, {} },
    { 101, Matrix12::PAGE_LFO4, Matrix12::LFO_WAVESHAPE,"Wave", "LFO 4", 0,  6, LfoShapes },
    { 102, Matrix12::PAGE_LFO4, Matrix12::LFO_SAMPLE_IN,"S&H In","LFO 4", 0, 26, ModSources },
    { 103, Matrix12::PAGE_LFO4, Matrix12::LFO_RETRIG,   "Retrig","LFO 4", 0, 63, {} },
    { 104, Matrix12::PAGE_LFO4, Matrix12::LFO_AMP,      "Amp",   "LFO 4", 0, 63, {} },
    { 105, Matrix12::PAGE_LFO4, Matrix12::LFO_LAG,      "Lag",   "LFO 4 Adv", 0,  1, {}, false, true },
    { 106, Matrix12::PAGE_LFO4, Matrix12::LFO_RETRIG_MODE,"Retrig","LFO 4 Adv", 0, 3, LfoRetrigModes },

    // -----------------------------------------------------------------------
    // LFO 5  (page 0x34)   IDs 110-116
    // -----------------------------------------------------------------------
    { 110, Matrix12::PAGE_LFO5, Matrix12::LFO_SPEED,    "Speed", "LFO 5", 0, 63, {} },
    { 111, Matrix12::PAGE_LFO5, Matrix12::LFO_WAVESHAPE,"Wave", "LFO 5", 0,  6, LfoShapes },
    { 112, Matrix12::PAGE_LFO5, Matrix12::LFO_SAMPLE_IN,"S&H In","LFO 5", 0, 26, ModSources },
    { 113, Matrix12::PAGE_LFO5, Matrix12::LFO_RETRIG,   "Retrig","LFO 5", 0, 63, {} },
    { 114, Matrix12::PAGE_LFO5, Matrix12::LFO_AMP,      "Amp",   "LFO 5", 0, 63, {} },
    { 115, Matrix12::PAGE_LFO5, Matrix12::LFO_LAG,      "Lag",   "LFO 5 Adv", 0,  1, {}, false, true },
    { 116, Matrix12::PAGE_LFO5, Matrix12::LFO_RETRIG_MODE,"Retrig","LFO 5 Adv", 0, 3, LfoRetrigModes },

    // -----------------------------------------------------------------------
    // ENV 1  (page 0x28)   IDs 130-143
    // -----------------------------------------------------------------------
    { 130, Matrix12::PAGE_ENV1, Matrix12::ENV_DELAY,       "Delay",  "ENV 1", 0, 63, {} },
    { 131, Matrix12::PAGE_ENV1, Matrix12::ENV_ATTACK,      "Atk", "ENV 1", 0, 63, {} },
    { 132, Matrix12::PAGE_ENV1, Matrix12::ENV_DECAY,       "Dec",  "ENV 1", 0, 63, {} },
    { 133, Matrix12::PAGE_ENV1, Matrix12::ENV_SUSTAIN,     "Sus.","ENV 1", 0, 63, {} },
    { 134, Matrix12::PAGE_ENV1, Matrix12::ENV_RELEASE,     "Rel.","ENV 1", 0, 63, {} },
    { 135, Matrix12::PAGE_ENV1, Matrix12::ENV_AMP,         "Amp",    "ENV 1", 0, 63, {} },
    { 136, Matrix12::PAGE_ENV1, Matrix12::ENV_MODE_RESET,  "Reset",  "ENV 1 Mode", 0, 1, {}, false, true },
    { 137, Matrix12::PAGE_ENV1, Matrix12::ENV_MODE_FREERUN,"FREE","ENV 1 Mode", 0, 1, {}, false, true },
    { 138, Matrix12::PAGE_ENV1, Matrix12::ENV_MODE_DADR,   "DADR",   "ENV 1 Mode", 0, 1, {}, false, true },
    { 139, Matrix12::PAGE_ENV1, Matrix12::ENV_TRIG_MULTI,  "Single/Multi", "ENV 1 Trig", 0, 1, {"Single","Multi"}, false, false, true },
    { 140, Matrix12::PAGE_ENV1, Matrix12::ENV_TRIG_EXTRIG, "Extrig",   "ENV 1 Trig", 0, 1, {}, false, true },
    { 141, Matrix12::PAGE_ENV1, Matrix12::ENV_TRIG_LFO,    "LFOTRG", "ENV 1 Trig", 0, 1, {}, false, true },
    { 142, Matrix12::PAGE_ENV1, Matrix12::ENV_TRIG_LFOSRC, "","ENV 1 Trig", 0, 5, LfoTrigSources, false, true },
    { 143, Matrix12::PAGE_ENV1, Matrix12::ENV_TRIG_GATED,  "Gated",  "ENV 1 Trig", 0, 1, {}, false, true },

    // -----------------------------------------------------------------------
    // ENV 2  (page 0x29)   IDs 150-163
    // -----------------------------------------------------------------------
    { 150, Matrix12::PAGE_ENV2, Matrix12::ENV_DELAY,       "Delay",  "ENV 2", 0, 63, {} },
    { 151, Matrix12::PAGE_ENV2, Matrix12::ENV_ATTACK,      "Atk", "ENV 2", 0, 63, {} },
    { 152, Matrix12::PAGE_ENV2, Matrix12::ENV_DECAY,       "Dec",  "ENV 2", 0, 63, {} },
    { 153, Matrix12::PAGE_ENV2, Matrix12::ENV_SUSTAIN,     "Sus.","ENV 2", 0, 63, {} },
    { 154, Matrix12::PAGE_ENV2, Matrix12::ENV_RELEASE,     "Rel.","ENV 2", 0, 63, {} },
    { 155, Matrix12::PAGE_ENV2, Matrix12::ENV_AMP,         "Amp",    "ENV 2", 0, 63, {} },
    { 156, Matrix12::PAGE_ENV2, Matrix12::ENV_MODE_RESET,  "Reset",  "ENV 2 Mode", 0, 1, {}, false, true },
    { 157, Matrix12::PAGE_ENV2, Matrix12::ENV_MODE_FREERUN,"FREE","ENV 2 Mode", 0, 1, {}, false, true },
    { 158, Matrix12::PAGE_ENV2, Matrix12::ENV_MODE_DADR,   "DADR",   "ENV 2 Mode", 0, 1, {}, false, true },
    { 159, Matrix12::PAGE_ENV2, Matrix12::ENV_TRIG_MULTI,  "Single/Multi", "ENV 2 Trig", 0, 1, {"Single","Multi"}, false, false, true },
    { 160, Matrix12::PAGE_ENV2, Matrix12::ENV_TRIG_EXTRIG, "Extrig",   "ENV 2 Trig", 0, 1, {}, false, true },
    { 161, Matrix12::PAGE_ENV2, Matrix12::ENV_TRIG_LFO,    "LFOTRG", "ENV 2 Trig", 0, 1, {}, false, true },
    { 162, Matrix12::PAGE_ENV2, Matrix12::ENV_TRIG_LFOSRC, "","ENV 2 Trig", 0, 5, LfoTrigSources, false, true },
    { 163, Matrix12::PAGE_ENV2, Matrix12::ENV_TRIG_GATED,  "Gated",  "ENV 2 Trig", 0, 1, {}, false, true },

    // -----------------------------------------------------------------------
    // ENV 3  (page 0x2A)   IDs 170-183
    // -----------------------------------------------------------------------
    { 170, Matrix12::PAGE_ENV3, Matrix12::ENV_DELAY,       "Delay",  "ENV 3", 0, 63, {} },
    { 171, Matrix12::PAGE_ENV3, Matrix12::ENV_ATTACK,      "Atk", "ENV 3", 0, 63, {} },
    { 172, Matrix12::PAGE_ENV3, Matrix12::ENV_DECAY,       "Dec",  "ENV 3", 0, 63, {} },
    { 173, Matrix12::PAGE_ENV3, Matrix12::ENV_SUSTAIN,     "Sus.","ENV 3", 0, 63, {} },
    { 174, Matrix12::PAGE_ENV3, Matrix12::ENV_RELEASE,     "Rel.","ENV 3", 0, 63, {} },
    { 175, Matrix12::PAGE_ENV3, Matrix12::ENV_AMP,         "Amp",    "ENV 3", 0, 63, {} },
    { 176, Matrix12::PAGE_ENV3, Matrix12::ENV_MODE_RESET,  "Reset",  "ENV 3 Mode", 0, 1, {}, false, true },
    { 177, Matrix12::PAGE_ENV3, Matrix12::ENV_MODE_FREERUN,"FREE","ENV 3 Mode", 0, 1, {}, false, true },
    { 178, Matrix12::PAGE_ENV3, Matrix12::ENV_MODE_DADR,   "DADR",   "ENV 3 Mode", 0, 1, {}, false, true },
    { 179, Matrix12::PAGE_ENV3, Matrix12::ENV_TRIG_MULTI,  "Single/Multi", "ENV 3 Trig", 0, 1, {"Single","Multi"}, false, false, true },
    { 180, Matrix12::PAGE_ENV3, Matrix12::ENV_TRIG_EXTRIG, "Extrig",   "ENV 3 Trig", 0, 1, {}, false, true },
    { 181, Matrix12::PAGE_ENV3, Matrix12::ENV_TRIG_LFO,    "LFOTRG", "ENV 3 Trig", 0, 1, {}, false, true },
    { 182, Matrix12::PAGE_ENV3, Matrix12::ENV_TRIG_LFOSRC, "","ENV 3 Trig", 0, 5, LfoTrigSources, false, true },
    { 183, Matrix12::PAGE_ENV3, Matrix12::ENV_TRIG_GATED,  "Gated",  "ENV 3 Trig", 0, 1, {}, false, true },

    // -----------------------------------------------------------------------
    // ENV 4  (page 0x2B)   IDs 190-203
    // -----------------------------------------------------------------------
    { 190, Matrix12::PAGE_ENV4, Matrix12::ENV_DELAY,       "Delay",  "ENV 4", 0, 63, {} },
    { 191, Matrix12::PAGE_ENV4, Matrix12::ENV_ATTACK,      "Atk", "ENV 4", 0, 63, {} },
    { 192, Matrix12::PAGE_ENV4, Matrix12::ENV_DECAY,       "Dec",  "ENV 4", 0, 63, {} },
    { 193, Matrix12::PAGE_ENV4, Matrix12::ENV_SUSTAIN,     "Sus.","ENV 4", 0, 63, {} },
    { 194, Matrix12::PAGE_ENV4, Matrix12::ENV_RELEASE,     "Rel.","ENV 4", 0, 63, {} },
    { 195, Matrix12::PAGE_ENV4, Matrix12::ENV_AMP,         "Amp",    "ENV 4", 0, 63, {} },
    { 196, Matrix12::PAGE_ENV4, Matrix12::ENV_MODE_RESET,  "Reset",  "ENV 4 Mode", 0, 1, {}, false, true },
    { 197, Matrix12::PAGE_ENV4, Matrix12::ENV_MODE_FREERUN,"FREE","ENV 4 Mode", 0, 1, {}, false, true },
    { 198, Matrix12::PAGE_ENV4, Matrix12::ENV_MODE_DADR,   "DADR",   "ENV 4 Mode", 0, 1, {}, false, true },
    { 199, Matrix12::PAGE_ENV4, Matrix12::ENV_TRIG_MULTI,  "Single/Multi", "ENV 4 Trig", 0, 1, {"Single","Multi"}, false, false, true },
    { 200, Matrix12::PAGE_ENV4, Matrix12::ENV_TRIG_EXTRIG, "Extrig",   "ENV 4 Trig", 0, 1, {}, false, true },
    { 201, Matrix12::PAGE_ENV4, Matrix12::ENV_TRIG_LFO,    "LFOTRG", "ENV 4 Trig", 0, 1, {}, false, true },
    { 202, Matrix12::PAGE_ENV4, Matrix12::ENV_TRIG_LFOSRC, "","ENV 4 Trig", 0, 5, LfoTrigSources, false, true },
    { 203, Matrix12::PAGE_ENV4, Matrix12::ENV_TRIG_GATED,  "Gated",  "ENV 4 Trig", 0, 1, {}, false, true },

    // -----------------------------------------------------------------------
    // ENV 5  (page 0x2C)   IDs 210-223
    // -----------------------------------------------------------------------
    { 210, Matrix12::PAGE_ENV5, Matrix12::ENV_DELAY,       "Delay",  "ENV 5", 0, 63, {} },
    { 211, Matrix12::PAGE_ENV5, Matrix12::ENV_ATTACK,      "Atk", "ENV 5", 0, 63, {} },
    { 212, Matrix12::PAGE_ENV5, Matrix12::ENV_DECAY,       "Dec",  "ENV 5", 0, 63, {} },
    { 213, Matrix12::PAGE_ENV5, Matrix12::ENV_SUSTAIN,     "Sus.","ENV 5", 0, 63, {} },
    { 214, Matrix12::PAGE_ENV5, Matrix12::ENV_RELEASE,     "Rel.","ENV 5", 0, 63, {} },
    { 215, Matrix12::PAGE_ENV5, Matrix12::ENV_AMP,         "Amp",    "ENV 5", 0, 63, {} },
    { 216, Matrix12::PAGE_ENV5, Matrix12::ENV_MODE_RESET,  "Reset",  "ENV 5 Mode", 0, 1, {}, false, true },
    { 217, Matrix12::PAGE_ENV5, Matrix12::ENV_MODE_FREERUN,"FREE","ENV 5 Mode", 0, 1, {}, false, true },
    { 218, Matrix12::PAGE_ENV5, Matrix12::ENV_MODE_DADR,   "DADR",   "ENV 5 Mode", 0, 1, {}, false, true },
    { 219, Matrix12::PAGE_ENV5, Matrix12::ENV_TRIG_MULTI,  "Single/Multi", "ENV 5 Trig", 0, 1, {"Single","Multi"}, false, false, true },
    { 220, Matrix12::PAGE_ENV5, Matrix12::ENV_TRIG_EXTRIG, "Extrig",   "ENV 5 Trig", 0, 1, {}, false, true },
    { 221, Matrix12::PAGE_ENV5, Matrix12::ENV_TRIG_LFO,    "LFOTRG", "ENV 5 Trig", 0, 1, {}, false, true },
    { 222, Matrix12::PAGE_ENV5, Matrix12::ENV_TRIG_LFOSRC, "","ENV 5 Trig", 0, 5, LfoTrigSources, false, true },
    { 223, Matrix12::PAGE_ENV5, Matrix12::ENV_TRIG_GATED,  "Gated",  "ENV 5 Trig", 0, 1, {}, false, true },

    // -----------------------------------------------------------------------
    // TRACK 1  (page 0x38)   IDs 230-235
    // -----------------------------------------------------------------------
    { 230, Matrix12::PAGE_TRACK1, Matrix12::TRACK_INPUT, "Input",  "TRACK 1", 0, 26, TrackLagSources },
    { 231, Matrix12::PAGE_TRACK1, Matrix12::TRACK_PT1,   "Pt 1",   "TRACK 1", 0, 63, {} },
    { 232, Matrix12::PAGE_TRACK1, Matrix12::TRACK_PT2,   "Pt 2",   "TRACK 1", 0, 63, {} },
    { 233, Matrix12::PAGE_TRACK1, Matrix12::TRACK_PT3,   "Pt 3",   "TRACK 1", 0, 63, {} },
    { 234, Matrix12::PAGE_TRACK1, Matrix12::TRACK_PT4,   "Pt 4",   "TRACK 1", 0, 63, {} },
    { 235, Matrix12::PAGE_TRACK1, Matrix12::TRACK_PT5,   "Pt 5",   "TRACK 1", 0, 63, {} },

    // -----------------------------------------------------------------------
    // TRACK 2  (page 0x39)   IDs 240-245
    // -----------------------------------------------------------------------
    { 240, Matrix12::PAGE_TRACK2, Matrix12::TRACK_INPUT, "Input",  "TRACK 2", 0, 26, TrackLagSources },
    { 241, Matrix12::PAGE_TRACK2, Matrix12::TRACK_PT1,   "Pt 1",   "TRACK 2", 0, 63, {} },
    { 242, Matrix12::PAGE_TRACK2, Matrix12::TRACK_PT2,   "Pt 2",   "TRACK 2", 0, 63, {} },
    { 243, Matrix12::PAGE_TRACK2, Matrix12::TRACK_PT3,   "Pt 3",   "TRACK 2", 0, 63, {} },
    { 244, Matrix12::PAGE_TRACK2, Matrix12::TRACK_PT4,   "Pt 4",   "TRACK 2", 0, 63, {} },
    { 245, Matrix12::PAGE_TRACK2, Matrix12::TRACK_PT5,   "Pt 5",   "TRACK 2", 0, 63, {} },

    // -----------------------------------------------------------------------
    // TRACK 3  (page 0x3A)   IDs 250-255
    // -----------------------------------------------------------------------
    { 250, Matrix12::PAGE_TRACK3, Matrix12::TRACK_INPUT, "Input",  "TRACK 3", 0, 26, TrackLagSources },
    { 251, Matrix12::PAGE_TRACK3, Matrix12::TRACK_PT1,   "Pt 1",   "TRACK 3", 0, 63, {} },
    { 252, Matrix12::PAGE_TRACK3, Matrix12::TRACK_PT2,   "Pt 2",   "TRACK 3", 0, 63, {} },
    { 253, Matrix12::PAGE_TRACK3, Matrix12::TRACK_PT3,   "Pt 3",   "TRACK 3", 0, 63, {} },
    { 254, Matrix12::PAGE_TRACK3, Matrix12::TRACK_PT4,   "Pt 4",   "TRACK 3", 0, 63, {} },
    { 255, Matrix12::PAGE_TRACK3, Matrix12::TRACK_PT5,   "Pt 5",   "TRACK 3", 0, 63, {} },

    // -----------------------------------------------------------------------
    // RAMP 1  (page 0x40)   IDs 260-265
    // -----------------------------------------------------------------------
    { 260, Matrix12::PAGE_RAMP1, Matrix12::RAMP_RATE,       "Rate",        "RAMP 1", 0, 63, {} },
    { 261, Matrix12::PAGE_RAMP1, Matrix12::RAMP_TRIG_MULTI, "Single/Multi","RAMP 1 Trig", 0, 1, {"Single","Multi"}, false, false, true },
    { 262, Matrix12::PAGE_RAMP1, Matrix12::RAMP_TRIG_EXT,   "Extrig",      "RAMP 1 Trig", 0, 1, {}, false, true },
    { 263, Matrix12::PAGE_RAMP1, Matrix12::RAMP_TRIG_LFO,   "LFOTRG",      "RAMP 1 Trig", 0, 1, {}, false, true },
    { 264, Matrix12::PAGE_RAMP1, Matrix12::RAMP_TRIG_LFOSRC,"",            "RAMP 1 Trig", 0, 5, LfoTrigSources },
    { 265, Matrix12::PAGE_RAMP1, Matrix12::RAMP_TRIG_GATED, "Gated",       "RAMP 1 Trig", 0, 1, {}, false, true },

    // -----------------------------------------------------------------------
    // RAMP 2  (page 0x41)   IDs 270-275
    // -----------------------------------------------------------------------
    { 270, Matrix12::PAGE_RAMP2, Matrix12::RAMP_RATE,       "Rate",        "RAMP 2", 0, 63, {} },
    { 271, Matrix12::PAGE_RAMP2, Matrix12::RAMP_TRIG_MULTI, "Single/Multi","RAMP 2 Trig", 0, 1, {"Single","Multi"}, false, false, true },
    { 272, Matrix12::PAGE_RAMP2, Matrix12::RAMP_TRIG_EXT,   "Extrig",      "RAMP 2 Trig", 0, 1, {}, false, true },
    { 273, Matrix12::PAGE_RAMP2, Matrix12::RAMP_TRIG_LFO,   "LFOTRG",      "RAMP 2 Trig", 0, 1, {}, false, true },
    { 274, Matrix12::PAGE_RAMP2, Matrix12::RAMP_TRIG_LFOSRC,"",            "RAMP 2 Trig", 0, 5, LfoTrigSources },
    { 275, Matrix12::PAGE_RAMP2, Matrix12::RAMP_TRIG_GATED, "Gated",       "RAMP 2 Trig", 0, 1, {}, false, true },

    // -----------------------------------------------------------------------
    // RAMP 3  (page 0x42)   IDs 280-285
    // -----------------------------------------------------------------------
    { 280, Matrix12::PAGE_RAMP3, Matrix12::RAMP_RATE,       "Rate",        "RAMP 3", 0, 63, {} },
    { 281, Matrix12::PAGE_RAMP3, Matrix12::RAMP_TRIG_MULTI, "Single/Multi","RAMP 3 Trig", 0, 1, {"Single","Multi"}, false, false, true },
    { 282, Matrix12::PAGE_RAMP3, Matrix12::RAMP_TRIG_EXT,   "Extrig",      "RAMP 3 Trig", 0, 1, {}, false, true },
    { 283, Matrix12::PAGE_RAMP3, Matrix12::RAMP_TRIG_LFO,   "LFOTRG",      "RAMP 3 Trig", 0, 1, {}, false, true },
    { 284, Matrix12::PAGE_RAMP3, Matrix12::RAMP_TRIG_LFOSRC,"",            "RAMP 3 Trig", 0, 5, LfoTrigSources },
    { 285, Matrix12::PAGE_RAMP3, Matrix12::RAMP_TRIG_GATED, "Gated",       "RAMP 3 Trig", 0, 1, {}, false, true },

    // -----------------------------------------------------------------------
    // RAMP 4  (page 0x43)   IDs 290-295
    // -----------------------------------------------------------------------
    { 290, Matrix12::PAGE_RAMP4, Matrix12::RAMP_RATE,       "Rate",        "RAMP 4", 0, 63, {} },
    { 291, Matrix12::PAGE_RAMP4, Matrix12::RAMP_TRIG_MULTI, "Single/Multi","RAMP 4 Trig", 0, 1, {"Single","Multi"}, false, false, true },
    { 292, Matrix12::PAGE_RAMP4, Matrix12::RAMP_TRIG_EXT,   "Extrig",      "RAMP 4 Trig", 0, 1, {}, false, true },
    { 293, Matrix12::PAGE_RAMP4, Matrix12::RAMP_TRIG_LFO,   "LFOTRG",      "RAMP 4 Trig", 0, 1, {}, false, true },
    { 294, Matrix12::PAGE_RAMP4, Matrix12::RAMP_TRIG_LFOSRC,"",            "RAMP 4 Trig", 0, 5, LfoTrigSources },
    { 295, Matrix12::PAGE_RAMP4, Matrix12::RAMP_TRIG_GATED, "Gated",       "RAMP 4 Trig", 0, 1, {}, false, true },
};
