#pragma once

#include <array>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "DrumKit.h"

// The web version's Web Audio graph, rebuilt sample by sample: every hit is a
// small set of oscillators, noise and filters with the same envelopes, gains
// and frequency sweeps; the master section is the same dry/tanh drive, volume
// and convolution reverb with the same generated impulse response.
class DrumEngine
{
public:
    void prepare (double sampleRate, int maximumBlockSize);
    void reset();

    // Starts a hit on the next rendered sample. `params` holds the slot's knob
    // values in knob units (LEVEL last); an accent plays at full velocity.
    void trigger (int slot, const float* params, bool accent);
    void chirp();   // the egret's two little calls

    void setMaster (float volumePct, float drivePct, float reverbPct);
    void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    int getNumActiveHits() const;
    bool isReverbReady() const { return reverb.getCurrentIRSize() > 0; }

    // Web Audio's BiquadFilterNode: highpass takes Q in dB, bandpass a plain Q.
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void setHighpass (double freq, double qDb, double sampleRate);
        void setBandpass (double freq, double q, double sampleRate);
        float process (float x);
    };

private:
    static constexpr int maxHits = 48;

    // Square waves as Chrome makes them for the web version: one cycle summed
    // from the odd harmonics below Nyquist, scaled so the full-band wave's
    // Gibbs peak is 1 (about 0.848 of a plain +-1 square).
    static constexpr int tableSize = 2048;
    using Table = std::array<float, tableSize + 1>;
    void buildSquare (Table&, double freq) const;
    static float readTable (const Table&, double phase);

    struct Hit
    {
        bool active = false;
        int type = 0;
        double t = 0.0, length = 0.0;     // seconds since the hit, and when it's over
        float vel = 1.0f, level = 1.0f;
        float p[3] {};                    // tune/tone, decay (s), third knob
        double phase[6] {};
        Biquad f1, f2;
        bool choked = false;
        double chokeAt = 0.0;
        float chokeFrom = 0.0f;
        std::uint64_t order = 0;
        Table square[2];   // the rim's and the cowbell's squares, built for their pitches
    };

    float renderHit (Hit&);
    Hit& allocate();
    float noise();

    double sampleRate = 44100.0;
    std::array<float, tableSize> sineTable {};
    std::array<Table, 6> metalTables {};   // the hats' six fixed squares
    std::array<Hit, maxHits> hits;
    std::uint64_t hitCounter = 0;
    std::uint32_t rng = 0x2545f491u;

    // the egret: two short rising sines straight to the output, like the web's chirp
    double chirpT = -1.0;
    double chirpPhase[2] {};

    // master
    float drive = 0.0f, driveGain = 1.0f;
    juce::SmoothedValue<float> driveMix, volume, reverbSend;
    juce::dsp::Convolution reverb;
    juce::AudioBuffer<float> sendBuffer;
    std::vector<float> dryBuffer;
    double reverbRate = 0.0;
    bool masterSet = false;   // the first setMaster after prepare jumps straight there
};

// The sequencer's own clock (standalone, or a host without a transport):
// counts 16th notes at the BPM knob, every second one late by the swing.
class StepClock
{
public:
    void start() { running = true; step = 0; untilNext = 0.0; }
    void stop()  { running = false; }
    bool isRunning() const { return running; }

    // Calls onStep (offset, step) for each step that falls in this block.
    template <typename Fn>
    void advance (int numSamples, double sampleRate, double bpm, double swingPct, Fn&& onStep)
    {
        if (! running)
            return;
        for (;;)
        {
            const auto stepLength = sampleRate * 60.0 / bpm / 4.0;
            const auto at = untilNext + (step % 2 == 1 ? swingPct / 100.0 * stepLength * 0.5 : 0.0);
            if (at >= numSamples)
                break;
            onStep (juce::jmax (0, (int) at), step);
            untilNext += stepLength;
            step = (step + 1) % daboom::numSteps;
        }
        untilNext -= numSamples;
    }

private:
    bool running = false;
    int step = 0;
    double untilNext = 0.0;   // samples from the block start to the next step (before swing)
};

// Steps of a host transport: the 16th notes (with swing) between this block's
// start position and the next one's. Remembers the last step it fired so a
// host's rounding can't play the same step twice.
class HostSteps
{
public:
    void reset() { last = std::numeric_limits<std::int64_t>::min(); }

    template <typename Fn>
    void advance (double ppqStart, double bpm, double sampleRate, int numSamples, double swingPct, Fn&& onStep)
    {
        const auto perSample = bpm / 60.0 * 4.0 / sampleRate;   // 16ths per sample
        const auto p0 = ppqStart * 4.0, p1 = p0 + numSamples * perSample;
        if (last != std::numeric_limits<std::int64_t>::min() && (double) last > p1 + 1.0)
            reset();                                              // the host jumped back
        for (auto k = (std::int64_t) std::floor (p0) - 1; (double) k < p1; ++k)
        {
            const auto odd = ((k % 2) + 2) % 2 == 1;
            const auto at = (double) k + (odd ? swingPct / 100.0 * 0.5 : 0.0);
            if (at < p0 || at >= p1 || (last != std::numeric_limits<std::int64_t>::min() && k <= last))
                continue;
            last = k;
            onStep (juce::jlimit (0, numSamples - 1, (int) ((at - p0) / perSample)),
                    (int) (((k % daboom::numSteps) + daboom::numSteps) % daboom::numSteps));
        }
    }

private:
    std::int64_t last = std::numeric_limits<std::int64_t>::min();
};
