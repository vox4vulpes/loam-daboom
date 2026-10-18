#pragma once

#include <array>
#include <set>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// The web version's :root palette, so the plugin and the browser drum machine
// read as the same instrument (and as Loam Eight Sub's sibling).
namespace LoamColours
{
    inline const juce::Colour panel       { 0xffe9e1cc };
    inline const juce::Colour panelRaised { 0xfff2ecd9 };
    inline const juce::Colour groove      { 0xffc7bb9c };
    inline const juce::Colour grooveDark  { 0xffa99c79 };
    inline const juce::Colour ink         { 0xff2a241d };
    inline const juce::Colour inkSoft     { 0x9e2a241d };
    inline const juce::Colour mustard     { 0xffc08f17 };
    inline const juce::Colour teal        { 0xff336f6b };
    inline const juce::Colour coral       { 0xffb85540 };
    inline const juce::Colour lavender    { 0xff8a78a3 };
    inline const juce::Colour sage        { 0xff5f7344 };
    inline const juce::Colour readoutBg   { 0xff171308 };
    inline const juce::Colour readoutInk  { 0xfff0a830 };
    inline const juce::Colour shadow      { 0x591e190f };
}

class LoamLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LoamLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

// One knob as on the web panel: dial, label under it, value under that.
class DaBoomKnob : public juce::Component
{
public:
    DaBoomKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label,
                juce::Colour accent, daboom::Unit unit);
    void resized() override;
    void setDimmed (bool);

private:
    void updateReadout();
    juce::Slider slider;
    juce::Label label, readout;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    daboom::Unit unit;
};

// A drum pad: shows the computer key that plays it, lights up on every hit.
class PadView : public juce::Component
{
public:
    PadView (juce::String keyText, juce::Colour accent);
    std::function<void()> onHit;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onHit) onHit(); }
    void setLit (bool);

private:
    juce::String keyText;
    juce::Colour accent;
    bool lit = false;
};

// The 16-step grid: a mute button and name per row, steps cycling off -> hit -> accent.
class SequencerView : public juce::Component
{
public:
    explicit SequencerView (LoamDaBoomProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();   // repaints if the pattern, the steps or the playing step changed

private:
    juce::Rectangle<float> stepBounds (int row, int step) const;
    juce::Rectangle<float> muteBounds (int row) const;
    bool isMuted (int slot) const;

    LoamDaBoomProcessor& processor;
    static constexpr float nameWidth = 92.0f, headHeight = 14.0f, rowHeight = 22.0f, rowGap = 3.0f, stepGap = 4.0f;
    juce::String shownSignature;
};

// Easter egg: 小鷺 (kosagi), the little egret, beside the subtitle.
class EgretView : public juce::Component, public juce::SettableTooltipClient
{
public:
    EgretView();
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (contains (e.getPosition()) && onClick) onClick(); }

private:
    std::unique_ptr<juce::Drawable> drawable;
};

class LoamDaBoomEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LoamDaBoomEditor (LoamDaBoomProcessor&);
    ~LoamDaBoomEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;

private:
    struct Module
    {
        juce::String title;
        juce::Colour accent;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    void paintModule (juce::Graphics&, const Module&, bool withTitle = true) const;
    void paintReadout (juce::Graphics&, juce::Rectangle<float>, const juce::String& label, const juce::String& value) const;

    void refreshKitBox();
    void refreshGrooveBox (int selectId = 0);
    void saveKit();
    void deleteKit();
    void saveGroove();
    void deleteGroove();
    void setKitStatus (const juce::String&);
    void setGrooveStatus (const juce::String&);

    LoamDaBoomProcessor& audioProcessor;
    LoamLookAndFeel lnf;
    juce::TooltipWindow tooltipWindow { this, 600 };

    // transport
    juce::TextButton playButton, clearButton { "CLEAR" };
    std::array<juce::TextButton, daboom::numPatterns> patternButtons;
    juce::Label patternLabel, libraryLabel, grooveStatus;
    juce::ComboBox grooveBox;
    juce::TextEditor grooveName;
    juce::TextButton grooveSave { "SAVE" }, grooveDelete { "DELETE" };
    juce::StringArray userGrooves;
    bool grooveDeleteArmed = false;
    DaBoomKnob bpmKnob, swingKnob;

    // kit
    juce::ComboBox kitBox;
    juce::TextEditor kitName;
    juce::TextButton kitSave { "SAVE" }, kitDelete { "DELETE" };
    juce::Label kitStatus;
    juce::StringArray userKits;
    juce::String shownKitSignature;
    bool kitDeleteArmed = false;

    // master
    DaBoomKnob volumeKnob, driveKnob, reverbKnob;

    // voices
    std::array<juce::ComboBox, daboom::numSlots> versionBoxes;
    std::array<std::unique_ptr<PadView>, daboom::numSlots> pads;
    std::vector<std::unique_ptr<DaBoomKnob>> voiceKnobs;
    std::array<std::vector<DaBoomKnob*>, daboom::numSlots> slotKnobs;
    std::array<juce::uint32, daboom::numSlots> shownHits {};
    std::array<int, daboom::numSlots> litTicks {};

    SequencerView sequencer;
    EgretView egret;
    juce::String brandSub;

    Module transport { "Transport", LoamColours::coral,   {} };
    Module kit       { "Kit",       LoamColours::sage,    {} };
    Module master    { "Master",    LoamColours::mustard, {} };
    Module seqModule { "Sequencer", LoamColours::teal,    {} };
    std::array<Module, daboom::numSlots> strips;
    juce::Rectangle<int> headerArea, instructionsArea;

    std::set<int> keysDown;
    bool focusTaken = false;
    juce::String shownStep, shownBpm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoamDaBoomEditor)
};
