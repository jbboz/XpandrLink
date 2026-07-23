#include "TimbreSpacePanel.h"

TimbreSpacePanel::TimbreSpacePanel()
{
    auto theme = ThemeData::getHardwareTheme();

    titleLabel_.setText("TIMBRE SPACE", juce::dontSendNotification);
    titleLabel_.setColour(juce::Label::textColourId, theme.textLabel);
    titleLabel_.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    addAndMakeVisible(titleLabel_);

    auto setupScopeBtn = [this](juce::TextButton& btn, const juce::String& text, bool wantsFavoritesOnly) {
        btn.setButtonText(text);
        btn.setClickingTogglesState(false);
        btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff2a2a2a));
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff1a4060));
        btn.setColour(juce::TextButton::textColourOffId,  juce::Colour(0xff888888));
        btn.setColour(juce::TextButton::textColourOnId,   juce::Colour(0xff55afd2));
        btn.onClick = [this, wantsFavoritesOnly] {
            if (onScopeToggled) onScopeToggled(wantsFavoritesOnly);
        };
        addAndMakeVisible(btn);
    };
    setupScopeBtn(scopeAllBtn_, "ALL", false);
    setupScopeBtn(scopeFavBtn_, "FAV", true);
    scopeAllBtn_.setToggleState(true,  juce::dontSendNotification);
    scopeFavBtn_.setToggleState(false, juce::dontSendNotification);

    readoutLabel_.setText("", juce::dontSendNotification);
    readoutLabel_.setColour(juce::Label::textColourId, theme.textValue.withAlpha(0.85f));
    readoutLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
    readoutLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(readoutLabel_);

    hintLabel_.setText("Load at least 3 library patches to build a space",
                       juce::dontSendNotification);
    hintLabel_.setColour(juce::Label::textColourId, theme.textLabel.withAlpha(0.4f));
    hintLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
    hintLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(hintLabel_);

    refreshBtn_.setButtonText("REFRESH");
    refreshBtn_.setLookAndFeel(&lf_);
    refreshBtn_.onClick = [this] { if (onRefreshRequested) onRefreshRequested(); };
    addAndMakeVisible(refreshBtn_);

    undoBtn_.setButtonText("UNDO");
    undoBtn_.setLookAndFeel(&lf_);
    undoBtn_.onClick = [this] { doUndo(); };
    addAndMakeVisible(undoBtn_);
}

TimbreSpacePanel::~TimbreSpacePanel()
{
    refreshBtn_.setLookAndFeel(nullptr);
    undoBtn_.setLookAndFeel(nullptr);
}

void TimbreSpacePanel::setPatches(std::vector<TimbrePatch> patches)
{
    patches_ = std::move(patches);
    engine_.computeLayout(patches_);
    hasCursor_ = false;
    lastWeights_.clear();
    readoutLabel_.setText("", juce::dontSendNotification);
    hintLabel_.setVisible(engine_.getPoints().size() < 3);
    repaint();
}

void TimbreSpacePanel::setBaseline(std::map<int,int> params,
                                   std::array<uint8_t,60> mod, juce::String name)
{
    baseParams_ = std::move(params);
    baseMod_    = mod;
    baseName_   = std::move(name);
    hasBaseline_ = true;
}

void TimbreSpacePanel::setScopeState(bool showFavoritesOnly)
{
    scopeAllBtn_.setToggleState(!showFavoritesOnly, juce::dontSendNotification);
    scopeFavBtn_.setToggleState(showFavoritesOnly,  juce::dontSendNotification);
}

juce::Rectangle<int> TimbreSpacePanel::mapArea() const
{
    auto area = getLocalBounds().reduced(12, 8);
    area.removeFromTop(24);    // top row: title + ALL/FAV/REFRESH/UNDO
    area.removeFromTop(4);
    area.removeFromBottom(18); // readout row
    area.removeFromBottom(4);
    return area;
}

void TimbreSpacePanel::paint(juce::Graphics& g)
{
    auto theme = ThemeData::getHardwareTheme();
    g.fillAll(theme.mainBackground);

    auto map = mapArea();
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(map);
    g.setColour(theme.groupOutline);
    g.drawRect(map, 1);

    const auto& pts = engine_.getPoints();
    if (pts.empty()) return;

    auto toPx = [&](float nx, float ny) {
        return juce::Point<float>(map.getX() + nx * map.getWidth(),
                                  map.getBottom() - ny * map.getHeight());
    };

    // Active-triangle links (cosmetic): connect the current weighted points.
    if (hasCursor_ && lastWeights_.size() >= 2)
    {
        g.setColour(theme.knobIndicatorActive.withAlpha(0.35f));
        for (size_t i = 0; i < lastWeights_.size(); ++i)
            for (size_t j = i + 1; j < lastWeights_.size(); ++j)
            {
                auto a = toPx(pts[(size_t)lastWeights_[i].index].x, pts[(size_t)lastWeights_[i].index].y);
                auto b = toPx(pts[(size_t)lastWeights_[j].index].x, pts[(size_t)lastWeights_[j].index].y);
                g.drawLine({ a, b }, 1.0f);
            }
    }

    // Patch dots.
    for (size_t i = 0; i < pts.size(); ++i)
    {
        auto c = toPx(pts[i].x, pts[i].y);
        bool active = false;
        for (auto& w : lastWeights_) if (w.index == (int)i) active = true;
        float r = active ? 5.0f : 3.5f;
        g.setColour(juce::Colour::fromFloatRGBA(pts[i].r, pts[i].g, pts[i].b, 1.0f));
        g.fillEllipse(c.x - r, c.y - r, r * 2, r * 2);
        if (active)
        {
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawEllipse(c.x - r, c.y - r, r * 2, r * 2, 1.2f);
        }
    }

    // Cursor crosshair.
    if (hasCursor_)
    {
        auto c = toPx(cursorNorm_.x, cursorNorm_.y);
        g.setColour(theme.knobIndicatorActive);
        g.drawLine(c.x - 6, c.y, c.x + 6, c.y, 1.2f);
        g.drawLine(c.x, c.y - 6, c.x, c.y + 6, 1.2f);
    }
}

void TimbreSpacePanel::resized()
{
    auto area = getLocalBounds().reduced(12, 8);
    auto topRow = area.removeFromTop(24);
    undoBtn_.setBounds(topRow.removeFromRight(60));
    topRow.removeFromRight(6);
    refreshBtn_.setBounds(topRow.removeFromRight(76));
    topRow.removeFromRight(10);
    scopeFavBtn_.setBounds(topRow.removeFromRight(56));
    topRow.removeFromRight(4);
    scopeAllBtn_.setBounds(topRow.removeFromRight(56));
    topRow.removeFromRight(8);
    titleLabel_.setBounds(topRow);
    area.removeFromTop(4);

    // Readout is now the bottommost row (buttons moved up to the top row).
    auto readoutRow = getLocalBounds().reduced(12, 8).removeFromBottom(18);
    readoutLabel_.setBounds(readoutRow);

    hintLabel_.setBounds(mapArea());
}

void TimbreSpacePanel::mouseDown(const juce::MouseEvent& e) { updateFromPosition(e.position); }
void TimbreSpacePanel::mouseDrag(const juce::MouseEvent& e) { updateFromPosition(e.position); }

void TimbreSpacePanel::updateFromPosition(juce::Point<float> local)
{
    auto map = mapArea().toFloat();
    if (!map.contains(local) || engine_.isEmpty()) return;

    float nx = (local.x - map.getX()) / map.getWidth();
    float ny = (map.getBottom() - local.y) / map.getHeight();   // up = higher y
    cursorNorm_ = { juce::jlimit(0.0f, 1.0f, nx), juce::jlimit(0.0f, 1.0f, ny) };
    hasCursor_ = true;

    lastWeights_ = engine_.interpolationDataForPoint(cursorNorm_.x, cursorNorm_.y);

    // Readout: patch numbers/names contributing to the blend (VFD-safe ASCII).
    juce::String r = "BASED ON ";
    for (auto& w : lastWeights_)
        r += engine_.getPatches()[(size_t)w.index].name.trim().toUpperCase() + " ";
    readoutLabel_.setText(r.trim(), juce::dontSendNotification);

    repaint();
    startTimer(300);   // coalesce hardware sends (same guard as MorphPanel)
}

void TimbreSpacePanel::timerCallback()
{
    stopTimer();
    if (!onApply || lastWeights_.empty()) return;
    // Weight indices refer to the ENGINE's filtered patch list (empty patches
    // dropped in computeLayout), which can differ from the panel's raw patches_.
    auto blended = PatchBlender::blend(lastWeights_, engine_.getPatches());
    onApply(blended.params, blended.modBytes, blended.name);
}

void TimbreSpacePanel::doUndo()
{
    if (!hasBaseline_ || !onApply) return;
    hasCursor_ = false;
    lastWeights_.clear();
    readoutLabel_.setText("", juce::dontSendNotification);
    repaint();
    onApply(baseParams_, baseMod_, baseName_);
}
