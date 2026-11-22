// MAVKA - AURA: nonlinear send effects (Echo, Forest, Twin, Spin)
#pragma once

#include "MavkaDSP.h"

namespace mavka
{

// level-independent soft shaper: the same relative "dirt" on every pass of a feedback loop
struct RelativeShaper
{
    Envelope env;
    void prepare (float fs) { env.set (4.0f, 90.0f, fs); env.reset(); }
    void reset() noexcept   { env.reset(); }
    inline float process (float x, float k) noexcept
    {
        const float e = env.process (x) + 1.0e-4f;
        return fastTanh (k * x / e) * e / k;
    }
};

//==============================================================================
// ECHO: ping-pong tape echo. Every repeat is re-shaped (relative saturation) and band-narrowed,
// so the echoes get grainier as they fade; the tape wobbles with the chord clock.
class AuraEcho
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (int c = 0; c < 2; ++c)
        {
            dl[c].prepare ((int) (2.6f * fs) + 16);
            lp[c].setCutoff (3200.0f, fs);
            hp[c].setCutoff (260.0f, fs);
            sh[c].prepare (fs);
        }
        aT = 1.0f - std::exp (-1.0f / (0.12f * fs));
        reset();
    }

    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c) { dl[c].reset(); lp[c].reset(); hp[c].reset(); sh[c].reset(); }
        tS = 0.0f;
    }

    inline void process (float in, float timeSamples, float fb, float dirt, float wow, float& outL, float& outR) noexcept
    {
        if (tS <= 0.0f) tS = timeSamples;
        tS += (timeSamples - tS) * aT;
        const float t = tS * (1.0f + wow);
        const float yL = dl[0].read (t), yR = dl[1].read (t);
        const float k = 1.0f + 5.0f * dirt;
        const float fL = sh[0].process (hp[0].hp (lp[0].lp (yR)), k);
        const float fR = sh[1].process (hp[1].hp (lp[1].lp (yL)), k);
        dl[0].push (in + fb * fL);
        dl[1].push (fb * fR);
        outL = yL;
        outR = yR;
    }

private:
    DelayLine dl[2];
    OnePole lp[2], hp[2];
    RelativeShaper sh[2];
    float fs = 44100.0f, aT = 0.001f, tS = 0.0f;
};

//==============================================================================
// FOREST: 8-line feedback delay network with saturation inside the loop.
// "Tune" pulls the line lengths onto multiples of the song's chord-tone periods: the room sings in key.
class AuraForest
{
public:
    static constexpr int N = 8;

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& d : dl) d.prepare ((int) (0.26f * fs) + 16);
        pre.prepare ((int) (0.06f * fs) + 16);
        reset();
    }

    void reset() noexcept
    {
        for (auto& d : dl) d.reset();
        pre.reset();
        for (float& z : damp) z = 0.0f;
    }

    void configure (int type, float decaySec, float tune, const float* chordHz, int numTones) noexcept
    {
        static const float lens[4][N] =
        {
            { 17.3f, 19.7f, 23.1f, 26.9f, 29.3f, 31.7f, 35.3f, 37.9f },        // Room
            { 11.1f, 13.7f, 15.9f, 18.3f, 20.9f, 23.3f, 25.7f, 28.1f },        // Plate
            { 37.3f, 43.1f, 47.9f, 53.3f, 59.9f, 64.7f, 71.3f, 79.1f },        // Hall
            { 61.3f, 71.9f, 83.1f, 92.3f, 103.7f, 114.1f, 127.3f, 139.9f },    // Cathedral
        };
        static const float dampHz[4] = { 6500.0f, 9500.0f, 5200.0f, 4200.0f };
        static const float preMs[4]  = { 10.0f, 18.0f, 28.0f, 42.0f };
        type = std::min (std::max (type, 0), 3);

        aD = std::exp (-kTwoPi * dampHz[type] / fs);
        preSamples = std::max (1, (int) (preMs[type] * 0.001f * fs));
        const float t60 = std::max (decaySec, 0.1f);
        for (int k = 0; k < N; ++k)
        {
            const float natural = lens[type][k] * 0.001f * fs;
            float tuned = natural;
            if (numTones > 0)
            {
                const float period = fs / clampf (chordHz[k % numTones], 40.0f, 2000.0f);
                tuned = std::max (1.0f, std::round (natural / period)) * period;
            }
            len[k] = std::min (natural + tune * (tuned - natural), 0.25f * fs);
            g[k] = std::pow (10.0f, -3.0f * len[k] / (fs * t60));
        }
    }

    inline void process (float in, float satLevel, float mod, float& outL, float& outR) noexcept
    {
        pre.push (in);
        const float x = pre.tap (preSamples) * 0.35f;
        float y[N];
        for (int k = 0; k < N; ++k)
        {
            const float m = (k == 0 ? mod : (k == 3 ? -mod : 0.0f)) * modDepth;
            y[k] = dl[k].read (len[k] + m);
            damp[k] = y[k] + aD * (damp[k] - y[k]);
        }
        float sum = 0.0f;
        for (float z : damp) sum += z;
        sum *= 2.0f / (float) N;
        const float inv = 1.0f / satLevel;
        for (int k = 0; k < N; ++k)
        {
            const float v = (damp[k] - sum) * g[k] + ((k & 1) ? -x : x);
            dl[k].push (satLevel * fastTanh (v * inv));
        }
        outL = 0.5f * (y[0] - y[2] + y[4] - y[6]);
        outR = 0.5f * (y[1] - y[3] + y[5] - y[7]);
    }

    float modDepth = 20.0f;   // samples

private:
    DelayLine dl[N], pre;
    float len[N] {}, g[N] {}, damp[N] {};
    float fs = 44100.0f, aD = 0.5f;
    int preSamples = 1;
};

//==============================================================================
// two-grain delay-line pitch shifter (Eventide-style): smooth for small detune, grainy for octaves
class GrainShifter
{
public:
    void prepare (float sampleRate, float windowMs)
    {
        fs = sampleRate;
        W = windowMs * 0.001f * fs;
        dl.prepare ((int) (W + 0.08f * fs) + 16);
        reset();
    }
    void reset() noexcept { dl.reset(); phase = 0.0f; }

    inline float process (float in, float ratio, float baseDelaySamples) noexcept
    {
        dl.push (in);
        phase += (1.0f - ratio) / W;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f;
        p2 -= std::floor (p2);
        const float s1 = fastSin (kPi * phase), s2 = fastSin (kPi * p2);
        return dl.read (baseDelaySamples + phase * W) * s1 * s1 + dl.read (baseDelaySamples + p2 * W) * s2 * s2;
    }

private:
    DelayLine dl;
    float fs = 44100.0f, W = 1000.0f, phase = 0.0f;
};

// TWIN: up to four detuned doubles; detune breathes with the voice and the clock,
// each twin has its own "throat" (asymmetric saturation)
class AuraTwin
{
public:
    static constexpr int kMax = 4;

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (int i = 0; i < kMax; ++i)
            gs[i].prepare (fs, 32.0f);
        reset();
    }
    void reset() noexcept { for (auto& s : gs) s.reset(); }

    // type: 0 Tight, 1 Wide, 2 Lush, 3 Choir, 4 Octave
    inline void process (float in, int type, float breathe, float wobble, float throat, float& outL, float& outR) noexcept
    {
        static const float cents[5][kMax] = { { -7, 7, 0, 0 }, { -12, 12, 0, 0 }, { -18, 18, 0, 0 },
                                              { -9, 11, -15, 17 }, { -1200, -1195, 0, 0 } };
        static const float delayMs[5][kMax] = { { 11, 14, 0, 0 }, { 17, 23, 0, 0 }, { 21, 29, 0, 0 },
                                                { 15, 21, 27, 33 }, { 18, 24, 0, 0 } };
        type = std::min (std::max (type, 0), 4);
        const int n = (type == 3) ? 4 : 2;
        const float k = 1.0f + 4.0f * throat;
        outL = outR = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float sign = (i & 1) ? 1.0f : -1.0f;
            const float c = cents[type][i] * (type == 4 ? 1.0f : (1.0f + 0.6f * breathe)) + sign * 6.0f * wobble;
            const float r = std::exp2 (c / 1200.0f);
            const float y = gs[i].process (in, r, delayMs[type][i] * 0.001f * fs);
            const float b = 0.25f * sign;   // its own throat
            const float t = (fastTanh (k * y + b) - fastTanh (b)) / k;
            if (i & 1) outR += t; else outL += t;
        }
        if (n == 4) { outL *= 0.75f; outR *= 0.75f; }
    }

private:
    GrainShifter gs[kMax];
    float fs = 44100.0f;
};

} // namespace mavka

namespace mavka
{
//==============================================================================
// Spectral ducking (zero latency, no FFT): 16 bands, 70 Hz .. 14 kHz. Voice and FX levels are measured with a
// selective band-pass bank; the FX is then carved by parallel dynamic peaking cuts (x - c * bandpass(x)), so with
// no voice it passes bit-exact. In every band where the voice is present, the FX is kept `margin` under the voice;
// bands the voice is not using pass untouched - the echo stays audible in the gaps of the voice's spectrum.
struct DuckBands
{
    static constexpr int kBands = 16;
    static float centreHz (int b) noexcept { return 70.0f * std::pow (200.0f, (float) b / (float) (kBands - 1)); }   // 70 Hz .. 14 kHz

    static void bandpass (Biquad& f, double fc, double q, double fs) noexcept   // 0 dB peak
    {
        const double w = 2.0 * kPiD * std::min (fc, 0.45 * fs) / fs;
        const double al = std::sin (w) / (2.0 * q);
        f.set (al, 0.0, -al, 1.0 + al, -2.0 * std::cos (w), 1.0 - al);
    }
};

// selective analysis bank (24 dB/oct skirts) - used only to MEASURE levels per band
class SelectiveBank
{
public:
    void prepare (float fs)
    {
        for (int b = 0; b < DuckBands::kBands; ++b)
        {
            for (int s = 0; s < 2; ++s)
                DuckBands::bandpass (f[b][s], DuckBands::centreHz (b), 2.2, fs);
            env[b].set (3.0f, 120.0f, fs);
        }
        reset();
    }
    void reset() noexcept
    {
        for (auto& band : f) for (auto& q : band) q.reset();
        for (auto& e : env) e.reset();
    }
    inline void process (float x, float* levels) noexcept
    {
        for (int b = 0; b < DuckBands::kBands; ++b)
            levels[b] = env[b].process (f[b][1].process (f[b][0].process (x)));
    }

private:
    Biquad f[DuckBands::kBands][2];
    Envelope env[DuckBands::kBands];
};

class DuckKey
{
public:
    void prepare (float fs)
    {
        bank.prepare (fs);
        fall = std::pow (10.0f, -8.0f / 20.0f / fs);     // band reference falls 8 dB per second
        reset();
    }
    void reset() noexcept
    {
        bank.reset();
        for (int b = 0; b < DuckBands::kBands; ++b) { ref[b] = 0.0f; level[b] = 0.0f; presence[b] = 0.0f; }
    }

    // voice: latency-aligned dry voice (mono); gate: 0..1 "the voice is singing at all"
    inline void process (float voice, float gate) noexcept
    {
        bank.process (voice, level);
        float strongest = 1.0e-7f;
        for (int b = 0; b < DuckBands::kBands; ++b)
        {
            ref[b] = std::max (level[b], ref[b] * fall);
            strongest = std::max (strongest, level[b]);
        }
        for (int b = 0; b < DuckBands::kBands; ++b)
        {
            // the voice occupies a band when the band is near its own recent peak (12..30 dB)
            // AND is not negligible next to the voice's strongest band (25..40 dB)
            presence[b] = gate * smoothstep (0.0316f, 0.25f, level[b] / (ref[b] + 1.0e-7f))
                               * smoothstep (0.01f, 0.056f, level[b] / strongest);
        }
    }

    float level[DuckBands::kBands] {};
    float presence[DuckBands::kBands] {};

private:
    SelectiveBank bank;
    float ref[DuckBands::kBands] {};
    float fall = 0.9999f;
};

class DuckFx
{
public:
    void prepare (float fs)
    {
        for (int c = 0; c < 2; ++c)
            for (int b = 0; b < DuckBands::kBands; ++b)
                DuckBands::bandpass (cut[c][b], DuckBands::centreHz (b), 2.0, fs);
        bank.prepare (fs);
        aAtk = timeCoef (4.0f, fs);
        aRel = timeCoef (140.0f, fs);
        reset();
    }
    void reset() noexcept
    {
        for (auto& ch : cut) for (auto& f : ch) f.reset();
        bank.reset();
        for (float& g : gain) g = 1.0f;
    }

    // L/R: FX return, ducked in place. margin: how far (linear) the FX must stay under the voice; floor: deepest cut.
    // Each band is a dynamic peaking cut (x - c * bandpass(x)): with no voice the signal passes exactly.
    inline void process (const DuckKey& key, float& L, float& R, float margin, float floor) noexcept
    {
        float fxLevel[DuckBands::kBands];
        bank.process (0.5f * (L + R), fxLevel);
        float yL = L, yR = R;
        for (int b = 0; b < DuckBands::kBands; ++b)
        {
            const float target = clampf (key.level[b] * margin / (fxLevel[b] + 1.0e-7f), floor, 1.0f);
            const float gt = 1.0f - key.presence[b] * (1.0f - target);
            gain[b] = gt + (gt < gain[b] ? aAtk : aRel) * (gain[b] - gt);
            const float c = 1.0f - gain[b];
            yL -= c * cut[0][b].process (L);
            yR -= c * cut[1][b].process (R);
        }
        L = yL;
        R = yR;
    }

    float gain[DuckBands::kBands] {};

private:
    Biquad cut[2][DuckBands::kBands];
    SelectiveBank bank;
    float aAtk = 0.9f, aRel = 0.99f;
};
} // namespace mavka
