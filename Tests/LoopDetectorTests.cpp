// =============================================================================
// LoopDetectorTests.cpp
//
// Standalone console runner that exercises lf::LoopDetector via JUCE's
// UnitTest framework. Build target: LoopDetectorTests (see CMakeLists.txt).
// =============================================================================

#include <juce_core/juce_core.h>

#include "LoopDetector.h"
#include "PitchDetector.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace
{
    // -- Helpers --------------------------------------------------------------
    constexpr double pi = 3.14159265358979323846;

    std::vector<float> makeSine (double freqHz, double durationSec, double sampleRate,
                                 float amp = 0.7f)
    {
        const int n = static_cast<int> (std::lround (durationSec * sampleRate));
        std::vector<float> out (static_cast<size_t> (n), 0.0f);
        for (int i = 0; i < n; ++i)
            out[(size_t) i] = amp * static_cast<float> (
                std::sin (2.0 * pi * freqHz * static_cast<double> (i) / sampleRate));
        return out;
    }

    float rms (const float* p, int n)
    {
        if (n <= 0) return 0.0f;
        double s = 0.0;
        for (int i = 0; i < n; ++i) s += static_cast<double> (p[i]) * p[i];
        return static_cast<float> (std::sqrt (s / n));
    }
}

// =============================================================================
// Test 1 — synthetic sine wave produces a near-perfect loop.
// =============================================================================
class SineWaveTest  : public juce::UnitTest
{
public:
    SineWaveTest() : juce::UnitTest ("LoopDetector — Sine Wave") {}

    void runTest() override
    {
        beginTest ("440Hz sine wave produces at least one loop with score > 0.99");

        constexpr double sr = 44100.0;
        const auto sine = makeSine (440.0, 1.0, sr);

        lf::LoopDetector det;
        lf::LoopDetector::Settings s;
        s.sampleRate     = static_cast<float> (sr);
        s.minLoopMs      = 10.0f;
        s.maxLoopMs      = 200.0f;
        s.scoreThreshold = 0.95f;

        const auto regions = det.analyze (sine.data(), (int) sine.size(), s);

        expect (! regions.empty(), "Expected at least one loop region");
        if (! regions.empty())
        {
            expectGreaterThan (regions.front().score, 0.99f,
                               "Top region should have near-perfect score for pure sine");
            // Loop length should correspond to an integer number of cycles.
            expectGreaterThan (regions.front().lengthSamples(), 32);
        }
    }
};

// =============================================================================
// Test 2 — silence: no crash, no regions returned.
// =============================================================================
class SilenceTest  : public juce::UnitTest
{
public:
    SilenceTest() : juce::UnitTest ("LoopDetector — Silence") {}

    void runTest() override
    {
        beginTest ("Buffer of zeros yields no regions and no crash");

        const std::vector<float> zeros (44100, 0.0f);
        lf::LoopDetector det;
        lf::LoopDetector::Settings s;
        s.sampleRate = 44100.0f;
        const auto regions = det.analyze (zeros.data(), (int) zeros.size(), s);
        expect (regions.empty(), "Silence must produce zero regions");
    }
};

// =============================================================================
// Test 3 — short file: gracefully handle below-minimum length.
// =============================================================================
class ShortFileTest  : public juce::UnitTest
{
public:
    ShortFileTest() : juce::UnitTest ("LoopDetector — Short File") {}

    void runTest() override
    {
        beginTest ("20 ms of audio produces no regions, no crash");

        const auto tiny = makeSine (440.0, 0.020, 44100.0);
        lf::LoopDetector det;
        lf::LoopDetector::Settings s;
        s.sampleRate = 44100.0f;
        const auto regions = det.analyze (tiny.data(), (int) tiny.size(), s);
        expect (regions.empty(), "20ms file is shorter than the minimum loop length");
    }
};

// =============================================================================
// Test 4 — crossfade reduces the audible click at the splice.
// =============================================================================
class CrossfadeTest  : public juce::UnitTest
{
public:
    CrossfadeTest() : juce::UnitTest ("LoopDetector — Crossfade") {}

    void runTest() override
    {
        beginTest ("Crossfade tail makes splice continuous (small wrap delta)");

        constexpr double sr = 44100.0;

        // 30 Hz sine — a realistic 808 sub-bass case. One period is ~735
        // samples, so the 2 ms (88 sample) crossfade is ~6% of one period,
        // which is the regime where the linear blend actually helps.
        constexpr double freq = 30.0;
        const auto sig = makeSine (freq, 1.0, sr);

        // Deliberately misaligned boundaries so the *original* splice has a
        // big discontinuity (peak → trough).
        lf::LoopRegion r;
        r.startSample = static_cast<int> (std::lround (sr / (4.0 * freq)));            // sin = +1
        r.endSample   = r.startSample
                      + static_cast<int> (std::lround (sr / (2.0 * freq))) + 2;       // ~half period later (~trough)
        r.score        = 0.80f;
        r.hasCrossfade = true;

        const int K = static_cast<int> (std::lround (0.002 * sr));                    // 2 ms
        expectGreaterThan (r.lengthSamples(), K * 2, "Loop must be long enough for crossfade");

        // Original click at the splice = | s[E-1] − s[S] |.
        const float clickBefore = std::abs (sig[(size_t) (r.endSample - 1)]
                                            - sig[(size_t) r.startSample]);

        const auto tail = lf::LoopDetector::buildCrossfadeTail (
            sig.data(), (int) sig.size(), r, K);

        expectEquals ((int) tail.size(), K, "Tail buffer has the requested length");

        // The PlaybackEngine, after applying this tail, advances the playhead
        // by K when wrapping — so the wrap discontinuity is between the last
        // crossfaded sample and sig[S + K], i.e. one normal sample step in the
        // smooth source signal. We measure that here.
        const float clickAfter = std::abs (tail.back() - sig[(size_t) (r.startSample + K)]);

        logMessage ("click before = " + juce::String (clickBefore, 4)
                  + ", after  = " + juce::String (clickAfter,  4));
        expect (clickAfter < clickBefore * 0.5f,
                "Crossfade should at least halve the splice discontinuity");

        // Sanity: blended tail RMS should remain in the same ballpark as the
        // original tail RMS (no level explosion).
        const float origRms = rms (sig.data() + r.endSample - K, K);
        const float xfRms   = rms (tail.data(), K);
        expect (xfRms > 0.0f && std::abs (xfRms - origRms) / origRms < 1.0f,
                "Crossfaded tail RMS should stay near the original tail RMS");
    }
};

// =============================================================================
// Test 5 — user-defined search range restricts where loops are found.
// =============================================================================
class SearchRangeTest  : public juce::UnitTest
{
public:
    SearchRangeTest() : juce::UnitTest ("LoopDetector — Search Range") {}

    void runTest() override
    {
        constexpr double sr = 44100.0;

        // First half: loud sine (loopable). Second half: silence.
        auto sig = makeSine (110.0, 1.0, sr);
        const int half = (int) sig.size() / 2;
        std::fill (sig.begin() + half, sig.end(), 0.0f);

        lf::LoopDetector det;
        lf::LoopDetector::Settings s;
        s.sampleRate     = static_cast<float> (sr);
        s.minLoopMs      = 10.0f;
        s.maxLoopMs      = 200.0f;
        s.scoreThreshold = 0.9f;

        beginTest ("Range over the loud first half finds loops inside the range");
        s.searchStartSample = 0;
        s.searchEndSample   = half;
        const auto inRange = det.analyze (sig.data(), (int) sig.size(), s);
        expect (! inRange.empty(), "Expected loops inside the highlighted range");
        for (const auto& r : inRange)
        {
            expectGreaterOrEqual (r.startSample, s.searchStartSample);
            expectLessOrEqual    (r.endSample,   s.searchEndSample);
        }

        beginTest ("Range over the silent second half finds nothing");
        s.searchStartSample = half;
        s.searchEndSample   = (int) sig.size();
        const auto silent = det.analyze (sig.data(), (int) sig.size(), s);
        expect (silent.empty(), "Silent range must produce zero regions");

        beginTest ("Range allows loops earlier than the default 5% dead zone");
        s.searchStartSample = 0;
        s.searchEndSample   = (int) (sr * 0.2);   // first 200 ms only
        const auto early = det.analyze (sig.data(), (int) sig.size(), s);
        expect (! early.empty(), "Expected loops in the first 200 ms");
        if (! early.empty())
            expectLessThan (early.front().endSample, (int) (sr * 0.2) + 1);
    }
};

// =============================================================================
// Test 6 — pitch detection finds the key of the sample.
// =============================================================================
class PitchDetectionTest  : public juce::UnitTest
{
public:
    PitchDetectionTest() : juce::UnitTest ("PitchDetector — Key Detection") {}

    void runTest() override
    {
        constexpr double sr = 44100.0;
        lf::PitchDetector::Settings s;
        s.sampleRate = (float) sr;

        beginTest ("110 Hz sine detects as A2 (MIDI 45)");
        {
            const auto sig = makeSine (110.0, 1.0, sr);
            const auto r = lf::PitchDetector::detect (sig.data(), (int) sig.size(), s);
            expect (r.isValid(), "Expected a pitch");
            expectWithinAbsoluteError (r.frequencyHz, 110.0f, 1.0f);
            expectEquals (r.nearestMidiNote(), 45);
            expect (std::abs (r.centsOffset()) < 15.0f, "Should be close to in tune");
        }

        beginTest ("Decaying saturated 41.2 Hz sub (808-style) detects as E1 (MIDI 28)");
        {
            constexpr double freq = 41.2034;   // E1
            const int n = (int) (sr * 2.0);
            std::vector<float> sig ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr;
                const double env = 0.9 * std::exp (-t / 0.8);
                sig[(size_t) i] = (float) (env * std::tanh (
                    1.5 * std::sin (2.0 * pi * freq * t)));
            }
            const auto r = lf::PitchDetector::detect (sig.data(), n, s);
            expect (r.isValid(), "Expected a pitch on the 808");
            expectEquals (r.nearestMidiNote(), 28);
            expect (std::abs (r.centsOffset()) < 20.0f, "808 should be near E1");
        }

        beginTest ("Silence detects nothing");
        {
            const std::vector<float> zeros (44100, 0.0f);
            const auto r = lf::PitchDetector::detect (zeros.data(), (int) zeros.size(), s);
            expect (! r.isValid(), "Silence must not produce a pitch");
        }
    }
};

// =============================================================================
// Test 7 — performance: 10s file analyses in < 2000 ms.
// =============================================================================
class PerformanceTest  : public juce::UnitTest
{
public:
    PerformanceTest() : juce::UnitTest ("LoopDetector — Performance") {}

    void runTest() override
    {
        beginTest ("10s 44.1kHz sine analyses in under 2 seconds");

        constexpr double sr = 44100.0;
        const auto sig = makeSine (220.0, 10.0, sr);

        lf::LoopDetector det;
        lf::LoopDetector::Settings s;
        s.sampleRate = static_cast<float> (sr);

        using clk = std::chrono::high_resolution_clock;
        const auto t0 = clk::now();
        const auto regions = det.analyze (sig.data(), (int) sig.size(), s);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds> (
                            clk::now() - t0).count();

        logMessage ("Found " + juce::String ((int) regions.size())
                  + " regions in " + juce::String ((int) ms) + " ms");

        expectLessThan ((int) ms, 2000, "Analysis must complete within 2 seconds");
        expect (! regions.empty(), "Should still find loop regions on a 10s sine");
    }
};

// =============================================================================
// Static instances — JUCE's UnitTestRunner picks them up automatically.
// =============================================================================
static SineWaveTest    sineWaveTest;
static SilenceTest     silenceTest;
static ShortFileTest   shortFileTest;
static CrossfadeTest   crossfadeTest;
static SearchRangeTest searchRangeTest;
static PitchDetectionTest pitchDetectionTest;
static PerformanceTest performanceTest;

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------
int main (int /*argc*/, char* /*argv*/[])
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* r = runner.getResult (i))
            failures += r->failures;

    juce::Logger::writeToLog ("\n" + juce::String (failures) + " failure(s).");
    return failures == 0 ? 0 : 1;
}
