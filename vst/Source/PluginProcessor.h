#pragma once

#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "DrumEngine.h"

// Loam DaBoom as a plugin: the web version's drum machine, with its 16-step
// sequencer following the DAW's transport (or its own clock in the standalone
// app), patterns and kit saved with the project, and user kits and grooves
// saved as files, like Loam Eight Sub's user presets.
class LoamDaBoomProcessor : public juce::AudioProcessor
{
public:
    LoamDaBoomProcessor();
    ~LoamDaBoomProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Loam DaBoom"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static juce::String paramId (int slot, int knob);   // "kick_tune", "snare_level", ...
    static juce::String muteId (int slot);              // "kick_mute"

    //==============================================================================
    // Sequencer. Steps are 0 off, 1 hit, 2 accent; the UI edits the current pattern.
    int getStep (int pattern, int slot, int step) const;
    void setStep (int pattern, int slot, int step, int value);
    int cycleStep (int slot, int step);   // off -> hit -> accent -> off; plays it when stopped
    void clearPattern();
    int getCurrentPattern() const { return currentPattern.load(); }
    int getQueuedPattern() const { return queuedPattern.load(); }
    void requestPattern (int pattern);    // while running: switches at the end of the bar

    // PLAY: in the standalone app it starts the own clock; in a DAW it arms the
    // sequencer to play along whenever the DAW's transport runs.
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }
    void setPlaying (bool);
    bool isPlayOn() const { return playOn.load(); }
    bool isRunning() const;               // actually stepping right now
    int getShownStep() const { return shownStep.load(); }
    double getShownBpm() const;           // the DAW's tempo when it gives one, else BPM
    bool isFollowingDawTempo() const { return ! isStandalone() && hostBpm.load() > 0.0; }

    // Pads, keys and the egret: picked up at the start of the next block.
    void hitPad (int slot, bool accent);
    void chirp() { chirpPending = true; }
    juce::uint32 getHitCount (int slot) const { return hitCount[(size_t) slot].load(); }

    //==============================================================================
    // Kits: each slot picks a version of its voice (808, 909, Lo-Fi), then knobs.
    juce::String getKitName() const;      // "808", "My kit", ...
    bool isKitEdited() const;             // the sounds differ from the kit they came from
    int getSlotVersion (int slot) const;
    void setSlotVersion (int slot, int factoryKit);   // that version's sound, and a preview hit
    void loadFactoryKit (int factoryKit);

    static juce::File getUserFolder (const juce::String& which);   // "Kits" or "Grooves"
    juce::StringArray getUserKitNames() const;
    bool saveUserKit (const juce::String& name);
    bool loadUserKit (const juce::String& name);
    bool deleteUserKit (const juce::String& name);

    // The pattern library: grooves load into the current pattern, whatever the kit.
    void loadFactoryGroove (int index);
    juce::StringArray getUserGrooveNames() const;
    bool saveUserGroove (const juce::String& name);
    bool loadUserGroove (const juce::String& name);
    bool deleteUserGroove (const juce::String& name);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::array<float, daboom::maxKnobs> slotValues (int slot) const;
    void setParam (const juce::String& id, float value);
    void setKitBase (const juce::String& name);   // remembers what "edited" compares against
    void loadPatternRows (int pattern, const std::array<juce::String, daboom::numSlots>& rows);
    juce::String patternRow (int pattern, int slot) const;

    DrumEngine engine;
    StepClock clock;
    HostSteps hostSteps;
    double sampleRate = 44100.0;

    std::array<std::atomic<int>, daboom::numPatterns * daboom::numSlots * daboom::numSteps> steps {};
    std::atomic<int> currentPattern { 0 }, queuedPattern { -1 }, shownStep { -1 };
    std::atomic<bool> playOn { false }, hostPlaying { false }, chirpPending { false };
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<juce::uint32> padAccent { 0 }, padPlain { 0 };
    std::array<std::atomic<juce::uint32>, daboom::numSlots> hitCount {};

    std::array<std::array<std::atomic<float>*, daboom::maxKnobs>, daboom::numSlots> knobs {};
    std::array<std::atomic<float>*, daboom::numSlots> mutes {};
    std::atomic<float>* bpm = nullptr;
    std::atomic<float>* swing = nullptr;
    std::atomic<float>* volume = nullptr;
    std::atomic<float>* drive = nullptr;
    std::atomic<float>* reverb = nullptr;

    // The kit "edited" compares against (message thread only).
    struct KitBase
    {
        bool valid = false;
        std::array<int, daboom::numSlots> src {};
        std::array<std::array<float, daboom::maxKnobs>, daboom::numSlots> values {};
    } kitBase;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoamDaBoomProcessor)
};
