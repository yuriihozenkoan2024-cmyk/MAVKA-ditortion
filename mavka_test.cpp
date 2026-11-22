// MAVKA offline test bench (no JUCE needed)
//   mavka_test list
//   mavka_test render <in.wav> <out.wav> <preset> [--bpm 95] [--sr] [--set id=value ...]
//   mavka_test renderall <in.wav> <outdir> [--bpm 95]
//   mavka_test latency | alias | stress | cpu | switch
#include "MavkaEngine.h"

#include <chrono>
#include <functional>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#if defined(__SSE__) || defined(__x86_64__)
 #include <xmmintrin.h>
 static void noDenormals() { _mm_setcsr (_mm_getcsr() | 0x8040); }
#else
 static void noDenormals() {}
#endif

using namespace mavka;

//==============================================================================
struct Audio { int sr = 48000; std::vector<std::vector<float>> ch; size_t frames() const { return ch.empty() ? 0 : ch[0].size(); } };

static bool readWav (const std::string& path, Audio& a)
{
    FILE* f = std::fopen (path.c_str(), "rb");
    if (! f) return false;
    std::vector<unsigned char> d;
    unsigned char buf[65536];
    size_t r;
    while ((r = std::fread (buf, 1, sizeof (buf), f)) > 0) d.insert (d.end(), buf, buf + r);
    std::fclose (f);
    auto u16 = [&] (size_t o) { return (unsigned) d[o] | ((unsigned) d[o + 1] << 8); };
    auto u32 = [&] (size_t o) { return u16 (o) | (u16 (o + 2) << 16); };
    if (d.size() < 12 || std::string ((char*) &d[0], 4) != "RIFF") return false;
    size_t pos = 12; int fmt = 0, nch = 0, bits = 0;
    while (pos + 8 <= d.size())
    {
        std::string id ((char*) &d[pos], 4);
        const size_t sz = u32 (pos + 4);
        const size_t body = pos + 8;
        if (id == "fmt ")
        {
            fmt = (int) u16 (body); nch = (int) u16 (body + 2); a.sr = (int) u32 (body + 4); bits = (int) u16 (body + 14);
            if (fmt == 0xFFFE) fmt = (int) u16 (body + 24);
        }
        else if (id == "data")
        {
            const int bps = bits / 8;
            const size_t n = std::min (sz, d.size() - body) / (size_t) (bps * nch);
            a.ch.assign ((size_t) nch, std::vector<float> (n));
            for (size_t i = 0; i < n; ++i)
                for (int c = 0; c < nch; ++c)
                {
                    const size_t o = body + (i * (size_t) nch + (size_t) c) * (size_t) bps;
                    float v = 0.0f;
                    if (fmt == 3 && bits == 32) { uint32_t x = u32 (o); std::memcpy (&v, &x, 4); }
                    else if (bits == 16) v = (float) (int16_t) u16 (o) / 32768.0f;
                    else if (bits == 24) { int32_t x = (int32_t) ((d[o] << 8) | (d[o + 1] << 16) | (d[o + 2] << 24)); v = (float) (x >> 8) / 8388608.0f; }
                    else if (bits == 32) v = (float) (int32_t) u32 (o) / 2147483648.0f;
                    a.ch[(size_t) c][i] = v;
                }
            return true;
        }
        pos = body + sz + (sz & 1);
    }
    return false;
}

static bool writeWav (const std::string& path, const Audio& a)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return false;
    const int nch = (int) a.ch.size();
    const uint32_t n = (uint32_t) a.frames();
    const uint32_t dataBytes = n * (uint32_t) nch * 3;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 ((uint16_t) nch); w32 ((uint32_t) a.sr); w32 ((uint32_t) a.sr * (uint32_t) nch * 3); w16 ((uint16_t) (nch * 3)); w16 (24);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
        for (int c = 0; c < nch; ++c)
        {
            const float v = clampf (a.ch[(size_t) c][i], -1.0f, 1.0f);
            const int32_t x = (int32_t) std::lrint (v * 8388607.0f);
            const unsigned char b[3] = { (unsigned char) (x & 255), (unsigned char) ((x >> 8) & 255), (unsigned char) ((x >> 16) & 255) };
            std::fwrite (b, 1, 3, f);
        }
    std::fclose (f);
    return true;
}

//==============================================================================
// Runs the engine over a whole buffer with host-like blocks and a playing transport.
static void runEngine (Engine& e, std::vector<std::vector<float>>& io, double sr, double bpm, int block = 512,
                       const std::function<void (size_t)>& perBlock = {})
{
    const size_t n = io[0].size();
    const int nch = (int) io.size();
    for (size_t pos = 0; pos < n; pos += (size_t) block)
    {
        const int len = (int) std::min ((size_t) block, n - pos);
        if (perBlock) perBlock (pos);
        e.setTransport (true, bpm, (double) pos / sr * bpm / 60.0);
        float* ptr[2] = { io[0].data() + pos, io[(size_t) std::min (1, nch - 1)].data() + pos };
        e.process (ptr, nch, len);
    }
}

static void linearParams (float* v)
{
    defaultValues (v);
    const char* zeros[] = { "drive", "grit", "melody", "motion", "ink", "slant", "ligature", "glass", "fold", "doppler",
                            "bias", "ghostDown", "ghostUp", "consist", "focus", "smooth", "preTilt", "tone", "warp",
                            "echo", "forest", "twin", "crush" };
    for (auto z : zeros) v[findParam (z)] = 0.0f;
    v[P_LAMP] = 1.0f; v[P_MIX] = 1.0f; v[P_LOWCUT] = 20.0f; v[P_HIGHCUT] = 20000.0f;
}


//==============================================================================
static int testLatency()
{
    const double sr = 48000.0;
    const size_t n = (size_t) (sr * 1.5);
    std::vector<float> in (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / sr;
        in[i] = (float) (0.001 * (std::sin (2 * kPiD * 100 * t) + std::sin (2 * kPiD * 1000 * t) + std::sin (2 * kPiD * 6000 * t)));
    }
    std::vector<std::vector<float>> outs;
    int lat = 0;
    for (int os = 1; os <= 5; ++os)
    {
        Engine e; e.prepare (sr, 1);
        float v[P_COUNT]; linearParams (v); v[P_OS] = (float) os; e.setParams (v);
        std::vector<std::vector<float>> io { in };
        runEngine (e, io, sr, 95.0);
        lat = e.getLatency();
        outs.push_back (io[0]);
    }
    // reported latency already includes the lead (lamp) pre-delay
    const int expected = lat;
    std::printf ("reported latency (all factors) = %d samples\n", lat);
    const size_t a = (size_t) (0.5 * sr), b = n - 2000;
    int fails = 0;
    for (int k = 0; k < 5; ++k)
    {
        // best integer lag against the input
        double best = -1e30; int bestLag = 0;
        for (int lag = expected - 20; lag <= expected + 20; ++lag)
        {
            double s = 0; for (size_t i = a; i < b; ++i) s += (double) outs[(size_t) k][i] * in[i - (size_t) lag];
            if (s > best) { best = s; bestLag = lag; }
        }
        double diff = 0, ref = 0;
        for (size_t i = a; i < b; ++i) { const double d = outs[(size_t) k][i] - outs[1][i]; diff += d * d; ref += (double) outs[1][i] * outs[1][i]; }
        const double rel = 10 * std::log10 (diff / ref + 1e-30);
        std::printf ("  %2dx: best lag %d, difference vs 2x: %.1f dB\n", 1 << k, bestLag, k == 1 ? -999.0 : rel);
        if (std::abs (bestLag - expected) > 1) ++fails;
        if (k >= 2 && rel > -60.0) ++fails;
    }
    std::printf (fails ? "LATENCY: FAIL\n" : "LATENCY: OK\n");
    return fails;
}

static void fftMag (const std::vector<double>& x, std::vector<double>& mag)
{
    const size_t n = x.size();
    std::vector<std::complex<double>> a (n);
    for (size_t i = 0; i < n; ++i) a[i] = x[i];
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * kPiD / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1);
            for (size_t k = 0; k < len / 2; ++k) { auto u = a[i + k], t = a[i + k + len / 2] * w; a[i + k] = u + t; a[i + k + len / 2] = u - t; w *= wl; }
        }
    }
    mag.resize (n / 2);
    for (size_t i = 0; i < n / 2; ++i) mag[i] = std::norm (a[i]);
}

static int testAlias()
{
    const double sr = 48000.0, f0 = 3150.0;
    const size_t N = 65536;
    std::printf ("aliasing test: %.0f Hz sine, heavy settings (drive 100%%, grit 80%%, fold 60%%) at %.0f Hz\n", f0, sr);
    double res[7] = {};
    for (int os = 0; os <= 5; ++os)
    {
        Engine e; e.prepare (sr, 1);
        float v[P_COUNT]; linearParams (v);
        v[P_DRIVE] = 1.0f; v[P_GRIT] = 0.8f; v[P_FOLD] = 0.6f; v[P_GLASS] = 0.5f; v[P_CONSIST] = 1.0f; v[P_OS] = (float) os;
        e.setParams (v);
        const size_t n = N + 24000;
        std::vector<std::vector<float>> io (1, std::vector<float> (n));
        for (size_t i = 0; i < n; ++i) io[0][i] = (float) (0.25 * std::sin (2 * kPiD * f0 * (double) i / sr));
        runEngine (e, io, sr, 95.0);
        std::vector<double> x (N);
        for (size_t i = 0; i < N; ++i)
        {
            const double w = 0.35875 - 0.48829 * std::cos (2 * kPiD * i / (N - 1)) + 0.14128 * std::cos (4 * kPiD * i / (N - 1)) - 0.01168 * std::cos (6 * kPiD * i / (N - 1));
            x[i] = io[0][i + 24000] * w;
        }
        std::vector<double> mag; fftMag (x, mag);
        double harm = 0, alias = 0;
        const double binHz = sr / (double) N;
        for (size_t k = 3; k < mag.size(); ++k)
        {
            const double f = (double) k * binHz;
            if (f > 20000.0) break;
            const double h = f / f0, dist = std::fabs (h - std::round (h)) * f0 / binHz;
            if (std::round (h) >= 1 && dist <= 6) harm += mag[k]; else alias += mag[k];
        }
        res[os] = 10 * std::log10 (alias / harm + 1e-30);
        std::printf ("  %-5s (running %2dx): non-harmonic energy %.1f dB below the harmonics\n", kOsChoices[os], e.getActiveFactor(), -res[os]);
    }
    const bool ok = res[0] < -60.0 && res[4] < -60.0 && res[5] < res[3];
    std::printf (ok ? "ALIAS: OK\n" : "ALIAS: CHECK\n");
    return ok ? 0 : 1;
}

static int testStress()
{
    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> U (0.0f, 1.0f);
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    int fails = 0;
    for (double sr : rates)
        for (int nch = 1; nch <= 2; ++nch)
        {
            Engine e; e.prepare (sr, nch);
            const size_t n = (size_t) (sr * 12.0);
            std::vector<std::vector<float>> io ((size_t) nch, std::vector<float> (n));
            for (size_t i = 0; i < n; ++i)
            {
                const double t = (double) i / sr;
                const int seg = (int) (t * 4.0) % 8;
                float s = 0.0f;
                switch (seg)
                {
                    case 0: s = 0.0f; break;
                    case 1: s = (U (rng) * 2.0f - 1.0f); break;
                    case 2: s = (float) std::sin (2 * kPiD * (80 + 6000 * std::fmod (t, 1.0)) * t); break;
                    case 3: s = (std::fmod (t * 110.0, 1.0) < 0.5 ? 1.0f : -1.0f); break;
                    case 4: s = (i % 4800 == 0) ? 1.0f : 0.0f; break;
                    case 5: s = 1.0e-30f; break;
                    case 6: s = 0.3f * (float) std::sin (2 * kPiD * 140 * t) * (U (rng) > 0.5f ? 1.0f : 0.2f); break;
                    default: s = 1.0f; break;   // DC
                }
                for (int c = 0; c < nch; ++c) io[(size_t) c][i] = s * (c ? 0.8f : 1.0f);
            }
            float v[P_COUNT];
            const ParamDef* d = paramDefs();
            runEngine (e, io, sr, 140.0, 333, [&] (size_t pos)
            {
                if (pos % (size_t) (sr * 0.25) < 333)
                    for (int p = 0; p < P_COUNT; ++p)
                    {
                        float x = d[p].minV + U (rng) * (d[p].maxV - d[p].minV);
                        if (d[p].type != PType::Float) x = std::round (x);
                        v[p] = x;
                    }
                v[P_OUTPUT] = 0.0f;
                e.setParams (v);
            });
            float peak = 0.0f; bool finite = true;
            for (auto& c : io) for (float x : c) { if (! std::isfinite (x)) finite = false; peak = std::max (peak, std::fabs (x)); }
            std::printf ("  %6.0f Hz, %d ch: finite=%s peak=%.2f\n", sr, nch, finite ? "yes" : "NO", peak);
            if (! finite || peak > 12.0f) ++fails;
        }
    std::printf (fails ? "STRESS: FAIL\n" : "STRESS: OK\n");
    return fails;
}

static int testCpu()
{
    const double sr = 48000.0;
    const size_t n = (size_t) (sr * 20.0);
    std::mt19937 rng (7);
    std::normal_distribution<float> G (0.0f, 0.1f);
    std::vector<float> src (n);
    for (size_t i = 0; i < n; ++i) src[i] = 0.3f * (float) std::sin (2 * kPiD * 150 * (double) i / sr) + G (rng);
    const char* names[] = { "Mavka (Auto)", "Torn Tape (Auto)", "Mavka forced 16x" };
    for (int t = 0; t < 3; ++t)
    {
        Engine e; e.prepare (sr, 2);
        float v[P_COUNT]; presetValues (t == 1 ? 7 : 0, v);
        if (t == 2) v[P_OS] = 5.0f;
        e.setParams (v);
        std::vector<std::vector<float>> io { src, src };
        const auto t0 = std::chrono::steady_clock::now();
        runEngine (e, io, sr, 95.0, 256);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  %-18s running %2dx: %.2f%% of one core (stereo, 48 kHz)\n", names[t], e.getActiveFactor(), 100.0 * secs / 20.0);
    }
    return 0;
}

static int testSwitch()
{
    const double sr = 48000.0;
    const size_t n = (size_t) (sr * 3.0);
    Engine e; e.prepare (sr, 1);
    float v[P_COUNT]; presetValues (0, v); v[P_MOTION] = 0.0f; v[P_INK] = 0.0f; v[P_SLANT] = 0.0f; v[P_LIGATURE] = 0.0f; v[P_MELODY] = 0.0f;
    v[P_OS] = 2.0f;  // 2x
    e.setParams (v);
    std::vector<std::vector<float>> io (1, std::vector<float> (n));
    for (size_t i = 0; i < n; ++i) io[0][i] = 0.2f * (float) std::sin (2 * kPiD * 440 * (double) i / sr);
    runEngine (e, io, sr, 95.0, 256, [&] (size_t pos)
    {
        if (pos >= (size_t) sr && v[P_OS] != 4.0f) { v[P_OS] = 4.0f; e.setParams (v); }   // -> 8x at t = 1 s
    });
    auto maxD2 = [&] (size_t a, size_t b)
    {
        double m = 0; for (size_t i = a + 2; i < b; ++i) m = std::max (m, (double) std::fabs (io[0][i] - 2 * io[0][i - 1] + io[0][i - 2])); return m;
    };
    const double steady = maxD2 ((size_t) (0.5 * sr), (size_t) (0.95 * sr));
    const double around = maxD2 ((size_t) (0.98 * sr), (size_t) (1.2 * sr));
    std::printf ("  factor switch 2x -> 8x: max 2nd difference steady %.5f, during switch %.5f (now running %dx)\n", steady, around, e.getActiveFactor());
    const bool ok = around < steady * 1.5;
    std::printf (ok ? "SWITCH: OK (no click)\n" : "SWITCH: CHECK\n");
    return ok ? 0 : 1;
}


static int testDuck()
{
    const float fs = 48000.0f;
    const size_t n = (size_t) (fs * 4.0f);
    std::mt19937 rng (11);
    std::normal_distribution<float> G (0.0f, 0.1f);
    std::vector<float> noise (n), key (n), outQuiet (n), outSung (n);
    // FX = an "echo" of the voice in the same band (louder than voice - 6 dB) + broadband noise
    for (size_t i = 0; i < n; ++i)
    {
        key[i] = 0.25f * (float) std::sin (2 * kPiD * 1000.0 * (double) i / fs);
        noise[i] = G (rng) + 0.4f * (float) std::sin (2 * kPiD * 1010.0 * (double) i / fs + 1.0);
    }
    int fails = 0;
    {   // silent voice: the filterbank must give back the exact input
        DuckKey k; DuckFx d; k.prepare (fs); d.prepare (fs);
        double err = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            k.process (0.0f, 0.0f);
            float L = noise[i], R = noise[i];
            d.process (k, L, R, 0.5f, 0.125f);
            err = std::max (err, (double) std::fabs (L - noise[i]));
        }
        std::printf ("  no voice: max deviation from the input %.2e\n", err);
        if (err > 1.0e-4) ++fails;
    }
    {   // a voice at 1 kHz: the FX must step aside around 1 kHz only
        DuckKey k; DuckFx d; k.prepare (fs); d.prepare (fs);
        for (size_t i = 0; i < n; ++i)
        {
            k.process (key[i], 1.0f);
            float L = noise[i], R = noise[i];
            d.process (k, L, R, 0.5f, 0.125f);
            outSung[i] = L;
        }
        auto bandDb = [&] (const std::vector<float>& x, double f0, double f1)
        {
            const size_t N = 65536; std::vector<double> w (N);
            for (size_t i = 0; i < N; ++i) w[i] = x[n - N + i] * (0.5 - 0.5 * std::cos (2 * kPiD * i / (N - 1)));
            std::vector<double> mag; fftMag (w, mag);
            double e = 0; for (size_t b = (size_t) (f0 / fs * N); b < (size_t) (f1 / fs * N); ++b) e += mag[b];
            return 10 * std::log10 (e + 1e-30);
        };
        const double d1k = bandDb (outSung, 850, 1180) - bandDb (noise, 850, 1180);
        const double d200 = bandDb (outSung, 150, 250) - bandDb (noise, 150, 250);
        const double d6k = bandDb (outSung, 5000, 7000) - bandDb (noise, 5000, 7000);
        std::printf ("  voice at 1 kHz, echo at 1 kHz + noise: FX change around 1 kHz %.1f dB, at 200 Hz %.1f dB, at 6 kHz %.1f dB\n", d1k, d200, d6k);
        if (d1k > -8.0 || std::fabs (d200) > 1.5 || std::fabs (d6k) > 1.5) ++fails;
    }
    std::printf (fails ? "DUCK: FAIL\n" : "DUCK: OK\n");
    return fails;
}


static int duckStats (const std::string& path, double bpm)
{
    Audio a;
    if (! readWav (path, a)) return 1;
    if (a.ch.size() == 1) a.ch.push_back (a.ch[0]);
    float v[P_COUNT]; presetValues (0, v); v[P_ECHO] = 0.6f; v[P_FOREST] = 0.4f;
    Engine e; e.prepare (a.sr, 2); e.setParams (v);
    double sum[16] = {}; double minG[16]; for (double& m : minG) m = 1.0; int active = 0, total = 0;
    const int block = 512;
    for (size_t pos = 0; pos + block < a.frames(); pos += block)
    {
        e.setTransport (true, bpm, (double) pos / a.sr * bpm / 60.0);
        float* ptr[2] = { a.ch[0].data() + pos, a.ch[1].data() + pos };
        e.process (ptr, 2, block);
        ++total;
        if (e.getRelativeDb() > -12.0f)   // the voice is singing
        {
            ++active;
            for (int b = 0; b < 16; ++b) { sum[b] += e.getDuckGain (b); minG[b] = std::min (minG[b], (double) e.getDuckGain (b)); }
        }
    }
    std::printf ("blocks with voice: %d of %d\nband  centre   avg cut while singing   deepest cut\n", active, total);
    for (int b = 0; b < 16; ++b)
        std::printf ("%4d %7.0f Hz   %6.1f dB              %6.1f dB\n", b, DuckBands::centreHz (b),
                     20 * std::log10 (sum[b] / std::max (1, active) + 1e-9), 20 * std::log10 (minG[b] + 1e-9));
    return 0;
}


static int testCrush()
{
    const double sr = 48000.0, f0 = 196.0;   // G3
    const size_t N = 65536;
    std::printf ("crusher test: voice-like tone at %.0f Hz, Crush 80%%\n", f0);
    double res[3] = {};
    const char* names[3] = { "Harmonic", "Digital", "off" };
    for (int mode = 0; mode < 3; ++mode)
    {
        Engine e; e.prepare (sr, 1);
        float v[P_COUNT]; linearParams (v);
        v[P_CRUSH] = mode == 2 ? 0.0f : 0.8f; v[P_CRUSHMODE] = (float) std::min (mode, 1); v[P_OS] = 4.0f;
        e.setParams (v);
        const size_t n = N + 48000;
        std::vector<std::vector<float>> io (1, std::vector<float> (n));
        for (size_t i = 0; i < n; ++i)
        {
            double x = 0; for (int h = 1; h <= 8; ++h) x += std::sin (2 * kPiD * f0 * h * (double) i / sr + 0.3 * h) / h;
            io[0][i] = (float) (0.12 * x);
        }
        runEngine (e, io, sr, 95.0);
        std::vector<double> x (N);
        for (size_t i = 0; i < N; ++i)
            x[i] = io[0][i + 48000] * (0.35875 - 0.48829 * std::cos (2 * kPiD * i / (N - 1)) + 0.14128 * std::cos (4 * kPiD * i / (N - 1)) - 0.01168 * std::cos (6 * kPiD * i / (N - 1)));
        std::vector<double> mag; fftMag (x, mag);
        double harm = 0, other = 0;
        const double binHz = sr / (double) N;
        for (size_t k = 30; k < mag.size(); ++k)
        {
            const double f = (double) k * binHz;
            if (f > 20000.0) break;
            const double h = f / f0, dist = std::fabs (h - std::round (h)) * f0 / binHz;
            if (std::round (h) >= 1 && dist <= 8) harm += mag[k]; else other += mag[k];
        }
        res[mode] = 10 * std::log10 (other / harm + 1e-30);
        std::printf ("  %-9s non-harmonic energy %.1f dB below the harmonics\n", names[mode], -res[mode]);
    }
    const bool ok = res[0] < res[1] - 15.0;
    std::printf (ok ? "CRUSH: OK (harmonic crush stays in tune)\n" : "CRUSH: CHECK\n");
    return ok ? 0 : 1;
}

//==============================================================================
static int findPreset (const std::string& s)
{
    int count = 0; const Preset* p = factoryPresets (count);
    for (int i = 0; i < count; ++i) if (s == p[i].name) return i;
    return std::atoi (s.c_str());
}

static int render (const std::string& inPath, const std::string& outPath, int preset, double bpm, const std::vector<std::string>& sets)
{
    Audio a;
    if (! readWav (inPath, a)) { std::fprintf (stderr, "cannot read %s\n", inPath.c_str()); return 1; }
    if (a.ch.size() == 1) a.ch.push_back (a.ch[0]);
    float v[P_COUNT]; presetValues (preset, v);
    for (auto& s : sets)
    {
        const auto eq = s.find ('=');
        const int p = findParam (s.substr (0, eq).c_str());
        if (p >= 0) v[p] = (float) std::atof (s.substr (eq + 1).c_str());
    }
    Engine e; e.prepare (a.sr, 2); e.setNonRealtime (true); e.setParams (v);
    const int lat = e.getLatency();
    for (auto& c : a.ch) c.insert (c.end(), (size_t) lat + (size_t) a.sr * 2, 0.0f);   // room for the tails
    runEngine (e, a.ch, a.sr, bpm, 512);
    for (auto& c : a.ch) c.erase (c.begin(), c.begin() + lat);                       // latency compensation
    writeWav (outPath, a);
    float peak = 0; for (auto& c : a.ch) for (float x : c) peak = std::max (peak, std::fabs (x));
    int count = 0; const Preset* pl = factoryPresets (count);
    std::printf ("rendered %-14s -> %s (peak %.2f dBFS, ran at %dx)\n", pl[preset].name, outPath.c_str(), 20 * std::log10 (peak + 1e-9), e.getActiveFactor());
    return 0;
}

int main (int argc, char** argv)
{
    noDenormals();
    if (argc < 2) { std::printf ("usage: see header\n"); return 1; }
    const std::string cmd = argv[1];
    if (cmd == "list") { int c = 0; const Preset* p = factoryPresets (c); for (int i = 0; i < c; ++i) std::printf ("%d %s\n", i, p[i].name); return 0; }
    if (cmd == "latency") return testLatency();
    if (cmd == "alias")   return testAlias();
    if (cmd == "stress")  return testStress();
    if (cmd == "cpu")     return testCpu();
    if (cmd == "switch")  return testSwitch();
    if (cmd == "duck")    return testDuck();
    if (cmd == "crush")   return testCrush();
    double bpm = 95.0; std::vector<std::string> sets;
    std::vector<std::string> pos;
    for (int i = 2; i < argc; ++i)
    {
        const std::string s = argv[i];
        if (s == "--bpm" && i + 1 < argc) bpm = std::atof (argv[++i]);
        else if (s == "--set" && i + 1 < argc) sets.push_back (argv[++i]);
        else pos.push_back (s);
    }
    if (cmd == "duckstats" && pos.size() >= 1) return duckStats (pos[0], bpm);
    if (cmd == "render" && pos.size() >= 3) return render (pos[0], pos[1], findPreset (pos[2]), bpm, sets);
    if (cmd == "renderall" && pos.size() >= 2)
    {
        int c = 0; const Preset* p = factoryPresets (c);
        for (int i = 0; i < c; ++i)
        {
            std::string name = p[i].name; for (auto& ch : name) if (ch == ' ') ch = '_';
            char buf[512]; std::snprintf (buf, sizeof (buf), "%s/%02d_%s.wav", pos[1].c_str(), i + 1, name.c_str());
            render (pos[0], buf, i, bpm, sets);
        }
        return 0;
    }
    std::printf ("unknown command\n");
    return 1;
}
