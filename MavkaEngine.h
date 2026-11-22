// MAVKA - vocal "sauce" distortion engine (no JUCE dependency)
//
// Signal flow (per channel):
//   in -> pre EQ -> consistency (level-independent drive) -> warp (phase) -> ghosts (sub/fifth + octave)
//      -> [oversampled core: Lamp | Doppler Glass | Fold voices -> clip]  -> DC -> smooth (anti-harsh)
//      -> restore dynamics * vowel focus * ink -> + Harp (strings in key) -> tone -> mix with delayed dry
//
// Handwriting ("Ink"): every word starts clean and the distortion spreads over it (Ink > 0),
// or hits hard and lifts off (Press, Ink < 0).
// Chord clock: all modulators run at rates that form a chord (4:5:6, 10:12:15, ...) locked to the host bars.
#pragma once

#include "MavkaAura.h"
#include "MavkaDSP.h"
#include "MavkaParams.h"

#include <atomic>
#include <cstring>

namespace mavka
{

inline float smoothCoef (float ms, float fs) noexcept { return 1.0f - std::exp (-1.0f / (0.001f * ms * fs)); }
inline bool  differs (float a, float b) noexcept { return std::memcmp (&a, &b, sizeof (float)) != 0; }

//==============================================================================
struct InnerControls
{
    float dLamp, dGlass, dDop, dFold;       // ms
    float kLamp, cLamp, tcLamp;             // lamp drive, bias offset, tanh(bias)
    float kGlass, dopSens;                  // glass drive, doppler modulator sensitivity
    float kFold, sFold, ssFold;             // fold drive, symmetry offset, sin(symmetry)
    float gLamp, gGlass, gFold;             // voice gains (level comp, pan, pulse)
    float clipIn, clipH, clipOut;           // final clipper
    float crushRate, crushStep, crushMix;   // crusher: hold rate (Hz), quantiser step, blend
    float crushSync;                        // 1 = a new period of the voice starts here: restart the step grid
};

// static shapers - shared by the oversampled core and the level probe
inline float shapeLamp (float x, float k, float c, float tc) noexcept  { return fastTanh (k * x + c) - tc; }
inline float shapeGlass (float x, float k) noexcept                    { return fastTanh (k * x); }
inline float shapeFold (float x, float k, float s, float ss) noexcept  { return fastSin (k * x + s) - ss; }
// hard clipper with a C1 knee (much less aliasing than a sharp corner)
inline float kneeClip (float u) noexcept
{
    constexpr float kk = 0.15f;
    const float a = std::fabs (u);
    if (a <= 1.0f - kk) return u;
    const float r = a >= 1.0f + kk ? 1.0f : 1.0f - (1.0f + kk - a) * (1.0f + kk - a) * (1.0f / (4.0f * kk));
    return u < 0.0f ? -r : r;
}

inline float shapeClip (float v, float in, float h, float out) noexcept
{
    const float u = v * in;
    return ((1.0f - h) * fastTanh (u) + h * kneeClip (u)) * out;
}

struct InnerState
{
    DelayLine dl;
    OnePole dopLP;
    float crushPhase = 0.0f, crushHold = 0.0f;

    void prepare (float topRate)
    {
        dl.prepare ((int) std::ceil (0.060f * topRate) + 8);
        dopLP.setCutoff (900.0f, topRate);
        reset();
    }
    void reset() noexcept { dl.reset(); dopLP.reset(); crushPhase = 0.0f; crushHold = 0.0f; }
};

// runs at the oversampled rate
inline void runInner (InnerState& st, float* top, const InnerControls* ctl, int n, int os, float topRate) noexcept
{
    const float msToS = 0.001f * topRate, invTop = 1.0f / topRate;
    for (int j = 0; j < n; ++j)
    {
        const InnerControls& c = ctl[j];
        const float dL = c.dLamp * msToS, dG = c.dGlass * msToS, dD = c.dDop * msToS, dF = c.dFold * msToS;
        const float crushInc = std::min (1.0f, c.crushRate * invTop);
        float* t = top + j * os;
        for (int k = 0; k < os; ++k)
        {
            float in = t[k];
            if (c.crushMix > 0.0f)
            {
                // Harmonic crush: the hold clock runs at N x the voice's pitch and restarts on every period,
                // so the stepped wave repeats exactly with the voice - every product is a harmonic
                if (k == 0 && c.crushSync > 0.0f)
                {
                    // the period started (crushSync - 1) base samples ago: place the grid exactly there
                    st.crushPhase = (c.crushSync - 1.0f) * (float) os * crushInc;
                    st.crushHold = in;
                }
                else
                    st.crushPhase += crushInc;
                if (st.crushPhase >= 1.0f)
                {
                    st.crushPhase -= std::floor (st.crushPhase);
                    st.crushHold = in;
                }
                const float q = c.crushStep > 0.0f ? c.crushStep * std::floor (st.crushHold / c.crushStep + 0.5f) : st.crushHold;
                in += c.crushMix * (q - in);
            }
            st.dl.push (in);
            // Lamp: asymmetric tube whose bias breathes with the clock
            const float lamp = shapeLamp (st.dl.read (dL), c.kLamp, c.cLamp, c.tcLamp);
            // Doppler Glass: the voice moves its own "speaker cone" (self-modulated delay)
            const float g0 = st.dl.read (dG);
            const float m = fastTanh (st.dopLP.lp (g0) * c.dopSens);
            const float glass = shapeGlass (st.dl.read (dG + dD * (1.0f + m)), c.kGlass);
            // Fold: diode wavefolder
            const float fold = shapeFold (st.dl.read (dF), c.kFold, c.sFold, c.ssFold);
            t[k] = shapeClip (c.gLamp * lamp + c.gGlass * glass + c.gFold * fold, c.clipIn, c.clipH, c.clipOut);
        }
    }
}

//==============================================================================
// One oversampling factor: up -> core -> down, padded to the common latency.
struct CorePath
{
    int factor = 1, basePad = 0;
    float topRate = 44100.0f;
    Oversampler os[2];
    InnerState inner[2];
    DelayLine pad[2];

    int prepareOversampling (double fs, int numStages, int maxBlock)
    {
        factor = 1 << numStages;
        topRate = (float) (fs * factor);
        for (int c = 0; c < 2; ++c)
        {
            os[c].prepare (numStages, maxBlock);
            inner[c].prepare (topRate);
        }
        return os[0].latency();
    }

    void setCommonLatency (int latencyMax)
    {
        basePad = latencyMax - os[0].latency();
        for (int c = 0; c < 2; ++c)
            pad[c].prepare (basePad + 4);
    }

    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            os[c].reset();
            inner[c].reset();
            pad[c].reset();
        }
    }

    void process (int ch, const float* in, const InnerControls* ctl, float* out, int n) noexcept
    {
        float* top = os[ch].up (in, n);
        runInner (inner[ch], top, ctl, n, factor, topRate);
        os[ch].down (top, out, n);
        if (basePad > 0)
            for (int i = 0; i < n; ++i)
            {
                pad[ch].push (out[i]);
                out[i] = pad[ch].tap (basePad);
            }
    }
};

//==============================================================================
// "Harp in the lamp": saturating strings tuned to the song key, excited by the distorted voice.
class Harp
{
public:
    static constexpr int kStrings = 6;

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& s : str)
            s.dl.prepare ((int) (fs / 30.0f) + 16);
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : str)
        {
            s.dl.reset();
            s.z = 0.0f;
        }
    }

    void configure (const float* hz, float decaySec, float bright, float drive) noexcept
    {
        const float fc = 1200.0f * std::exp2 (bright * 3.6f);
        aLP = std::exp (-kTwoPi * std::min (fc, 0.45f * fs) / fs);
        sat = 0.9f / (1.0f + 6.0f * drive);
        invSat = 1.0f / sat;
        for (int i = 0; i < kStrings; ++i)
        {
            Str& s = str[i];
            const float f = clampf (hz[i], 30.0f, 0.2f * fs);
            const float w = kTwoPi * f / fs;
            const float pd = std::atan2 (aLP * std::sin (w), 1.0f - aLP * std::cos (w)) / w;   // LP phase delay
            s.len = std::max (1.0f, fs / f - 1.0f - pd);
            const float mag = (1.0f - aLP) / std::sqrt (1.0f - 2.0f * aLP * std::cos (w) + aLP * aLP);
            s.g = std::min (0.9995f, std::pow (10.0f, -3.0f / (f * std::max (decaySec, 0.05f))) / std::max (mag, 1.0e-3f));
            s.inG = 0.9f * std::sqrt (std::max (1.0e-6f, 1.0f - s.g * s.g));
            s.shSign = (i & 1) ? -1.0f : 1.0f;
        }
    }

    inline float process (float exc, float shimmer) noexcept
    {
        float sum = 0.0f;
        for (auto& s : str)
        {
            const float y = s.dl.read (s.len * (1.0f + shimmer * s.shSign));
            s.z = y + aLP * (s.z - y);
            const float v = exc * s.inG + s.g * s.z;
            s.dl.push (sat * fastTanh (v * invSat));
            sum += y;
        }
        return sum;
    }

private:
    struct Str
    {
        DelayLine dl;
        float len = 100.0f, g = 0.9f, inG = 0.1f, z = 0.0f, shSign = 1.0f;
    };
    Str str[kStrings];
    float fs = 44100.0f, aLP = 0.5f, sat = 0.9f, invSat = 1.0f / 0.9f;
};

//==============================================================================
class Engine
{
public:
    static constexpr int   kSub   = 64;      // internal block
    static constexpr int   kPaths = 5;       // 1x, 2x, 4x, 8x, 16x
    static constexpr float kRefDb = -12.0f;  // consistency reference (peak envelope)

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;

    void setParams (const float* values) noexcept { std::copy (values, values + P_COUNT, prm); }
    void setTransport (bool isPlaying, double bpm, double ppqAtBlockStart) noexcept
    {
        hostPlaying = isPlaying;
        if (bpm > 1.0 && bpm < 2000.0)
            hostBpm = bpm;
        hostPpq = ppqAtBlockStart;
        blockPos = 0;
    }
    void setNonRealtime (bool b) noexcept { offline = b; }

    int   getLatency() const noexcept           { return totalLat; }
    int   getActiveFactor() const noexcept      { return guiFactor.load (std::memory_order_relaxed); }
    float getClockPhase (int i) const noexcept  { return guiPhase[i & 3].load (std::memory_order_relaxed); }
    float getInkStroke() const noexcept         { return guiInk.load (std::memory_order_relaxed); }
    float getVoiced() const noexcept            { return guiVoiced.load (std::memory_order_relaxed); }
    float getSlantMs() const noexcept           { return guiSlant.load (std::memory_order_relaxed); }
    float getInputDb() const noexcept           { return guiLevel.load (std::memory_order_relaxed); }
    float getRelativeDb() const noexcept        { return guiRel.load (std::memory_order_relaxed); }
    float getDuckGain (int band) const noexcept { return guiDuck[std::min (std::max (band, 0), DuckBands::kBands - 1)].load (std::memory_order_relaxed); }

    void process (float* const* io, int numCh, int n) noexcept
    {
        numCh = std::min (numCh, 2);
        for (int off = 0; off < n; off += kSub)
            processSub (io, numCh, off, std::min (kSub, n - off));
    }

    void processBypassed (float* const* io, int numCh, int n) noexcept
    {
        numCh = std::min (numCh, 2);
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                ch[c].dry.push (io[c][i]);
                io[c][i] = ch[c].dry.tap (totalLat);
            }
    }

private:
    void processSub (float* const* io, int numCh, int off, int n) noexcept;
    void updateBlock() noexcept;
    void computeProbe() noexcept;
    int  desiredPath (int n) noexcept;

    static float panGain (float pan, int c) noexcept
    {
        const float th = (clampf (pan, -1.0f, 1.0f) + 1.0f) * (0.25f * kPi);
        return 1.41421356f * (c == 0 ? std::cos (th) : std::sin (th));
    }

    //--------------------------------------------------------------------------
    double fs = 44100.0;
    float fsf = 44100.0f;
    float prm[P_COUNT] {};
    bool offline = false, first = true, snap = true;

    // transport / chord clock
    bool hostPlaying = false;
    double hostBpm = 120.0, hostPpq = 0.0, freePpq = 0.0;
    long blockPos = 0;

    // oversampled core
    CorePath paths[kPaths];
    int latencyMax = 0, lampPre = 1, totalLat = 0;
    float lampBaseMs = 0.8f;
    int activePath = 0, nextPath = -1, warmLeft = 0, fadeLeft = 0, warmLen = 1, fadeLen = 1;
    int autoChoice = 0, autoPending = 0;
    long autoStable = 0;

    struct Chan
    {
        DelayLine dry;
        Biquad preHP, preLP, tiltLo, tiltHi;
        float apZ[4] {};
        Biquad ghostLP, ghostSubLP, ghostUpLP;
        Envelope ghostEnv;
        OnePole ghostUpHP;
        float zPrev = 0.0f, flip = 1.0f;
        bool armed = false;
        int crossCount = 0;
        float f0 = 150.0f, lastDelta = 0.0f;
        bool newPeriod = false;
        Envelope crushEnv;
        float dLampS = 0.8f, dGlassS = 23.0f, dFoldS = 37.0f, dDopS = 0.0f;
        OnePole dc;
        Biquad smLP;
        Envelope smEnvLo, smEnvHi;
        float smGain = 1.0f;
        Biquad harpHP, harpLP;
        Harp harp;
        Biquad toneLo, toneHi;
        DelayLine ctlRestore, ctlFocus, ctlInkWet, ctlInkHarp;
    };
    Chan ch[2];

    // mono analysis
    Envelope envCons, inkFast, inkSlow, vEnvLo, vEnvHi;
    Biquad vLP, vHP;
    float gNdb = 0.0f, voiced = 1.0f, strokeRem = 0.0f, lastEnvDb = -100.0f;
    long sinceOnset = 1L << 30, minOnsetGap = 3000;
    float inkDriveDb = 0.0f, inkDop = 1.0f, inkWet = 1.0f, inkHarp = 1.0f;
    float warpMix = 0.0f;

    // Slant: per-stroke pitch gesture of the doubles (gamma-shaped delay bump, continuous)
    float slS1 = 0.0f, slS2 = 0.0f;
    // Ligature: tempo-synced ping-pong echo that only speaks between words
    DelayLine ligL, ligR;
    OnePole ligLP[2], ligHP[2];
    float ligTL = 0.0f, ligTR = 0.0f, ligDuck = 1.0f, aLigT = 0.001f, aDuckAtk = 0.9f, aDuckRel = 0.99f;
    float duckBuf[kSub] {};
    float loudRef = -70.0f, loudFall = 0.0f;

    // AURA: nonlinear send effects
    AuraEcho aEcho;
    AuraForest aForest;
    AuraTwin aTwin;
    Biquad fxHP, fxLP;
    float tEcho = 0.0f, sEcho = 0.0f, tForest = 0.0f, sForest = 0.0f, tTwin = 0.0f, sTwin = 0.0f;
    bool echoLive = false, forestLive = false, twinLive = false;
    float harpHz[6] {};
    int harpVersion = 0, forestVersion = -1, lastForestType = -1;
    float lastForestDecay = -1.0f, lastForestTune = -1.0f;
    float auraA[kSub] {}, auraB[kSub] {}, auraC[kSub] {}, breathe[kSub] {};
    // spectral ducking (Ligature + Aura echo/forest)
    DuckKey duckKey;
    DuckFx duckLig, duckAura;

    // smoothing
    float aS = 0.01f, aCtl = 0.01f, aInk = 0.01f, aGn = 0.01f, aWarp = 0.01f;
    float aVfast = 0.9f, aVslow = 0.99f, aSmAtk = 0.9f, aSmRel = 0.99f;
    float tDrive = 1.0f, sDrive = 1.0f, tDryG = 1.0f, sDryG = 1.0f, tWetG = 0.0f, sWetG = 0.0f, tOut = 1.0f, sOut = 1.0f;
    float tLamp = 1.0f, sLamp = 1.0f, tGlass = 0.0f, sGlass = 0.0f, tFold = 0.0f, sFold = 0.0f;
    float tComp = 1.0f, sComp = 1.0f, tHarpLvl = 0.0f, sHarpLvl = 0.0f, tHarpExc = 0.0f, sHarpExc = 0.0f;
    float compLamp = 1.0f, compGlass = 1.0f, compFold = 1.0f, mixNorm = 1.0f;
    float smThr = 0.5f, smExp = 1.0f, smStatic = 1.0f, harpShim = 0.0f;

    // cached settings
    float lastLowCut = -1.0f, lastHighCut = -1.0f, lastTilt = 1.0e9f, lastTone = 1.0e9f;
    float lastDecay = -1.0f, lastBright = -1.0f, lastHarpDrive = -1.0f, lastWidth = -1.0f;
    int lastKey = -1, lastChord = -1, lastReg = -1;

    float probe[64] {};

    // scratch
    float coreIn[2][kSub] {}, coreOutA[2][kSub] {}, coreOutB[2][kSub] {}, dryOut[2][kSub] {}, shim[2][kSub] {};
    InnerControls ctl[2][kSub] {};

    std::atomic<float> guiPhase[4];
    std::atomic<int>   guiFactor { 1 };
    std::atomic<float> guiDuck[DuckBands::kBands];
    std::atomic<float> guiInk { 0.0f }, guiVoiced { 1.0f }, guiSlant { 0.0f }, guiLevel { -100.0f }, guiRel { -60.0f };
};

//==============================================================================
namespace tables
{
    static const float kRatios[8][4] =
    {
        { 1.0f, 1.0f,       1.0f,       1.0f       },   // Unison (quadrature phases)
        { 1.0f, 2.0f,       4.0f,       8.0f       },   // Octaves
        { 1.0f, 1.5f,       2.0f,       3.0f       },   // Fifths 2:3
        { 1.0f, 1.25f,      1.5f,       2.0f       },   // Major 4:5:6
        { 1.0f, 1.2f,       1.5f,       2.0f       },   // Minor 10:12:15
        { 1.0f, 4.0f / 3.0f,1.5f,       2.0f       },   // Sus 6:8:9
        { 1.0f, 4.0f / 3.0f,2.0f,       8.0f / 3.0f},   // Poly 3:4
        { 1.0f, 1.618034f,  2.618034f,  4.236068f  },   // Golden (never repeats)
    };
    static const double kBeatsPerCycle[7] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25 };
    static const float  kUnisonOffset[4]  = { 0.0f, 0.25f, 0.5f, 0.75f };
    static const float  kStereoOffset[4]  = { 0.25f, -0.25f, 0.25f, -0.25f };

    static const int kChords[12][6] =
    {
        { 0, 12, 24,  0, 12, 24 },   // Octaves (second set detuned)
        { 0,  7, 12, 19, 24, 31 },   // Power
        { 0,  4,  7, 12, 16, 19 },   // Major
        { 0,  3,  7, 12, 15, 19 },   // Minor
        { 0,  2,  7, 12, 14, 19 },   // Sus2
        { 0,  5,  7, 12, 17, 19 },   // Sus4
        { 0,  4,  7, 11, 14, 19 },   // Maj7
        { 0,  3,  7, 10, 12, 15 },   // Min7
        { 0,  3,  7, 10, 14, 19 },   // Min9
        { 0,  4,  7, 10, 12, 16 },   // Dom7
        { 0,  3,  6,  7, 10, 14 },   // Ukrainian Dorian colour (b3, #4, b7, 9)
        { 0,  1,  4,  7, 10, 12 },   // Hijaz / Phrygian dominant
    };
    static const float kStringDetune[6] = { 0.0f, 1.5f, -1.5f, 1.0f, -1.0f, 0.5f };   // cents
}

//==============================================================================
inline void Engine::prepare (double sampleRate, int /*numChannels*/)
{
    fs = sampleRate;
    fsf = (float) sampleRate;

    int lat = 0;
    for (int p = 0; p < kPaths; ++p)
        lat = std::max (lat, paths[p].prepareOversampling (fs, p, kSub));
    latencyMax = lat;
    for (int p = 0; p < kPaths; ++p)
        paths[p].setCommonLatency (latencyMax);
    // the Lamp (lead) voice sits on an integer pre-delay that leaves room for its vibrato;
    // the dry path and the reported latency include it, so dry and lead line up exactly
    lampPre = std::max (1, (int) std::lrint (0.0008 * fs));
    lampBaseMs = 1000.0f * (float) lampPre / fsf;
    totalLat = latencyMax + lampPre;

    for (auto& c : ch)
    {
        c.dry.prepare (totalLat + 8);
        c.ctlRestore.prepare (totalLat + kSub + 8);
        c.ctlFocus.prepare (totalLat + kSub + 8);
        c.ctlInkWet.prepare (totalLat + kSub + 8);
        c.ctlInkHarp.prepare (totalLat + kSub + 8);
        c.ghostLP.lowpass (320.0, 0.707, fs);
        c.ghostSubLP.lowpass (700.0, 0.707, fs);
        c.ghostUpLP.lowpass (1000.0, 0.707, fs);
        c.ghostEnv.set (1.0f, 50.0f, fsf);
        c.ghostUpHP.setCutoff (40.0f, fsf);
        c.crushEnv.set (1.0f, 60.0f, fsf);
        c.dc.setCutoff (20.0f, fsf);
        c.smLP.lowpass (4200.0, 0.707, fs);
        c.smEnvLo.set (1.0f, 40.0f, fsf);
        c.smEnvHi.set (1.0f, 40.0f, fsf);
        c.harpHP.highpass (150.0, 0.707, fs);
        c.harpLP.lowpass (3000.0, 0.707, fs);
        c.harp.prepare (fsf);
    }

    envCons.set (5.0f, 150.0f, fsf);
    inkFast.set (0.5f, 15.0f, fsf);
    inkSlow.set (25.0f, 150.0f, fsf);
    vEnvLo.set (1.0f, 20.0f, fsf);
    vEnvHi.set (1.0f, 20.0f, fsf);
    vLP.lowpass (1500.0, 0.707, fs);
    vHP.highpass (4500.0, 0.707, fs);

    aS     = smoothCoef (15.0f, fsf);
    aCtl   = smoothCoef (4.0f, fsf);
    aInk   = smoothCoef (3.0f, fsf);
    aGn    = smoothCoef (8.0f, fsf);
    aWarp  = smoothCoef (20.0f, fsf);
    aVfast = timeCoef (2.0f, fsf);
    aVslow = timeCoef (15.0f, fsf);
    aSmAtk = timeCoef (1.0f, fsf);
    aSmRel = timeCoef (40.0f, fsf);
    minOnsetGap = (long) (0.07 * fs);
    ligL.prepare ((int) (1.6 * fs) + 8);
    ligR.prepare ((int) (1.6 * fs) + 8);
    for (int c = 0; c < 2; ++c)
    {
        ligLP[c].setCutoff (3500.0f, fsf);
        ligHP[c].setCutoff (180.0f, fsf);
    }
    aLigT = smoothCoef (80.0f, fsf);
    aEcho.prepare (fsf);
    aForest.prepare (fsf);
    aForest.modDepth = 0.00045f * fsf;
    aTwin.prepare (fsf);
    duckKey.prepare (fsf);
    duckLig.prepare (fsf);
    duckAura.prepare (fsf);
    for (auto& g : guiDuck) g.store (1.0f);
    fxHP.highpass (120.0, 0.707, fs);
    fxLP.lowpass (9000.0, 0.707, fs);
    loudFall = 8.0f / fsf;            // the loudness reference falls 8 dB per second
    forestVersion = -1;
    aDuckAtk = timeCoef (8.0f, fsf);
    aDuckRel = timeCoef (150.0f, fsf);
    warmLen = (int) (0.05 * fs) + latencyMax;
    fadeLen = std::max (1, (int) (0.03 * fs));

    // probe: a vocal-like periodic wave, peak = reference level
    {
        static const float amp[6] = { 1.0f, 0.6f, 0.45f, 0.3f, 0.2f, 0.12f };
        static const float phs[6] = { 0.0f, 0.7f, 1.9f, 0.4f, 2.6f, 1.1f };
        float peak = 0.0f;
        for (int i = 0; i < 64; ++i)
        {
            float s = 0.0f;
            for (int h = 0; h < 6; ++h)
                s += amp[h] * std::sin (kTwoPi * (float) (h + 1) * (float) i / 64.0f + phs[h]);
            probe[i] = s;
            peak = std::max (peak, std::fabs (s));
        }
        const float ref = dbToGain (kRefDb);
        for (float& p : probe)
            p *= ref / peak;
    }

    for (auto& g : guiPhase)
        g.store (0.0f);

    lastLowCut = lastHighCut = -1.0f;
    lastTilt = lastTone = 1.0e9f;
    lastDecay = lastBright = lastHarpDrive = lastWidth = -1.0f;
    lastKey = lastChord = lastReg = -1;
    reset();
}

inline void Engine::reset() noexcept
{
    for (auto& p : paths)
        p.reset();
    for (auto& c : ch)
    {
        c.dry.reset();
        c.ctlRestore.reset(); c.ctlFocus.reset(); c.ctlInkWet.reset(); c.ctlInkHarp.reset();
        c.preHP.reset(); c.preLP.reset(); c.tiltLo.reset(); c.tiltHi.reset();
        for (float& z : c.apZ) z = 0.0f;
        c.ghostLP.reset(); c.ghostSubLP.reset(); c.ghostUpLP.reset(); c.ghostEnv.reset(); c.ghostUpHP.reset();
        c.zPrev = 0.0f; c.flip = 1.0f; c.armed = false;
        c.crossCount = 0; c.f0 = 150.0f; c.lastDelta = 0.0f; c.newPeriod = false; c.crushEnv.reset();
        c.dLampS = lampBaseMs; c.dGlassS = 23.0f; c.dFoldS = 37.0f; c.dDopS = 0.0f;
        c.dc.reset();
        c.smLP.reset(); c.smEnvLo.reset(); c.smEnvHi.reset(); c.smGain = 1.0f;
        c.harpHP.reset(); c.harpLP.reset(); c.harp.reset();
        c.toneLo.reset(); c.toneHi.reset();
    }
    envCons.reset(); inkFast.reset(); inkSlow.reset(); vEnvLo.reset(); vEnvHi.reset();
    vLP.reset(); vHP.reset();
    gNdb = 0.0f; voiced = 1.0f; strokeRem = 0.0f; lastEnvDb = -100.0f;
    sinceOnset = 1L << 30;
    inkDriveDb = 0.0f; inkDop = inkWet = inkHarp = 1.0f;
    warpMix = 0.0f;
    slS1 = slS2 = 0.0f;
    ligL.reset(); ligR.reset();
    for (int c = 0; c < 2; ++c) { ligLP[c].reset(); ligHP[c].reset(); }
    ligTL = ligTR = 0.0f;
    ligDuck = 1.0f;
    loudRef = -70.0f;
    aEcho.reset(); aForest.reset(); aTwin.reset(); fxHP.reset(); fxLP.reset();
    duckKey.reset(); duckLig.reset(); duckAura.reset();
    echoLive = forestLive = twinLive = false;
    nextPath = -1; warmLeft = fadeLeft = 0;
    autoStable = 0;
    first = true;
    snap = true;
}

//==============================================================================
inline void Engine::computeProbe() noexcept
{
    const float drv = tDrive, grit = prm[P_GRIT], bias = prm[P_BIAS];
    const float c0 = 1.2f * bias, tc0 = fastTanh (c0);
    const float kG = 0.65f * drv;
    const float kF = std::min (24.0f, drv * (0.6f + 2.4f * grit));
    const float s0 = 0.45f * grit, ss0 = fastSin (s0);

    const float gl = prm[P_LAMP], gg = prm[P_GLASS], gf = prm[P_FOLD];
    const float powSum = std::sqrt (gl * gl + gg * gg + gf * gf);
    mixNorm = 1.0f / std::max (1.0f, powSum);

    if (prm[P_AUTOGAIN] < 0.5f)
    {
        compLamp = compGlass = compFold = 1.0f;
        tComp = 1.0f;
        return;
    }

    auto acRms = [] (const float* x, int n)
    {
        double s = 0.0, s2 = 0.0;
        for (int i = 0; i < n; ++i) { s += x[i]; s2 += (double) x[i] * x[i]; }
        const double m = s / n;
        return (float) std::sqrt (std::max (1.0e-12, s2 / n - m * m));
    };

    float bl[64], bg[64], bf[64];
    for (int i = 0; i < 64; ++i)
    {
        bl[i] = shapeLamp (probe[i], drv, c0, tc0);
        bg[i] = shapeGlass (probe[i], kG);
        bf[i] = shapeFold (probe[i], kF, s0, ss0);
    }
    const float rIn = acRms (probe, 64);
    compLamp  = clampf (rIn / acRms (bl, 64), 0.05f, 20.0f);
    compGlass = clampf (rIn / acRms (bg, 64), 0.05f, 20.0f);
    compFold  = clampf (rIn / acRms (bf, 64), 0.05f, 20.0f);

    // clipper on a representative waveform at the expected (decorrelated) mix level
    const float grit2 = grit * grit;
    const float clipGain = dbToGain (grit * 15.0f);
    const float level = std::min (1.0f, powSum) ;
    float y[64];
    for (int i = 0; i < 64; ++i)
        y[i] = shapeClip (bl[i] * compLamp * level, 2.0f * clipGain, grit2, 0.5f / clipGain);
    const float rY = acRms (y, 64);
    tComp = clampf ((rIn * std::max (level, 0.05f)) / rY, 0.1f, 10.0f);
}

inline int Engine::desiredPath (int n) noexcept
{
    const int sel = std::min (std::max ((int) (prm[P_OS] + 0.5f), 0), 5);
    if (sel > 0)
        return sel - 1;

    const float dEff = clampf (prm[P_DRIVE] + (1.0f - prm[P_CONSIST]) * std::max (0.0f, lastEnvDb - kRefDb) / 36.0f, 0.0f, 1.0f);
    const float grit = prm[P_GRIT];
    const float harsh = std::max (std::max (std::max (dEff, 0.85f * grit), 0.7f * prm[P_CRUSH]),
                                  std::max (0.6f * prm[P_DOPPLER] * prm[P_GLASS] + 0.3f * dEff,
                                            prm[P_FOLD] * (0.4f + 0.6f * grit) * (0.3f + dEff)));
    // never below ~88 kHz inside (keeps the modulated delays clean), more as the distortion grows
    const float target = harsh < 0.30f ? 96000.0f : (harsh < 0.62f ? 192000.0f : 384000.0f);

    int idx = 0;
    while (idx < kPaths - 1 && fsf * (float) (1 << idx) < 0.9f * target)
        ++idx;
    if (offline && harsh >= 0.03f && idx < kPaths - 1)
        ++idx;   // rendering: one step better

    if (first || offline)
    {
        autoChoice = autoPending = idx;
        autoStable = 0;
        return idx;
    }
    if (idx != autoPending) { autoPending = idx; autoStable = 0; }
    else if (autoStable < (1L << 30)) autoStable += n;

    const long need = (long) ((autoPending > autoChoice ? 0.05 : 1.5) * fs);
    if (autoPending != autoChoice && autoStable >= need)
        autoChoice = autoPending;
    return autoChoice;
}

inline void Engine::updateBlock() noexcept
{
    const float* v = prm;
    tDrive   = dbToGain (v[P_DRIVE] * 36.0f);
    const float mix = clampf (v[P_MIX], 0.0f, 1.0f);
    tDryG    = std::pow (1.0f - mix, 0.75f);
    tWetG    = std::pow (mix, 0.75f);
    tOut     = dbToGain (v[P_OUTPUT]);
    tLamp    = v[P_LAMP];
    tGlass   = v[P_GLASS];
    tFold    = v[P_FOLD];
    tHarpLvl = v[P_MELODY] * 1.1f;
    tEcho    = v[P_ECHO] * v[P_ECHO];
    tForest  = v[P_FOREST] * v[P_FOREST];
    tTwin    = v[P_TWIN] * v[P_TWIN];
    tHarpExc = 0.5f + v[P_MELODY];

    for (auto& c : ch)
    {
        if (differs (v[P_LOWCUT], lastLowCut))   c.preHP.highpass (v[P_LOWCUT], 0.707, fs);
        if (differs (v[P_HIGHCUT], lastHighCut)) c.preLP.lowpass (v[P_HIGHCUT], 0.707, fs);
        if (differs (v[P_PRETILT], lastTilt))
        {
            c.tiltLo.lowShelf (400.0, -0.5 * v[P_PRETILT], fs);
            c.tiltHi.highShelf (1500.0, 0.5 * v[P_PRETILT], fs);
        }
        if (differs (v[P_TONE], lastTone))
        {
            c.toneLo.lowShelf (250.0, -4.0 * v[P_TONE], fs);
            c.toneHi.highShelf (3000.0, 8.0 * v[P_TONE], fs);
        }
    }
    lastLowCut = v[P_LOWCUT]; lastHighCut = v[P_HIGHCUT]; lastTilt = v[P_PRETILT]; lastTone = v[P_TONE];

    const float sm = v[P_SMOOTH];
    smThr = lerpf (0.70f, 0.12f, sm);
    smExp = 0.6f + 0.8f * sm;
    smStatic = 1.0f - 0.3f * sm;
    harpShim = 0.0008f * v[P_MOTION];

    // harp tuning
    const int key = std::min (std::max ((int) (v[P_KEY] + 0.5f), 0), 11);
    const int chord = std::min (std::max ((int) (v[P_CHORD] + 0.5f), 0), 11);
    const int reg = std::min (std::max ((int) (v[P_REGISTER] + 0.5f), 0), 2);
    if (key != lastKey || chord != lastChord || reg != lastReg || differs (v[P_WIDTH], lastWidth)
        || differs (v[P_DECAY], lastDecay) || differs (v[P_BRIGHT], lastBright) || differs (v[P_HARPDRIVE], lastHarpDrive))
    {
        const int root = 36 + 12 * reg + key;
        for (int c = 0; c < 2; ++c)
        {
            float hz[Harp::kStrings];
            for (int i = 0; i < Harp::kStrings; ++i)
            {
                float cents = tables::kStringDetune[i];
                if (chord == 0 && i >= 3) cents += 4.0f;
                if (c == 1) cents += 5.0f * v[P_WIDTH];
                const float midi = (float) (root + tables::kChords[chord][i]) + 0.01f * cents;
                hz[i] = 440.0f * std::exp2 ((midi - 69.0f) / 12.0f);
            }
            ch[c].harp.configure (hz, v[P_DECAY], v[P_BRIGHT], v[P_HARPDRIVE]);
            if (c == 0)
                for (int i = 0; i < Harp::kStrings; ++i)
                    harpHz[i] = hz[i];
        }
        ++harpVersion;
        lastKey = key; lastChord = chord; lastReg = reg; lastWidth = v[P_WIDTH];
        lastDecay = v[P_DECAY]; lastBright = v[P_BRIGHT]; lastHarpDrive = v[P_HARPDRIVE];
    }

    const int ftype = std::min (std::max ((int) (v[P_FORESTTYPE] + 0.5f), 0), 3);
    if (forestVersion != harpVersion || ftype != lastForestType
        || differs (v[P_FORESTDECAY], lastForestDecay) || differs (v[P_FORESTTUNE], lastForestTune))
    {
        aForest.configure (ftype, v[P_FORESTDECAY], v[P_FORESTTUNE], harpHz, 6);
        forestVersion = harpVersion; lastForestType = ftype;
        lastForestDecay = v[P_FORESTDECAY]; lastForestTune = v[P_FORESTTUNE];
    }

    computeProbe();

    if (snap)
    {
        sDrive = tDrive; sDryG = tDryG; sWetG = tWetG; sOut = tOut; sLamp = tLamp; sGlass = tGlass; sFold = tFold;
        sComp = tComp; sHarpLvl = tHarpLvl; sHarpExc = tHarpExc;
        sEcho = tEcho; sForest = tForest; sTwin = tTwin;
        warpMix = (v[P_WARP] * v[P_MOTION] > 0.001f) ? 1.0f : 0.0f;
        snap = false;
    }
}

//==============================================================================
inline void Engine::processSub (float* const* io, int numCh, int off, int n) noexcept
{
    updateBlock();

    const float* v = prm;
    const float motion = v[P_MOTION];
    const float vib = v[P_VIBRATO] * motion, ens = v[P_ENSEMBLE] * motion, breath = v[P_BREATH] * motion;
    const float pulse = v[P_PULSE] * motion, orbit = v[P_ORBIT] * motion, warpD = v[P_WARP] * motion;
    const float width = v[P_WIDTH];
    const int ratioIdx = std::min (std::max ((int) (v[P_RATIO] + 0.5f), 0), 7);
    const int rateIdx = std::min (std::max ((int) (v[P_RATE] + 0.5f), 0), 6);
    const double bpc = tables::kBeatsPerCycle[rateIdx];
    const float* rr = tables::kRatios[ratioIdx];
    const double dppq = hostBpm / (60.0 * fs);
    const float consist = v[P_CONSIST], focus = v[P_FOCUS];
    const float ink = v[P_INK], inkA = std::fabs (ink);
    const float inkDecay = std::exp (-2.5f / (0.001f * std::max (v[P_INKTIME], 10.0f) * fsf));
    const float gd = v[P_GHOSTDOWN], gu = v[P_GHOSTUP];
    const float doppler = v[P_DOPPLER], bias = v[P_BIAS], grit = v[P_GRIT];
    const float clipGain = dbToGain (grit * 15.0f);
    const float clipIn = 2.0f * clipGain, clipOut = 0.5f / clipGain, clipH = grit * grit;
    const float warpTarget = (warpD > 0.001f) ? 1.0f : 0.0f;
    const bool stereo = numCh > 1;
    const float slant = v[P_SLANT];
    const float crushParam = v[P_CRUSH];
    const int crushMode = std::min (std::max ((int) (v[P_CRUSHMODE] + 0.5f), 0), 2);
    const float slantTime = std::max (0.015f, 0.001f * v[P_INKTIME] / 3.0f);
    const float aSl = std::exp (-1.0f / (slantTime * fsf));
    const float slantImpulse = 2.5f * slant * 2.718282f;   // peak of the gesture = 2.5 ms * slant

    for (int j = 0; j < n; ++j)
    {
        // ---- chord clock
        double ppq;
        if (hostPlaying) { ppq = hostPpq + (double) blockPos * dppq; freePpq = ppq; }
        else             { freePpq += dppq; ppq = freePpq; }
        ++blockPos;

        float lfoL[4], lfoR[4];
        for (int i = 0; i < 4; ++i)
        {
            double ph = ppq / bpc * (double) rr[i] + (ratioIdx == 0 ? (double) tables::kUnisonOffset[i] : 0.0);
            ph -= std::floor (ph);
            const float phf = (float) ph;
            lfoL[i] = fastSin (kTwoPi * phf);
            lfoR[i] = fastSin (kTwoPi * (phf + width * tables::kStereoOffset[i]));
            if (j == n - 1)
                guiPhase[i].store (phf, std::memory_order_relaxed);
        }

        sDrive += (tDrive - sDrive) * aS;
        sLamp  += (tLamp - sLamp) * aS;
        sGlass += (tGlass - sGlass) * aS;
        sFold  += (tFold - sFold) * aS;

        // ---- input, latency-aligned dry, pre EQ
        float xe[2] = { 0.0f, 0.0f };
        for (int c = 0; c < numCh; ++c)
        {
            Chan& C = ch[c];
            const float x = io[c][off + j];
            C.dry.push (x);
            dryOut[c][j] = C.dry.tap (totalLat);
            xe[c] = C.tiltHi.process (C.tiltLo.process (C.preLP.process (C.preHP.process (x))));
        }
        const float mono = stereo ? 0.5f * (xe[0] + xe[1]) : xe[0];

        // ---- analysis: consistency (level-independent drive)
        const float envDb = gainToDb (envCons.process (mono));
        lastEnvDb = envDb;
        // recent loudness of this voice: quiet and loud recordings behave the same,
        // and room noise far below the voice is never pulled up
        loudRef = envDb > loudRef ? envDb : std::max (-70.0f, loudRef - loudFall);
        const float rel = envDb - loudRef;
        const float gateW = smoothstep (-66.0f, -48.0f, envDb) * smoothstep (-42.0f, -26.0f, rel);
        const float gTarget = consist * clampf (kRefDb - envDb, -18.0f, 24.0f) * gateW;
        gNdb += (gTarget - gNdb) * aGn;
        const float gN = dbToGain (gNdb);

        // ---- vowel / consonant detector
        const float eLo = vEnvLo.process (vLP.process (mono));
        const float eHi = vEnvHi.process (vHP.process (mono));
        const float rr2 = eHi / (eLo + eHi + 1.0e-7f);
        const float vt = 1.0f - smoothstep (0.22f, 0.55f, rr2);
        voiced = vt + (vt < voiced ? aVfast : aVslow) * (voiced - vt);

        // ---- Ink: syllable onsets drive the handwriting stroke
        const float dF = gainToDb (inkFast.process (mono));
        const float dS = gainToDb (inkSlow.process (mono));
        if (sinceOnset > minOnsetGap && dF > -50.0f && dF - dS > 7.0f)
        {
            sinceOnset = 0;
            strokeRem = 1.0f;
            slS1 += slantImpulse;
        }
        else
        {
            if (sinceOnset < (1L << 30))
                ++sinceOnset;
            strokeRem *= inkDecay;
        }
        slS1 *= aSl;
        slS2 = aSl * slS2 + (1.0f - aSl) * slS1;
        const float slantMs = clampf (slS2, -6.0f, 6.0f);

        // ducking for Ligature / Aura: relative to the voice's own recent loudness,
        // so quiet and loud recordings behave the same
        const float duckT = 1.0f - 0.92f * smoothstep (-30.0f, -12.0f, rel) * smoothstep (-62.0f, -50.0f, envDb);
        breathe[j] = smoothstep (-24.0f, 0.0f, rel);
        auraA[j] = lfoL[1];
        auraB[j] = lfoL[2];
        auraC[j] = lfoL[0];
        ligDuck = duckT + (duckT < ligDuck ? aDuckAtk : aDuckRel) * (ligDuck - duckT);
        duckBuf[j] = ligDuck;
        float tDriveDb, tDop, tWet, tHarp;
        if (ink >= 0.0f)
        {   // Ink: starts clean, the distortion spreads over the word
            tDriveDb = -20.0f * inkA * strokeRem;
            tDop  = 1.0f - 0.85f * inkA * strokeRem;
            tWet  = 1.0f - 0.55f * inkA * strokeRem;
            tHarp = 1.0f - 0.70f * inkA * strokeRem;
        }
        else
        {   // Press: the pen hits hard, then lifts off
            const float post = 1.0f - strokeRem;
            tDriveDb = inkA * (12.0f * strokeRem - 3.0f * post);
            tDop  = 1.0f + inkA * strokeRem;
            tWet  = 1.0f + inkA * (0.5f * strokeRem - 0.15f * post);
            tHarp = 1.0f + 0.8f * inkA * strokeRem;
        }
        inkDriveDb += (tDriveDb - inkDriveDb) * aInk;
        inkDop     += (tDop - inkDop) * aInk;
        inkWet     += (tWet - inkWet) * aInk;
        inkHarp    += (tHarp - inkHarp) * aInk;
        const float drv = sDrive * dbToGain (inkDriveDb);

        warpMix += (warpTarget - warpMix) * aWarp;

        for (int c = 0; c < numCh; ++c)
        {
            Chan& C = ch[c];
            const float* lfo = (c == 0) ? lfoL : lfoR;
            float x = xe[c] * gN;

            // Warp: chord-clocked allpass chain (changes the waveform the shapers see)
            if (warpMix > 1.0e-4f)
            {
                const float fc = std::min (650.0f * std::exp2 (1.6f * warpD * lfo[0]), 0.4f * fsf);
                const float t = std::tan (kPi * fc / fsf);
                const float a = (t - 1.0f) / (t + 1.0f);
                float y = x;
                for (int s = 0; s < 4; ++s)
                {
                    const float o = a * y + C.apZ[s];
                    C.apZ[s] = y - a * o;
                    y = o;
                }
                x += warpMix * (y - x);
            }

            // Ghosts: flip-flop sub octave (with its natural fifth) + rectified octave up.
            // They go INTO the shapers, so the distortion intermodulates as a chord.
            const float z = C.ghostLP.process (x);
            const float thr = 0.12f * C.ghostEnv.process (z) + 1.0e-6f;
            if (z < -thr) C.armed = true;
            ++C.crossCount;
            C.newPeriod = false;
            if (C.armed && C.zPrev < 0.0f && z >= 0.0f)
            {
                C.flip = -C.flip;
                C.armed = false;
                // pitch of the voice from the period between upward crossings (45..520 Hz), sub-sample accurate
                const float delta = C.zPrev / (C.zPrev - z + 1.0e-12f);
                const float period = (float) C.crossCount + delta - C.lastDelta;
                if (period > fsf / 520.0f && period < fsf / 45.0f)
                {
                    C.f0 += 0.35f * (fsf / period - C.f0);
                    C.newPeriod = true;
                }
                C.lastDelta = delta;
                C.crossCount = 0;
            }
            if (C.crossCount > (int) fsf) C.crossCount = (int) fsf;
            C.zPrev = z;
            const float sub = C.ghostSubLP.process (C.flip * z);
            const float up = C.ghostUpHP.hp (std::fabs (C.ghostUpLP.process (x)));
            x += voiced * (gd * 1.3f * sub + gu * 2.2f * up);
            coreIn[c][j] = x;

            // crusher controls (the crush follows the handwriting stroke too)
            const float cEnv = C.crushEnv.process (x);
            const float crush = std::min (1.0f, crushParam * (ink >= 0.0f ? 1.0f - 0.8f * inkA * strokeRem
                                                                            : 1.0f + 0.6f * inkA * strokeRem));
            InnerControls& kc = ctl[c][j];
            if (crush > 0.0005f)
            {
                const float bits = 12.0f - 9.0f * crush;
                float rate;
                if (crushMode == 1)
                    rate = 16000.0f * std::exp2 (-4.0f * crush);                 // Digital: fixed 16 kHz .. 1 kHz
                else
                {
                    float mult = 1.0f;                                           // Clock: resolution jumps in octaves
                    if (crushMode == 2) mult = lfo[2] > 0.5f ? 2.0f : (lfo[2] < -0.5f ? 0.5f : 1.0f);
                    // Harmonic: a WHOLE number of steps per cycle (64 .. 4), so every product is a harmonic
                    const float steps = std::max (2.0f, std::round (64.0f * std::exp2 (-4.0f * crush) * mult));
                    rate = steps * C.f0;
                }
                kc.crushRate = rate;
                kc.crushSync = (crushMode != 1 && C.newPeriod) ? 1.0f + (1.0f - C.lastDelta) : 0.0f;
                kc.crushStep = (crushMode == 1 ? 1.0f : cEnv + 1.0e-4f) * std::exp2 (1.0f - bits);
                kc.crushMix = std::min (1.0f, 3.0f * crush);
            }
            else
            {
                kc.crushRate = 0.0f;
                kc.crushStep = 0.0f;
                kc.crushMix = 0.0f;
                kc.crushSync = 0.0f;
            }

            // core controls
            const float dLampT  = lampBaseMs + 0.55f * vib * lfo[0];
            const float dGlassT = (c == 0 ? 23.0f : 23.0f + 6.0f * width) + 3.0f * ens * lfo[1];
            const float dFoldT  = (c == 0 ? 37.0f : 37.0f - 7.0f * width) + 3.0f * ens * lfo[2];
            const float dDopT   = 0.32f * doppler * inkDop * (1.0f + 0.6f * motion * lfo[3]);
            C.dLampS  += (dLampT - C.dLampS) * aCtl;
            C.dGlassS += (dGlassT - C.dGlassS) * aCtl;
            C.dFoldS  += (dFoldT - C.dFoldS) * aCtl;
            C.dDopS   += (dDopT - C.dDopS) * aCtl;

            InnerControls& k = ctl[c][j];
            k.dLamp  = C.dLampS;
            k.dGlass = C.dGlassS + slantMs;          // the doubles draw the stroke,
            k.dFold  = C.dFoldS + 1.4f * slantMs;    // the lamp (lead) stays in tune
            k.dDop   = std::max (0.0f, C.dDopS);
            k.kLamp  = drv;
            k.cLamp  = 1.2f * bias + 0.6f * breath * lfo[1];
            k.tcLamp = fastTanh (k.cLamp);
            k.kGlass = 0.65f * drv;
            k.dopSens = 4.0f;
            k.kFold  = std::min (24.0f, drv * (0.6f + 2.4f * grit));
            k.sFold  = 0.45f * grit + 0.35f * grit * motion * lfo[3];
            k.ssFold = fastSin (k.sFold);

            const float panG = -0.55f * width + 0.45f * orbit * width * lfo[2];
            const float panF =  0.55f * width - 0.45f * orbit * width * lfo[1];
            const float pg = stereo ? panGain (panG, c) : 1.0f;
            const float pf = stereo ? panGain (panF, c) : 1.0f;
            const float pulG = 1.0f - pulse * (0.5f + 0.5f * lfo[2]);
            const float pulF = 1.0f - pulse * (0.5f - 0.5f * lfo[2]);
            k.gLamp  = sLamp * compLamp * mixNorm;
            k.gGlass = sGlass * compGlass * mixNorm * pg * pulG;
            k.gFold  = sFold * compFold * mixNorm * pf * pulF;
            k.clipIn = clipIn;
            k.clipH  = clipH;
            k.clipOut = clipOut;

            shim[c][j] = harpShim * lfo[3];

            // controls used after the core (delayed by the core latency)
            C.ctlRestore.push (1.0f / gN);
            C.ctlFocus.push (1.0f - focus * (1.0f - voiced));
            C.ctlInkWet.push (inkWet);
            C.ctlInkHarp.push (inkHarp);
        }
    }
    guiInk.store (strokeRem, std::memory_order_relaxed);
    guiVoiced.store (voiced, std::memory_order_relaxed);
    guiSlant.store (slS2, std::memory_order_relaxed);
    guiLevel.store (lastEnvDb, std::memory_order_relaxed);
    guiRel.store (lastEnvDb - loudRef, std::memory_order_relaxed);

    // ---- oversampled core with click-free factor changes (constant latency)
    const int want = desiredPath (n);
    if (first)
    {
        activePath = want;
        first = false;
    }
    if (nextPath < 0 && want != activePath)
    {
        nextPath = want;
        paths[nextPath].reset();
        warmLeft = warmLen;
        fadeLeft = fadeLen;
    }
    for (int c = 0; c < numCh; ++c)
        paths[activePath].process (c, coreIn[c], ctl[c], coreOutA[c], n);
    if (nextPath >= 0)
    {
        for (int c = 0; c < numCh; ++c)
            paths[nextPath].process (c, coreIn[c], ctl[c], coreOutB[c], n);
        int w = warmLeft, f = fadeLeft;
        for (int j = 0; j < n; ++j)
        {
            float t;
            if (w > 0)      { t = 0.0f; --w; }
            else if (f > 0) { t = 1.0f - (float) f / (float) fadeLen; --f; }
            else            t = 1.0f;
            for (int c = 0; c < numCh; ++c)
                coreOutA[c][j] += t * (coreOutB[c][j] - coreOutA[c][j]);
        }
        warmLeft = w;
        fadeLeft = f;
        if (warmLeft == 0 && fadeLeft == 0)
        {
            activePath = nextPath;
            nextPath = -1;
        }
    }
    guiFactor.store (1 << activePath, std::memory_order_relaxed);

    // ---- post: anti-harsh, dynamics, focus, ink, ligature, harp, tone, mix
    const bool smOn = v[P_SMOOTH] > 0.001f;
    const int duckMode = std::min (std::max ((int) (v[P_DUCK] + 0.5f), 0), 2);   // 0 spectral, 1 classic, 2 off
    const bool spectral = duckMode == 0;
    // Depth: how far the echo must stay under the voice (spectral) / how deep the whole return dips (classic)
    const float depthDb = 6.0f + 18.0f * v[P_DUCKDEPTH];
    const float auraMargin = dbToGain (-depthDb), ligMargin = dbToGain (-(depthDb + 3.0f)), duckFloor = dbToGain (-30.0f);
    const float ligClassic = dbToGain (-(depthDb + 6.0f));
    float guiDuckSum = 0.0f;
    int guiDuckCount = 0;
    const float feed = v[P_FEED], spin = v[P_SPIN], forestBloom = v[P_FORESTBLOOM], echoFb = v[P_ECHOFB];
    const int twinType = std::min (std::max ((int) (v[P_TWINTYPE] + 0.5f), 0), 4);
    static const float kEchoBeats[7] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
    const int echoIdx = std::min (std::max ((int) (v[P_ECHOTIME] + 0.5f), 0), 6);
    const float echoTime = std::min (2.5f * fsf, echoIdx == 0 ? 0.085f * fsf : kEchoBeats[echoIdx] * (float) (60.0 / hostBpm) * fsf);
    const float lig = v[P_LIGATURE];
    const bool ligOn = lig > 0.0005f;
    const float ligFb = 0.2f + 0.5f * lig, ligOut = 0.9f * lig;
    const float beatS = (float) (60.0 / hostBpm) * fsf;
    const float ligTLt = std::min (0.5f * beatS, 1.5f * fsf);    // 1/8
    const float ligTRt = std::min (0.75f * beatS, 1.5f * fsf);   // dotted 1/8
    if (ligTL <= 0.0f) { ligTL = ligTLt; ligTR = ligTRt; }

    float check = 0.0f;
    for (int j = 0; j < n; ++j)
    {
        sComp    += (tComp - sComp) * aS;
        sDryG    += (tDryG - sDryG) * aS;
        sWetG    += (tWetG - sWetG) * aS;
        sOut     += (tOut - sOut) * aS;
        sHarpLvl += (tHarpLvl - sHarpLvl) * aS;
        sHarpExc += (tHarpExc - sHarpExc) * aS;
        const int dly = totalLat + (n - 1 - j);

        float wv[2] = { 0.0f, 0.0f };
        const float voiceGate = clampf ((1.0f - duckBuf[j]) * (1.0f / 0.92f), 0.0f, 1.0f);
        if (spectral)
            duckKey.process (stereo ? 0.5f * (dryOut[0][j] + dryOut[1][j]) : dryOut[0][j], voiceGate);
        for (int c = 0; c < numCh; ++c)
        {
            Chan& C = ch[c];
            float w = C.dc.hp (coreOutA[c][j]) * sComp;

            const float lo = C.smLP.process (w), hi = w - lo;
            const float eL = C.smEnvLo.process (lo), eH = C.smEnvHi.process (hi);
            const float ratio = eH / (eL + 1.0e-5f);
            const float gt = (smOn && ratio > smThr) ? std::pow (smThr / ratio, smExp) : 1.0f;
            C.smGain = gt + (gt < C.smGain ? aSmAtk : aSmRel) * (C.smGain - gt);
            w = lo + hi * C.smGain * smStatic;

            wv[c] = w * C.ctlRestore.tap (dly) * C.ctlFocus.tap (dly) * C.ctlInkWet.tap (dly);
        }

        // Ligature: saturated ping-pong echo in time with the song, ducked while the voice sings
        if (ligOn)
        {
            ligTL += (ligTLt - ligTL) * aLigT;
            ligTR += (ligTRt - ligTR) * aLigT;
            const float oL = ligL.read (ligTL);
            const float oR = stereo ? ligR.read (ligTR) : 0.0f;
            const float duck = (duckMode == 1 ? 1.0f - voiceGate * (1.0f - ligClassic) : 1.0f) * ligOut;
            float rL = duck * oL, rR = stereo ? duck * oR : 0.0f;
            if (spectral)
                duckLig.process (duckKey, rL, rR, ligMargin, duckFloor);   // keep (depth + 3) dB under the voice
            if (stereo)
            {
                const float fL = fastTanh (1.5f * ligHP[0].hp (ligLP[0].lp (oR))) * (1.0f / 1.5f);
                const float fR = fastTanh (1.5f * ligHP[1].hp (ligLP[1].lp (oL))) * (1.0f / 1.5f);
                ligL.push (wv[0] + ligFb * fL);
                ligR.push (wv[1] + ligFb * fR);
                wv[0] += rL;
                wv[1] += rR;
            }
            else
            {
                const float f = fastTanh (1.5f * ligHP[0].hp (ligLP[0].lp (oL))) * (1.0f / 1.5f);
                ligL.push (wv[0] + ligFb * f);
                wv[0] += rL;
            }
        }

        for (int c = 0; c < numCh; ++c)
        {
            Chan& C = ch[c];
            float w = wv[c];
            const float e = C.harpLP.process (C.harpHP.process (w)) * sHarpExc * C.ctlInkHarp.tap (dly);
            const float h = C.harp.process (e, shim[c][j]);
            w += sHarpLvl * 0.7f * fastTanh (h * (0.55f / 0.7f));

            wv[c] = C.toneHi.process (C.toneLo.process (w));
        }

        // AURA: nonlinear send effects, added after the dry/wet mix (like a send return)
        float aura[2] = { 0.0f, 0.0f };
        sEcho   += (tEcho - sEcho) * aS;
        sForest += (tForest - sForest) * aS;
        sTwin   += (tTwin - sTwin) * aS;
        const bool wantEcho = tEcho > 1.0e-7f || sEcho > 1.0e-5f;
        const bool wantForest = tForest > 1.0e-7f || sForest > 1.0e-5f;
        const bool wantTwin = tTwin > 1.0e-7f || sTwin > 1.0e-5f;
        if (wantEcho || wantForest || wantTwin)
        {
            const float d0 = dryOut[0][j], d1 = stereo ? dryOut[1][j] : d0;
            const float w0 = wv[0], w1 = stereo ? wv[1] : w0;
            const float src = fxLP.process (fxHP.process (0.5f * ((1.0f - feed) * (d0 + d1) + feed * (w0 + w1))));
            const float motionA = (0.3f + spin) * auraA[j];
            float eL = 0.0f, eR = 0.0f, fL = 0.0f, fR = 0.0f, tL = 0.0f, tR = 0.0f;
            if (wantEcho)
            {
                if (! echoLive) { aEcho.reset(); echoLive = true; }
                aEcho.process (src, echoTime, echoFb, 0.25f + 0.75f * grit, 0.004f * motionA, eL, eR);
            }
            else echoLive = false;
            if (wantForest)
            {
                if (! forestLive) { aForest.reset(); forestLive = true; }
                aForest.process (src, 1.2f / (1.0f + 3.0f * grit), (0.3f + spin) * auraB[j], fL, fR);
            }
            else forestLive = false;
            if (wantTwin)
            {
                if (! twinLive) { aTwin.reset(); twinLive = true; }
                aTwin.process (src, twinType, breathe[j], motionA, grit, tL, tR);
            }
            else twinLive = false;

            const float duck = duckBuf[j];
            const float ge = sEcho * (duckMode == 1 ? 1.0f - voiceGate * (1.0f - auraMargin) : 1.0f);
            const float gf = sForest * 1.6f * (duckMode == 2 ? 1.0f : 1.0f - forestBloom * (1.0f - duck));
            const float gt = sTwin;
            float dL = ge * eL + gf * fL, dR = ge * eR + gf * fR;
            if (spectral)
            {
                // the echo and the forest step aside only in the frequencies the voice is using right now
                duckAura.process (duckKey, dL, dR, auraMargin, duckFloor);   // keep `depth` dB under the voice
            }
            else if (duckMode == 1)
            {
                guiDuckSum += 1.0f - voiceGate * (1.0f - auraMargin);
                ++guiDuckCount;
            }
            float aL = dL + gt * tL;      // the twins are meant to sing WITH the voice: never ducked
            float aR = dR + gt * tR;
            // Spin: the whole aura turns around the head with the clock
            const float th = spin * 0.75f * auraC[j];
            const float cs = std::cos (th), sn = std::sin (th);
            const float rL = cs * aL + sn * aR, rR = cs * aR - sn * aL;
            if (stereo) { aura[0] = rL; aura[1] = rR; }
            else        aura[0] = 0.5f * (rL + rR);
        }

        for (int c = 0; c < numCh; ++c)
        {
            const float out = (sDryG * dryOut[c][j] + sWetG * wv[c] + aura[c]) * sOut;
            io[c][off + j] = out;
            check += out;
        }
    }

    for (int b = 0; b < DuckBands::kBands; ++b)
    {
        const float g = spectral ? duckAura.gain[b] : (guiDuckCount > 0 ? guiDuckSum / (float) guiDuckCount : 1.0f);
        guiDuck[b].store (g, std::memory_order_relaxed);
    }

    if (! std::isfinite (check))
    {
        reset();
        for (int c = 0; c < numCh; ++c)
            for (int j = 0; j < n; ++j)
                io[c][off + j] = 0.0f;
    }
}

} // namespace mavka
