#include "PitchDetector.h"

#include <algorithm>
#include <vector>

namespace lf
{
    namespace
    {
        /** Convert a frequency to a fractional MIDI note (A4 = 440 Hz = 69). */
        float hzToMidi (float hz)
        {
            return 69.0f + 12.0f * std::log2 (hz / 440.0f);
        }
    }

    PitchResult PitchDetector::detect (const float* samples, int numSamples,
                                       const Settings& s)
    {
        PitchResult none;

        if (samples == nullptr || numSamples <= 0 || s.sampleRate <= 0.0f)
            return none;

        const int maxLag = (int) std::lround (s.sampleRate / std::max (1.0f, s.minHz));
        const int minLag = std::max (2, (int) std::lround (s.sampleRate / s.maxHz));
        if (maxLag <= minLag)
            return none;

        // ---------------------------------------------------------------------
        // Choose the analysis window: start shortly after the loudest sample
        // so the (non-periodic) attack transient is skipped, and the sustain
        // or decay portion — which carries the perceived pitch — is analysed.
        // ---------------------------------------------------------------------
        int peakIdx = 0;
        float peakVal = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float a = std::abs (samples[i]);
            if (a > peakVal) { peakVal = a; peakIdx = i; }
        }
        if (peakVal < 1.0e-5f)   // effectively silent
            return none;

        const int skip   = (int) std::lround (0.03 * s.sampleRate); // 30 ms past the peak
        int start        = std::min (peakIdx + skip, numSamples - 1);
        int window       = std::min (s.maxWindowSamples, numSamples - start);

        // Need at least two periods of the lowest detectable pitch; if the
        // tail is too short, slide the window back toward the start.
        const int needed = 2 * maxLag;
        if (window < needed)
        {
            start  = std::max (0, numSamples - s.maxWindowSamples);
            window = numSamples - start;
        }
        if (window < std::max (needed / 2, 4 * minLag))
            return none;

        const float* x = samples + start;
        const int W = window;
        const int lagCap = std::min (maxLag, W / 2);
        if (lagCap <= minLag)
            return none;

        // ---------------------------------------------------------------------
        // NSDF: nsdf[tau] = 2 * Σ x[i]·x[i+tau] / Σ (x[i]² + x[i+tau]²)
        // ---------------------------------------------------------------------
        std::vector<float> nsdf ((size_t) lagCap + 1, 0.0f);
        for (int tau = minLag; tau <= lagCap; ++tau)
        {
            double acf = 0.0, norm = 0.0;
            const int n = W - tau;
            for (int i = 0; i < n; ++i)
            {
                const double a = x[i];
                const double b = x[i + tau];
                acf  += a * b;
                norm += a * a + b * b;
            }
            nsdf[(size_t) tau] = norm > 1.0e-12 ? (float) (2.0 * acf / norm) : 0.0f;
        }

        // ---------------------------------------------------------------------
        // Peak picking (MPM style): collect local maxima, then take the FIRST
        // peak that reaches 90% of the tallest one — this prefers the true
        // fundamental over its subharmonics at larger lags.
        // ---------------------------------------------------------------------
        struct Peak { int lag; float value; };
        std::vector<Peak> peaks;
        for (int tau = minLag + 1; tau < lagCap; ++tau)
        {
            const float v = nsdf[(size_t) tau];
            if (v > nsdf[(size_t) tau - 1] && v >= nsdf[(size_t) tau + 1] && v > 0.0f)
                peaks.push_back ({ tau, v });
        }
        if (peaks.empty())
            return none;

        float best = 0.0f;
        for (const auto& p : peaks) best = std::max (best, p.value);
        if (best < s.minClarity)
            return none;

        const float threshold = best * 0.9f;
        Peak chosen { 0, 0.0f };
        for (const auto& p : peaks)
            if (p.value >= threshold) { chosen = p; break; }

        // Parabolic interpolation around the chosen lag for sub-sample accuracy.
        double lag = (double) chosen.lag;
        {
            const double y0 = nsdf[(size_t) chosen.lag - 1];
            const double y1 = nsdf[(size_t) chosen.lag];
            const double y2 = nsdf[(size_t) chosen.lag + 1];
            const double denom = y0 - 2.0 * y1 + y2;
            if (std::abs (denom) > 1.0e-12)
                lag += 0.5 * (y0 - y2) / denom;
        }

        PitchResult r;
        r.frequencyHz = (float) (s.sampleRate / lag);
        r.midiNote    = hzToMidi (r.frequencyHz);
        r.clarity     = chosen.value;
        return r;
    }
}
