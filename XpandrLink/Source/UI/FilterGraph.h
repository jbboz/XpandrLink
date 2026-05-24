/*
  FilterGraph.h
  Version: 18.1 (Fixed Compile Error)
*/
#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"
#include "OberheimLookAndFeel.h"

class FilterGraph : public juce::Component
{
public:
    FilterGraph() {}

    void setParameters(int frequency, int resonance, int modeIndex)
    {
        cutoff = frequency / 127.0f; 
        res    = resonance / 63.0f;  
        mode   = modeIndex;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto theme = ThemeData::getHardwareTheme();
        juce::Colour baseColor = theme.textValue;  // Bright mint green

        // Fill VFD background
        g.setColour(theme.vfdBackground);
        g.fillRect(getLocalBounds());
        g.setColour(theme.groupOutline);
        g.drawRect(getLocalBounds().toFloat(), 1.0f);

        g.setColour(baseColor);

        auto area = getLocalBounds().toFloat().reduced(2);
        float w = area.getWidth();
        float h = area.getHeight();
        float base = h;

        float cx = w * cutoff;
        float peakH = res * (h * 0.5f);

        juce::Path p;
        p.startNewSubPath(0, base);

        // 0-3: Low Pass (1, 2, 3, 4 Pole)
        if (mode <= 3) {
            float slope = 0.3f + (mode * 0.2f); 
            p.lineTo(cx - 10, base);
            p.cubicTo(cx, base - peakH, cx, 0, w, 0 + (1.0f-slope)*h); 
            
            p.clear(); p.startNewSubPath(0, base/2);
            p.lineTo(cx, base/2 - peakH);
            p.lineTo(w, h);
        }
        // 4-6: High Pass (1, 2, 3 Pole)
        else if (mode >= 4 && mode <= 6) {
            p.clear(); p.startNewSubPath(0, h);
            p.lineTo(cx, base/2 - peakH);
            p.lineTo(w, base/2);
        }
        // 7-8: Band Pass (2, 4 Pole)
        else if (mode == 7 || mode == 8) {
            p.clear(); p.startNewSubPath(0, h);
            p.quadraticTo(cx, 0 - peakH, w, h);
        }
        // 9: Notch — dip at cutoff flanked by resonant peaks
        else if (mode == 9) {
            float flank = w * 0.12f;
            p.clear(); p.startNewSubPath(0, base/2);
            p.lineTo(juce::jmax(0.0f, cx - flank), base/2 - peakH);
            p.lineTo(cx, h); // Dip
            p.lineTo(juce::jmin(w, cx + flank), base/2 - peakH);
            p.lineTo(w, base/2);
        }
        // 10: Phase Shift — broad peak at cutoff, height scales with resonance
        else if (mode == 10) {
            p.clear(); p.startNewSubPath(0, base/2);
            p.quadraticTo(cx, base/2 - peakH * 1.6f, w, base/2);
        }
        // 11: 2H+1L — high-pass then low-pass: band-limited peak at cutoff
        else if (mode == 11) {
            p.clear(); p.startNewSubPath(0, h);
            p.lineTo(cx, base/2 - peakH);
            p.lineTo(w, h);
        }
        // 12: 3H+1L — steeper band peak at cutoff
        else if (mode == 12) {
            p.clear(); p.startNewSubPath(0, h);
            p.quadraticTo(cx, base/2 - peakH * 2.0f, w, h);
        }
        // 13: 2N+1L — notch with a low-pass roll-off
        else if (mode == 13) {
            float flank = w * 0.12f;
            p.clear(); p.startNewSubPath(0, base/2);
            p.lineTo(juce::jmax(0.0f, cx - flank), base/2 - peakH);
            p.lineTo(cx, h); // Dip
            p.lineTo(juce::jmin(w, cx + flank), base/2 - peakH * 0.6f);
            p.lineTo(w, h);
        }
        // 14: 3Ph+1L — phase peak with a low-pass roll-off
        else {
            p.clear(); p.startNewSubPath(0, base/2);
            p.quadraticTo(cx * 0.6f, base/2 - peakH * 1.6f, cx, base/2 - peakH * 0.4f);
            p.lineTo(w, h);
        }

        // 2. Stroke the line with the solid color
        g.strokePath(p, juce::PathStrokeType(2.0f));
        
        // 3. Fill with a transparent version of the SAME base color
        g.setColour(baseColor.withAlpha(0.2f));
        
        // Close the path for filling
        if (mode <= 3) { 
            p.lineTo(w, h); p.lineTo(0, h); 
        } else if (mode >= 4 && mode <= 6) { 
            p.lineTo(w, h); p.lineTo(0, h);
        } else {
            p.lineTo(w, h); p.lineTo(0, h);
        }
        p.closeSubPath();
        
        g.fillPath(p);
    }

private:
    float cutoff = 0.5f;
    float res = 0.0f;
    int mode = 0;
};