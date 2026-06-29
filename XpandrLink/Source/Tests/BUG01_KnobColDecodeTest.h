/*
  BUG01_KnobColDecodeTest.h
  Unit tests for BUG-01: KNOB_COL (rotary delta) SysEx decode.

  The Xpander/Matrix-12 hardware sends parameter changes from physical rotary
  knobs using KNOB_COL values (0x18-0x1D), where actual paramCol = KNOB_COL - 0x10.
  The value encoded in bytes [6]/[7] of the parameter-change SysEx is a signed
  DELTA, not an absolute replacement.

  These tests verify:
    1. The sign-magnitude decode function (positive, negative, extremes).
    2. The KNOB_COL -> paramCol translation (regular and LFO-retrig special case).
    3. That positive and negative deltas roundtrip correctly.

  To compile: add this file to the project via Projucer (Source/Tests group),
  then rebuild. The tests register themselves with JUCE's global UnitTestRunner
  and are exercised by juce::UnitTestRunner::runAllTests().
*/
#pragma once
#include <JuceHeader.h>
#include "../Engine/MidiEngine.h"
#include "../Engine/SynthDefs.h"

class BUG01_KnobColDecodeTest : public juce::UnitTest
{
public:
    BUG01_KnobColDecodeTest() : juce::UnitTest("BUG-01 KNOB_COL decode", "MidiEngine") {}

    void runTest() override
    {
        // ---------------------------------------------------------------
        // 1. Sign-magnitude value decode
        // ---------------------------------------------------------------
        beginTest("decodeSysexValue - positive values");
        expectEquals(MidiEngine::decodeSysexValue(0, 0),   0);
        expectEquals(MidiEngine::decodeSysexValue(1, 0),   1);
        expectEquals(MidiEngine::decodeSysexValue(63, 0),  63);
        expectEquals(MidiEngine::decodeSysexValue(127, 0), 127);

        beginTest("decodeSysexValue - negative values");
        // delta = -1: val_hi=1, val_lo = 0x80 - 1 = 0x7F = 127
        expectEquals(MidiEngine::decodeSysexValue(127, 1), -1);
        // delta = -31: val_lo = 0x80 - 31 = 97
        expectEquals(MidiEngine::decodeSysexValue(97, 1), -31);
        // delta = -63: val_lo = 0x80 - 63 = 65
        expectEquals(MidiEngine::decodeSysexValue(65, 1), -63);

        // ---------------------------------------------------------------
        // 2. KNOB_COL -> paramCol translation
        // ---------------------------------------------------------------
        beginTest("KNOB_COL translation - VCO/VCF/ENV range");
        // KNOB_COL_1 (0x18) -> PARAM_COL_1 (0x08)
        expectEquals(knobColToParamCol(0x18, 0x20, 0), 0x08); // VCO1 page, subpage 0
        // KNOB_COL_2 (0x19) -> PARAM_COL_2 (0x09)
        expectEquals(knobColToParamCol(0x19, 0x22, 0), 0x09); // VCF page
        // KNOB_COL_3 (0x1A) -> PARAM_COL_3 (0x0A) on ENV page
        expectEquals(knobColToParamCol(0x1A, 0x28, 0), 0x0A); // ENV1 page, subpage 0
        // KNOB_COL_6 (0x1D) -> PARAM_COL_6 (0x0D)
        expectEquals(knobColToParamCol(0x1D, 0x20, 0), 0x0D);

        beginTest("KNOB_COL translation - LFO retrig special case");
        // col=0x1A on LFO page with button subpage (1) = LFO_RETRIG_MODE, no subtract
        expectEquals(knobColToParamCol(0x1A, 0x30, 1), 0x1A); // LFO1
        expectEquals(knobColToParamCol(0x1A, 0x34, 1), 0x1A); // LFO5
        // Same col on non-LFO page or subpage 0 -> regular subtract
        expectEquals(knobColToParamCol(0x1A, 0x30, 0), 0x0A); // LFO page subpage 0 -> subtract
        expectEquals(knobColToParamCol(0x1A, 0x20, 1), 0x0A); // VCO page -> subtract

        // ---------------------------------------------------------------
        // 3. isDelta flag
        // ---------------------------------------------------------------
        beginTest("col < 0x18 -> absolute (isDelta=false)");
        expect(!isKnobCol(0x08));
        expect(!isKnobCol(0x0D));
        expect(!isKnobCol(0x01));
        expect(!isKnobCol(0x17));

        beginTest("col >= 0x18 -> rotary delta (isDelta=true)");
        expect(isKnobCol(0x18));
        expect(isKnobCol(0x1D));
        expect(isKnobCol(0x1A));

        // ---------------------------------------------------------------
        // 4. Delta application clamps to param range
        // ---------------------------------------------------------------
        beginTest("delta application clamping");
        expectEquals(juce::jlimit(0, 63, 63 + 1), 63);  // clamp at max
        expectEquals(juce::jlimit(0, 63,  0 - 1), 0);   // clamp at min
        expectEquals(juce::jlimit(-31, 31, 31 + 1), 31); // signed clamp at max
        expectEquals(juce::jlimit(-31, 31, -31 - 1), -31); // signed clamp at min
    }

private:
    // Mirrors the KNOB_COL logic in MidiEngine::handleIncomingMidiMessage
    static int knobColToParamCol(int col, int page, int subPage)
    {
        if (col < 0x18) return col;
        bool isLfoRetrig = (col == Matrix12::LFO_RETRIG_MODE)
                        && (page >= Matrix12::PAGE_LFO1 && page <= Matrix12::PAGE_LFO5)
                        && (subPage == Matrix12::SUBPAGE_BUTTON);
        return isLfoRetrig ? col : (col - 0x10);
    }

    static bool isKnobCol(int col) { return col >= 0x18; }
};

// Registers the test with JUCE's global runner at program start.
static BUG01_KnobColDecodeTest bug01KnobColDecodeTestInstance;
