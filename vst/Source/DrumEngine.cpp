#include "DrumEngine.h"

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    constexpr float floorGain = 0.0001f;   // where the web's exponential ramps start and end

    // The web's env(): from 0.0001 up to `peak` in 2 ms, then an exponential
    // ramp back down to 0.0001 at `decay` seconds; silent before and after.
    float envelope (double t, float peak, double decay)
    {
        if (t < 0.0 || t >= decay || peak <= floorGain)
            return 0.0f;
        if (t < 0.002)
            return floorGain + (peak - floorGain) * (float) (t / 0.002);
        return peak * (float) std::pow ((double) (floorGain / peak), (t - 0.002) / (decay - 0.002));
    }

    // frequency.setValueAtTime(f0) + exponentialRampToValueAtTime(f1, time)
    double sweep (double t, double f0, double f1, double time)
    {
        return t >= time ? f1 : f0 * std::pow (f1 / f0, t / time);
    }

    double polyBlep (double t, double dt)
    {
        if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    // Band-limited like Web Audio's oscillators: polyBLEP square, plain triangle
    // (its harmonics fall off fast enough), both starting at phase 0 going up.
    float square (double phase, double dt)
    {
        auto v = phase < 0.5 ? 1.0 : -1.0;
        v += polyBlep (phase, dt);
        v -= polyBlep (std::fmod (phase + 0.5, 1.0), dt);
        return (float) v;
    }

    float triangle (double phase)
    {
        return (float) (phase < 0.25 ? 4.0 * phase : phase < 0.75 ? 2.0 - 4.0 * phase : 4.0 * phase - 4.0);
    }

    void advance (double& phase, double freq, double sampleRate)
    {
        phase += freq / sampleRate;
        phase -= std::floor (phase);
    }

    // The 808's six square waves at inharmonic ratios, the core of hats and cymbals.
    constexpr double metalFreqs[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
}

//==============================================================================
void DrumEngine::Biquad::setHighpass (double freq, double qDb, double sr)
{
    const auto w0 = twoPi * juce::jmin (freq, sr * 0.499) / sr;
    const auto alpha = std::sin (w0) / (2.0 * std::pow (10.0, qDb / 20.0));
    const auto c = std::cos (w0), a0 = 1.0 + alpha;
    b0 = (float) ((1.0 + c) / 2.0 / a0);
    b1 = (float) (-(1.0 + c) / a0);
    b2 = b0;
    a1 = (float) (-2.0 * c / a0);
    a2 = (float) ((1.0 - alpha) / a0);
}

void DrumEngine::Biquad::setBandpass (double freq, double q, double sr)
{
    const auto w0 = twoPi * juce::jmin (freq, sr * 0.499) / sr;
    const auto alpha = std::sin (w0) / (2.0 * q);
    const auto c = std::cos (w0), a0 = 1.0 + alpha;
    b0 = (float) (alpha / a0);
    b1 = 0.0f;
    b2 = (float) (-alpha / a0);
    a1 = (float) (-2.0 * c / a0);
    a2 = (float) ((1.0 - alpha) / a0);
}

float DrumEngine::Biquad::process (float x)
{
    const auto y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x;
    y2 = y1; y1 = y;
    return y;
}

//==============================================================================
void DrumEngine::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate;
    const auto block = juce::jmax (1, maximumBlockSize);
    sendBuffer.setSize (2, block);
    dryBuffer.assign ((size_t) block, 0.0f);

    for (auto* s : { &driveMix, &volume, &reverbSend })
        s->reset (sampleRate, 0.03);   // the web's setTargetAtTime(..., 0.01)

    reverb.prepare ({ sampleRate, (juce::uint32) block, 2 });

    // The web's impulse response: 1.6 s of stereo noise fading as (1 - x)^3,
    // fixed-seed here so a project sounds the same every time it's opened.
    if (reverbRate != sampleRate)
    {
        reverbRate = sampleRate;
        const auto len = (int) std::floor (sampleRate * 1.6);
        juce::AudioBuffer<float> ir (2, len);
        juce::Random random (0x10a3);
        double power = 0.0;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = ir.getWritePointer (ch);
            for (int i = 0; i < len; ++i)
            {
                d[i] = (random.nextFloat() * 2.0f - 1.0f) * (float) std::pow (1.0 - (double) i / len, 3.0);
                power += (double) d[i] * d[i];
            }
        }
        // ConvolverNode's normalize = true: scale by RMS power, calibrated as the spec says.
        power = juce::jmax (0.000125, std::sqrt (power / (2.0 * len)));
        ir.applyGain ((float) (1.0 / power * 0.00125 * 44100.0 / sampleRate));
        reverb.loadImpulseResponse (std::move (ir), sampleRate, juce::dsp::Convolution::Stereo::yes,
                                    juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    }

    reset();
}

void DrumEngine::reset()
{
    for (auto& h : hits)
        h.active = false;
    chirpT = -1.0;
    reverb.reset();
    driveMix.setCurrentAndTargetValue (drive / 100.0f);
    volume.setCurrentAndTargetValue (volume.getTargetValue());
    reverbSend.setCurrentAndTargetValue (reverbSend.getTargetValue());
}

void DrumEngine::setMaster (float volumePct, float drivePct, float reverbPct)
{
    if (drivePct != drive)
    {
        drive = drivePct;
        // The web's curve: tanh(g x) / tanh(g), g from 1 to 10.
        driveGain = 1.0f + drive / 100.0f * 9.0f;
    }
    driveMix.setTargetValue (drive / 100.0f);
    volume.setTargetValue (volumePct / 100.0f);
    reverbSend.setTargetValue (reverbPct / 100.0f * 0.6f);
}

float DrumEngine::noise()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float) rng / 2147483648.0f - 1.0f;
}

DrumEngine::Hit& DrumEngine::allocate()
{
    Hit* oldest = &hits[0];
    for (auto& h : hits)
    {
        if (! h.active)
            return h;
        if (h.order < oldest->order)
            oldest = &h;
    }
    return *oldest;
}

int DrumEngine::getNumActiveHits() const
{
    int n = 0;
    for (const auto& h : hits)
        n += h.active ? 1 : 0;
    return n;
}

void DrumEngine::chirp()
{
    chirpT = 0.0;
    chirpPhase[0] = chirpPhase[1] = 0.0;
}

void DrumEngine::trigger (int slot, const float* params, bool accent)
{
    using namespace daboom;
    const auto& voice = voices()[(size_t) slot];

    // A closed hat chokes the open one, as on the 808.
    if (slot == closedHat)
        for (auto& h : hits)
            if (h.active && h.type == openHat && ! h.choked)
            {
                h.choked = true;
                h.chokeAt = h.t;
                h.chokeFrom = envelope (h.t, h.vel * 0.32f, h.p[1]);
            }

    auto& h = allocate();
    h = Hit {};
    h.active = true;
    h.type = slot;
    h.order = ++hitCounter;
    h.vel = accent ? 1.0f : 0.7f;
    h.level = params[voice.numKnobs - 1] / 100.0f;
    h.p[0] = params[0];
    h.p[1] = params[1] / 1000.0f;                  // DECAY, in seconds
    h.p[2] = voice.numKnobs > 3 ? params[2] : 0.0f;

    const auto decay = (double) h.p[1];
    switch (slot)
    {
        case kick:      h.f1.setHighpass (2500.0, 1.0, sampleRate); h.length = juce::jmax (decay, 0.06); break;
        case snare:     h.f1.setHighpass (1800.0, 1.0, sampleRate); h.length = decay; break;
        case clap:      h.f1.setBandpass (h.p[0], 1.4, sampleRate); h.length = 3.0 * h.p[2] / 1000.0 + decay; break;
        case closedHat:
        case openHat:   h.f1.setBandpass (h.p[0], 1.0, sampleRate); h.f2.setHighpass (7000.0, 1.0, sampleRate); h.length = decay; break;
        case rim:       h.f1.setBandpass (h.p[0], 3.0, sampleRate); h.length = decay; break;
        case cowbell:   h.f1.setBandpass (2640.0, 1.5, sampleRate); h.length = decay; break;
        default:        h.length = decay; break;
    }
    h.length += 0.02;   // let the filters ring out
}

float DrumEngine::renderHit (Hit& h)
{
    using namespace daboom;
    const auto t = h.t, sr = sampleRate;
    const auto vel = h.vel;
    const auto decay = (double) h.p[1];
    float out = 0.0f;

    switch (h.type)
    {
        case kick:
        {
            const auto tune = (double) h.p[0];
            const auto punch = h.p[2] / 100.0f;
            const auto f = sweep (t, tune * 3.2, tune, 0.04 + 0.03 * (1.0 - punch));
            out = (float) std::sin (twoPi * h.phase[0]) * envelope (t, vel, decay);
            advance (h.phase[0], f, sr);
            if (punch > 0.0f && t < 0.06)                       // click of the beater
                out += h.f1.process (noise()) * envelope (t, vel * punch * 0.5f, 0.008);
            break;
        }
        case snare:
        {
            const auto tune = (double) h.p[0];
            const auto snappy = h.p[2] / 100.0f;
            const auto body = (triangle (h.phase[0]) + triangle (h.phase[1]))
                              * envelope (t, vel * 0.7f * (1.0f - snappy / 2.0f), decay * 0.6);
            advance (h.phase[0], tune, sr);
            advance (h.phase[1], tune * 1.74, sr);
            out = body + h.f1.process (noise()) * envelope (t, vel * 0.8f * snappy + 0.05f, decay);
            break;
        }
        case clap:
        {
            const auto spread = h.p[2] / 1000.0;
            float x = 0.0f;
            for (int i = 0; i < 3; ++i)                         // three quick slaps, then the tail
                x += noise() * envelope (t - i * spread, vel * 1.6f, 0.012);
            x += noise() * envelope (t - 3.0 * spread, vel * 1.3f, decay);
            out = h.f1.process (x);
            break;
        }
        case closedHat:
        case openHat:
        {
            float x = 0.0f;
            for (int i = 0; i < 6; ++i)
            {
                x += square (h.phase[i], metalFreqs[i] / sr);
                advance (h.phase[i], metalFreqs[i], sr);
            }
            const auto peak = vel * (h.type == closedHat ? 0.35f : 0.32f);
            auto gain = envelope (t, peak, decay);
            if (h.choked)   // setTargetAtTime(0.0001, t, 0.005) from where it was
            {
                gain = floorGain + (h.chokeFrom - floorGain) * (float) std::exp (-(t - h.chokeAt) / 0.005);
                if (t - h.chokeAt > 0.08)
                    h.active = false;
            }
            out = h.f2.process (h.f1.process (x)) * gain;
            break;
        }
        case tom:
        {
            const auto tune = (double) h.p[0];
            out = (float) std::sin (twoPi * h.phase[0]) * envelope (t, vel * 0.9f, decay);
            advance (h.phase[0], sweep (t, tune * 1.6, tune, 0.08), sr);
            break;
        }
        case rim:
        {
            const auto tune = (double) h.p[0];
            const auto x = triangle (h.phase[0]) + square (h.phase[1], tune * 0.53 / sr);
            advance (h.phase[0], tune, sr);
            advance (h.phase[1], tune * 0.53, sr);
            out = h.f1.process (x) * envelope (t, vel * 0.6f, decay);
            break;
        }
        case cowbell:
        {
            const auto tune = (double) h.p[0];
            const auto x = square (h.phase[0], tune / sr) + square (h.phase[1], tune * 1.48 / sr);
            advance (h.phase[0], tune, sr);
            advance (h.phase[1], tune * 1.48, sr);
            out = h.f1.process (x) * envelope (t, vel * 0.35f, decay);
            break;
        }
        default:
            break;
    }

    h.t += 1.0 / sr;
    if (h.t >= h.length)
        h.active = false;
    return out * h.level;
}

void DrumEngine::render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const auto maxBlock = sendBuffer.getNumSamples();
    while (numSamples > maxBlock)            // a host may send more than it promised
    {
        render (buffer, startSample, maxBlock);
        startSample += maxBlock;
        numSamples -= maxBlock;
    }
    if (numSamples <= 0)
        return;

    auto* sendL = sendBuffer.getWritePointer (0);
    auto* sendR = sendBuffer.getWritePointer (1);
    const auto tanhG = std::tanh (driveGain);

    for (int i = 0; i < numSamples; ++i)
    {
        float x = 0.0f;
        for (auto& h : hits)
            if (h.active)
                x += renderHit (h);

        const auto d = driveMix.getNextValue();
        const auto shaped = std::tanh (driveGain * juce::jlimit (-1.0f, 1.0f, x)) / tanhG;
        const auto m = (x * (1.0f - d) + shaped * d) * volume.getNextValue();
        dryBuffer[(size_t) i] = m;
        sendL[i] = sendR[i] = m * reverbSend.getNextValue();
    }

    auto block = juce::dsp::AudioBlock<float> (sendBuffer).getSubBlock (0, (size_t) numSamples);
    reverb.process (juce::dsp::ProcessContextReplacing<float> (block));

    const auto channels = buffer.getNumChannels();
    for (int i = 0; i < numSamples; ++i)
    {
        // The egret's call goes straight out, past the master section (as on the web).
        float bird = 0.0f;
        if (chirpT >= 0.0)
        {
            for (int c = 0; c < 2; ++c)
            {
                const auto local = chirpT - 0.11 * c;
                if (local < 0.0)
                    continue;
                bird += (float) std::sin (twoPi * chirpPhase[c]) * envelope (local, 0.12f, 0.08);
                advance (chirpPhase[c], sweep (local, 2600.0 + 500.0 * c, 3800.0 + 400.0 * c, 0.06), sampleRate);
            }
            chirpT += 1.0 / sampleRate;
            if (chirpT > 0.25)
                chirpT = -1.0;
        }

        const auto dry = dryBuffer[(size_t) i] + bird;
        if (channels == 1)
            buffer.addSample (0, startSample + i, dry + 0.5f * (sendL[i] + sendR[i]));
        else
        {
            buffer.addSample (0, startSample + i, dry + sendL[i]);
            buffer.addSample (1, startSample + i, dry + sendR[i]);
        }
    }
}
