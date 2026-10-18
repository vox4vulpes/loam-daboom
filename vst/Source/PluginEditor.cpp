#include "PluginEditor.h"

using namespace LoamColours;
using namespace daboom;

namespace
{
    // Layout mirrors the web panel: header, Transport / Kit / Master, eight
    // voice strips, the sequencer, and a line of instructions.
    constexpr int editorWidth = 900, editorHeight = 818;
    constexpr int pad = 20, gap = 12, headerHeight = 50, topRowHeight = 140, stripHeight = 236, seqHeight = 272;
    constexpr int titleHeight = 26, knobWidth = 44, knobHeight = 68;

    juce::Font sansFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font monoFont (float height)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
    }

    juce::String utf8 (const char* text) { return juce::String (juce::CharPointer_UTF8 (text)); }
    juce::String quoted (const juce::String& s) { return utf8 ("\xe2\x80\x9c") + s + utf8 ("\xe2\x80\x9d"); }

    // Same drawing as the web versions and Loam Eight Sub.
    constexpr const char* egretSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24"> <path d="M14.5 4.2c1.6-.3 2.8.6 2.8 1.8 0 .9-.6 1.5-1.4 1.7-.4 1.7-.2 3.4.6 4.8 1 1.8.5 4-1.6 5.2-1.9 1.1-4.6 1-6.4-.4-1.3-1-1.9-2.4-1.7-3.6 2.2.8 4.1.4 5.4-.9.9-1 .9-2.4.3-3.8-.6-1.4-.4-3.2 2-4.8z" fill="#fbf8ef" stroke="#2a241d" stroke-width=".8" stroke-linejoin="round"/> <path d="M17.2 5.4l4.6 1.1-4.4.6z" fill="#2a241d"/> <circle cx="15.6" cy="5.2" r=".45" fill="#2a241d"/> <path d="M14.2 4.4c-1.2-.6-2-.3-2.6.4" fill="none" stroke="#2a241d" stroke-width=".5"/> <path d="M10.8 17.6l-.6 4.2M12.6 17.7l.4 4.1" stroke="#2a241d" stroke-width=".9" stroke-linecap="round"/> <path d="M9.3 21.9h1.8M12.3 21.9h1.8" stroke="#e8c32a" stroke-width="1.4" stroke-linecap="round"/> </svg>)svg";

    juce::String subtitleText()  { return utf8 ("\xe5\xb0\x8f\xe9\xb7\xba \xc2\xb7 RHYTHM COMPOSER"); }
    juce::String egretNameText() { return utf8 ("\xe3\x82\xb3\xe3\x82\xb5\xe3\x82\xae \xc2\xb7 KOSAGI \xc2\xb7 LITTLE EGRET"); }

    juce::Colour voiceColour (int slot) { return juce::Colour (voices()[(size_t) slot].colour); }

    juce::String keyLabel (int slot)
    {
        const auto key = voices()[(size_t) slot].key;
        return juce::String::charToString ((juce::juce_wchar) key);
    }

    void styleLabel (juce::Label& l, const juce::String& text, juce::Font font, juce::Justification just = juce::Justification::centredLeft)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (font);
        l.setJustificationType (just);
        l.setBorderSize ({});
        l.setInterceptsMouseClicks (false, false);
    }

    void styleNameField (juce::TextEditor& t, const juce::String& placeholder)
    {
        t.setTextToShowWhenEmpty (placeholder, inkSoft);
        t.setInputRestrictions (24);
        t.setFont (sansFont (12.0f));
        t.setIndents (6, 5);
    }

    // Items in the kit and groove menus: factory ones from 1, the user's from 100.
    constexpr int userIdBase = 100, editedId = 999;
}

//==============================================================================
LoamLookAndFeel::LoamLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::outlineColourId, grooveDark);
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::arrowColourId, ink);
    setColour (juce::ComboBox::focusedOutlineColourId, teal);
    setColour (juce::PopupMenu::backgroundColourId, panelRaised);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::headerTextColourId, inkSoft);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, mustard);
    setColour (juce::PopupMenu::highlightedTextColourId, panelRaised);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, panelRaised);
    setColour (juce::TextButton::buttonOnColourId, coral);
    setColour (juce::TextEditor::backgroundColourId, panel);
    setColour (juce::TextEditor::outlineColourId, grooveDark);
    setColour (juce::TextEditor::focusedOutlineColourId, teal);
    setColour (juce::TextEditor::textColourId, ink);
    setColour (juce::TextEditor::highlightColourId, mustard.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, ink);
    setColour (juce::Label::textColourId, inkSoft);
    setColour (juce::TooltipWindow::backgroundColourId, panelRaised);
    setColour (juce::TooltipWindow::textColourId, ink);
    setColour (juce::TooltipWindow::outlineColourId, grooveDark);
}

void LoamLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
    const auto centre = area.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto arcRadius = radius - 3.0f;
    const juce::PathStrokeType arcStroke (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    // Recessed well with the 270-degree groove, gap at the bottom, as in the web knob.
    g.setColour (grooveDark.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (grooveDark);
    g.strokePath (track, arcStroke);

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (value, arcStroke);

    const auto capRadius = radius * 0.72f;
    const auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
    g.setColour (shadow);
    g.fillEllipse (cap.translated (0.0f, 1.5f));
    // The highlight sits straight up, at 12 o'clock: the knob's 50% position.
    g.setGradientFill (juce::ColourGradient (panelRaised, centre.x, centre.y - capRadius * 0.5f,
                                             groove, centre.x, centre.y + capRadius * 1.3f, true));
    g.fillEllipse (cap);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -capRadius * 0.85f, 3.0f, capRadius * 0.55f, 1.5f);
    g.setColour (accent);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void LoamLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool isMouseOverButton, bool isButtonDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto on = button.getToggleState();
    const auto accent = button.findColour (juce::TextButton::buttonOnColourId);
    const auto alpha = button.isEnabled() ? 1.0f : 0.45f;

    auto fill = on ? accent : panel;
    if (! on && button.isEnabled() && (isMouseOverButton || isButtonDown))
        fill = fill.brighter (0.15f);

    g.setColour (fill.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (bounds, 6.0f);
    // A queued pattern (switching at the end of the bar) gets an accent ring.
    const auto queued = (bool) button.getProperties().getWithDefault ("queued", false);
    g.setColour ((on || queued ? accent : grooveDark).withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (bounds, 6.0f, queued ? 2.0f : 1.0f);
}

juce::Font LoamLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return sansFont (11.0f, true); }
juce::Font LoamLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return sansFont ((bool) box.getProperties().getWithDefault ("slot", false) ? 12.5f : 12.0f, true);
}

void LoamLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    LookAndFeel_V4::positionComboBoxText (box, label);
    label.setMinimumHorizontalScale (0.6f);   // "COWBELL LO-FI" squeezes rather than turning into "COWBELL..."
}

//==============================================================================
DaBoomKnob::DaBoomKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& labelText,
                        juce::Colour accent, Unit u)
    : attachment (apvts, paramId, slider), unit (u)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setMouseDragSensitivity (150);   // the web knob: 150 px for the whole range
    slider.setWantsKeyboardFocus (false);
    if (auto* p = apvts.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { updateReadout(); };
    addAndMakeVisible (slider);

    styleLabel (label, labelText.toUpperCase(), sansFont (8.5f, true), juce::Justification::centred);
    styleLabel (readout, {}, monoFont (9.5f), juce::Justification::centred);
    addAndMakeVisible (label);
    addAndMakeVisible (readout);
    updateReadout();
}

void DaBoomKnob::updateReadout()
{
    const auto v = (float) slider.getValue();
    juce::String text;
    if (unit == Unit::hz)
        text = v >= 1000.0f ? juce::String (v / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (v)) + "Hz";
    else if (unit == Unit::ms)
        text = juce::String (juce::roundToInt (v)) + "ms";
    else if (unit == Unit::pct)
        text = juce::String (juce::roundToInt (v)) + "%";
    else
        text = juce::String (juce::roundToInt (v));
    readout.setText (text, juce::dontSendNotification);
}

void DaBoomKnob::resized()
{
    auto area = getLocalBounds();
    readout.setBounds (area.removeFromBottom (13));
    label.setBounds (area.removeFromBottom (12));
    slider.setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight()), area.getHeight()));
}

void DaBoomKnob::setDimmed (bool dimmed) { setAlpha (dimmed ? 0.45f : 1.0f); }

//==============================================================================
PadView::PadView (juce::String key, juce::Colour c) : keyText (std::move (key)), accent (c)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PadView::setLit (bool on)
{
    if (on != lit)
    {
        lit = on;
        repaint();
    }
}

void PadView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f).withTrimmedBottom (2.0f);
    if (lit)
        r.translate (0.0f, 1.0f);
    g.setColour (grooveDark);
    g.fillRoundedRectangle (r.translated (0.0f, lit ? 1.0f : 2.0f), 7.0f);   // the web pad's bottom shadow
    if (lit)
        g.setColour (accent);
    else
        g.setGradientFill (juce::ColourGradient (panel, r.getX(), r.getY(), groove, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (grooveDark);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    g.setColour (lit ? panelRaised : ink);
    g.setFont (monoFont (11.0f));
    g.drawText (keyText, r, juce::Justification::centred, false);
}

//==============================================================================
SequencerView::SequencerView (LoamDaBoomProcessor& p) : processor (p) {}

bool SequencerView::isMuted (int slot) const
{
    if (auto* param = processor.apvts.getParameter (LoamDaBoomProcessor::muteId (slot)))
        return param->getValue() >= 0.5f;
    return false;
}

juce::Rectangle<float> SequencerView::stepBounds (int row, int step) const
{
    const auto w = ((float) getWidth() - nameWidth - stepGap * (numSteps - 1)) / numSteps;
    return { nameWidth + (float) step * (w + stepGap), headHeight + rowGap + (float) row * (rowHeight + rowGap), w, rowHeight };
}

juce::Rectangle<float> SequencerView::muteBounds (int row) const
{
    return { 0.0f, headHeight + rowGap + (float) row * (rowHeight + rowGap) + 2.0f, 18.0f, 18.0f };
}

void SequencerView::refresh()
{
    juce::String sig;
    sig << processor.getCurrentPattern() << ":" << processor.getShownStep() << ":";
    for (int s = 0; s < numSlots; ++s)
    {
        sig << (isMuted (s) ? "m" : "-");
        for (int i = 0; i < numSteps; ++i)
            sig << processor.getStep (processor.getCurrentPattern(), s, i);
    }
    if (sig != shownSignature)
    {
        shownSignature = sig;
        repaint();
    }
}

void SequencerView::paint (juce::Graphics& g)
{
    const auto now = processor.getShownStep(), pattern = processor.getCurrentPattern();

    // Step numbers, the playing one in coral.
    for (int i = 0; i < numSteps; ++i)
    {
        const auto r = stepBounds (0, i).withY (0.0f).withHeight (headHeight);
        g.setColour (i == now ? coral : inkSoft);
        g.setFont (i == now ? monoFont (9.5f).boldened() : monoFont (9.0f));
        g.drawText (juce::String (i + 1), r, juce::Justification::centred, false);
    }

    for (int s = 0; s < numSlots; ++s)
    {
        const auto muted = isMuted (s);
        const auto colour = voiceColour (s);

        const auto m = muteBounds (s);
        g.setColour (muted ? ink : panel);
        g.fillRoundedRectangle (m, 4.0f);
        g.setColour (grooveDark);
        g.drawRoundedRectangle (m.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (muted ? panelRaised : inkSoft);
        g.setFont (sansFont (9.0f, true));
        g.drawText ("M", m, juce::Justification::centred, false);

        g.setColour (ink);
        g.setFont (monoFont (10.0f));
        g.drawText (voices()[(size_t) s].name, juce::Rectangle<float> (24.0f, m.getY(), nameWidth - 26.0f, 18.0f),
                    juce::Justification::centredLeft, false);

        for (int i = 0; i < numSteps; ++i)
        {
            const auto r = stepBounds (s, i).reduced (0.5f);
            const auto v = processor.getStep (pattern, s, i);
            const auto alpha = muted ? 0.4f : 1.0f;
            g.setColour ((v == 0 ? panel : colour.withAlpha (v == 1 ? 0.75f : 1.0f)).withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour ((v == 2 ? ink : v == 1 ? colour : grooveDark).withMultipliedAlpha (alpha));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            if (v == 2)
            {
                g.setColour (juce::Colours::white.withAlpha (0.55f * alpha));
                g.drawRoundedRectangle (r.reduced (2.0f), 3.0f, 2.0f);
            }
            if (i % 4 == 0)   // the beat steps: a thicker bottom edge
            {
                g.setColour ((v == 0 ? grooveDark : colour).withMultipliedAlpha (alpha));
                g.fillRect (r.getX() + 2.0f, r.getBottom() - 2.0f, r.getWidth() - 4.0f, 2.0f);
            }
            if (i == now)
            {
                g.setColour (readoutInk);
                g.drawRoundedRectangle (r.expanded (0.5f), 4.0f, 2.0f);
            }
        }
    }
}

void SequencerView::mouseDown (const juce::MouseEvent& e)
{
    for (int s = 0; s < numSlots; ++s)
    {
        if (muteBounds (s).expanded (2.0f).contains (e.position))
        {
            if (auto* param = processor.apvts.getParameter (LoamDaBoomProcessor::muteId (s)))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (isMuted (s) ? 0.0f : 1.0f);
                param->endChangeGesture();
            }
            refresh();
            return;
        }
        for (int i = 0; i < numSteps; ++i)
            if (stepBounds (s, i).contains (e.position))
            {
                processor.cycleStep (s, i);
                refresh();
                return;
            }
    }
}

//==============================================================================
EgretView::EgretView()
{
    drawable = juce::Drawable::createFromImageData (egretSvg, std::strlen (egretSvg));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Kosagi, the little egret");
    setTooltip (utf8 ("\xe5\xb0\x8f\xe9\xb7\xba"));
}

void EgretView::paint (juce::Graphics& g)
{
    if (drawable != nullptr)
        drawable->drawWithin (g, getLocalBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
}

//==============================================================================
LoamDaBoomEditor::LoamDaBoomEditor (LoamDaBoomProcessor& p)
    : AudioProcessorEditor (p), audioProcessor (p),
      bpmKnob (p.apvts, "bpm", "BPM", coral, Unit::none),
      swingKnob (p.apvts, "swing", "Swing", coral, Unit::pct),
      volumeKnob (p.apvts, "volume", "Volume", mustard, Unit::pct),
      driveKnob (p.apvts, "drive", "Drive", mustard, Unit::pct),
      reverbKnob (p.apvts, "reverb", "Reverb", mustard, Unit::pct),
      sequencer (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    brandSub = subtitleText();
    // The safe pointer is made out here: MSVC can't take `this` inside a nested lambda's capture.
    egret.onClick = [this, safe = juce::Component::SafePointer<LoamDaBoomEditor> (this)]
    {
        brandSub = egretNameText();
        repaint (headerArea);
        audioProcessor.chirp();
        juce::Timer::callAfterDelay (2600, [safe]
        {
            if (safe != nullptr)
            {
                safe->brandSub = subtitleText();
                safe->repaint (safe->headerArea);
            }
        });
    };
    addAndMakeVisible (egret);

    auto noFocus = [] (juce::Component& c) { c.setWantsKeyboardFocus (false); c.setMouseClickGrabsKeyboardFocus (false); };

    // ---- transport
    playButton.setColour (juce::TextButton::buttonOnColourId, coral);
    playButton.setTooltip (p.isStandalone() ? "Start and stop the sequencer (Space)"
                                            : "While lit, the pattern plays along with the DAW's transport and tempo (Space)");
    playButton.onClick = [this] { audioProcessor.setPlaying (! audioProcessor.isPlayOn()); };
    for (int i = 0; i < numPatterns; ++i)
    {
        auto& b = patternButtons[(size_t) i];
        b.setButtonText (juce::String::charToString ((juce::juce_wchar) ('A' + i)));
        b.setColour (juce::TextButton::buttonOnColourId, coral);
        b.setTooltip ("Pattern " + b.getButtonText() + " (switches at the end of the bar while playing)");
        b.onClick = [this, i] { audioProcessor.requestPattern (i); refreshGrooveBox(); };
        noFocus (b);
        addAndMakeVisible (b);
    }
    clearButton.setTooltip ("Clear the current pattern");
    clearButton.onClick = [this] { audioProcessor.clearPattern(); sequencer.refresh(); };
    styleLabel (patternLabel, "PATTERN", monoFont (10.0f).withExtraKerningFactor (0.12f));
    styleLabel (libraryLabel, "LIBRARY", monoFont (10.0f).withExtraKerningFactor (0.12f));
    styleLabel (grooveStatus, {}, monoFont (10.0f), juce::Justification::centredRight);
    grooveBox.setTooltip ("Pattern library: load a groove into the current pattern");
    grooveBox.onChange = [this]
    {
        const auto id = grooveBox.getSelectedId();
        if (id <= 0)
            return;
        const auto pattern = juce::String::charToString ((juce::juce_wchar) ('A' + audioProcessor.getCurrentPattern()));
        if (id < userIdBase)
        {
            audioProcessor.loadFactoryGroove (id - 1);
            setGrooveStatus ("Loaded " + quoted (factoryGrooves()[(size_t) id - 1].name) + " into " + pattern);
        }
        else
        {
            const auto name = userGrooves[id - userIdBase];
            setGrooveStatus (audioProcessor.loadUserGroove (name) ? "Loaded " + quoted (name) + " into " + pattern
                                                                  : "Couldn't read " + quoted (name));
        }
        sequencer.refresh();
        grooveDeleteArmed = false;
        grooveDelete.setButtonText ("DELETE");
        grooveDelete.setEnabled (id >= userIdBase);
    };
    styleNameField (grooveName, "Groove name");
    grooveName.onReturnKey = [this] { saveGroove(); };
    grooveSave.setTooltip ("Save the current pattern to the library");
    grooveSave.onClick = [this] { saveGroove(); };
    grooveDelete.onClick = [this] { deleteGroove(); };
    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &clearButton, &patternLabel, &libraryLabel, &grooveStatus,
                                                             &grooveBox, &grooveName, &grooveSave, &grooveDelete, &bpmKnob, &swingKnob })
        addAndMakeVisible (*c);
    for (auto* b : { &playButton, &clearButton, &grooveSave, &grooveDelete })
        noFocus (*b);

    // ---- kit
    kitBox.setTooltip ("Kit: the factory ones, and yours");
    kitBox.onChange = [this]
    {
        const auto id = kitBox.getSelectedId();
        if (id <= 0 || id == editedId)
            return;
        if (id < userIdBase)
            audioProcessor.loadFactoryKit (id - 1);
        else if (! audioProcessor.loadUserKit (userKits[id - userIdBase]))
            setKitStatus ("Couldn't read that kit");
        setKitStatus ({});
        refreshKitBox();
    };
    styleNameField (kitName, "Kit name");
    kitName.onReturnKey = [this] { saveKit(); };
    kitSave.onClick = [this] { saveKit(); };
    kitDelete.onClick = [this] { deleteKit(); };
    styleLabel (kitStatus, "Pick each voice's version from its name above the pad.", monoFont (10.0f), juce::Justification::topLeft);
    kitStatus.setMinimumHorizontalScale (1.0f);
    for (auto* c : std::initializer_list<juce::Component*> { &kitBox, &kitName, &kitSave, &kitDelete, &kitStatus })
        addAndMakeVisible (*c);
    for (auto* b : { &kitSave, &kitDelete })
        noFocus (*b);

    // ---- master
    for (auto* k : { &volumeKnob, &driveKnob, &reverbKnob })
        addAndMakeVisible (*k);

    // ---- voices: a version menu, a pad and the knobs per slot
    for (int s = 0; s < numSlots; ++s)
    {
        const auto& v = voices()[(size_t) s];
        auto& box = versionBoxes[(size_t) s];
        box.getProperties().set ("slot", true);
        for (int k = 0; k < numFactoryKits; ++k)
            box.addItem (juce::String (v.name) + " " + factoryKitName (k), k + 1);
        box.setJustificationType (juce::Justification::centred);
        for (auto id : { juce::ComboBox::backgroundColourId, juce::ComboBox::outlineColourId })
            box.setColour (id, juce::Colours::transparentBlack);
        box.setColour (juce::ComboBox::textColourId, voiceColour (s));
        box.setColour (juce::ComboBox::arrowColourId, voiceColour (s));
        box.setTooltip ("Which version of the " + juce::String (v.name).toLowerCase() + " to use");
        box.setSelectedId (p.getSlotVersion (s) + 1, juce::dontSendNotification);
        box.onChange = [this, s] { audioProcessor.setSlotVersion (s, versionBoxes[(size_t) s].getSelectedId() - 1); };
        noFocus (box);
        addAndMakeVisible (box);

        pads[(size_t) s] = std::make_unique<PadView> (keyLabel (s), voiceColour (s));
        pads[(size_t) s]->onHit = [this, s] { audioProcessor.hitPad (s, true); };
        addAndMakeVisible (*pads[(size_t) s]);

        for (int k = 0; k < v.numKnobs; ++k)
        {
            const auto& kd = v.knobs[(size_t) k];
            voiceKnobs.push_back (std::make_unique<DaBoomKnob> (p.apvts, LoamDaBoomProcessor::paramId (s, k), kd.label, voiceColour (s), kd.unit));
            slotKnobs[(size_t) s].push_back (voiceKnobs.back().get());
            addAndMakeVisible (*voiceKnobs.back());
        }
        strips[(size_t) s] = { {}, voiceColour (s), {} };
    }

    addAndMakeVisible (sequencer);

    userKits = p.getUserKitNames();
    userGrooves = p.getUserGrooveNames();
    refreshKitBox();
    refreshGrooveBox();

    setSize (editorWidth, editorHeight);
    startTimerHz (30);
}

LoamDaBoomEditor::~LoamDaBoomEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void LoamDaBoomEditor::refreshKitBox()
{
    const auto name = audioProcessor.getKitName();
    const auto edited = audioProcessor.isKitEdited();
    kitBox.clear (juce::dontSendNotification);
    kitBox.addSectionHeading ("Factory");
    for (int k = 0; k < numFactoryKits; ++k)
        kitBox.addItem (factoryKitName (k), k + 1);
    if (! userKits.isEmpty())
    {
        kitBox.addSectionHeading ("Your kits");
        for (int i = 0; i < userKits.size(); ++i)
            kitBox.addItem (userKits[i], userIdBase + i);
    }

    int selected = editedId;
    if (! edited)
    {
        for (int k = 0; k < numFactoryKits; ++k)
            if (name == factoryKitName (k))
                selected = k + 1;
        if (const auto i = userKits.indexOf (name, true); i >= 0)
            selected = userIdBase + i;
    }
    if (selected == editedId)
        kitBox.addItem (name + " (edited)", editedId);   // a kit that's only in this session
    kitBox.setSelectedId (selected, juce::dontSendNotification);

    const auto isUserKit = userKits.contains (name, true);
    kitDelete.setEnabled (isUserKit);
    if (! isUserKit)
    {
        kitDeleteArmed = false;
        kitDelete.setButtonText ("DELETE");
    }
    shownKitSignature = name + (edited ? "*" : "") + juce::String (userKits.size());
}

void LoamDaBoomEditor::refreshGrooveBox (int selectId)
{
    grooveBox.clear (juce::dontSendNotification);
    grooveBox.addSectionHeading ("Factory");
    for (int i = 0; i < (int) factoryGrooves().size(); ++i)
        grooveBox.addItem (factoryGrooves()[(size_t) i].name, i + 1);
    if (! userGrooves.isEmpty())
    {
        grooveBox.addSectionHeading ("Your grooves");
        for (int i = 0; i < userGrooves.size(); ++i)
            grooveBox.addItem (userGrooves[i], userIdBase + i);
    }
    const auto pattern = juce::String::charToString ((juce::juce_wchar) ('A' + audioProcessor.getCurrentPattern()));
    grooveBox.setTextWhenNothingSelected (utf8 ("\xe2\x80\x94 load into ") + pattern + utf8 (" \xe2\x80\x94"));
    grooveBox.setSelectedId (selectId, juce::dontSendNotification);
    grooveDeleteArmed = false;
    grooveDelete.setButtonText ("DELETE");
    grooveDelete.setEnabled (selectId >= userIdBase);
}

void LoamDaBoomEditor::setKitStatus (const juce::String& text)
{
    kitStatus.setText (text.isEmpty() ? "Pick each voice's version from its name above the pad." : text, juce::dontSendNotification);
}

void LoamDaBoomEditor::setGrooveStatus (const juce::String& text) { grooveStatus.setText (text, juce::dontSendNotification); }

void LoamDaBoomEditor::saveKit()
{
    const auto current = audioProcessor.getKitName();
    auto name = kitName.getText().trim();
    if (name.isEmpty())
        name = userKits.contains (current, true) ? current : "My kit " + juce::String (userKits.size() + 1);
    for (int k = 0; k < numFactoryKits; ++k)
        if (name == factoryKitName (k))
            name << " (mine)";

    const auto existed = userKits.contains (name, true);
    if (! audioProcessor.saveUserKit (name))
    {
        setKitStatus ("Couldn't save " + quoted (name));
        return;
    }
    kitName.clear();
    userKits = audioProcessor.getUserKitNames();
    refreshKitBox();
    setKitStatus ((existed ? "Updated " : "Saved ") + quoted (name));
    grabKeyboardFocus();
}

void LoamDaBoomEditor::deleteKit()
{
    const auto name = audioProcessor.getKitName();
    if (! userKits.contains (name, true))
        return;
    // Two-step delete: the first click arms the button, the second deletes.
    if (! kitDeleteArmed)
    {
        kitDeleteArmed = true;
        kitDelete.setButtonText ("SURE?");
        setKitStatus ("Click again to delete " + quoted (name));
        return;
    }
    const auto deleted = audioProcessor.deleteUserKit (name);
    userKits = audioProcessor.getUserKitNames();
    refreshKitBox();
    setKitStatus ((deleted ? "Deleted " : "Couldn't delete ") + quoted (name));
}

void LoamDaBoomEditor::saveGroove()
{
    auto name = grooveName.getText().trim();
    if (name.isEmpty())
        name = "My groove " + juce::String (userGrooves.size() + 1);
    for (const auto& g : factoryGrooves())
        if (name == g.name)
            name << " (mine)";

    const auto existed = userGrooves.contains (name, true);
    const auto pattern = juce::String::charToString ((juce::juce_wchar) ('A' + audioProcessor.getCurrentPattern()));
    if (! audioProcessor.saveUserGroove (name))
    {
        setGrooveStatus ("Couldn't save " + quoted (name));
        return;
    }
    grooveName.clear();
    userGrooves = audioProcessor.getUserGrooveNames();
    refreshGrooveBox (userIdBase + userGrooves.indexOf (name, true));
    setGrooveStatus ((existed ? "Updated " : "Saved ") + quoted (name) + " from " + pattern);
    grabKeyboardFocus();
}

void LoamDaBoomEditor::deleteGroove()
{
    const auto id = grooveBox.getSelectedId();
    if (id < userIdBase)
        return;
    const auto name = userGrooves[id - userIdBase];
    if (! grooveDeleteArmed)
    {
        grooveDeleteArmed = true;
        grooveDelete.setButtonText ("SURE?");
        setGrooveStatus ("Click again to delete " + quoted (name));
        return;
    }
    const auto deleted = audioProcessor.deleteUserGroove (name);
    userGrooves = audioProcessor.getUserGrooveNames();
    refreshGrooveBox();
    setGrooveStatus ((deleted ? "Deleted " : "Couldn't delete ") + quoted (name));
}

//==============================================================================
bool LoamDaBoomEditor::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    if (code == juce::KeyPress::spaceKey)
    {
        if (keysDown.insert (code).second)
            audioProcessor.setPlaying (! audioProcessor.isPlayOn());
        return true;
    }
    const auto c = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code);
    for (int s = 0; s < numSlots; ++s)
        if (c == (juce::juce_wchar) voices()[(size_t) s].key)
        {
            if (keysDown.insert (code).second)   // no machine-gun from key repeat
                audioProcessor.hitPad (s, true);
            return true;
        }
    return false;
}

bool LoamDaBoomEditor::keyStateChanged (bool)
{
    for (auto it = keysDown.begin(); it != keysDown.end();)
        it = juce::KeyPress::isKeyCurrentlyDown (*it) ? std::next (it) : keysDown.erase (it);
    return false;
}

//==============================================================================
void LoamDaBoomEditor::timerCallback()
{
    // Pads light for ~90 ms on every hit, from the sequencer, MIDI, keys or clicks.
    for (int s = 0; s < numSlots; ++s)
    {
        const auto hits = audioProcessor.getHitCount (s);
        if (hits != shownHits[(size_t) s])
        {
            shownHits[(size_t) s] = hits;
            litTicks[(size_t) s] = 3;
        }
        pads[(size_t) s]->setLit (litTicks[(size_t) s] > 0);
        if (litTicks[(size_t) s] > 0)
            --litTicks[(size_t) s];

        // The version menus follow the state (a loaded kit, a project, undo...).
        const auto version = audioProcessor.getSlotVersion (s) + 1;
        if (versionBoxes[(size_t) s].getSelectedId() != version)
            versionBoxes[(size_t) s].setSelectedId (version, juce::dontSendNotification);
    }

    sequencer.refresh();

    const auto on = audioProcessor.isPlayOn();
    playButton.setToggleState (on, juce::dontSendNotification);
    playButton.setButtonText (on ? utf8 ("\xe2\x96\xa0 STOP") : utf8 ("\xe2\x96\xb6 PLAY"));
    for (int i = 0; i < numPatterns; ++i)
    {
        auto& b = patternButtons[(size_t) i];
        b.setToggleState (i == audioProcessor.getCurrentPattern(), juce::dontSendNotification);
        const auto queued = i == audioProcessor.getQueuedPattern();
        if ((bool) b.getProperties().getWithDefault ("queued", false) != queued)
        {
            b.getProperties().set ("queued", queued);
            b.repaint();
        }
    }

    // BPM: the DAW's tempo when it gives one (the BPM knob is then dimmed).
    bpmKnob.setDimmed (audioProcessor.isFollowingDawTempo());
    const auto step = audioProcessor.getShownStep();
    const auto stepText = step < 0 ? utf8 ("\xe2\x80\x94") : juce::String (step + 1);
    const auto bpmText = juce::String (juce::roundToInt (audioProcessor.getShownBpm()));
    if (stepText != shownStep || bpmText != shownBpm)
    {
        shownStep = stepText;
        shownBpm = bpmText;
        repaint (headerArea);
    }

    // The kit menu says "(edited)" once the sounds differ from the kit's.
    if (audioProcessor.getKitName() + (audioProcessor.isKitEdited() ? "*" : "") + juce::String (userKits.size()) != shownKitSignature)
        refreshKitBox();

    // Let the keys play straight away, without clicking the panel first.
    if (! focusTaken && isShowing())
    {
        grabKeyboardFocus();
        focusTaken = true;
    }
}

//==============================================================================
void LoamDaBoomEditor::paintModule (juce::Graphics& g, const Module& m, bool withTitle) const
{
    const auto bounds = m.bounds.toFloat();
    // Coloured top edge (the web module's border-top) peeking out above the face.
    g.setColour (m.accent);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (panelRaised);
    g.fillRoundedRectangle (bounds.withTrimmedTop (3.0f), 12.0f);
    g.setColour (grooveDark.withAlpha (0.6f));
    g.drawRoundedRectangle (bounds.withTrimmedTop (3.0f), 12.0f, 1.0f);

    if (withTitle)
    {
        g.setColour (m.accent);
        g.setFont (sansFont (14.0f, true).withExtraKerningFactor (0.12f));
        g.drawText (m.title.toUpperCase(), m.bounds.reduced (12, 0).withTrimmedTop (8).withHeight (16), juce::Justification::centredLeft);
    }
}

void LoamDaBoomEditor::paintReadout (juce::Graphics& g, juce::Rectangle<float> window, const juce::String& label, const juce::String& value) const
{
    g.setColour (readoutBg);
    g.fillRoundedRectangle (window, 6.0f);
    window.reduce (10.0f, 0.0f);
    g.setColour (readoutInk.withAlpha (0.55f));
    g.setFont (monoFont (9.0f).withExtraKerningFactor (0.1f));
    g.drawText (label, window.removeFromLeft (32.0f), juce::Justification::centredLeft, false);
    g.setColour (readoutInk);
    g.setFont (monoFont (15.0f));
    g.drawText (value, window, juce::Justification::centredLeft, false);
}

void LoamDaBoomEditor::paint (juce::Graphics& g)
{
    g.fillAll (panel);

    // Brand: LOAM DABOOM, the subtitle and the egret under it.
    auto brand = headerArea.toFloat();
    auto titleRow = brand.removeFromTop (32.0f);
    const auto brandFont = sansFont (30.0f, true);
    const std::pair<juce::String, juce::Colour> words[] = { { "LOAM", ink }, { "DABOOM", coral } };
    for (const auto& [text, colour] : words)
    {
        g.setFont (brandFont);
        g.setColour (colour);
        const auto w = juce::GlyphArrangement::getStringWidth (brandFont, text);
        g.drawText (text, titleRow.removeFromLeft (w + 8.0f), juce::Justification::bottomLeft, false);
    }
    g.setColour (inkSoft);
    g.setFont (monoFont (10.0f).withExtraKerningFactor (0.14f));
    g.drawText (brandSub, brand.removeFromTop (18.0f), juce::Justification::centredLeft, false);

    auto readouts = headerArea.toFloat();
    paintReadout (g, readouts.removeFromRight (96.0f).withSizeKeepingCentre (96.0f, 28.0f), "STEP", shownStep);
    readouts.removeFromRight (10.0f);
    paintReadout (g, readouts.removeFromRight (96.0f).withSizeKeepingCentre (96.0f, 28.0f), "BPM", shownBpm);

    for (const auto* m : { &transport, &kit, &master, &seqModule })
        paintModule (g, *m);
    for (const auto& strip : strips)
        paintModule (g, strip, false);

    g.setColour (inkSoft);
    g.setFont (sansFont (12.0f));
    g.drawFittedText (audioProcessor.isStandalone()
                          ? "Hit the pads or play Z X C V B N M , on the keyboard; Space starts and stops. Click a step once for a hit, "
                            "again for an accent, again to clear. MIDI notes play the voices too (General MIDI drums: C1 kick, D1 snare, F#1 closed hat...)."
                          : "In the DAW the pattern plays along with the transport while PLAY is lit. Hit the pads or play Z X C V B N M , ; "
                            "click a step once for a hit, again for an accent, again to clear. MIDI notes play the voices (General MIDI drums: C1 kick, D1 snare, F#1 closed hat...).",
                      instructionsArea, juce::Justification::topLeft, 2, 1.0f);
}

void LoamDaBoomEditor::resized()
{
    auto area = getLocalBounds().reduced (pad);
    headerArea = area.removeFromTop (headerHeight);
    area.removeFromTop (gap);

    const auto subWidth = juce::GlyphArrangement::getStringWidth (monoFont (10.0f).withExtraKerningFactor (0.14f), subtitleText());
    egret.setBounds (headerArea.getX() + juce::roundToInt (subWidth) + 8, headerArea.getY() + 30, 22, 22);

    // ---- top row: Transport | Kit | Master
    auto top = area.removeFromTop (topRowHeight);
    area.removeFromTop (gap);
    transport.bounds = top.removeFromLeft (470);
    top.removeFromLeft (gap);
    kit.bounds = top.removeFromLeft (190);
    top.removeFromLeft (gap);
    master.bounds = top;

    auto content = [] (const Module& m) { return m.bounds.reduced (12).withTrimmedTop (titleHeight); };

    grooveStatus.setBounds (transport.bounds.reduced (12, 0).withTrimmedTop (8).withHeight (16).withTrimmedLeft (110));
    auto t = content (transport);
    auto knobsArea = t.removeFromRight (2 * knobWidth + 8);
    bpmKnob.setBounds (knobsArea.removeFromLeft (knobWidth).withHeight (knobHeight));
    knobsArea.removeFromLeft (8);
    swingKnob.setBounds (knobsArea.removeFromLeft (knobWidth).withHeight (knobHeight));
    t.removeFromRight (8);
    auto row1 = t.removeFromTop (26);
    playButton.setBounds (row1.removeFromLeft (70));
    row1.removeFromLeft (8);
    patternLabel.setBounds (row1.removeFromLeft (54));
    for (auto& b : patternButtons)
    {
        b.setBounds (row1.removeFromLeft (26));
        row1.removeFromLeft (4);
    }
    row1.removeFromLeft (4);
    clearButton.setBounds (row1.removeFromLeft (54));
    t.removeFromTop (8);
    auto row2 = t.removeFromTop (26);
    libraryLabel.setBounds (row2.removeFromLeft (52));
    grooveDelete.setBounds (row2.removeFromRight (52));
    row2.removeFromRight (4);
    grooveSave.setBounds (row2.removeFromRight (44));
    row2.removeFromRight (4);
    grooveName.setBounds (row2.removeFromRight (74));
    row2.removeFromRight (4);
    grooveBox.setBounds (row2);

    auto k = content (kit);
    kitBox.setBounds (k.removeFromTop (26));
    k.removeFromTop (6);
    auto kitRow = k.removeFromTop (26);
    kitDelete.setBounds (kitRow.removeFromRight (52));
    kitRow.removeFromRight (4);
    kitSave.setBounds (kitRow.removeFromRight (44));
    kitRow.removeFromRight (4);
    kitName.setBounds (kitRow);
    k.removeFromTop (4);
    kitStatus.setBounds (k);

    auto m = content (master);
    auto masterRow = m.withSizeKeepingCentre (3 * knobWidth + 2 * 8, knobHeight).withY (m.getY());
    for (auto* knob : { &volumeKnob, &driveKnob, &reverbKnob })
    {
        knob->setBounds (masterRow.removeFromLeft (knobWidth));
        masterRow.removeFromLeft (8);
    }

    // ---- voice strips: version menu, pad, knobs two to a row
    auto stripsRow = area.removeFromTop (stripHeight);
    area.removeFromTop (gap);
    const auto stripWidth = (stripsRow.getWidth() - (numSlots - 1) * 8) / numSlots;
    for (int s = 0; s < numSlots; ++s)
    {
        auto strip = stripsRow.removeFromLeft (stripWidth);
        stripsRow.removeFromLeft (8);
        strips[(size_t) s].bounds = strip;
        auto inner = strip.reduced (4, 10).withTrimmedTop (2);
        versionBoxes[(size_t) s].setBounds (inner.removeFromTop (22));
        inner.removeFromTop (6);
        pads[(size_t) s]->setBounds (inner.removeFromTop (36).reduced (2, 0));
        inner.removeFromTop (6);
        const auto& list = slotKnobs[(size_t) s];
        for (size_t i = 0; i < list.size(); i += 2)
        {
            auto row = inner.removeFromTop (knobHeight);
            inner.removeFromTop (4);
            const auto inRow = (int) juce::jmin ((size_t) 2, list.size() - i);
            auto cells = row.withSizeKeepingCentre (inRow * knobWidth + (inRow - 1) * 4, knobHeight);
            for (int j = 0; j < inRow; ++j)
            {
                list[i + (size_t) j]->setBounds (cells.removeFromLeft (knobWidth));
                cells.removeFromLeft (4);
            }
        }
    }

    // ---- sequencer and instructions
    seqModule.bounds = area.removeFromTop (seqHeight);
    sequencer.setBounds (content (seqModule));
    area.removeFromTop (8);
    instructionsArea = area;
}
