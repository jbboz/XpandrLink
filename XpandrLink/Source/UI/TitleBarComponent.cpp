#include "TitleBarComponent.h"
#include "ThemeData.h"

TitleBarComponent::TitleBarComponent()
{
    // LED activity indicators (standalone only — the DAW owns MIDI in the plugin)
    if (showMidiActivity_)
    {
        addAndMakeVisible(ledIn);  ledIn.setColour(juce::Colour(0xff00fc2e));
        addAndMakeVisible(ledOut); ledOut.setColour(juce::Colour(0xff00fc2e));
    }

    // Patch name label — display only; clicks handled in mouseDown()
    addAndMakeVisible(patchNameLabel);
    patchNameLabel.setText("--------", juce::dontSendNotification);
    patchNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffbfffd7));
    patchNameLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    patchNameLabel.setFont(ThemeData::getVfdFont(20.0f));
    patchNameLabel.setJustificationType(juce::Justification::centredRight);
    patchNameLabel.setEditable(false, false, false);
    patchNameLabel.setInterceptsMouseClicks(false, false);

    // Patch number label — inline edit; fires onPatchNumberEdited callback
    addAndMakeVisible(patchNumberLabel);
    patchNumberLabel.setText("00", juce::dontSendNotification);
    patchNumberLabel.setColour(juce::Label::textColourId,             juce::Colour(0xffbfffd7));
    patchNumberLabel.setColour(juce::Label::backgroundColourId,       juce::Colours::transparentBlack);
    patchNumberLabel.setFont(ThemeData::getVfdFont(20.0f));
    patchNumberLabel.setJustificationType(juce::Justification::centred);
    patchNumberLabel.setEditable(true, false, true);
    patchNumberLabel.setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(0xff0a2d16));
    patchNumberLabel.setColour(juce::Label::textWhenEditingColourId,       juce::Colour(0xffbfffd7));
    patchNumberLabel.setColour(juce::Label::outlineWhenEditingColourId,    juce::Colours::transparentBlack);
    patchNumberLabel.onEditorShow = [this] {
        if (auto* ed = patchNumberLabel.getCurrentTextEditor()) {
            ed->setInputRestrictions(2, "0123456789");
            ed->setJustification(juce::Justification::centred);
            ed->setFont(ThemeData::getVfdFont(20.0f));
            ed->setColour(juce::TextEditor::highlightColourId, juce::Colour(0xff1c4028));
            ed->selectAll();
        }
    };
    patchNumberLabel.onTextChange = [this] {
        if (onPatchNumberEdited)
            onPatchNumberEdited(juce::jlimit(0, 99, patchNumberLabel.getText().getIntValue()));
    };

    // Library button — hidden; panel opened via VFD name area click
    addChildComponent(btnLibrary);
    btnLibrary.setButtonText("LIBRARY");
    btnLibrary.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff1a1a1a));
    btnLibrary.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff1a1a1a));
    btnLibrary.setColour(juce::TextButton::textColourOffId,  juce::Colour(0xff55afd2));
    btnLibrary.onClick = [this] { if (onLibraryClicked) onLibraryClicked(); };

    // Prev / Next navigation buttons
    {
        auto styleNavBtn = [](juce::TextButton& btn) {
            btn.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff1a1a1a));
            btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff1a1a1a));
            btn.setColour(juce::TextButton::textColourOffId,  juce::Colour(0xff55afd2));
        };
        styleNavBtn(btnPrev);
        styleNavBtn(btnNext);
    }
    addAndMakeVisible(btnPrev);
    addAndMakeVisible(btnNext);
    btnPrev.onClick = [this] { if (onPrevClicked) onPrevClicked(); };
    btnNext.onClick = [this] { if (onNextClicked) onNextClicked(); };

    // MIDI IN/OUT labels (standalone only — paired with the activity LEDs)
    if (showMidiActivity_)
    {
        addAndMakeVisible(midiInLabel);
        midiInLabel.setText("MIDI IN", juce::dontSendNotification);
        midiInLabel.setColour(juce::Label::textColourId, juce::Colour(0xff55afd2));
        midiInLabel.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        midiInLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(midiOutLabel);
        midiOutLabel.setText("MIDI OUT", juce::dontSendNotification);
        midiOutLabel.setColour(juce::Label::textColourId, juce::Colour(0xff55afd2));
        midiOutLabel.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        midiOutLabel.setJustificationType(juce::Justification::centredLeft);
    }

    // Apply membrane LookAndFeel to nav buttons
    btnPrev.setLookAndFeel(&hwButtonLF_);
    btnNext.setLookAndFeel(&hwButtonLF_);
}

TitleBarComponent::~TitleBarComponent()
{
    btnPrev.setLookAndFeel(nullptr);
    btnNext.setLookAndFeel(nullptr);
}

void TitleBarComponent::paint(juce::Graphics& g)
{
    auto theme = ThemeData::getHardwareTheme();

    // Title bar background
    g.setColour(juce::Colour(0xff1e1e1e));
    g.fillRect(0, 0, getWidth(), getHeight());
    g.setColour(juce::Colour(0xff121212));
    g.fillRect(0, getHeight() - 2, getWidth(), 2);

    // Embossed "XPANDRLINK" wordmark + subtitle
    {
        const float tx = 16.0f, ty = 9.0f;
        const juce::Rectangle<float> word(tx, ty, 200.0f, 24.0f);
        g.setFont(juce::Font(juce::FontOptions(21.0f, juce::Font::bold)));
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawText("XPANDRLINK", word.translated(1.0f, 1.5f), juce::Justification::topLeft, false);
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xffaee0f5), word.getX(), word.getY(),
            juce::Colour(0xff3f7f9e), word.getX(), word.getBottom(), false));
        g.drawText("XPANDRLINK", word, juce::Justification::topLeft, false);
        g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
        g.setColour(theme.textLabel.withAlpha(0.5f));
        g.drawText("XPANDER / MATRIX-12 EDITOR",
                   juce::Rectangle<float>(tx + 1.0f, 33.0f, 200.0f, 12.0f),
                   juce::Justification::topLeft, false);
    }

    // VFD strip background + ghost segments (only once resized() has set the areas)
    if (!vfdNameArea.isEmpty())
    {
        auto stripLeft  = vfdNameArea.getX() - 14;
        auto stripRight = vfdNumberArea.getRight() + 12;
        auto stripTop   = vfdNameArea.getY() - 4;
        auto stripH     = vfdNameArea.getHeight() + 8;
        juce::Rectangle<float> stripR((float)stripLeft, (float)stripTop,
                                      (float)(stripRight - stripLeft), (float)stripH);

        g.setColour(theme.vfdBackground);
        g.fillRoundedRectangle(stripR, 2.0f);
        g.setColour(theme.groupOutline.brighter(0.5f));
        g.drawRoundedRectangle(stripR, 2.0f, 1.0f);

        float sepX = (float)(vfdNameArea.getRight() + 14);
        g.setColour(theme.groupOutline.brighter(0.5f));
        g.drawLine(sepX, stripR.getY() + 4.0f, sepX, stripR.getBottom() - 4.0f, 1.0f);

        g.setFont(ThemeData::getVfdFont(20.0f));
        g.setColour(theme.vfdGhost);
        g.drawText("~~~~~~~~", vfdNameArea, juce::Justification::centredRight, false);
        g.drawText("~~",       vfdNumberArea, juce::Justification::centred, false);
    }
}

void TitleBarComponent::resized()
{
    auto area = getLocalBounds();
    const int cy = area.getCentreY();

    // Left zone (180px): wordmark area + Library button
    auto left = area.removeFromLeft(180);
    left.reduce(12, 0);
    left.removeFromLeft(52 + 6);  // skip the wordmark area (painted in paint(), not a label)
    btnLibrary.setBounds(left.removeFromLeft(86).withSizeKeepingCentre(86, 26));

    // Right zone (180px): MIDI IN/OUT activity indicators (standalone only).
    // The 180px is reserved in both builds so the centre VFD strip stays centred.
    auto right = area.removeFromRight(180);
    if (showMidiActivity_)
    {
        right.reduce(12, 0);
        ledIn.setBounds(right.removeFromLeft(10).withSizeKeepingCentre(10, 10));
        right.removeFromLeft(5);
        midiInLabel.setBounds(right.removeFromLeft(52).withSizeKeepingCentre(52, 16));
        right.removeFromLeft(10);
        ledOut.setBounds(right.removeFromLeft(10).withSizeKeepingCentre(10, 10));
        right.removeFromLeft(5);
        midiOutLabel.setBounds(right.removeFromLeft(60).withSizeKeepingCentre(60, 16));
    }

    // Centre zone: ◀ [VFD strip] ▶
    auto center = area;
    const int vfdStripH  = 38;
    const int nameW = 200, numW = 76;
    const int stripPadL = 14, stripPadR = 12, stripPadV = (vfdStripH - 22) / 2;
    const int stripW = nameW + 1 + numW + stripPadL + stripPadR;
    const int ctrlTotalW = 28 + 8 + 300 + 8 + 28;  // 300 matches original centering

    int cx0 = center.getCentreX() - ctrlTotalW / 2;
    btnPrev.setBounds(cx0, cy - 15, 28, 30);
    cx0 += 28 + 8;

    int stripX = cx0;
    int stripY = cy - vfdStripH / 2;
    vfdNameArea   = juce::Rectangle<int>(stripX + stripPadL,
                                         stripY + stripPadV,
                                         nameW,
                                         vfdStripH - 2 * stripPadV);
    vfdNumberArea = juce::Rectangle<int>(stripX + stripPadL + nameW + 1 + 12,
                                         stripY + stripPadV,
                                         numW - 24,
                                         vfdStripH - 2 * stripPadV);
    patchNameLabel.setBounds(vfdNameArea);
    patchNumberLabel.setBounds(vfdNumberArea);

    cx0 += stripW + 8;
    btnNext.setBounds(cx0, cy - 15, 28, 30);
}

void TitleBarComponent::mouseDown(const juce::MouseEvent& e)
{
    if (vfdNameArea.contains(e.getPosition()) && onLibraryClicked)
        onLibraryClicked();
}

void TitleBarComponent::parentHierarchyChanged()
{
    hideStandaloneOptionsButton();
}

void TitleBarComponent::hideStandaloneOptionsButton()
{
    // Hides the JUCE standalone wrapper's "Options" button. Its Audio/MIDI
    // Settings dialog is redundant: audio is disabled in standalone, and the
    // MIDI pane (nav bar) is the app's MIDI I/O UI in every build.
    if (optionsButtonHidden_) return;
    auto* top = getTopLevelComponent();
    if (top == nullptr) return;

    std::function<juce::TextButton*(juce::Component*)> find =
        [&](juce::Component* c) -> juce::TextButton* {
            for (auto* child : c->getChildren())
            {
                if (auto* tb = dynamic_cast<juce::TextButton*>(child))
                    if (tb->getButtonText() == "Options") return tb;
                if (auto* found = find(child)) return found;
            }
            return nullptr;
        };

    if (auto* opt = find(top))
    {
        opt->setVisible(false);
        optionsButtonHidden_ = true;
    }
}

void TitleBarComponent::setPatchName(const juce::String& name)
{
    patchNameLabel.setText(name, juce::dontSendNotification);
    repaint();
}

void TitleBarComponent::setProgramNumber(int n)
{
    patchNumberLabel.setText(juce::String::formatted("%02d", n), juce::dontSendNotification);
    repaint();
}

void TitleBarComponent::flashMidiLed(bool isInput)
{
    if (!showMidiActivity_) return;   // no LEDs in the plugin

    // Coalesce MIDI bursts: only one callAsync pending per direction at a time.
    auto& pending = isInput ? midiInFlashPending : midiOutFlashPending;
    bool expected = false;
    if (!pending.compare_exchange_strong(expected, true))
        return;

    juce::Component::SafePointer<TitleBarComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, isInput] {
        if (safeThis == nullptr) return;
        if (isInput) { safeThis->midiInFlashPending  = false; safeThis->ledIn.flash(); }
        else         { safeThis->midiOutFlashPending = false; safeThis->ledOut.flash(); }
    });
}

juce::Rectangle<int> TitleBarComponent::getVfdNameBounds() const
{
    // Returns vfdNameArea in parent coordinates for browser panel positioning.
    return vfdNameArea.translated(getX(), getY());
}
