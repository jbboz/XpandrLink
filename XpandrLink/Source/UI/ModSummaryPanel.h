/*
  ModSummaryPanel.h
  Version: 5.0 — Split into .h/.cpp (TASK-13a)

  Shows DEST (selected destination name) at top, then 6 source slots arranged as columns:
    SRC   [vfd] [vfd] [vfd] [vfd] [vfd] [vfd]
    VAL   [vfd] [vfd] [vfd] [vfd] [vfd] [vfd]
          [knb] [knb] [knb] [knb] [knb] [knb]

  When a destination is selected (setCurrentDestination), slots filter to mods for that dest.
  Without a selection, the first 6 active mods across all destinations are shown.
  Left-click a slot → ModAmountEditor; right-click → remove menu.
*/
#pragma once
#include <JuceHeader.h>
#include "OberheimLookAndFeel.h"
#include "ThemeData.h"
#include "HardwareComponents.h"
#include "ModAmountEditor.h"
#include "../Data/XpanderData.h"
#include "../Engine/MidiEngine.h"

class ModSummaryPanel : public juce::Component
{
public:
    static constexpr int   kSlotCount   = 6;
    static constexpr float kSlotFontSize = 10.0f;
    static constexpr float kXBtnW       = 10.0f;  // width/height of the × remove button on each slot

    ModSummaryPanel() {}

    void setMidiEngine(MidiEngine* e) { engine = e; }

    // Called when the user clicks a param to select it as the current mod destination.
    // Pass -1 to clear the filter (show first 6 active mods from all destinations).
    void setCurrentDestination(int destIdx);

    std::function<void(int srcIdx, int destIdx)> onRemoveRequested;
    std::function<void(int srcIdx, int destIdx, int newAmount)> onAmountChanged;
    std::function<void(int oldSrcIdx, int newSrcIdx, int destIdx, int amount, int idSource)> onSourceChanged;
    // Called when user picks a source for an empty slot (creates a new mod entry)
    std::function<void(int srcIdx, int destIdx)> onNewSourceSelected;

    void addEntry(int srcIdx, int destIdx, int amount, int idSource = -1);
    int  getNextIdSourceForDest(int destIdx) const;
    int  getIdSourceForEntry(int srcIdx, int destIdx) const;
    void decrementIdSourceAfterRemove(int destIdx, int removedIdSource);
    void removeEntry(int srcIdx, int destIdx);
    void updateFromPatch(const std::vector<int>& patchData);

    // Slot-keyed API (session 60): hardware front-panel mod-matrix edits (cmd=0x0F)
    // address an existing entry by (destIdx, idSource) alone -- several of the
    // commands (amount dial, sign, quantize, delete) don't carry the source id at
    // all, only the destination (from page/subpage context) and the hardware slot.
    struct SlotEntry { int srcIdx = -1; int amount = 0; bool quantize = false; };
    bool getEntryAtSlot(int destIdx, int idSource, SlotEntry& out) const;
    void setSourceAtSlot  (int destIdx, int idSource, int newSrcIdx);
    void setAmountAtSlot  (int destIdx, int idSource, int newAmount);
    void setQuantizeAtSlot(int destIdx, int idSource, bool quantize);
    void removeAtSlot     (int destIdx, int idSource);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent&) override { dragSlotIdx = -1; dragModIdx = -1; }

private:
    MidiEngine* engine = nullptr;
    int currentDestIdx = -1;
    juce::String currentDestName { "--------" };

    // Drag state for bipolar knob interaction
    int dragSlotIdx  = -1;
    int dragModIdx   = -1;
    int dragStartY   = 0;
    int dragStartAmt = 0;

    struct ModEntry {
        int slotIndex = 0;
        int srcIdx = -1;
        int destIdx = -1;
        juce::String sourceName;
        juce::String destName;
        int amount = 0;
        bool quantize = false;
        int idSource = -1;
    };
    std::vector<ModEntry> activeMods;

    std::vector<const ModEntry*> buildSlots() const;
    void renumber();

    struct SlotLayout {
        int   srcRowTop;   // Y top of SRC row (derived from paint() constants)
        int   srcRowBot;   // Y bottom of SRC row
        float slotColW;    // width of one slot column
        float slotAreaX;   // X of the first slot column
    };

    // Single source of truth for slot geometry — derived from the same constants
    // used in paint(), so mouse handlers stay in sync if the layout ever changes.
    SlotLayout computeSlotLayout() const;

    // Returns slot index 0-5 for the click position, or -1 if outside slot area.
    int hitTestSlot(int mx, int my) const;

    void drawVfdBox(juce::Graphics& g, const ThemeData& theme,
                    juce::Rectangle<float> r, const juce::String& text, bool lit,
                    juce::Colour overrideColor = juce::Colour(0)) const;

    void drawMiniKnob(juce::Graphics& g, const ThemeData& theme,
                      float x, float y, float sz, float norm, bool negative) const;
};
