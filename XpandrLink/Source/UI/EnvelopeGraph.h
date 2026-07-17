/*
  EnvelopeGraph.h
*/
#pragma once
#include <JuceHeader.h>
#include "OberheimLookAndFeel.h"
#include "ThemeData.h"

class EnvelopeGraph : public juce::Component
{
public:
    EnvelopeGraph()
    {
        setInterceptsMouseClicks(true, false);
    }

    // Callback: Index 0=Delay, 1=Attack, 2=Decay, 3=Sustain, 4=Release
    std::function<void(int parameterIndex, int newValue)> onParameterChange;

    void setParameters(int d, int a, int dec, int s, int r)
    {
        delay = d; attack = a; decay = dec; sustain = s; release = r;
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        draggingNode = -1;

        float screenDel, screenAtt, screenDec, screenSus, screenRel, screenSusY;
        nodePositions(screenDel, screenAtt, screenDec, screenSus, screenRel, screenSusY);

        const float tol = 15.0f;

        // Pick the nearest handle within tolerance. x-only distance for time handles;
        // 2D distance for the sustain knee so it beats a coincident release handle
        // (release=0 puts screenRel at the same x as the sustain knee).
        struct Candidate { int node; float dist; int startVal; };
        Candidate best { -1, tol + 1.0f, 0 };

        auto considerX = [&](int node, float screenX, int val)
        {
            float d = std::abs(e.position.x - screenX);
            if (d < tol && d < best.dist)
                best = { node, d, val };
        };
        auto consider2D = [&](int node, float sx, float sy, int val)
        {
            float dx = e.position.x - sx, dy = e.position.y - sy;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < tol && d < best.dist)
                best = { node, d, val };
        };

        considerX (0, screenDel, delay);
        considerX (1, screenAtt, attack);
        considerX (2, screenDec, decay);
        consider2D(3, screenSus, screenSusY, sustain); // knee before release
        considerX (4, screenRel, release);

        draggingNode = best.node;
        dragStartVal = best.startVal;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (draggingNode == -1) return;

        int newVal;
        if (draggingNode == 3)
        {
            // Scale to graph height so dragging top-to-bottom covers the full 0-63 range.
            float h = getLocalBounds().toFloat().reduced(5).getHeight();
            int delta = (int)std::round(-e.getDistanceFromDragStartY() * 63.0f / h);
            newVal = std::clamp(dragStartVal + delta, 0, 63);
        }
        else
        {
            // Use total distance from drag origin so repeated callbacks are idempotent.
            int delta = e.getDistanceFromDragStartX() / 2;
            newVal = std::clamp(dragStartVal + delta, 0, 63);
        }

        if (onParameterChange)
            onParameterChange(draggingNode, newVal);
    }

    void paint(juce::Graphics& g) override
    {
        auto theme = ThemeData::getHardwareTheme();

        g.setColour(theme.vfdBackground);
        g.fillRect(getLocalBounds());
        g.setColour(theme.groupOutline);
        g.drawRect(getLocalBounds().toFloat(), 1.0f);

        auto area = getLocalBounds().toFloat().reduced(5);
        float h        = area.getHeight();
        float w        = area.getWidth();
        float baseLine = area.getBottom();

        const float totalTime = 300.0f;
        float xD   = (delay   / 63.0f) * 50.0f;
        float xA   = xD  + (attack  / 63.0f) * 50.0f;
        float xDec = xA  + (decay   / 63.0f) * 50.0f;
        float xS   = xDec + 40.0f;
        float xR   = xS  + (release / 63.0f) * 50.0f;
        float scaleX = w / totalTime;
        float susY   = baseLine - (sustain / 63.0f) * h;

        juce::Point<float> pStart  { area.getX(),                  baseLine };
        juce::Point<float> pDelay  { area.getX() + xD   * scaleX,  baseLine };
        juce::Point<float> pAttack { area.getX() + xA   * scaleX,  area.getY() };
        juce::Point<float> pDecay  { area.getX() + xDec * scaleX,  susY };
        juce::Point<float> pSustain{ area.getX() + xS   * scaleX,  susY };
        juce::Point<float> pRelease{ area.getX() + xR   * scaleX,  baseLine };

        juce::Path path;
        path.startNewSubPath(pStart);
        path.lineTo(pDelay);
        path.lineTo(pAttack);
        path.lineTo(pDecay);
        path.lineTo(pSustain);
        path.lineTo(pRelease);

        g.setColour(theme.textValue.withAlpha(0.15f));
        g.fillPath(path);
        g.setColour(theme.textValue);
        g.strokePath(path, juce::PathStrokeType(2.0f));

        // Draw interactive handles
        g.setColour(juce::Colours::white);
        const float r = 4.0f;
        auto drawDot = [&](juce::Point<float> p)
        {
            g.fillEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        };

        drawDot(pDelay);
        drawDot(pAttack);
        drawDot(pDecay);
        drawDot(pSustain);  // knee before release — drag up/down for sustain level
        drawDot(pRelease);
    }

private:
    // Computes the screen x/y positions that both paint() and mouseDown() need,
    // so the hit-test geometry is always identical to the drawn geometry.
    void nodePositions(float& screenDel, float& screenAtt, float& screenDec,
                       float& screenSus, float& screenRel, float& screenSusY) const
    {
        auto area = getLocalBounds().toFloat().reduced(5);
        float h        = area.getHeight();
        float w        = area.getWidth();
        float baseLine = area.getBottom();

        const float totalTime = 300.0f;
        float xD   = (delay   / 63.0f) * 50.0f;
        float xA   = xD  + (attack  / 63.0f) * 50.0f;
        float xDec = xA  + (decay   / 63.0f) * 50.0f;
        float xS   = xDec + 40.0f;
        float xR   = xS  + (release / 63.0f) * 50.0f;
        float scaleX = w / totalTime;

        screenDel  = area.getX() + xD   * scaleX;
        screenAtt  = area.getX() + xA   * scaleX;
        screenDec  = area.getX() + xDec * scaleX;
        screenSus  = area.getX() + xS   * scaleX;
        screenRel  = area.getX() + xR   * scaleX;
        screenSusY = baseLine - (sustain / 63.0f) * h;
    }

    int delay = 0, attack = 0, decay = 0, sustain = 63, release = 0;
    int draggingNode = -1;
    int dragStartVal = 0;
};
