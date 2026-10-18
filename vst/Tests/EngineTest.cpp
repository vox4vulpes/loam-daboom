// Assert-style smoke test for the DrumEngine and the sequencer clocks. No
// framework: build and run it, a non-zero exit means something broke.
//   cmake --build build --target EngineTest && ./build/EngineTest

#include <cstdio>
#include <cmath>
#include <vector>
#include "../Source/DrumEngine.h"

using namespace daboom;

static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok)
        ++failures;
}

static std::vector<float> paramsFor (int slot, int kit)
{
    std::vector<float> p ((size_t) maxKnobs, 0.0f);
    for (int k = 0; k < voices()[(size_t) slot].numKnobs; ++k)
        p[(size_t) k] = factoryValue (slot, k, kit);
    return p;
}

// Renders `seconds` into a stereo buffer, in 512-sample blocks.
static juce::AudioBuffer<float> render (DrumEngine& engine, double seconds, double sampleRate = 48000.0)
{
    juce::AudioBuffer<float> out (2, (int) (seconds * sampleRate));
    out.clear();
    for (int start = 0; start < out.getNumSamples(); start += 512)
        engine.render (out, start, juce::jmin (512, out.getNumSamples() - start));
    return out;
}

static float peak (const juce::AudioBuffer<float>& b, int ch, int start, int num)
{
    return b.getMagnitude (ch, start, juce::jmin (num, b.getNumSamples() - start));
}

static float rms (const juce::AudioBuffer<float>& b, int ch, int start, int num)
{
    return b.getRMSLevel (ch, start, juce::jmin (num, b.getNumSamples() - start));
}

int main()
{
    const double sr = 48000.0;

    // ---- every voice sounds, then stops
    for (int slot = 0; slot < numSlots; ++slot)
    {
        DrumEngine engine;
        engine.prepare (sr, 512);
        engine.setMaster (100.0f, 0.0f, 0.0f);
        const auto p = paramsFor (slot, 0);
        engine.trigger (slot, p.data(), true);
        const auto out = render (engine, 2.0);
        char what[96];
        std::snprintf (what, sizeof (what), "%s sounds (peak %.3f) and ends", voices()[(size_t) slot].name, peak (out, 0, 0, out.getNumSamples()));
        check (peak (out, 0, 0, out.getNumSamples()) > 0.02f && engine.getNumActiveHits() == 0, what);
    }

    // ---- an accent is louder
    {
        DrumEngine a, n;
        for (auto* e : { &a, &n }) { e->prepare (sr, 512); e->setMaster (100.0f, 0.0f, 0.0f); }
        const auto p = paramsFor (kick, 0);
        a.trigger (kick, p.data(), true);
        n.trigger (kick, p.data(), false);
        check (peak (render (a, 0.3), 0, 0, 14400) > 1.3f * peak (render (n, 0.3), 0, 0, 14400), "accent is louder than a plain hit");
    }

    // ---- the closed hat chokes the open one
    {
        auto openTail = [&] (bool choke)
        {
            DrumEngine e;
            e.prepare (sr, 512);
            e.setMaster (100.0f, 0.0f, 0.0f);
            const auto oh = paramsFor (openHat, 0);
            e.trigger (openHat, oh.data(), true);
            render (e, 0.1);
            if (choke)
            {
                auto ch = paramsFor (closedHat, 0);
                ch[(size_t) (voices()[closedHat].numKnobs - 1)] = 0.0f;   // a silent closed hat: only its choke is heard
                e.trigger (closedHat, ch.data(), true);
            }
            const auto out = render (e, 0.3);
            return rms (out, 0, (int) (0.1 * sr), (int) (0.2 * sr));
        };
        check (openTail (true) < 0.05f * openTail (false), "closed hat chokes the open hat");
    }

    // ---- drive: changes the sound, stays sane
    {
        DrumEngine clean, driven;
        clean.prepare (sr, 512); driven.prepare (sr, 512);
        clean.setMaster (100.0f, 0.0f, 0.0f); driven.setMaster (100.0f, 100.0f, 0.0f);
        const auto p = paramsFor (snare, 0);
        clean.trigger (snare, p.data(), true); driven.trigger (snare, p.data(), true);
        const auto c = render (clean, 0.3), d = render (driven, 0.3);
        const auto pc = peak (c, 0, 0, c.getNumSamples()), pd = peak (d, 0, 0, d.getNumSamples());
        check (std::abs (pd - pc) > 0.01f && pd <= 1.01f, "drive changes the hit and keeps it within full scale");
    }

    // ---- reverb: a stereo tail after a short rim shot
    {
        DrumEngine e;
        e.prepare (sr, 512);
        e.setMaster (100.0f, 0.0f, 100.0f);
        for (int i = 0; i < 400 && ! e.isReverbReady(); ++i)   // the impulse response loads in the background
        {
            render (e, 0.01);
            juce::Thread::sleep (5);
        }
        check (e.isReverbReady(), "reverb impulse response loaded");
        for (int i = 0; i < 40; ++i)   // and give the convolution engine time to swap in (offline runs far faster than real time)
        {
            render (e, 0.01);
            juce::Thread::sleep (10);
        }
        const auto p = paramsFor (rim, 0);
        e.trigger (rim, p.data(), true);
        const auto out = render (e, 1.0);
        const int from = (int) (0.3 * sr), num = (int) (0.4 * sr);
        double diff = 0.0;
        for (int i = from; i < from + num; ++i)
            diff += std::abs (out.getSample (0, i) - out.getSample (1, i));
        check (rms (out, 0, from, num) > 1e-4f && diff > 1e-3, "reverb leaves a stereo tail");
    }

    // ---- the egret's call
    {
        DrumEngine e;
        e.prepare (sr, 512);
        e.setMaster (0.0f, 0.0f, 0.0f);   // the master is silent; the call goes past it
        e.chirp();
        const auto out = render (e, 0.3);
        check (peak (out, 0, 0, out.getNumSamples()) > 0.05f, "egret chirps past the master volume");
    }

    // ---- own clock: 120 BPM at 48 kHz is a 16th every 6000 samples; swing delays the odd ones
    {
        auto collect = [&] (double swing)
        {
            StepClock clock;
            clock.start();
            std::vector<long> at;
            long base = 0;
            while (at.size() < 16)
            {
                clock.advance (512, sr, 120.0, swing, [&] (int offset, int) { at.push_back (base + offset); });
                base += 512;
            }
            return at;
        };
        const auto straight = collect (0.0), swung = collect (50.0);
        bool ok = true;
        for (size_t i = 0; i < 16; ++i)
        {
            ok = ok && straight[i] == (long) (i * 6000);
            ok = ok && swung[i] == (long) (i * 6000 + (i % 2 ? 1500 : 0));
        }
        check (ok, "own clock: steps every 6000 samples, swing 50% delays the odd ones by 1500");
    }

    // ---- host clock: every step once, at the right sample, and again after a jump back
    {
        HostSteps host;
        std::vector<std::pair<long, int>> got;
        double ppq = 0.0;
        const double bpm = 120.0, perBlock = 512 * bpm / 60.0 / sr;
        long base = 0;
        for (int b = 0; b < 200; ++b)
        {
            host.advance (ppq, bpm, sr, 512, 0.0, [&] (int offset, int step) { got.push_back ({ base + offset, step }); });
            ppq += perBlock;
            base += 512;
        }
        bool ok = got.size() > 16 && got[0].first == 0 && got[0].second == 0;
        for (size_t i = 0; i < got.size(); ++i)
            ok = ok && got[i].second == (int) (i % 16) && std::abs (got[i].first - (long) (i * 6000)) <= 1;
        check (ok, "host clock: each 16th once, every 6000 samples, steps counting 0..15");

        // the host loops back to bar 1: steps start over without being skipped
        std::vector<int> again;
        host.advance (0.0, bpm, sr, 512, 0.0, [&] (int, int step) { again.push_back (step); });
        check (again.size() == 1 && again[0] == 0, "host clock: a jump back plays step 1 again");
    }

    // ---- data, as in the web version
    check (factoryValue (kick, 0, 1) == 56.0f && factoryValue (snare, 2, 2) == 40.0f && factoryValue (tom, 2, 1) == 80.0f,
           "kit values match the web version (909 kick tune, Lo-Fi snappy, level)");
    check (slotForNote (36) == kick && slotForNote (44) == closedHat && slotForNote (50) == tom && slotForNote (60) == -1,
           "General MIDI notes map to the voices");
    check (stepValue (factoryGrooves()[0].rows[0], 0) == 2 && stepValue (factoryGrooves()[0].rows[0], 4) == 1
               && stepValue (factoryGrooves()[0].rows[1], 3) == 0,
           "grooves read: X accent, x hit, empty rows silent");

    std::printf ("%s\n", failures == 0 ? "all passed" : "FAILED");
    return failures == 0 ? 0 : 1;
}
