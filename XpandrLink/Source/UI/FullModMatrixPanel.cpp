/*
  FullModMatrixPanel.cpp
  Implementations split out from FullModMatrixPanel.h (TASK-13a).
*/
#include "FullModMatrixPanel.h"

FullModMatrixPanel::FullModMatrixPanel(ModAssignmentLogic* modLogic)
    : modAssignmentLogic(modLogic)
{
    // Pre-populate 20 empty slots (Matrix-12 hardware maximum)
    slots.resize(20);
    for (int i = 0; i < 20; ++i) slots[i].slotIndex = i + 1;
}

void FullModMatrixPanel::addEntry(int srcIdx, int destIdx, int amount, int idSource)
{
    for (auto& s : slots)
    {
        if (s.active && s.srcIdx == srcIdx && s.destIdx == destIdx)
        {
            s.amount = amount;
            if (idSource >= 0) s.idSource = idSource;
            repaint();
            return;
        }
    }
    for (auto& s : slots)
    {
        if (!s.active)
        {
            s.active   = true;
            s.srcIdx   = srcIdx;
            s.destIdx   = destIdx;
            s.srcName  = (srcIdx  < (int)ModSources.size())      ? ModSources[srcIdx]       : "Src:" + juce::String(srcIdx);
            s.dstName  = (destIdx < (int)ModDestinations.size()) ? ModDestinations[destIdx] : "Dst:" + juce::String(destIdx);
            s.amount   = amount;
            s.quantize = false;
            s.idSource = idSource;
            repaint();
            return;
        }
    }
}

void FullModMatrixPanel::updateEntrySource(int oldSrcIdx, int destIdx, int newSrcIdx)
{
    for (auto& s : slots)
    {
        if (s.active && s.srcIdx == oldSrcIdx && s.destIdx == destIdx)
        {
            s.srcIdx  = newSrcIdx;
            s.srcName = newSrcIdx < (int)ModSources.size()
                      ? ModSources[newSrcIdx] : "Src:" + juce::String(newSrcIdx);
            repaint();
            return;
        }
    }
}

void FullModMatrixPanel::updateEntryDestination(int srcIdx, int oldDstIdx, int newDstIdx, int newIdSource)
{
    for (auto& s : slots)
    {
        if (s.active && s.srcIdx == srcIdx && s.destIdx == oldDstIdx)
        {
            s.destIdx   = newDstIdx;
            s.dstName  = newDstIdx < (int)ModDestinations.size()
                       ? ModDestinations[newDstIdx] : "Dst:" + juce::String(newDstIdx);
            if (newIdSource >= 0) s.idSource = newIdSource;
            repaint();
            return;
        }
    }
}

int FullModMatrixPanel::getIdSourceForEntry(int srcIdx, int destIdx) const
{
    for (const auto& s : slots)
        if (s.active && s.srcIdx == srcIdx && s.destIdx == destIdx)
            return s.idSource;
    return -1;
}

void FullModMatrixPanel::decrementIdSourceAfterRemove(int destIdx, int removedIdSource)
{
    for (auto& s : slots)
        if (s.active && s.destIdx == destIdx && s.idSource > removedIdSource)
            s.idSource--;
}

void FullModMatrixPanel::removeEntry(int srcIdx, int destIdx)
{
    for (auto& s : slots)
    {
        if (s.active && s.srcIdx == srcIdx && s.destIdx == destIdx)
        {
            s.active  = false;
            s.srcIdx  = -1;
            s.destIdx  = -1;
            s.srcName = "";
            s.dstName = "";
            s.amount  = 0;
            repaint();
            return;
        }
    }
}

void FullModMatrixPanel::updateFromPatch(const std::vector<int>& patchData)
{
    slots.clear();
    slots.resize(20);
    for (int i = 0; i < 20; ++i) slots[i].slotIndex = i + 1;

    if ((int)patchData.size() < 188) { repaint(); return; }

    std::map<int, int> destSlotCount;
    for (int i = 0; i < 20; ++i)
    {
        int base   = 128 + i * 3;
        int src    = patchData[base];
        int amt    = patchData[base + 1];
        int dst    = patchData[base + 2];
        int absAmt = amt & 0x3F;
        bool neg   = (amt & 0x40) != 0;
        bool quant = (amt & 0x80) != 0;
        int amount = neg ? -absAmt : absAmt;

        slots[i].active    = (src <= 26 && dst <= 46 && amount != 0);
        slots[i].srcIdx    = src;
        slots[i].destIdx    = dst;
        slots[i].srcName   = slots[i].active ? ((src < (int)ModSources.size())      ? ModSources[src]       : "Src:" + juce::String(src)) : "";
        slots[i].dstName   = slots[i].active ? ((dst < (int)ModDestinations.size()) ? ModDestinations[dst]  : "Dst:" + juce::String(dst)) : "";
        slots[i].amount    = amount;
        slots[i].quantize  = quant;
        slots[i].idSource  = slots[i].active ? destSlotCount[dst]++ : -1;
    }
    repaint();
}

bool FullModMatrixPanel::getEntryAtSlot(int destIdx, int idSource, SlotEntry& out) const
{
    for (const auto& s : slots)
        if (s.active && s.destIdx == destIdx && s.idSource == idSource)
        {
            out.srcIdx   = s.srcIdx;
            out.amount   = s.amount;
            out.quantize = s.quantize;
            return true;
        }
    return false;
}

void FullModMatrixPanel::setSourceAtSlot(int destIdx, int idSource, int newSrcIdx)
{
    for (auto& s : slots)
        if (s.active && s.destIdx == destIdx && s.idSource == idSource)
        {
            s.srcIdx  = newSrcIdx;
            s.srcName = newSrcIdx < (int)ModSources.size()
                      ? ModSources[newSrcIdx] : "Src:" + juce::String(newSrcIdx);
            repaint();
            return;
        }
}

void FullModMatrixPanel::setAmountAtSlot(int destIdx, int idSource, int newAmount)
{
    for (auto& s : slots)
        if (s.active && s.destIdx == destIdx && s.idSource == idSource)
        {
            s.amount = newAmount;
            repaint();
            return;
        }
}

void FullModMatrixPanel::setQuantizeAtSlot(int destIdx, int idSource, bool quantize)
{
    for (auto& s : slots)
        if (s.active && s.destIdx == destIdx && s.idSource == idSource)
        {
            s.quantize = quantize;
            repaint();
            return;
        }
}

void FullModMatrixPanel::removeAtSlot(int destIdx, int idSource)
{
    for (auto& s : slots)
        if (s.active && s.destIdx == destIdx && s.idSource == idSource)
        {
            s.active  = false;
            s.srcIdx  = -1;
            s.destIdx  = -1;
            s.srcName = "";
            s.dstName = "";
            s.amount  = 0;
            s.quantize = false;
            repaint();
            return;
        }
}

void FullModMatrixPanel::getCurrentModBytes(std::array<uint8_t, 60>& out) const
{
    out.fill(0);
    for (int i = 0; i < 20 && i < (int)slots.size(); ++i)
    {
        const auto& s = slots[i];
        if (!s.active) continue;
        int base       = i * 3;
        int absAmt     = std::abs(s.amount) & 0x3F;
        out[(size_t)base]     = (uint8_t)s.srcIdx;
        out[(size_t)base + 1] = (uint8_t)(absAmt
                                          | (s.amount  < 0 ? 0x40 : 0)
                                          | (s.quantize    ? 0x80 : 0));
        out[(size_t)base + 2] = (uint8_t)s.destIdx;
    }
}

void FullModMatrixPanel::paint(juce::Graphics& g)
{
    auto theme = ThemeData::getHardwareTheme();
    g.fillAll(theme.mainBackground);
    // Inset content 10px L/R so the matrix edges line up with the 3-column module
    // grid above (VoiceArchitectureComponent insets by reduced(10)). (TASK-31)
    auto full  = getLocalBounds().reduced(10, 0);

    // Header band
    const int hdrH = 24;
    g.setColour(theme.headerBand);
    g.fillRect(full.withHeight(hdrH));
    g.setColour(theme.textLabel);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText("MODULATION MATRIX", full.withHeight(hdrH).withTrimmedLeft(10), juce::Justification::centredLeft);

    // Grid area (below header)
    auto gridArea = full.withTrimmedTop(hdrH);
    float colW    = (float)gridArea.getWidth() / 5.0f;
    float colHdrH = 16.0f;
    float rowH    = 30.0f;

    // Column content proportions — use shared helper so paint and mouseDown stay in sync.

    for (int col = 0; col < 5; ++col)
    {
        float cx = gridArea.getX() + col * colW;
        auto [rx, sw, aw, dw, cw] = cellLayout(colW, cx);

        // Column background
        juce::Colour colBg = (col % 2 == 0) ? theme.moduleBackground : theme.moduleBackground.brighter(0.05f);
        g.setColour(colBg);
        g.fillRect(juce::Rectangle<float>(cx, (float)gridArea.getY(), colW, (float)gridArea.getHeight()));

        // Column header
        g.setColour(theme.moduleBackground.darker(0.2f));
        g.fillRect(juce::Rectangle<float>(cx, (float)gridArea.getY(), colW, colHdrH));
        g.setColour(theme.textLabel);
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        float hy = (float)gridArea.getY();
        g.drawText("SOURCE",      (int)rx,            (int)hy, (int)sw, (int)colHdrH, juce::Justification::centredLeft);
        g.drawText("AMT.",        (int)(rx + sw),      (int)hy, (int)aw, (int)colHdrH, juce::Justification::centred);
        g.drawText("DESTINATION", (int)(rx+sw+aw),     (int)hy, (int)dw, (int)colHdrH, juce::Justification::centredLeft);
        g.drawText("X",           (int)(rx+sw+aw+dw),  (int)hy, (int)cw, (int)colHdrH, juce::Justification::centred);

        // Column divider
        if (col > 0) {
            g.setColour(theme.groupOutline.withAlpha(0.5f));
            g.drawVerticalLine((int)cx, (float)gridArea.getY(), (float)gridArea.getBottom());
        }

        // 4 rows for this column
        for (int row = 0; row < 4; ++row)
        {
            int slotIdx = col * 4 + row;
            if (slotIdx >= (int)slots.size()) break;
            const auto& s = slots[slotIdx];

            float ry = gridArea.getY() + colHdrH + row * rowH;

            // Row alternate tint
            if (row % 2 != 0) {
                g.setColour(theme.mainBackground.withAlpha(0.4f));
                g.fillRect(juce::Rectangle<float>(cx, ry, colW, rowH));
            }

            // Row separator
            g.setColour(theme.groupOutline.withAlpha(0.25f));
            g.drawHorizontalLine((int)(ry + rowH - 1), cx, cx + colW);

            if (s.active)
            {
                // Source VFD — interactive (hover hint: blue border)
                drawVfdCell(g, theme, rx, ry + 4, sw - 4, rowH - 8, s.srcName, true);

                // Amount: mini knob + value text (both enlarged for readability)
                float norm = std::abs(s.amount) / 63.0f;
                bool neg   = s.amount < 0;
                float kcx  = rx + sw + aw * 0.28f;
                float kcy  = ry + rowH * 0.5f;
                float kr   = rowH * 0.42f;
                drawMiniKnobAt(g, theme, kcx, kcy, kr, norm, neg);

                juce::String amtStr = (s.amount > 0 ? "+" : "") + juce::String(s.amount);
                g.setColour(neg ? juce::Colour(0xffff5050) : juce::Colour(0xff6478ff));
                g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
                g.drawText(amtStr, (int)(rx + sw + aw * 0.55f), (int)ry, (int)(aw * 0.45f), (int)rowH,
                           juce::Justification::centredLeft);

                // Destination VFD
                drawVfdCell(g, theme, rx + sw + aw + 2, ry + 4, dw - 6, rowH - 8, s.dstName, false);

                // Clear button (×)
                g.setColour(theme.textLabel.withAlpha(0.5f));
                g.setFont(juce::Font(juce::FontOptions(10.0f)));
                g.drawText("x", (int)(rx + sw + aw + dw), (int)ry, (int)cw, (int)rowH,
                           juce::Justification::centred);
            }
            else
            {
                g.setColour(theme.textLabel.withAlpha(0.2f));
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.drawText("----", (int)rx, (int)ry, (int)(colW - 8), (int)rowH, juce::Justification::centredLeft);
            }
        }
    }

    // Outer border
    g.setColour(theme.groupOutline);
    g.drawRect(full, 1.0f);
}

void FullModMatrixPanel::mouseDown(const juce::MouseEvent& e)
{
    auto gridBounds = getLocalBounds().reduced(10, 0).withTrimmedTop(24);

    float colW    = (float)gridBounds.getWidth() / 5.0f;
    float colHdrH = 16.0f;
    float rowH    = 30.0f;

    float lx = e.x - gridBounds.getX();
    float ly = e.y - gridBounds.getY() - colHdrH;
    if (ly < 0) return;

    int col = (int)(lx / colW);
    int row = (int)(ly / rowH);
    if (col < 0 || col >= 5 || row < 0 || row >= 4) return;

    int slotIdx = col * 4 + row;
    if (slotIdx >= (int)slots.size()) return;

    float colBaseX = gridBounds.getX() + col * colW;
    float rowBaseY = gridBounds.getY() + colHdrH + row * rowH;
    auto [rx, sw, aw, dw, cw] = cellLayout(colW, colBaseX);
    float lxInCol = lx - col * colW - 4.0f;  // 4.0f = colOffset (rx = cx+4)

    // Clear (×) button — hit area extended to ≥16px for easier clicking
    float clearHitX = sw + aw + dw - juce::jmax(0.0f, 16.0f - cw);
    if (lxInCol > clearHitX && slots[slotIdx].active)
    {
        if (onRemoveRequested)
            onRemoveRequested(slots[slotIdx].srcIdx, slots[slotIdx].destIdx);
        return;
    }

    if (!slots[slotIdx].active) return;

    // Right-click → remove
    if (e.mods.isRightButtonDown())
    {
        auto& s = slots[slotIdx];
        juce::PopupMenu m;
        m.addItem(1, "Remove: " + s.srcName + " > " + s.dstName);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [this, slotIdx](int result) {
                if (result == 1 && slotIdx < (int)slots.size() && slots[slotIdx].active && onRemoveRequested)
                    onRemoveRequested(slots[slotIdx].srcIdx, slots[slotIdx].destIdx);
            });
        return;
    }

    // Click on SRC VFD → VFD-style source picker
    if (lxInCol < sw)
    {
        auto srcRect = localAreaToGlobal(juce::Rectangle<int>(
            (int)rx, (int)(rowBaseY + 4),
            (int)(sw - 4), (int)(rowH - 8)));
        int curSrc = slots[slotIdx].srcIdx;
        VfdPopupList::show(ModSources, curSrc, srcRect,
            [this, slotIdx](int newSrcIdx) {
                if (slotIdx >= (int)slots.size() || !slots[slotIdx].active) return;
                int oldSrcIdx = slots[slotIdx].srcIdx;
                if (newSrcIdx == oldSrcIdx) return;
                slots[slotIdx].srcIdx  = newSrcIdx;
                slots[slotIdx].srcName = newSrcIdx < (int)ModSources.size()
                                       ? ModSources[newSrcIdx] : "Src:" + juce::String(newSrcIdx);
                repaint();
                if (onSourceChanged)
                    onSourceChanged(oldSrcIdx, newSrcIdx, slots[slotIdx].destIdx,
                                    slots[slotIdx].amount, slots[slotIdx].idSource);
            });
        return;
    }

    // Click on DEST VFD → VFD-style destination picker; also focuses ModSummaryPanel
    if (lxInCol >= sw + aw && lxInCol < sw + aw + dw)
    {
        // Focus this destination in the mod summary panel
        if (modAssignmentLogic) modAssignmentLogic->notifyDestinationFocusedDirect(slots[slotIdx].destIdx);

        auto dstRect = localAreaToGlobal(juce::Rectangle<int>(
            (int)(rx + sw + aw + 2), (int)(rowBaseY + 4),
            (int)(dw - 6), (int)(rowH - 8)));
        int curDst = slots[slotIdx].destIdx;
        VfdPopupList::show(ModDestinations, curDst, dstRect,
            [this, slotIdx](int newDstIdx) {
                if (slotIdx >= (int)slots.size() || !slots[slotIdx].active) return;
                int oldDstIdx = slots[slotIdx].destIdx;
                if (newDstIdx == oldDstIdx) return;
                slots[slotIdx].destIdx  = newDstIdx;
                slots[slotIdx].dstName = newDstIdx < (int)ModDestinations.size()
                                       ? ModDestinations[newDstIdx] : "Dst:" + juce::String(newDstIdx);
                repaint();
                if (onDestinationChanged)
                    onDestinationChanged(slots[slotIdx].srcIdx, oldDstIdx, newDstIdx,
                                         slots[slotIdx].amount, slots[slotIdx].idSource);
            });
        return;
    }

    // Click on AMT area → start drag (same as ModSummaryPanel)
    dragSlotIdx  = slotIdx;
    dragStartY   = e.y;
    dragStartAmt = slots[slotIdx].amount;
}

void FullModMatrixPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (dragSlotIdx < 0 || dragSlotIdx >= (int)slots.size() || !slots[dragSlotIdx].active) return;
    int delta  = -(e.y - dragStartY) * 63 / 100;
    int newAmt = juce::jlimit(-63, 63, dragStartAmt + delta);
    if (newAmt != slots[dragSlotIdx].amount)
    {
        slots[dragSlotIdx].amount = newAmt;
        repaint();
        if (engine) engine->changeModulationAmount(slots[dragSlotIdx].destIdx,
                                                   slots[dragSlotIdx].idSource, newAmt);
        if (onAmountChanged) onAmountChanged(slots[dragSlotIdx].srcIdx,
                                             slots[dragSlotIdx].destIdx, newAmt);
    }
}

std::tuple<float,float,float,float,float>
FullModMatrixPanel::cellLayout(float colW, float cx) const
{
    float inner = colW - 8.0f;
    float sw = inner * 0.33f;   // source — narrower (fits 8 chars), equal to dest
    float aw = inner * 0.28f;   // amount — wider for a larger knob + value font
    float dw = inner * 0.33f;   // destination — wider, equal to source
    float cw = inner * 0.06f;
    float rx = cx + 4.0f;
    return { rx, sw, aw, dw, cw };
}

void FullModMatrixPanel::drawVfdCell(juce::Graphics& g, const ThemeData& theme,
                                      float x, float y, float w, float h,
                                      const juce::String& text, bool interactive)
{
    auto cell = juce::Rectangle<float>(x, y, w, h);
    g.setColour(theme.vfdBackground);
    g.fillRect(cell);
    // Interactive SRC cells get a subtle blue border hint
    g.setColour(interactive ? theme.textLabel.withAlpha(0.5f) : theme.groupOutline.withAlpha(0.5f));
    g.drawRect(cell, 1.0f);
    g.setColour(theme.textValue);
    g.setFont(ThemeData::getVfdFont(10.0f));
    g.drawText(text, cell.reduced(3, 0).toNearestInt(), juce::Justification::centredLeft, true);
}

void FullModMatrixPanel::drawMiniKnobAt(juce::Graphics& g, const ThemeData& /*theme*/,
                                         float cx, float cy, float r,
                                         float norm, bool negative)
{
    // Bipolar arc from 12-o'clock — matches ModSummaryPanel style
    juce::ColourGradient grad(juce::Colour(0xff3a3a3a), cx, cy - r * 0.5f,
                              juce::Colour(0xff111111), cx, cy + r, false);
    g.setGradientFill(grad);
    g.fillEllipse(cx - r * 0.76f, cy - r * 0.76f, r * 1.52f, r * 1.52f);
    g.setColour(juce::Colour(0xff050505));
    g.drawEllipse(cx - r * 0.76f, cy - r * 0.76f, r * 1.52f, r * 1.52f, 0.8f);

    // Center-reference dot at 12-o'clock
    g.setColour(juce::Colour(0xff555555));
    g.fillEllipse(cx - 1.5f, cy - r - 1.5f, 3.0f, 3.0f);

    if (norm > 0.01f)
    {
        const float halfArcRad = 145.0f * juce::MathConstants<float>::pi / 180.0f;
        float endRad = (negative ? -1.0f : 1.0f) * norm * halfArcRad;
        juce::Path arc;
        arc.addCentredArc(cx, cy, r, r, 0.0f, 0.0f, endRad, true);
        g.setColour(negative ? juce::Colour(0xffff5050) : juce::Colour(0xff6478ff));
        g.strokePath(arc, juce::PathStrokeType(1.2f));
    }
}
