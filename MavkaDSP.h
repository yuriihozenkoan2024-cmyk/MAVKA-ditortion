// MAVKA - DSP building blocks (no JUCE dependency)
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mavka
{

constexpr float  kPi    = 3.14159265358979323846f;
constexpr float  kTwoPi = 6.28318530717958647692f;
constexpr double kPiD   = 3.14159265358979323846;

inline float dbToGain (float db) noexcept   { return std::pow (10.0f, 0.05f * db); }
inline float gainToDb (float g) noexcept    { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clampf (float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerpf (float a, float b, float t) noexcept    { return a + (b - a) * t; }

inline float smoothstep (float e0, float e1, float x) noexcept
{
    const float t = clampf ((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float timeCoef (float ms, float fs) noexcept
{
    return ms <= 0.0f ? 0.0f : std::exp (-1.0f / (0.001f * ms * fs));
}

// tanh approximation: exact slope at 0, reaches +-1 with zero slope at |x| = 3
inline float fastTanh (float x) noexcept
{
    if (x >= 3.0f)  return 1.0f;
    if (x <= -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// sine approximation for any argument (abs error < 1.1e-3)
inline float fastSin (float x) noexcept
{
    x -= kTwoPi * std::floor ((x + kPi) * (1.0f / kTwoPi));
    constexpr float B = 4.0f / kPi;
    constexpr float C = -4.0f / (kPi * kPi);
    const float y = B * x + C * x * std::fabs (x);
    return 0.225f * (y * std::fabs (y) - y) + y;
}

//==============================================================================
struct OnePole
{
    float a = 0.0f, z = 0.0f;

    void setCutoff (float hz, float fs) noexcept { a = std::exp (-kTwoPi * std::min (hz, 0.45f * fs) / fs); }
    void setTime (float ms, float fs) noexcept   { a = timeCoef (ms, fs); }
    inline float lp (float x) noexcept           { z = x + a * (z - x); return z; }
    inline float hp (float x) noexcept           { return x - lp (x); }
    void reset (float v = 0.0f) noexcept         { z = v; }
};

//==============================================================================
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0f; }

    void set (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }

    static double omega (double fc, double fs) noexcept
    {
        fc = std::min (std::max (fc, 1.0), 0.48 * fs);
        return 2.0 * kPiD * fc / fs;
    }

    void lowpass (double fc, double q, double fs) noexcept
    {
        const double w = omega (fc, fs), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
    }

    void highpass (double fc, double q, double fs) noexcept
    {
        const double w = omega (fc, fs), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
    }

    void lowShelf (double fc, double db, double fs) noexcept
    {
        const double A = std::pow (10.0, db / 40.0), w = omega (fc, fs), c = std::cos (w);
        const double al = std::sin (w) * 0.5 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }

    void highShelf (double fc, double db, double fs) noexcept
    {
        const double A = std::pow (10.0, db / 40.0), w = omega (fc, fs), c = std::cos (w);
        const double al = std::sin (w) * 0.5 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

//==============================================================================
struct Envelope
{
    float atk = 0.0f, rel = 0.0f, env = 0.0f;

    void set (float atkMs, float relMs, float fs) noexcept { atk = timeCoef (atkMs, fs); rel = timeCoef (relMs, fs); }
    inline float process (float x) noexcept
    {
        const float a = std::fabs (x);
        env = a + (a > env ? atk : rel) * (env - a);
        return env;
    }
    void reset() noexcept { env = 0.0f; }
};

//==============================================================================
// Power-of-two ring buffer. tap(0) is the newest sample.
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int n = 16;
        while (n < maxDelaySamples + 8)
            n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
        maxRead = (float) (n - 4);
    }

    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

    inline void push (float x) noexcept { w = (w + 1) & mask; buf[(size_t) w] = x; }
    inline float tap (int d) const noexcept { return buf[(size_t) ((w - d) & mask)]; }

    // cubic Hermite read, d in samples (clamped to [1, size-4])
    inline float read (float d) const noexcept
    {
        d = clampf (d, 1.0f, maxRead);
        const int di = (int) d;
        const float f = d - (float) di;
        const float xm1 = tap (di - 1), x0 = tap (di), x1 = tap (di + 1), x2 = tap (di + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    int size() const noexcept { return mask + 1; }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
    float maxRead = 1.0f;
};

//==============================================================================
// Linear-phase half-band FIR (Kaiser window), polyphase up/down by 2.
// Up + down latency = M samples at the lower rate (always an integer).
class HalfbandStage
{
public:
    void design (int numTaps, double attenuationDb)
    {
        M = (numTaps - 1) / 2;          // numTaps = 4K + 3  ->  M odd
        K = (M - 1) / 2;
        const double beta = 0.1102 * (attenuationDb - 8.7);
        std::vector<double> h ((size_t) M + 1);
        double sum = 0.0;
        for (int i = 0; i <= M; ++i)
        {
            const int m = 2 * i - M;    // odd offsets -M..M
            const double sinc = std::sin (kPiD * m * 0.5) / (kPiD * m);
            const double r = (double) m / (double) M;
            const double win = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
            h[(size_t) i] = sinc * win;
            sum += h[(size_t) i];
        }
        c.resize ((size_t) M + 1);
        for (int i = 0; i <= M; ++i)
            c[(size_t) i] = (float) (h[(size_t) i] * 0.5 / sum);   // odd taps sum to 0.5, centre is 0.5

        L  = M + 1;
        Lo = K + 2;
        upH.assign ((size_t) (2 * L), 0.0f);
        dnE.assign ((size_t) (2 * L), 0.0f);
        dnO.assign ((size_t) (2 * Lo), 0.0f);
        reset();
    }

    void reset() noexcept
    {
        std::fill (upH.begin(), upH.end(), 0.0f);
        std::fill (dnE.begin(), dnE.end(), 0.0f);
        std::fill (dnO.begin(), dnO.end(), 0.0f);
        pu = pe = po = 0;
    }

    int latency() const noexcept { return M; }

    inline void up (float x, float& y0, float& y1) noexcept
    {
        pu = (pu == 0 ? L - 1 : pu - 1);
        upH[(size_t) pu] = x;
        upH[(size_t) (pu + L)] = x;
        const float* hx = &upH[(size_t) pu];
        float acc = 0.0f;
        for (int i = 0; i < L; ++i)
            acc += c[(size_t) i] * hx[i];
        y0 = 2.0f * acc;
        y1 = hx[K];
    }

    inline float down (float v0, float v1) noexcept
    {
        pe = (pe == 0 ? L - 1 : pe - 1);
        dnE[(size_t) pe] = v0;
        dnE[(size_t) (pe + L)] = v0;
        po = (po == 0 ? Lo - 1 : po - 1);
        dnO[(size_t) po] = v1;
        dnO[(size_t) (po + Lo)] = v1;
        const float* e = &dnE[(size_t) pe];
        float acc = 0.0f;
        for (int i = 0; i < L; ++i)
            acc += c[(size_t) i] * e[i];
        return acc + 0.5f * dnO[(size_t) (po + K + 1)];
    }

private:
    static double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 64; ++k)
        {
            const double t = x / (2.0 * k);
            term *= t * t;
            sum += term;
            if (term < 1.0e-14 * sum)
                break;
        }
        return sum;
    }

    int M = 1, K = 0, L = 2, Lo = 2;
    std::vector<float> c, upH, dnE, dnO;
    int pu = 0, pe = 0, po = 0;
};

//==============================================================================
// 2^S oversampler made of cascaded half-bands. A small delay at the top rate makes
// the total latency an integer number of base-rate samples for every factor.
class Oversampler
{
public:
    void prepare (int numStages, int maxBlock)
    {
        S = numStages;
        stages.assign ((size_t) S, HalfbandStage());
        static const int taps[4] = { 83, 27, 19, 19 };
        for (int s = 0; s < S; ++s)
            stages[(size_t) s].design (taps[s], 100.0);

        const int F = 1 << S;
        bufA.assign ((size_t) (maxBlock * F), 0.0f);
        bufB.assign ((size_t) (maxBlock * F), 0.0f);

        long T = 0;
        for (int s = 0; s < S; ++s)
            T += (long) stages[(size_t) s].latency() * (long) (F >> s);
        pad = (int) ((F - (T % F)) % F);
        lat = (int) ((T + pad) / F);
        padLine.prepare (pad + 4);
    }

    void reset() noexcept
    {
        for (auto& s : stages) s.reset();
        padLine.reset();
    }

    int latency() const noexcept { return lat; }
    int factor() const noexcept  { return 1 << S; }

    // returns the top-rate buffer (n << S samples)
    float* up (const float* in, int n) noexcept
    {
        if (S == 0)
        {
            std::copy (in, in + n, bufA.data());
            return bufA.data();
        }
        const float* src = in;
        float* dst = bufA.data();
        float* other = bufB.data();
        int len = n;
        for (int s = 0; s < S; ++s)
        {
            auto& st = stages[(size_t) s];
            for (int i = 0; i < len; ++i)
                st.up (src[i], dst[2 * i], dst[2 * i + 1]);
            src = dst;
            len *= 2;
            std::swap (dst, other);
        }
        return const_cast<float*> (src);
    }

    // top must be the pointer returned by up(); writes n base-rate samples to out
    void down (float* top, float* out, int n) noexcept
    {
        if (S == 0)
        {
            std::copy (top, top + n, out);
            return;
        }
        int len = n << S;
        if (pad > 0)
            for (int i = 0; i < len; ++i)
            {
                padLine.push (top[i]);
                top[i] = padLine.tap (pad);
            }
        float* src = top;
        for (int s = S - 1; s >= 0; --s)
        {
            auto& st = stages[(size_t) s];
            const int outLen = len / 2;
            float* dst = (s == 0) ? out : (src == bufA.data() ? bufB.data() : bufA.data());
            for (int i = 0; i < outLen; ++i)
                dst[i] = st.down (src[2 * i], src[2 * i + 1]);
            src = dst;
            len = outLen;
        }
    }

private:
    int S = 0, pad = 0, lat = 0;
    std::vector<HalfbandStage> stages;
    std::vector<float> bufA, bufB;
    DelayLine padLine;
};

} // namespace mavka
