/*
  ThemeData.h
  Version: 21.0 (Consolidated: Hardware + Vintage)
*/
#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

struct ThemeData
{
    juce::String name;

    // --- Backgrounds ---
    juce::Colour mainBackground;    // Outer casing (Dark Grey)
    juce::Colour moduleBackground;  // Inner module (Slightly lighter)
    juce::Colour groupOutline;      // Tech Borders (Cyan/Blue)
    juce::Colour headerBand;        // Header Strip
    juce::Colour vfdBackground;     // Background for digital values
    
    // --- Text ---
    juce::Colour textHeader;        // Section Titles (White/Grey)
    juce::Colour textLabel;         // Param Labels (Cyan)
    juce::Colour textValue;         // Values (Glowing Green)
    
    // --- Controls ---
    juce::Colour knobBody;
    juce::Colour knobPointer;       
    juce::Colour knobIndicatorBack; 
    juce::Colour knobIndicatorActive; 
    
    juce::Colour buttonNormal;
    juce::Colour buttonActive; 
    juce::Colour signalFlowLine;

    // --- LEDs ---
    juce::Colour ledOn;
    juce::Colour ledOff;

    // --- VFD deactivated/off-state text colour (readable, brighter) ---
    juce::Colour vfdDim;
    // --- VFD ghost backing segments drawn behind a lit value (much dimmer than vfdDim) ---
    juce::Colour vfdGhost;
    // --- VFD display border (greenish, per --vfd-border) ---
    juce::Colour vfdBorder;

    // --- PRESET 1: CLASSIC HARDWARE VFD STYLE (Default) ---
    // Constructed once (static local) — safe to call on every paint().
    static const ThemeData& getHardwareTheme()
    {
        static const ThemeData t = []() {
            ThemeData t;
            t.name = "Hardware Classic";

            // Backgrounds — per DESIGN-v2 §0 tokens
            t.mainBackground      = juce::Colour(0xff3a3a3a);  // --win-bg: sRGB 58,58,58
            t.moduleBackground    = juce::Colour(0xff404040);  // --panel-bg: sRGB 64,64,64 (LIGHTER than window)
            t.groupOutline        = juce::Colour(0xff262626);  // --panel-border: sRGB 38,38,38
            t.headerBand          = juce::Colour(0xff2c5fbe);  // --title-blue: sRGB 44,95,190
            t.vfdBackground       = juce::Colour(0xff0a2d16);  // --vfd-bg: sRGB 10,45,22 — dark green (TASK-02)

            // Text — per DESIGN-v2 §0 tokens
            t.textHeader          = juce::Colour(0xffd9d8d1);  // --title-text: sRGB 217,216,209
            t.textLabel           = juce::Colour(0xff55afd2);  // --param-label: sRGB 85,175,210
            t.textValue           = juce::Colour(0xffbfffd7);  // --vfd-text: sRGB 191,255,215

            // Controls
            t.knobBody            = juce::Colour(0xff1e1e1e);  // --knob-body: sRGB 26,26,26
            t.knobPointer         = juce::Colour(0xffd2d8dc);  // notch indicator (light)
            t.knobIndicatorBack   = juce::Colour(0xff000000);
            t.knobIndicatorActive = juce::Colour(0xff78c8ff);  // --knob-arc: sRGB 120,200,255

            t.buttonNormal        = juce::Colour(0xff2c2c2c);
            t.buttonActive        = juce::Colour(0xff2c5fbe);  // matches headerBand

            // LEDs — per DESIGN-v2 §0
            t.ledOn               = juce::Colour(0xffe6281e);  // --led-toggle-on: sRGB 230,40,30
            t.ledOff              = juce::Colour(0xff461c1a);  // --led-toggle-off: sRGB 70,28,26
            t.signalFlowLine      = juce::Colour(0xff55afd2).withAlpha(0.3f);

            // VFD off-state text — brightened so deactivated text + dropdown items read
            // clearly against the darker-green vfdBackground (sRGB 95,160,122).
            t.vfdDim              = juce::Colour(0xff5fa07a);
            // Ghost backing segments behind a lit value — stays dim so active readouts
            // (knob values, patch name) don't look like they have heavy ghosting.
            t.vfdGhost            = juce::Colour(0xff28403a);
            // VFD display border — --vfd-border: rgb(28,64,40)
            t.vfdBorder           = juce::Colour(0xff1c4028);

            return t;
        }();
        return t;
    }

    // Returns the DSEG14 typeface at the requested pixel height.
    // Falls back to the default monospace font if BinaryData is not yet available.
    static juce::Font getVfdFont(float pixelHeight)
    {
        auto& face = vfdFaceRef();
        if (face == nullptr)
            face = juce::Typeface::createSystemTypefaceFor(
                BinaryData::DSEG14ClassicBoldItalic_ttf,
                BinaryData::DSEG14ClassicBoldItalic_ttfSize);
        if (face != nullptr)
            return juce::Font(juce::FontOptions(face).withHeight(pixelHeight));
        return juce::Font(juce::FontOptions(
            juce::Font::getDefaultMonospacedFontName(), pixelHeight, juce::Font::bold));
    }

    // Call during app/plugin teardown (before __cxa_finalize) so the static
    // typeface reference is released while the ObjC runtime is still alive.
    static void clearCachedFonts()
    {
        vfdFaceRef() = nullptr;
    }

private:
    static juce::Typeface::Ptr& vfdFaceRef()
    {
        static juce::Typeface::Ptr face;
        return face;
    }

public:

    // --- PRESET 2: VINTAGE VFD (Legacy Support) ---
    static ThemeData getVintageTheme()
    {
        ThemeData t;
        t.name = "Vintage VFD";
        
        t.mainBackground      = juce::Colour(0xff181818); 
        t.moduleBackground    = juce::Colour(0xff222222); 
        t.groupOutline        = juce::Colour(0xff0055aa); 
        t.headerBand          = juce::Colour(0xff004488); 
        t.vfdBackground       = juce::Colour(0xff000000); // Pure black
        
        t.textHeader          = juce::Colour(0xffffffff); 
        t.textLabel           = juce::Colour(0xff00d0ff); 
        t.textValue           = juce::Colour(0xff66ff66); 
        
        t.knobBody            = juce::Colour(0xff101010);
        t.knobPointer         = juce::Colour(0xffaaaaaa);
        t.knobIndicatorBack   = juce::Colour(0xff000000);
        t.knobIndicatorActive = juce::Colour(0xff66ff66); 
        
        t.buttonNormal        = juce::Colour(0xff333333);
        t.buttonActive        = juce::Colour(0xff66ff66).withAlpha(0.6f);
        t.ledOn               = juce::Colour(0xff66ff66); // Green LEDs
        t.ledOff              = juce::Colour(0xff003300);
        t.signalFlowLine      = juce::Colour(0xff0088ff).withAlpha(0.3f);
        t.vfdDim              = juce::Colour(0xff1a3320);
        t.vfdGhost            = juce::Colour(0xff1a3320);
        
        return t;
    }
};