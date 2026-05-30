/*
  FullModMatrixPanel.h
  Version: 5.0 — Split into .h/.cpp (TASK-13a)

  5-column × 4-row grid, VFD-style source/destination pickers.
  Fits in a 180px-tall bottom pane. Shows 20 hardware slots (Matrix-12 maximum).
*/
#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"
#include "HardwareComponents.h"
#include "ModAmountEditor.h"
#include "ModAssignmentLogic.h"
#include "../Data/XpanderData.h"
#include "../Engine/MidiEngine.h"

class FullModMatrixPanel : public juce::Component
{
public:
    explicit FullModMatrixPanel(ModAssignmentLogic* modLogic = nullptr);

    void setMidiEngine(MidiEngine* e) { engine = e; }

    std::function<void(int srcIdx, int destIdx)> onRemoveRequested;
    std::function<void(int srcIdx, int destIdx, int newAmount)> onAmountChanged;
    std::function<void(int oldSrcIdx, int newSrcIdx, int destIdx, int amount, int idSource)> onSourceChanged;
    std::function<void(int srcIdx, int oldDstIdx, int newDstIdx, int amount, int idSource)> onDestinationChanged;

    void addEntry(int srcIdx, int destIdx, int amount, int idSource = -1);

    // Encode the current live slot state back to the 60-byte mod region (bytes 128–187
    // of a 196-byte decoded patch). Used by save paths so edits made via the matrix
    // UI are preserved without needing a round-trip patch dump from hardware.
    void getCurrentModBytes(std::array<uint8_t, 60>& out) const;

    // Update source in-place (preserves slot position and idSource)
    void updateEntrySource(int oldSrcIdx, int destIdx, int newSrcIdx);

    // Update destination in-place (preserves slot position)
    void updateEntryDestination(int srcIdx, int oldDstIdx, int newDstIdx, int newIdSource);

    int  getIdSourceForEntry(int srcIdx, int destIdx) const;
    void decrementIdSourceAfterRemove(int destIdx, int removedIdSource);
    void removeEntry(int srcIdx, int destIdx);
    void updateFromPatch(const std::vector<int>& patchData);

    void resized() override {} // all layout is handled in paint()
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent&) override { dragSlotIdx = -1; }

private:
    MidiEngine*         engine = nullptr;
    ModAssignmentLogic* modAssignmentLogic = nullptr;
    int dragSlotIdx = -1, dragStartY = 0, dragStartAmt = 0;

    // Column geometry shared by paint() and mouseDown().
    // Returns { rx, sw, aw, dw, cw } — positions/widths within one column.
    std::tuple<float,float,float,float,float> cellLayout(float colW, float cx) const;

    struct ModSlot {
        int slotIndex = 0;
        bool active = false;
        int srcIdx = -1, destIdx = -1;
        juce::String srcName, dstName;
        int amount = 0;
        bool quantize = false;
        int idSource = -1;
    };
    std::vector<ModSlot> slots;

    void drawVfdCell(juce::Graphics& g, const ThemeData& theme,
                     float x, float y, float w, float h,
                     const juce::String& text, bool interactive = false);

    void drawMiniKnobAt(juce::Graphics& g, const ThemeData& theme,
                        float cx, float cy, float r,
                        float norm, bool negative);
};
