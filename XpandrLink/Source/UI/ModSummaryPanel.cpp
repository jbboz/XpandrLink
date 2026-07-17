/*
  ModSummaryPanel.cpp
  Implementations split out from ModSummaryPanel.h (TASK-13a).
*/
#include "ModSummaryPanel.h"

void ModSummaryPanel::setCurrentDestination(int destIdx)
{
    currentDestIdx = destIdx;
    // ENV destinations (destIdx 20-44 = ENV1-5, 5 params each): display as "ENV1"-"ENV5"
    if (destIdx >= 20 && destIdx <= 44) {
        int envNum = (destIdx - 20) / 5 + 1;
        currentDestName = "ENV" + juce::String(envNum);
    } else if (destIdx >= 0 && destIdx < (int)ModDestinations.size()) {
        currentDestName = ModDestinations[destIdx];
    } else {
        currentDestName = "--------";
    }
    repaint();
}

void ModSummaryPanel::addEntry(int srcIdx, int destIdx, int amount, int idSource)
{
    for (auto& e : activeMods)
    {
        if (e.srcIdx == srcIdx && e.destIdx == destIdx)
        {
            e.amount = amount;
            if (idSource >= 0) e.idSource = idSource;
            renumber();
            repaint();
            return;
        }
    }
    ModEntry e;
    e.srcIdx     = srcIdx;
    e.destIdx    = destIdx;
    e.sourceName = (srcIdx  < (int)ModSources.size())      ? ModSources[srcIdx]       : "Src:" + juce::String(srcIdx);
    e.destName   = (destIdx < (int)ModDestinations.size())  ? ModDestinations[destIdx] : "Dst:" + juce::String(destIdx);
    e.amount     = amount;
    e.idSource   = idSource;
    activeMods.push_back(e);
    renumber();
    repaint();
}

int ModSummaryPanel::getNextIdSourceForDest(int destIdx) const
{
    bool used[kSlotCount] = {};
    for (const auto& e : activeMods)
        if (e.destIdx == destIdx && e.idSource >= 0 && e.idSource < 6)
            used[e.idSource] = true;
    for (int i = 0; i < kSlotCount; ++i)
        if (!used[i]) return i;
    return -1;
}

int ModSummaryPanel::getIdSourceForEntry(int srcIdx, int destIdx) const
{
    for (const auto& e : activeMods)
        if (e.srcIdx == srcIdx && e.destIdx == destIdx)
            return e.idSource;
    return -1;
}

void ModSummaryPanel::decrementIdSourceAfterRemove(int destIdx, int removedIdSource)
{
    for (auto& e : activeMods)
        if (e.destIdx == destIdx && e.idSource > removedIdSource)
            e.idSource--;
}

void ModSummaryPanel::removeEntry(int srcIdx, int destIdx)
{
    activeMods.erase(std::remove_if(activeMods.begin(), activeMods.end(),
        [&](const ModEntry& e){ return e.srcIdx == srcIdx && e.destIdx == destIdx; }),
        activeMods.end());
    renumber();
    repaint();
}

bool ModSummaryPanel::getEntryAtSlot(int destIdx, int idSource, SlotEntry& out) const
{
    for (const auto& e : activeMods)
        if (e.destIdx == destIdx && e.idSource == idSource)
        {
            out.srcIdx   = e.srcIdx;
            out.amount   = e.amount;
            out.quantize = e.quantize;
            return true;
        }
    return false;
}

void ModSummaryPanel::setSourceAtSlot(int destIdx, int idSource, int newSrcIdx)
{
    for (auto& e : activeMods)
        if (e.destIdx == destIdx && e.idSource == idSource)
        {
            e.srcIdx     = newSrcIdx;
            e.sourceName = (newSrcIdx < (int)ModSources.size()) ? ModSources[newSrcIdx]
                                                                 : "Src:" + juce::String(newSrcIdx);
            repaint();
            return;
        }
}

void ModSummaryPanel::setAmountAtSlot(int destIdx, int idSource, int newAmount)
{
    for (auto& e : activeMods)
        if (e.destIdx == destIdx && e.idSource == idSource)
        {
            e.amount = newAmount;
            repaint();
            return;
        }
}

void ModSummaryPanel::setQuantizeAtSlot(int destIdx, int idSource, bool quantize)
{
    for (auto& e : activeMods)
        if (e.destIdx == destIdx && e.idSource == idSource)
        {
            e.quantize = quantize;
            repaint();
            return;
        }
}

void ModSummaryPanel::removeAtSlot(int destIdx, int idSource)
{
    activeMods.erase(std::remove_if(activeMods.begin(), activeMods.end(),
        [&](const ModEntry& e){ return e.destIdx == destIdx && e.idSource == idSource; }),
        activeMods.end());
    renumber();
    repaint();
}

void ModSummaryPanel::updateFromPatch(const std::vector<int>& patchData)
{
    activeMods.clear();
    if ((int)patchData.size() < 188) { repaint(); return; }

    std::map<int, int> destSlotCount;
    for (int i = 0; i < 20; ++i)
    {
        int base    = 128 + i * 3;
        int src     = patchData[base];
        int amtByte = patchData[base + 1];
        int dst     = patchData[base + 2];

        int absAmt = amtByte & 0x3F;
        bool neg   = (amtByte & 0x40) != 0;
        bool quant = (amtByte & 0x80) != 0;
        int amount = neg ? -absAmt : absAmt;

        if (src <= 26 && dst <= 46 && amount != 0)
        {
            ModEntry e;
            e.srcIdx     = src;
            e.destIdx    = dst;
            e.slotIndex  = i + 1;
            e.sourceName = (src < (int)ModSources.size())      ? ModSources[src]       : "Src:" + juce::String(src);
            e.destName   = (dst < (int)ModDestinations.size()) ? ModDestinations[dst]  : "Dst:" + juce::String(dst);
            e.amount     = amount;
            e.quantize   = quant;
            e.idSource   = destSlotCount[dst]++;
            activeMods.push_back(e);
        }
    }
    repaint();
}

void ModSummaryPanel::paint(juce::Graphics& g)
{
    auto theme = ThemeData::getHardwareTheme();
    auto bounds = getLocalBounds().toFloat();

    // Background + border
    g.setColour(theme.moduleBackground);
    g.fillRect(bounds);
    g.setColour(theme.groupOutline);
    g.drawRect(bounds, 1.0f);

    // Header band
    float hdrH = 22.0f;
    g.setColour(theme.headerBand);
    g.fillRect(bounds.removeFromTop(hdrH));
    g.setColour(theme.textLabel);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText("MODULATION", getLocalBounds().withHeight((int)hdrH).withTrimmedLeft(10), juce::Justification::centredLeft);

    auto inner = bounds.reduced(6, 4);

    // DEST row
    const float destRowH = 22.0f;
    auto destRow = inner.removeFromTop(destRowH);
    inner.removeFromTop(3.0f);

    g.setColour(theme.textLabel);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    auto destLbl = destRow.removeFromLeft(36.0f);
    g.drawText("DEST:", destLbl.toNearestInt(), juce::Justification::centredLeft);

    drawVfdBox(g, theme, destRow.reduced(2, 1), currentDestName, currentDestIdx >= 0);

    inner.removeFromTop(4.0f);

    auto slots = buildSlots();

    // Remaining area split into: SRC labels col + 6 slot columns
    float labelColW = 26.0f;
    float slotColW  = (inner.getWidth() - labelColW) / (float)kSlotCount;

    // SRC row label
    const float srcRowH  = 22.0f;
    const float valRowH  = 20.0f;
    const float knobSize = 32.0f;

    auto srcRow  = inner.removeFromTop(srcRowH);
    inner.removeFromTop(2.0f);
    auto valRow  = inner.removeFromTop(valRowH);
    inner.removeFromTop(2.0f);
    auto knobRow = inner.removeFromTop(knobSize);

    g.setColour(theme.textLabel);
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.drawText("SRC", srcRow.removeFromLeft((int)labelColW).toNearestInt(), juce::Justification::centredRight);
    g.drawText("VAL", valRow.removeFromLeft((int)labelColW).toNearestInt(), juce::Justification::centredRight);
    knobRow.removeFromLeft((int)labelColW);

    for (int s = 0; s < kSlotCount; ++s)
    {
        const ModEntry* slot = slots[s];
        bool active = (slot != nullptr);

        float sx = srcRow.getX()  + s * slotColW;
        float vx = valRow.getX()  + s * slotColW;
        float kx = knobRow.getX() + s * slotColW;

        // Use the compact 4-char source name in the cramped slot VFD (TASK-04);
        // slot->sourceName keeps the full name for the right-click remove menu.
        juce::String srcStr = active
            ? (slot->srcIdx < (int)ModSourcesShort.size() ? ModSourcesShort[slot->srcIdx]
                                                          : slot->sourceName)
            : "---";
        juce::String valStr = active ? (slot->amount > 0 ? "+" : "") + juce::String(slot->amount) : "---";

        // Source VFD
        drawVfdBox(g, theme,
            juce::Rectangle<float>(sx + 2, srcRow.getY() + 1, slotColW - 4, srcRowH - 2),
            srcStr, active);

        // × remove button — top-right corner of the SRC VFD, visible only on active slots
        if (active)
        {
            float xBtnLeft = sx + slotColW - 2.0f - kXBtnW;
            float xBtnTop  = srcRow.getY() + 1.0f;
            g.setColour(theme.textLabel.withAlpha(0.5f));
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.drawText("x", juce::Rectangle<float>(xBtnLeft, xBtnTop, kXBtnW, kXBtnW).toNearestInt(),
                       juce::Justification::centred);
        }

        // Value VFD — blue for positive, red for negative (bipolar design spec)
        drawVfdBox(g, theme,
            juce::Rectangle<float>(vx + 2, valRow.getY() + 1, slotColW - 4, valRowH - 2),
            valStr, active,
            active ? (slot->amount > 0 ? juce::Colour(0xff6478ff)
                    : slot->amount < 0 ? juce::Colour(0xffff5050)
                                       : theme.textValue)
                   : theme.vfdDim);

        // Mini knob arc
        float norm = active ? std::abs(slot->amount) / 63.0f : 0.0f;
        drawMiniKnob(g, theme,
            kx + (slotColW - knobSize) * 0.5f,
            knobRow.getY(),
            knobSize, norm, active && slot->amount < 0);
    }
}

void ModSummaryPanel::mouseDown(const juce::MouseEvent& e)
{
    auto lay = computeSlotLayout();

    if (e.y >= lay.srcRowTop && e.y < lay.srcRowBot && e.x >= (int)lay.slotAreaX
        && !e.mods.isRightButtonDown())
    {
        // Click on SRC VFD — open source picker (for both active and empty slots)
        int col = juce::jlimit(0, kSlotCount - 1, (int)((e.x - lay.slotAreaX) / lay.slotColW));
        auto builtSlots = buildSlots();

        // × remove button — top-right corner of the SRC VFD
        float xBtnLeft = lay.slotAreaX + (col + 1) * lay.slotColW - 2.0f - kXBtnW;
        bool inXBtn = (e.x >= (int)xBtnLeft && e.y <= lay.srcRowTop + 1 + (int)kXBtnW);
        if (inXBtn && col < (int)builtSlots.size() && builtSlots[col] != nullptr)
        {
            if (onRemoveRequested)
                onRemoveRequested(builtSlots[col]->srcIdx, builtSlots[col]->destIdx);
            dragSlotIdx = -1;
            return;
        }

        auto srcVfdScreen = localAreaToGlobal(juce::Rectangle<int>(
            (int)(lay.slotAreaX + col * lay.slotColW + 2), lay.srcRowTop + 1,
            (int)(lay.slotColW - 4), lay.srcRowBot - lay.srcRowTop - 2));

        if (col < (int)builtSlots.size() && builtSlots[col] != nullptr)
        {
            // Active slot — change source
            const ModEntry* target = builtSlots[col];
            int modIdx = -1;
            for (int i = 0; i < (int)activeMods.size(); ++i)
                if (&activeMods[i] == target) { modIdx = i; break; }

            if (modIdx >= 0)
            {
                VfdPopupList::show(ModSources, activeMods[modIdx].srcIdx, srcVfdScreen,
                    [this, modIdx](int newSrcIdx) {
                        if (modIdx >= (int)activeMods.size()) return;
                        int oldSrcIdx = activeMods[modIdx].srcIdx;
                        if (newSrcIdx == oldSrcIdx) return;
                        activeMods[modIdx].srcIdx    = newSrcIdx;
                        activeMods[modIdx].sourceName = newSrcIdx < (int)ModSources.size()
                            ? ModSources[newSrcIdx] : "Src:" + juce::String(newSrcIdx);
                        repaint();
                        if (onSourceChanged)
                            onSourceChanged(oldSrcIdx, newSrcIdx, activeMods[modIdx].destIdx,
                                            activeMods[modIdx].amount, activeMods[modIdx].idSource);
                    });
            }
        }
        else if (currentDestIdx >= 0)
        {
            // Empty slot — open picker to create a new mod entry
            int destIdx = currentDestIdx;
            VfdPopupList::show(ModSources, 0, srcVfdScreen,
                [this, destIdx](int newSrcIdx) {
                    if (onNewSourceSelected) onNewSourceSelected(newSrcIdx, destIdx);
                });
        }
        dragSlotIdx = -1;
        return;
    }

    // VAL / knob row — drag to adjust amount
    int slotIdx = hitTestSlot(e.x, e.y);
    dragSlotIdx = slotIdx;
    dragStartY  = e.y;

    if (slotIdx < 0) return;
    auto builtSlots = buildSlots();
    if (slotIdx >= (int)builtSlots.size() || builtSlots[slotIdx] == nullptr) { dragSlotIdx=-1; return; }

    const ModEntry* target = builtSlots[slotIdx];
    int modIdx = -1;
    for (int i = 0; i < (int)activeMods.size(); ++i)
        if (&activeMods[i] == target) { modIdx = i; break; }
    if (modIdx < 0) { dragSlotIdx=-1; return; }
    dragModIdx    = modIdx;
    dragStartAmt  = activeMods[modIdx].amount;

    if (e.mods.isRightButtonDown())
    {
        dragSlotIdx = -1;
        auto entry = activeMods[modIdx];
        juce::PopupMenu m;
        m.addItem(1, "Remove: " + entry.sourceName + " > " + entry.destName);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [this, entry](int result) {
                if (result == 1 && onRemoveRequested)
                    onRemoveRequested(entry.srcIdx, entry.destIdx);
            });
    }
}

void ModSummaryPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (dragSlotIdx < 0 || dragModIdx < 0) return;
    if (dragModIdx >= (int)activeMods.size()) return;

    // ~100px = full 0→63 range; drag up = more positive, down = more negative
    int delta = -(e.y - dragStartY) * 63 / 100;
    int newAmt = juce::jlimit(-63, 63, dragStartAmt + delta);

    if (newAmt != activeMods[dragModIdx].amount)
    {
        activeMods[dragModIdx].amount = newAmt;
        repaint();
        int destIdx = activeMods[dragModIdx].destIdx;
        int idSrc   = activeMods[dragModIdx].idSource;
        int srcIdx  = activeMods[dragModIdx].srcIdx;
        if (engine) engine->changeModulationAmount(destIdx, idSrc, newAmt);
        if (onAmountChanged) onAmountChanged(srcIdx, destIdx, newAmt);
    }
}

std::vector<const ModSummaryPanel::ModEntry*> ModSummaryPanel::buildSlots() const
{
    std::vector<const ModEntry*> slots;
    for (const auto& me : activeMods) {
        if (currentDestIdx < 0 || me.destIdx == currentDestIdx)
            slots.push_back(&me);
        if ((int)slots.size() == kSlotCount) break;
    }
    while ((int)slots.size() < kSlotCount) slots.push_back(nullptr);
    return slots;
}

void ModSummaryPanel::renumber()
{
    for (int i = 0; i < (int)activeMods.size(); ++i)
        activeMods[i].slotIndex = i + 1;
}

ModSummaryPanel::SlotLayout ModSummaryPanel::computeSlotLayout() const
{
    const float hdrH         = 22.0f;
    const float insetTop     =  4.0f;   // from bounds.reduced(6, 4)
    const float insetLeft    =  6.0f;
    const float destRowH     = 22.0f;
    const float gapAfterDest =  3.0f;
    const float gapBeforeSrc =  4.0f;
    const float srcRowH      = 22.0f;
    const float labelColW    = 26.0f;

    float srcTop = hdrH + insetTop + destRowH + gapAfterDest + gapBeforeSrc;
    float colW   = ((float)getWidth() - insetLeft * 2.0f - labelColW) / (float)kSlotCount;
    return { (int)srcTop, (int)(srcTop + srcRowH), colW, insetLeft + labelColW };
}

int ModSummaryPanel::hitTestSlot(int mx, int my) const
{
    auto lay = computeSlotLayout();

    if (my < lay.srcRowTop) return -1;
    if (mx < (int)lay.slotAreaX) return -1;

    int col = juce::jlimit(0, kSlotCount - 1, (int)((mx - lay.slotAreaX) / lay.slotColW));
    return col;
}

void ModSummaryPanel::drawVfdBox(juce::Graphics& g, const ThemeData& theme,
                                  juce::Rectangle<float> r, const juce::String& text, bool lit,
                                  juce::Colour overrideColor) const
{
    g.setColour(theme.vfdBackground);
    g.fillRect(r);
    g.setColour(theme.vfdBorder);
    g.drawRect(r, 1.0f);

    juce::Colour textColor = lit ? theme.textValue : theme.vfdDim;
    if (overrideColor.getAlpha() > 0) textColor = overrideColor;

    g.setColour(textColor);
    g.setFont(ThemeData::getVfdFont(kSlotFontSize));
    g.drawText(text, r.reduced(3, 0).toNearestInt(), juce::Justification::centred, true);
}

void ModSummaryPanel::drawMiniKnob(juce::Graphics& g, const ThemeData& /*theme*/,
                                    float x, float y, float sz, float norm, bool negative) const
{
    // norm = |amount|/63  (0-1),  negative = sign flag
    // Bipolar: arc from 12-o'clock toward + (blue, CW) or - (red, CCW)
    float cx = x + sz * 0.5f;
    float cy = y + sz * 0.5f;
    float r  = sz * 0.38f;

    // Body
    juce::ColourGradient grad(juce::Colour(0xff3a3a3a), cx, cy - r * 0.5f,
                              juce::Colour(0xff111111), cx, cy + r, false);
    g.setGradientFill(grad);
    g.fillEllipse(x + sz * 0.12f, y + sz * 0.12f, sz * 0.76f, sz * 0.76f);
    g.setColour(juce::Colour(0xff050505));
    g.drawEllipse(x + sz * 0.12f, y + sz * 0.12f, sz * 0.76f, sz * 0.76f, 1.2f);

    // JUCE addCentredArc: 0 = 12 o'clock (top), positive angle = CW.
    // x = cx + r*sin(a),  y = cy - r*cos(a)  in screen coords.
    const float pi         = juce::MathConstants<float>::pi;
    const float halfArcRad = 145.0f * pi / 180.0f;   // 2.531 rad = 145° CW from top
    const float centerRad  = 0.0f;                   // 12 o'clock

    // Gray center-reference dot at 12 o'clock (sin(0)=0, cos(0)=1 → top)
    g.setColour(juce::Colour(0xff555555));
    g.fillEllipse(cx - 1.5f, cy - r - 1.5f, 3.0f, 3.0f);

    // Arc and tick — endRad positive=CW(+), negative=CCW(−)
    float endRad = (negative ? -1.0f : 1.0f) * norm * halfArcRad;

    if (norm > 0.01f)
    {
        // addCentredArc sweeps from centerRad to endRad; CCW when endRad < centerRad
        juce::Path arc;
        arc.addCentredArc(cx, cy, r, r, 0.0f, centerRad, endRad, true);
        juce::Colour arcCol = negative ? juce::Colour(0xffff5050) : juce::Colour(0xff6478ff);
        g.setColour(arcCol);
        g.strokePath(arc, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    // Tick pointer using JUCE angle convention (sin/cos swapped from math convention)
    float nx1 = cx + (r - sz * 0.16f) * std::sin(endRad);
    float ny1 = cy - (r - sz * 0.16f) * std::cos(endRad);
    float nx2 = cx + (r + sz * 0.02f) * std::sin(endRad);
    float ny2 = cy - (r + sz * 0.02f) * std::cos(endRad);
    g.setColour(juce::Colour(0xffd4d8dc));
    g.drawLine(nx1, ny1, nx2, ny2, 1.3f);
}
