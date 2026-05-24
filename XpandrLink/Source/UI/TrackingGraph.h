/*
  TrackingGraph.h
  5-point piecewise-linear tracking generator display with draggable handles.
  Points are evenly spaced along the X axis; Y = output value (0–63).
*/
#pragma once
#include <JuceHeader.h>
#include "OberheimLookAndFeel.h"
#include "ThemeData.h"

class TrackingGraph : public juce::Component
{
public:
    TrackingGraph()
    {
        setInterceptsMouseClicks(true, false);
    }

    // Callback: index 0–4 = Pt1–Pt5
    std::function<void(int pointIndex, int newValue)> onParameterChange;

    void setParameters(int p1, int p2, int p3, int p4, int p5)
    {
        pts[0] = p1;  pts[1] = p2;  pts[2] = p3;  pts[3] = p4;  pts[4] = p5;
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragging = -1;
        auto area = getLocalBounds().toFloat().reduced(5);
        float spacing = area.getWidth() / 4.0f;
        for (int i = 0; i < 5; ++i)
        {
            float px = area.getX() + i * spacing;
            float py = area.getBottom() - (pts[i] / 63.0f) * area.getHeight();
            if (std::abs(e.position.x - px) < 14.0f && std::abs(e.position.y - py) < 14.0f)
            {
                dragging = i;
                break;
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragging < 0) return;
        auto area = getLocalBounds().toFloat().reduced(5);
        float norm = 1.0f - (e.position.y - area.getY()) / area.getHeight();
        int newVal = juce::jlimit(0, 63, (int)std::round(norm * 63.0f));
        if (onParameterChange) onParameterChange(dragging, newVal);
    }

    void mouseUp(const juce::MouseEvent&) override { dragging = -1; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto theme = ThemeData::getHardwareTheme();

        g.setColour(theme.vfdBackground);
        g.fillRect(getLocalBounds());
        g.setColour(theme.groupOutline);
        g.drawRect(getLocalBounds().toFloat(), 1.0f);

        auto area = getLocalBounds().toFloat().reduced(5);
        float h = area.getHeight();
        float spacing = area.getWidth() / 4.0f;

        // Subtle grid
        g.setColour(theme.vfdDim.withAlpha(0.5f));
        for (int i = 0; i < 5; ++i)
            g.drawLine(area.getX() + i * spacing, area.getY(),
                       area.getX() + i * spacing, area.getBottom(), 0.5f);
        float midY = area.getY() + h * 0.5f;
        g.drawLine(area.getX(), midY, area.getRight(), midY, 0.5f);

        // Build 5-point path
        juce::Path path;
        for (int i = 0; i < 5; ++i)
        {
            float x = area.getX() + i * spacing;
            float y = area.getBottom() - (pts[i] / 63.0f) * h;
            if (i == 0) path.startNewSubPath(x, y);
            else        path.lineTo(x, y);
        }

        // Fill below line
        juce::Path fill = path;
        fill.lineTo(area.getRight(), area.getBottom());
        fill.lineTo(area.getX(),     area.getBottom());
        fill.closeSubPath();
        g.setColour(theme.textValue.withAlpha(0.15f));
        g.fillPath(fill);

        // Line
        g.setColour(theme.textValue);
        g.strokePath(path, juce::PathStrokeType(2.0f));

        // Handles
        float r = 4.0f;
        for (int i = 0; i < 5; ++i)
        {
            float x = area.getX() + i * spacing;
            float y = area.getBottom() - (pts[i] / 63.0f) * h;
            g.setColour(dragging == i ? juce::Colours::yellow : juce::Colours::white);
            g.fillEllipse(x - r, y - r, r * 2, r * 2);
        }
    }

private:
    int pts[5]  = { 31, 31, 31, 31, 31 };
    int dragging = -1;
};
