#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace daboom;

namespace
{
    const juce::Identifier kitProp ("kit"), patternProp ("pattern"), playProp ("playing"), patternsTag ("PATTERNS");
    constexpr const char* kitTag = "LoamDaBoomKit";
    constexpr const char* grooveTag = "LoamDaBoomGroove";

    juce::Identifier srcProp (int slot) { return juce::Identifier ("src_" + juce::String (voices()[(size_t) slot].id)); }
    juce::Identifier rowProp (int pattern, int slot)
    {
        return juce::Identifier ("p" + juce::String (pattern) + "_" + juce::String (voices()[(size_t) slot].id));
    }
    int stepIndex (int pattern, int slot, int step) { return (pattern * numSlots + slot) * numSteps + step; }

    // The web knobs' mapping: logarithmic ones are exactly exponential.
    juce::NormalisableRange<float> rangeFor (const KnobDef& k)
    {
        if (! k.log)
            return { k.min, k.max };
        return { k.min, k.max,
                 [] (float start, float end, float t) { return start * std::pow (end / start, t); },
                 [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
                 nullptr };
    }

    juce::String titleCase (const juce::String& s) { return s.substring (0, 1).toUpperCase() + s.substring (1).toLowerCase(); }

    // Finds the file holding a user kit or groove by its display name (case-insensitive).
    juce::File findByName (const juce::File& folder, const char* tag, const juce::String& name)
    {
        for (const auto& file : folder.findChildFiles (juce::File::findFiles, false, "*.xml"))
            if (auto xml = juce::XmlDocument::parse (file); xml != nullptr && xml->hasTagName (tag)
                    && xml->getStringAttribute ("name").equalsIgnoreCase (name))
                return file;
        return {};
    }

    juce::StringArray namesIn (const juce::File& folder, const char* tag)
    {
        juce::StringArray names;
        for (const auto& file : folder.findChildFiles (juce::File::findFiles, false, "*.xml"))
            if (auto xml = juce::XmlDocument::parse (file); xml != nullptr && xml->hasTagName (tag))
                names.addIfNotAlreadyThere (xml->getStringAttribute ("name"), true);
        names.sortNatural();
        return names;
    }

    bool writeNamed (const juce::File& folder, const char* tag, const juce::String& name, juce::XmlElement& xml)
    {
        if (! folder.createDirectory())
            return false;
        auto file = findByName (folder, tag, name);
        if (file == juce::File())
            file = folder.getNonexistentChildFile (juce::File::createLegalFileName (name), ".xml");
        return xml.writeTo (file);
    }
}

//==============================================================================
juce::String LoamDaBoomProcessor::paramId (int slot, int knob)
{
    const auto& v = voices()[(size_t) slot];
    return juce::String (v.id) + "_" + v.knobs[(size_t) knob].id;
}

juce::String LoamDaBoomProcessor::muteId (int slot) { return juce::String (voices()[(size_t) slot].id) + "_mute"; }

juce::AudioProcessorValueTreeState::ParameterLayout LoamDaBoomProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int s = 0; s < numSlots; ++s)
    {
        const auto& v = voices()[(size_t) s];
        for (int k = 0; k < v.numKnobs; ++k)
        {
            const auto& kd = v.knobs[(size_t) k];
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { paramId (s, k), 1 },
                                                                     titleCase (v.name) + " " + titleCase (kd.label), rangeFor (kd), kd.def));
        }
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { muteId (s), 1 }, titleCase (v.name) + " Mute", false));
    }
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "bpm", 1 }, "BPM", juce::NormalisableRange<float> (60.0f, 200.0f, 1.0f), 120.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "swing", 1 }, "Swing", juce::NormalisableRange<float> (0.0f, 75.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "volume", 1 }, "Volume", juce::NormalisableRange<float> (0.0f, 100.0f), 75.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "drive", 1 }, "Drive", juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "reverb", 1 }, "Reverb", juce::NormalisableRange<float> (0.0f, 100.0f), 15.0f));
    return layout;
}

LoamDaBoomProcessor::LoamDaBoomProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LoamDaBoom", createLayout())
{
    for (int s = 0; s < numSlots; ++s)
    {
        for (int k = 0; k < voices()[(size_t) s].numKnobs; ++k)
            knobs[(size_t) s][(size_t) k] = apvts.getRawParameterValue (paramId (s, k));
        mutes[(size_t) s] = apvts.getRawParameterValue (muteId (s));
        apvts.state.setProperty (srcProp (s), 0, nullptr);
    }
    bpm = apvts.getRawParameterValue ("bpm");
    swing = apvts.getRawParameterValue ("swing");
    volume = apvts.getRawParameterValue ("volume");
    drive = apvts.getRawParameterValue ("drive");
    reverb = apvts.getRawParameterValue ("reverb");

    // Patterns A-D start as the library's first four grooves, as on the web.
    for (int p = 0; p < numPatterns; ++p)
    {
        std::array<juce::String, numSlots> rows;
        for (int s = 0; s < numSlots; ++s)
            rows[(size_t) s] = factoryGrooves()[(size_t) p].rows[(size_t) s];
        loadPatternRows (p, rows);
    }

    // In a DAW the sequencer is armed from the start, so pressing play there plays the pattern.
    playOn = ! isStandalone();
    setKitBase (factoryKitName (0));
}

bool LoamDaBoomProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void LoamDaBoomProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    hostSteps.reset();
}

//==============================================================================
std::array<float, maxKnobs> LoamDaBoomProcessor::slotValues (int slot) const
{
    std::array<float, maxKnobs> values {};
    for (int k = 0; k < voices()[(size_t) slot].numKnobs; ++k)
        values[(size_t) k] = knobs[(size_t) slot][(size_t) k]->load();
    return values;
}

void LoamDaBoomProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const auto n = buffer.getNumSamples();
    engine.setMaster (volume->load(), drive->load(), reverb->load());

    struct Hit { int offset, slot; bool accent; };
    std::array<Hit, 256> hits;
    int count = 0;
    auto add = [&] (int offset, int slot, bool accent)
    {
        if (count < (int) hits.size())
            hits[(size_t) count++] = { offset, slot, accent };
    };

    if (chirpPending.exchange (false))
        engine.chirp();
    const auto accented = padAccent.exchange (0), plain = padPlain.exchange (0);
    for (int s = 0; s < numSlots; ++s)
    {
        if ((accented >> s) & 1u) add (0, s, true);
        if ((plain >> s) & 1u)    add (0, s, false);
    }

    // MIDI: General MIDI drum notes; velocity 100 and up is an accent, as on the web.
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            if (const auto slot = slotForNote (msg.getNoteNumber()); slot >= 0)
                add (metadata.samplePosition, slot, msg.getVelocity() >= 100);
    }

    // Sequencer steps falling in this block.
    auto onStep = [&] (int offset, int step)
    {
        if (step == 0)
            if (const auto queued = queuedPattern.exchange (-1); queued >= 0)
                currentPattern = queued;
        const auto pattern = currentPattern.load();
        for (int s = 0; s < numSlots; ++s)
            if (const auto v = steps[(size_t) stepIndex (pattern, s, step)].load(); v != 0 && mutes[(size_t) s]->load() < 0.5f)
                add (offset, s, v == 2);
        shownStep = step;
    };

    juce::Optional<juce::AudioPlayHead::PositionInfo> position;
    if (auto* playHead = getPlayHead())
        position = playHead->getPosition();
    const auto dawBpm = position && position->getBpm() ? *position->getBpm() : 0.0;
    const auto dawRunning = ! isStandalone() && position && position->getIsPlaying() && position->getPpqPosition() && dawBpm > 0.0;
    hostBpm = dawBpm;
    hostPlaying = dawRunning;

    if (! playOn)
    {
        clock.stop();
        hostSteps.reset();
        shownStep = -1;
    }
    else if (! isStandalone())
    {
        if (dawRunning)
            hostSteps.advance (*position->getPpqPosition(), dawBpm, sampleRate, n, swing->load(), onStep);
        else
        {
            hostSteps.reset();
            shownStep = -1;
        }
    }
    else
    {
        if (! clock.isRunning())
            clock.start();
        clock.advance (n, sampleRate, bpm->load(), swing->load(), onStep);
    }

    // Render up to each hit, start it, carry on: every hit lands on its sample.
    std::stable_sort (hits.begin(), hits.begin() + count, [] (const Hit& a, const Hit& b) { return a.offset < b.offset; });
    int at = 0;
    for (int i = 0; i < count; ++i)
    {
        const auto& h = hits[(size_t) i];
        const auto offset = juce::jlimit (0, n, h.offset);
        if (offset > at)
        {
            engine.render (buffer, at, offset - at);
            at = offset;
        }
        const auto values = slotValues (h.slot);
        engine.trigger (h.slot, values.data(), h.accent);
        ++hitCount[(size_t) h.slot];
    }
    if (at < n)
        engine.render (buffer, at, n - at);
}

//==============================================================================
int LoamDaBoomProcessor::getStep (int pattern, int slot, int step) const
{
    return steps[(size_t) stepIndex (pattern, slot, step)].load();
}

void LoamDaBoomProcessor::setStep (int pattern, int slot, int step, int value)
{
    steps[(size_t) stepIndex (pattern, slot, step)] = juce::jlimit (0, 2, value);
}

int LoamDaBoomProcessor::cycleStep (int slot, int step)
{
    const auto pattern = currentPattern.load();
    const auto v = (getStep (pattern, slot, step) + 1) % 3;
    setStep (pattern, slot, step, v);
    if (v != 0 && ! isRunning())
        hitPad (slot, v == 2);   // hear what you put in
    return v;
}

void LoamDaBoomProcessor::clearPattern()
{
    for (int s = 0; s < numSlots; ++s)
        for (int i = 0; i < numSteps; ++i)
            setStep (currentPattern, s, i, 0);
}

void LoamDaBoomProcessor::requestPattern (int pattern)
{
    if (isRunning() && pattern != currentPattern.load())
        queuedPattern = pattern;
    else
    {
        currentPattern = pattern;
        queuedPattern = -1;
    }
}

void LoamDaBoomProcessor::setPlaying (bool on)
{
    playOn = on;
    if (! on)
        queuedPattern = -1;
}

bool LoamDaBoomProcessor::isRunning() const
{
    return playOn.load() && (isStandalone() || hostPlaying.load());
}

double LoamDaBoomProcessor::getShownBpm() const
{
    const auto daw = hostBpm.load();
    return ! isStandalone() && daw > 0.0 ? daw : (double) bpm->load();
}

void LoamDaBoomProcessor::hitPad (int slot, bool accent)
{
    (accent ? padAccent : padPlain).fetch_or (1u << slot);
}

//==============================================================================
void LoamDaBoomProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

juce::String LoamDaBoomProcessor::getKitName() const
{
    return apvts.state.getProperty (kitProp, factoryKitName (0)).toString();
}

int LoamDaBoomProcessor::getSlotVersion (int slot) const
{
    return juce::jlimit (0, numFactoryKits - 1, (int) apvts.state.getProperty (srcProp (slot), 0));
}

void LoamDaBoomProcessor::setSlotVersion (int slot, int factoryKit)
{
    apvts.state.setProperty (srcProp (slot), factoryKit, nullptr);
    for (int k = 0; k < voices()[(size_t) slot].numKnobs; ++k)
        setParam (paramId (slot, k), factoryValue (slot, k, factoryKit));
    hitPad (slot, true);   // let you hear what you picked
}

void LoamDaBoomProcessor::loadFactoryKit (int factoryKit)
{
    for (int s = 0; s < numSlots; ++s)
    {
        apvts.state.setProperty (srcProp (s), factoryKit, nullptr);
        for (int k = 0; k < voices()[(size_t) s].numKnobs; ++k)
            setParam (paramId (s, k), factoryValue (s, k, factoryKit));
    }
    setKitBase (factoryKitName (factoryKit));
}

void LoamDaBoomProcessor::setKitBase (const juce::String& name)
{
    apvts.state.setProperty (kitProp, name, nullptr);
    kitBase = {};
    for (int f = 0; f < numFactoryKits; ++f)
        if (name == factoryKitName (f))
        {
            kitBase.valid = true;
            for (int s = 0; s < numSlots; ++s)
            {
                kitBase.src[(size_t) s] = f;
                for (int k = 0; k < voices()[(size_t) s].numKnobs; ++k)
                    kitBase.values[(size_t) s][(size_t) k] = factoryValue (s, k, f);
            }
            return;
        }

    if (auto xml = juce::XmlDocument::parse (findByName (getUserFolder ("Kits"), kitTag, name)))
    {
        kitBase.valid = true;
        for (int s = 0; s < numSlots; ++s)
        {
            const auto& v = voices()[(size_t) s];
            auto* slot = xml->getChildByAttribute ("id", v.id);
            kitBase.src[(size_t) s] = slot != nullptr ? slot->getIntAttribute ("src", 0) : 0;
            for (int k = 0; k < v.numKnobs; ++k)
                kitBase.values[(size_t) s][(size_t) k] = slot != nullptr ? (float) slot->getDoubleAttribute (v.knobs[(size_t) k].id, v.knobs[(size_t) k].def)
                                                                         : v.knobs[(size_t) k].def;
        }
    }
}

bool LoamDaBoomProcessor::isKitEdited() const
{
    if (! kitBase.valid)
        return true;
    for (int s = 0; s < numSlots; ++s)
    {
        if (getSlotVersion (s) != kitBase.src[(size_t) s])
            return true;
        const auto& v = voices()[(size_t) s];
        for (int k = 0; k < v.numKnobs; ++k)
        {
            const auto& kd = v.knobs[(size_t) k];
            if (std::abs (knobs[(size_t) s][(size_t) k]->load() - kitBase.values[(size_t) s][(size_t) k]) > 1e-3f * (kd.max - kd.min))
                return true;
        }
    }
    return false;
}

juce::File LoamDaBoomProcessor::getUserFolder (const juce::String& which)
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Loam").getChildFile ("Loam DaBoom").getChildFile (which);
}

juce::StringArray LoamDaBoomProcessor::getUserKitNames() const { return namesIn (getUserFolder ("Kits"), kitTag); }

bool LoamDaBoomProcessor::saveUserKit (const juce::String& name)
{
    juce::XmlElement xml (kitTag);
    xml.setAttribute ("name", name);
    for (int s = 0; s < numSlots; ++s)
    {
        const auto& v = voices()[(size_t) s];
        auto* slot = xml.createNewChildElement ("SLOT");
        slot->setAttribute ("id", v.id);
        slot->setAttribute ("src", getSlotVersion (s));
        for (int k = 0; k < v.numKnobs; ++k)
            slot->setAttribute (v.knobs[(size_t) k].id, knobs[(size_t) s][(size_t) k]->load());
    }
    if (! writeNamed (getUserFolder ("Kits"), kitTag, name, xml))
        return false;
    setKitBase (name);
    return true;
}

bool LoamDaBoomProcessor::loadUserKit (const juce::String& name)
{
    auto xml = juce::XmlDocument::parse (findByName (getUserFolder ("Kits"), kitTag, name));
    if (xml == nullptr)
        return false;
    for (int s = 0; s < numSlots; ++s)
    {
        const auto& v = voices()[(size_t) s];
        auto* slot = xml->getChildByAttribute ("id", v.id);
        apvts.state.setProperty (srcProp (s), slot != nullptr ? juce::jlimit (0, numFactoryKits - 1, slot->getIntAttribute ("src", 0)) : 0, nullptr);
        for (int k = 0; k < v.numKnobs; ++k)
        {
            const auto& kd = v.knobs[(size_t) k];
            setParam (paramId (s, k), slot != nullptr ? (float) slot->getDoubleAttribute (kd.id, kd.def) : kd.def);
        }
    }
    setKitBase (xml->getStringAttribute ("name"));
    return true;
}

bool LoamDaBoomProcessor::deleteUserKit (const juce::String& name)
{
    const auto file = findByName (getUserFolder ("Kits"), kitTag, name);
    if (file == juce::File() || ! file.deleteFile())
        return false;
    if (getKitName().equalsIgnoreCase (name))
        loadFactoryKit (0);   // as on the web: back to the 808
    return true;
}

//==============================================================================
juce::String LoamDaBoomProcessor::patternRow (int pattern, int slot) const
{
    juce::String row;
    for (int i = 0; i < numSteps; ++i)
    {
        const auto v = getStep (pattern, slot, i);
        row << (v == 2 ? "X" : v == 1 ? "x" : ".");
    }
    return row;
}

void LoamDaBoomProcessor::loadPatternRows (int pattern, const std::array<juce::String, numSlots>& rows)
{
    for (int s = 0; s < numSlots; ++s)
        for (int i = 0; i < numSteps; ++i)
        {
            const auto c = i < rows[(size_t) s].length() ? rows[(size_t) s][i] : '.';
            setStep (pattern, s, i, c == 'X' ? 2 : c == 'x' ? 1 : 0);
        }
}

void LoamDaBoomProcessor::loadFactoryGroove (int index)
{
    const auto& groove = factoryGrooves()[(size_t) juce::jlimit (0, (int) factoryGrooves().size() - 1, index)];
    std::array<juce::String, numSlots> rows;
    for (int s = 0; s < numSlots; ++s)
        rows[(size_t) s] = groove.rows[(size_t) s];
    loadPatternRows (currentPattern, rows);
}

juce::StringArray LoamDaBoomProcessor::getUserGrooveNames() const { return namesIn (getUserFolder ("Grooves"), grooveTag); }

bool LoamDaBoomProcessor::saveUserGroove (const juce::String& name)
{
    juce::XmlElement xml (grooveTag);
    xml.setAttribute ("name", name);
    for (int s = 0; s < numSlots; ++s)
    {
        auto* row = xml.createNewChildElement ("ROW");
        row->setAttribute ("id", voices()[(size_t) s].id);
        row->setAttribute ("steps", patternRow (currentPattern, s));
    }
    return writeNamed (getUserFolder ("Grooves"), grooveTag, name, xml);
}

bool LoamDaBoomProcessor::loadUserGroove (const juce::String& name)
{
    auto xml = juce::XmlDocument::parse (findByName (getUserFolder ("Grooves"), grooveTag, name));
    if (xml == nullptr)
        return false;
    std::array<juce::String, numSlots> rows;
    for (int s = 0; s < numSlots; ++s)
        if (auto* row = xml->getChildByAttribute ("id", voices()[(size_t) s].id))
            rows[(size_t) s] = row->getStringAttribute ("steps");
    loadPatternRows (currentPattern, rows);
    return true;
}

bool LoamDaBoomProcessor::deleteUserGroove (const juce::String& name)
{
    const auto file = findByName (getUserFolder ("Grooves"), grooveTag, name);
    return file != juce::File() && file.deleteFile();
}

//==============================================================================
void LoamDaBoomProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (patternProp, currentPattern.load(), nullptr);
    state.setProperty (playProp, playOn.load(), nullptr);
    state.removeChild (state.getChildWithName (patternsTag), nullptr);
    juce::ValueTree patterns (patternsTag);
    for (int p = 0; p < numPatterns; ++p)
        for (int s = 0; s < numSlots; ++s)
            patterns.setProperty (rowProp (p, s), patternRow (p, s), nullptr);
    state.appendChild (patterns, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LoamDaBoomProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml (*xml);

    if (auto patterns = state.getChildWithName (patternsTag); patterns.isValid())
    {
        for (int p = 0; p < numPatterns; ++p)
        {
            std::array<juce::String, numSlots> rows;
            for (int s = 0; s < numSlots; ++s)
                rows[(size_t) s] = patterns.getProperty (rowProp (p, s)).toString();
            loadPatternRows (p, rows);
        }
        state.removeChild (patterns, nullptr);
    }
    currentPattern = juce::jlimit (0, numPatterns - 1, (int) state.getProperty (patternProp, 0));
    queuedPattern = -1;
    playOn = (bool) state.getProperty (playProp, ! isStandalone());

    apvts.replaceState (state);
    setKitBase (getKitName());
}

juce::AudioProcessorEditor* LoamDaBoomProcessor::createEditor()
{
    return new LoamDaBoomEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LoamDaBoomProcessor();
}
