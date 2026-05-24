/*
  ModulePanel.cpp
  Implementations split out from ModulePanel.h (TASK-13a).
*/
#include "ModulePanel.h"

ModulePanel::ModulePanel(const juce::String& title, const juce::String& groupID, MidiEngine* engine,
                         const juce::String& additionalGroup, ModAssignmentLogic* modLogic)
    : moduleTitle(title), myGroupID(groupID), modAssignmentLogic(modLogic)
{
    // Src buttons removed — routing via MOD panel label clicks per design spec.

    // VISUALIZERS
    if (groupID == "VCF") {
        filterGraph.reset(new FilterGraph());
        addAndMakeVisible(filterGraph.get());
    }
    else if (groupID.startsWith("ENV ") && groupID.length() == 5) {
        envGraph.reset(new EnvelopeGraph());
        envGraph->onParameterChange = [this, engine](int paramOffset, int newVal) {
            if (knobMap.empty()) return;
            int targetID = knobMap.begin()->first + paramOffset;
            updateParameter(targetID, newVal);
            for (const auto& def : XpanderParams) {
                if (def.id == targetID) {
                    engine->sendParameterToSynth(def.page, def.paramCol, newVal, false);
                    break;
                }
            }
        };
        addAndMakeVisible(envGraph.get());
    }
    else if (groupID.startsWith("TRACK ") && groupID.length() == 7) {
        trackGraph.reset(new TrackingGraph());
        trackGraph->onParameterChange = [this, engine](int pointIdx, int newVal) {
            if (knobMap.empty()) return;
            int targetID = knobMap.begin()->first + pointIdx;
            updateParameter(targetID, newVal);
            for (const auto& def : XpanderParams) {
                if (def.id == targetID) {
                    engine->sendParameterToSynth(def.page, def.paramCol, newVal, false);
                    break;
                }
            }
        };
        addAndMakeVisible(trackGraph.get());
    }

    // 3. PARAMETERS
    for (const auto& def : XpanderParams)
    {
        if (def.group == groupID || (!additionalGroup.isEmpty() && def.group == additionalGroup))
        {
            if (def.isRadioLED)              addRadioLEDControl(def, engine);
            else if (def.isBitmask)          addWaveformControl(def, engine);
            else if (def.choices.size() > 0) addMenuControl(def, engine);
            else if (def.isButton)           addButtonControl(def, engine);
            else                             addKnobControl(def, engine);
        }
    }
}

void ModulePanel::setWlistButtonStyle(bool useWlist) {
    wlistButtonMode = useWlist;
    for (auto& [id, btn] : ledMap) {
        btn->useWlistStyle = useWlist;
        btn->repaint();
    }
    for (auto& [id, btns] : radioMap) {
        for (auto* btn : btns) { btn->useWlistStyle = useWlist; btn->repaint(); }
    }
}

void ModulePanel::setPage2ButtonStyle(bool useP2) {
    wlistButtonMode = useP2;
    page2Mode = useP2;
    for (auto& [id, btn] : ledMap) {
        btn->useP2Style = useP2;
        btn->useWlistStyle = false;
        btn->repaint();
    }
    for (auto& [id, btns] : radioMap) {
        for (auto* btn : btns) { btn->useP2Style = useP2; btn->useWlistStyle = false; btn->repaint(); }
    }
}

void ModulePanel::setButtonEnabled(int paramID, bool enabled) {
    if (ledMap.count(paramID)) {
        ledMap[paramID]->setEnabled(enabled);
        if (!enabled)
            ledMap[paramID]->setToggleState(false, juce::dontSendNotification);
        ledMap[paramID]->repaint();
    }
}

void ModulePanel::setScrollListEnabled(int paramID, bool enabled) {
    if (scrollListMap.count(paramID)) {
        scrollListMap[paramID]->setEnabled(enabled);
        scrollListMap[paramID]->repaint();
    }
}

void ModulePanel::registerParams(const std::function<void(int, ModulePanel*)>& cb) {
    for (auto& [id, _] : knobMap)       cb(id, this);
    for (auto& [id, _] : menuMap)       cb(id, this);
    for (auto& [id, _] : scrollListMap) cb(id, this);
    for (auto& [id, _] : ledMap)        cb(id, this);
    for (auto& [id, _] : radioMap)      cb(id, this);
    for (auto& [id, _] : waveMap)       cb(id, this);
}

void ModulePanel::paint(juce::Graphics& g)
{
    if (isEmbed) return;
    auto theme = ThemeData::getHardwareTheme();
    auto bounds = getLocalBounds().toFloat();

    // Outer border
    g.setColour(theme.groupOutline);
    g.drawRect(bounds, 1.0f);

    // Header band (same style as GroupTabPanel)
    g.setColour(theme.headerBand);
    g.fillRect(1.0f, 1.0f, bounds.getWidth() - 2.0f, 20.0f);

    // Title — use param-label color per design spec
    g.setColour(theme.textLabel);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText(moduleTitle, 10, 0, (int)bounds.getWidth() - 20, 20, juce::Justification::centredLeft);
}

void ModulePanel::resized()
{
    auto area = getLocalBounds();
    if (!isEmbed) area.removeFromTop(20);  // skip header band

    // Embedded panels (inside GroupTabPanel) skip the top reduce so their knob labels
    // land at the same absolute Y as sibling direct-ModulePanels in the same row.
    // Bottom reduce is kept on all panels for graph clearance / LED spacing.
    area.removeFromLeft(12);
    area.removeFromRight(12);
    area.removeFromBottom(6);
    if (!isEmbed) area.removeFromTop(6);

    // Save content bottom before any flex layout; graphs fill this space dynamically.
    int panelContentBottom = area.getBottom();
    juce::Rectangle<int> filterArea, envArea; // kept for compactLayout branch only

    // Compact layout: items centered as a group horizontally (used for LFO panel)
    if (compactLayout)
    {
        // Calculate total item width and center it
        int totalW = 0;
        int nItems = 0;
        for (auto* comp : componentList) {
            if (comp->getProperties().contains("isLed")) continue;
            totalW += comp->getProperties().contains("isWaveform") ? 76 :
                      comp->getProperties().contains("isDropdown") ? 90 : 50;
            ++nItems;
        }
        const int compactGap = 14;
        totalW += (nItems > 1) ? (nItems - 1) * compactGap : 0;
        int startX = area.getX() + juce::jmax(0, (area.getWidth() - totalW) / 2);
        int cy = area.getCentreY();
        int cx = startX;
        for (auto* comp : componentList) {
            if (comp->getProperties().contains("isLed")) continue;
            int w = comp->getProperties().contains("isWaveform") ? 76 :
                    comp->getProperties().contains("isDropdown") ? 90 : 50;
            int h = 76;
            comp->setBounds(cx, cy - h/2, w, h);
            cx += w + compactGap;
        }
        // LED buttons below
        int flexBottom = 0;
        for (auto* comp : componentList)
            if (!comp->getProperties().contains("isLed"))
                flexBottom = juce::jmax(flexBottom, comp->getBottom());
        int ledX = area.getX(), ledY = flexBottom + 4;
        for (auto* comp : componentList)
            if (comp->getProperties().contains("isLed"))
                { comp->setBounds(ledX, ledY, 90, 20); ledX += 96; }
        if (filterGraph && !filterArea.isEmpty()) filterGraph->setBounds(filterArea.reduced(2));
        else if (envGraph && !envArea.isEmpty()) envGraph->setBounds(envArea.reduced(2));
        return;
    }

    // Page-2 uniform grid: every control box is the same fixed size and snaps to a
    // grid so buttons/dropdowns are uniform across all P2 modules, evenly spaced
    // vertically, and horizontally aligned. Labeled dropdowns (e.g. LFO S&H In,
    // Retrig) get +labelH on top, with their box bottom-aligned to the row so all
    // boxes in a row line up at the same height as the buttons.
    if (page2Mode)
    {
        // Cells are a uniform fixed size (cellW) so buttons + short dropdowns match.
        // Only dropdowns whose longest value can't fit (sources, retrig modes) widen
        // to wideW — these live in roomy panels (LFO Adv) so they don't force extra
        // rows. Short dropdowns (ENV/RAMP LFO Src = "LFO 1") stay uniform.
        const int cellW = 66, boxH = 26, labelH = 14;
        const int gapX  = 6,  gapY  = 8;
        int n = componentList.size();
        if (n > 0)
        {
            // Long-value dropdowns (S&H In sources up to 8 chars, retrig modes 6 chars)
            // widen to fit their text at the P2 font (~11px/char); short ones (ENV/RAMP
            // LFO Src = "LFO 1") stay uniform with the buttons.
            auto itemW = [&](int i) -> int {
                if (auto* dd = dynamic_cast<VfdDropdown*>(componentList[i])) {
                    int m = dd->maxItemLength();
                    if (m > 5) return juce::jlimit(cellW, 130, m * 11 + 18);
                }
                return cellW;
            };
            auto itemHasLabel = [&](int i) -> bool {
                if (auto* dd = dynamic_cast<VfdDropdown*>(componentList[i])) return dd->hasLabel();
                return false;
            };

            // Flow items left→right with their own widths, wrapping on the panel width.
            std::vector<int> rowOf((size_t)n), xOf((size_t)n);
            std::vector<bool> rowHasLabel; rowHasLabel.push_back(false);
            int curRow = 0, curX = area.getX();
            for (int i = 0; i < n; ++i)
            {
                int w = itemW(i);
                if (i > 0 && curX + w > area.getRight()) { curRow++; curX = area.getX(); rowHasLabel.push_back(false); }
                rowOf[(size_t)i] = curRow;
                xOf[(size_t)i]   = curX;
                if (itemHasLabel(i)) rowHasLabel[(size_t)curRow] = true;
                curX += w + gapX;
            }
            int rows = curRow + 1;

            std::vector<int> rowH((size_t)rows), rowY((size_t)rows);
            int gridH = 0;
            for (int r = 0; r < rows; ++r) { rowH[(size_t)r] = boxH + (rowHasLabel[(size_t)r] ? labelH : 0); gridH += rowH[(size_t)r]; }
            gridH += (rows - 1) * gapY;
            int y = area.getY() + juce::jmax(0, (area.getHeight() - gridH) / 2);
            for (int r = 0; r < rows; ++r) { rowY[(size_t)r] = y; y += rowH[(size_t)r] + gapY; }

            for (int i = 0; i < n; ++i)
            {
                int r = rowOf[(size_t)i];
                int ih = boxH + (itemHasLabel(i) ? labelH : 0);
                int iy = rowY[(size_t)r] + (rowH[(size_t)r] - ih); // bottom-align box
                componentList[i]->setBounds(xOf[(size_t)i], iy, itemW(i), ih);
            }
        }
        return;
    }

    // Wlist + menu: 2-row layout — menus have label above VFD, LED buttons
    // positioned so their VFD (starts at +2 within their bounds) aligns with
    // the menu VFD (starts at labelH=14 within menu bounds). Bottom-aligned.
    if (wlistButtonMode && !menuMap.empty())
    {
        const int vfdH   = 26;
        const int labelH = 14;
        const int gap    = 6;
        int x = area.getX();
        for (auto* comp : componentList)
        {
            if (comp->getProperties().contains("isLed"))
            {
                // +2 offset so VFD top (at comp.y+2) == area.getBottom()-vfdH
                comp->setBounds(x, area.getBottom() - vfdH - 2, 90, vfdH);
                x += 90 + gap;
            }
            else if (dynamic_cast<HardwareMenu*>(comp))
            {
                comp->setBounds(x, area.getBottom() - labelH - vfdH, 60, labelH + vfdH);
                x += 60 + gap;
            }
        }
        return;
    }

    // Main flex: knobs, dropdowns, waveforms.
    // In wlist mode, LED buttons join the flex (left-aligned) so they sit
    // in the same row as any menus — e.g. LFO Lag appears right of Retrig Mode.
    juce::FlexBox flex;
    flex.flexWrap       = juce::FlexBox::Wrap::wrap;
    flex.justifyContent = wlistButtonMode
        ? juce::FlexBox::JustifyContent::flexStart
        : juce::FlexBox::JustifyContent::spaceBetween;
    flex.alignContent   = juce::FlexBox::AlignContent::flexStart;

    // Track Input VfdDropdown only needs ~5 character widths; all other dropdowns use 90px.
    const bool isTrackLayout = myGroupID.startsWith("TRACK");

    for (auto* comp : componentList)
    {
        if (comp->getProperties().contains("isLed") && !wlistButtonMode) continue;

        int w = 50, h = 76;
        if      (comp->getProperties().contains("isWaveform")) { w=76; h=comp->getHeight(); }
        else if (comp->getProperties().contains("isDropdown")) { w = (isEmbed || isTrackLayout) ? 60 : 90; h = isEmbed ? 36 : 76; }
        else if (dynamic_cast<HardwareMenu*>(comp))            { w=isEmbed?60:80; h=isEmbed?26:40; }
        else if (comp->getProperties().contains("isLed"))      { w=isEmbed?60:90; h=isEmbed?26:40; }
        flex.items.add(juce::FlexItem(*comp).withWidth(w).withHeight(h).withMargin(3));
    }
    flex.performLayout(area);

    // flexBottom: includes wlist buttons (they're in the flex in that mode).
    int flexBottom = area.getY();
    for (auto* comp : componentList)
        if (!comp->getProperties().contains("isLed") || wlistButtonMode)
            flexBottom = juce::jmax(flexBottom, comp->getBottom());

    // LED section: only for normal (non-wlist) panels — buttons placed below flex.
    if (!wlistButtonMode)
    {
        const int ledW   = isEmbed ? 58 : 90;
        const int ledGap = 4;
        int ledX = area.getX();
        int ledY = flexBottom + 4;
        for (auto* comp : componentList)
        {
            if (!comp->getProperties().contains("isLed")) continue;
            // VCO2: Sync (paramID=25) gets special placement directly below Vol knob
            if (myGroupID == "VCO 2" && ledMap.count(25) && comp == ledMap.at(25)) continue;
            if (ledX + ledW > area.getRight() && ledX > area.getX()) {
                ledX = area.getX();
                ledY += 30;
            }
            comp->setBounds(ledX, ledY, ledW, 26);
            ledX += ledW + ledGap;
        }

        // VCO2: place Sync button centered below the Detune knob, VFD+LED style
        if (myGroupID == "VCO 2" && knobMap.count(21) && ledMap.count(25)) {
            auto* detuneKnob = knobMap.at(21);
            auto* syncBtn    = ledMap.at(25);
            auto detB = detuneKnob->getBounds();
            const int syncW = 76, syncH = 26;
            int syncX = detB.getX() - (syncW - detB.getWidth()) / 2;
            syncX = juce::jmax(area.getX(), juce::jmin(area.getRight() - syncW, syncX));
            syncBtn->setBounds(syncX, detB.getBottom() + 4, syncW, syncH);
        }
    }

    // Graphs: fill all space from below content to panel content bottom.
    int graphTop = flexBottom + 6;
    int graphH   = panelContentBottom - graphTop;
    if      (filterGraph && graphH > 20) filterGraph->setBounds(area.getX(), graphTop, area.getWidth(), graphH);
    else if (envGraph    && graphH > 20) envGraph->setBounds(area.getX(), graphTop, area.getWidth(), graphH);
    else if (trackGraph  && graphH > 20) trackGraph->setBounds(area.getX(), graphTop, area.getWidth(), graphH);
}

void ModulePanel::updateParameter(int paramID, int value) {
    bool changed = false;
    if (knobMap.count(paramID)) {
        knobMap[paramID]->setValue(value, juce::dontSendNotification);
        knobMap[paramID]->repaint();
        changed = true;
    }
    if (menuMap.count(paramID)) {
        menuMap[paramID]->setSelectedId(value + 1, juce::dontSendNotification);
        menuMap[paramID]->repaint();
        changed = true;
    }
    if (scrollListMap.count(paramID)) {
        scrollListMap[paramID]->setSelectedIndex(value);
        scrollListMap[paramID]->repaint();
        changed = true;
    }
    if (waveMap.count(paramID)) {
        auto& btns = waveMap[paramID];
        for (int i = 0; i < (int)btns.size(); ++i) {
            bool on = (value >> i) & 1;
            btns[i]->setToggleState(on, juce::dontSendNotification);
            btns[i]->repaint();
        }
        changed = true;
    }
    if (radioMap.count(paramID)) {
        auto& btns = radioMap[paramID];
        for (int i=0; i<(int)btns.size(); ++i) {
            btns[i]->setToggleState(i == value, juce::dontSendNotification);
            btns[i]->repaint();
        }
        changed = true;
    }
    if (ledMap.count(paramID)) {
        bool on = (value != 0);
        ledMap[paramID]->setToggleState(on, juce::dontSendNotification);
        if (radioLabelMap.count(paramID)) {
            const auto& labels = radioLabelMap.at(paramID);
            ledMap[paramID]->setName(on ? labels.second : labels.first);
        }
        ledMap[paramID]->repaint();
        changed = true;
    }
    if (changed) updateVisualizers();
}

int ModulePanel::getParameterValue(int paramID) {
    if (knobMap.count(paramID))       return (int)knobMap[paramID]->getValue();
    if (menuMap.count(paramID))       return menuMap[paramID]->getSelectedId() - 1;
    if (scrollListMap.count(paramID)) return scrollListMap[paramID]->getSelectedIndex(); // VfdDropdown
    if (waveMap.count(paramID)) { int val=0; auto& btns=waveMap[paramID]; for(int i=0; i<btns.size(); ++i) if(btns[i]->getToggleState()) val|=(1<<i); return val; }
    if (radioMap.count(paramID)) {
        auto& btns = radioMap[paramID];
        for (int i=0; i<(int)btns.size(); ++i)
            if (btns[i]->getToggleState()) return i;
        return 0;
    }
    if (ledMap.count(paramID)) return ledMap[paramID]->getToggleState() ? 1 : 0;
    return -999;
}

void ModulePanel::updateVisualizers() {
    if (filterGraph && !knobMap.empty()) {
        auto kit = knobMap.begin();
        int freq = (int)kit->second->getValue(); ++kit;
        int res  = (kit != knobMap.end()) ? (int)kit->second->getValue() : 0;
        int mode = 0;
        if (!menuMap.empty())        mode = menuMap.begin()->second->getSelectedId() - 1;
        else if (!scrollListMap.empty()) mode = scrollListMap.begin()->second->getSelectedIndex(); // VfdDropdown
        filterGraph->setParameters(std::max(0, freq), std::max(0, res), std::max(0, mode));
    }
    else if (envGraph && !knobMap.empty()) {
        // ENV module: first five knobs are Delay, Attack, Decay, Sustain, Release
        int b = knobMap.begin()->first;
        envGraph->setParameters(getParameterValue(b), getParameterValue(b+1),
                                getParameterValue(b+2), getParameterValue(b+3),
                                getParameterValue(b+4));
    }
    else if (trackGraph && !knobMap.empty()) {
        // TRACK module: knobMap holds Pt1–Pt5 (Input is in scrollListMap)
        int b = knobMap.begin()->first;
        trackGraph->setParameters(getParameterValue(b),   getParameterValue(b+1),
                                  getParameterValue(b+2), getParameterValue(b+3),
                                  getParameterValue(b+4));
    }
}

void ModulePanel::addRadioLEDControl(const XpanderParam& def, MidiEngine* engine) {
    // Single toggle button: OFF → choices[0] label (value 0), ON → choices[1] label (value 1).
    juce::String offLabel = def.choices.size() > 0 ? def.choices[0] : def.name;
    juce::String onLabel  = def.choices.size() > 1 ? def.choices[1] : def.name;
    radioLabelMap[def.id] = { offLabel, onLabel };

    auto* b = new WaveformButton(offLabel);
    b->setClickingTogglesState(true);
    b->getProperties().set("isLed", true);
    // A radio always shows a current selection — render lit in both states (P2).
    b->getProperties().set("alwaysActive", true);
    b->useWlistStyle = true;
    b->onClick = [this, engine, def, b, offLabel, onLabel] {
        bool on = b->getToggleState();
        b->setName(on ? onLabel : offLabel);
        b->repaint();
        engine->sendParameterToSynth(def.page, def.paramCol, on ? 1 : 0, true);
    };
    addAndMakeVisible(b);
    componentList.add(b);
    ledMap[def.id] = b;
}

void ModulePanel::addButtonControl(const XpanderParam& def, MidiEngine* engine) {
    auto* b = new WaveformButton(def.name);
    b->setClickingTogglesState(true);
    b->getProperties().set("isLed", true);
    b->useWlistStyle = true;
    b->onClick = [this, engine, def, b] {
        int val = b->getToggleState() ? 1 : 0;
        engine->sendParameterToSynth(def.page, def.paramCol, val, true);
        if (onParameterInteracted) onParameterInteracted(def.id, val);
    };
    b->setTitle(def.name);
    addAndMakeVisible(b); componentList.add(b); ledMap[def.id] = b;
}

void ModulePanel::addKnobControl(const XpanderParam& def, MidiEngine* engine) {
    auto* k = new HardwareKnob(def.name, modAssignmentLogic);
    k->setRange(def.min, def.max, 1);
    k->paramID = def.id;
    if (modAssignmentLogic)
        k->onAssignmentCallback = [def, engine, ml = modAssignmentLogic] { ml->assignToDestination(def.id, engine); };
    k->onValueChange = [this, engine, def, k] {
        engine->sendParameterToSynth(def.page, def.paramCol, (int)k->getValue(), false);
        updateVisualizers();
    };
    k->setTitle(def.name);
    addAndMakeVisible(k); componentList.add(k); knobMap[def.id] = k;
}

void ModulePanel::addMenuControl(const XpanderParam& def, MidiEngine* engine) {
    // Params with 4+ choices become scrollable VFD lists (BUG-21: was 6+).
    if (def.choices.size() >= 4) { addScrollListControl(def, engine); return; }

    auto* m = new HardwareMenu(def.name, modAssignmentLogic);
    for(int i=0; i<def.choices.size(); ++i) m->addItem(def.choices[i], i+1);
    m->setParamID(def.id);
    if (modAssignmentLogic)
        m->onAssignmentCallback = [def, engine, ml = modAssignmentLogic] { ml->assignToDestination(def.id, engine); };
    m->onChange = [this, engine, def, m] {
        engine->sendParameterToSynth(def.page, def.paramCol, m->getSelectedId()-1, false);
        updateVisualizers();
    };
    m->setTitle(def.name);
    addAndMakeVisible(m); componentList.add(m); menuMap[def.id] = m;
}

void ModulePanel::addScrollListControl(const XpanderParam& def, MidiEngine* engine) {
    auto* dd = new VfdDropdown(def.name, modAssignmentLogic);
    dd->setItems(def.choices);
    dd->paramID = def.id;
    dd->getProperties().set("isDropdown", true);
    if (modAssignmentLogic) {
        auto ml = modAssignmentLogic;
        dd->onAssignmentCallback = [def, engine, ml] { ml->assignToDestination(def.id, engine); };
    }
    dd->onChange = [this, engine, def](int idx) {
        engine->sendParameterToSynth(def.page, def.paramCol, idx, def.isButton);
        updateVisualizers();
        // Notify like a button click so constraint callbacks (e.g. LFO Wave=Sample
        // gating the S&H In list) re-evaluate on local scroll-list edits too (TASK-27).
        if (onParameterInteracted) onParameterInteracted(def.id, idx);
    };
    dd->setTitle(def.name);
    addAndMakeVisible(dd); componentList.add(dd); scrollListMap[def.id] = dd;
}

void ModulePanel::addWaveformControl(const XpanderParam& def, MidiEngine* engine) {
    auto* container = new juce::Component(); container->getProperties().set("isWaveform", true);
    std::vector<WaveformButton*> btns; int y=0; const int btnH=26, btnW=76;
    for (int i=0; i<def.choices.size(); ++i) {
        int btnCol = def.paramCol + i;
        auto* b = new WaveformButton(def.choices[i]);
        b->setClickingTogglesState(true);
        b->setBounds(0, y, btnW, btnH);
        b->onClick = [this, engine, def, b, btnCol] {
            engine->sendParameterToSynth(def.page, btnCol, b->getToggleState() ? 1 : 0, true);
            updateVisualizers();
        };
        container->addAndMakeVisible(b); btns.push_back(b); y += btnH + 3;
    }
    container->setSize(btnW, y); addAndMakeVisible(container); componentList.add(container); waveMap[def.id] = btns;
}
