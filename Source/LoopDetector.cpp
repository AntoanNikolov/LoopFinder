#include "LoopDetector.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace lf
{
    // -------------------------------------------------------------------------
    // Helpers (file-local, anonymous namespace)
    // -------------------------------------------------------------------------
    namespace
    {
        /** Find every rising zero crossing in `samples`, restricted to the
         *  inner `[startIdx, endIdx)` portion of the buffer.
         *
         *  A "rising" crossing is the first sample i with samples[i] > 0 and
         *  samples[i - 1] <= 0.
         */
        std::vector<int> findRisingZeroCrossings (const float* samples,
                                                  int startIdx,
                                                  int endIdx)
        {
            std::vector<int> out;
            if (samples == nullptr || endIdx <= startIdx + 1)
                return out;

            out.reserve (static_cast<size_t> ((endIdx - startIdx) / 32 + 16));

            for (int i = std::max (1, startIdx); i < endIdx; ++i)
            {
                if (samples[i] > 0.0f && samples[i - 1] <= 0.0f)
                    out.push_back (i);
            }
            return out;
        }

        /** Pearson normalised cross-correlation between two equal-length
         *  windows. Returns a value in [-1, 1]. Returns 0 on degenerate input
         *  (zero variance in either window).
         */
        float normalisedCrossCorrelation (const float* a, const float* b, int n)
        {
            if (a == nullptr || b == nullptr || n <= 1)
                return 0.0f;

            // Two-pass algorithm: compute means first, then correlation.
            double sumA = 0.0, sumB = 0.0;
            for (int i = 0; i < n; ++i) { sumA += a[i]; sumB += b[i]; }

            const double meanA = sumA / static_cast<double> (n);
            const double meanB = sumB / static_cast<double> (n);

            double num = 0.0, varA = 0.0, varB = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double da = static_cast<double> (a[i]) - meanA;
                const double db = static_cast<double> (b[i]) - meanB;
                num  += da * db;
                varA += da * da;
                varB += db * db;
            }

            const double denom = std::sqrt (varA * varB);
            if (denom < 1.0e-12)
                return 0.0f;

            const double r = num / denom;
            // Clamp to handle minor floating-point overshoot.
            return static_cast<float> (std::max (-1.0, std::min (1.0, r)));
        }

        /** Score a candidate (S, E) pair using a centred window of size `win`.
         *  Window indices that fall outside the buffer cause the candidate
         *  to be rejected (returns -1.0).
         */
        float scorePair (const float* samples, int numSamples,
                         int s, int e, int win)
        {
            const int half = win / 2;
            const int aStart = s - half;
            const int bStart = e - half;
            if (aStart < 0 || bStart < 0)                return -1.0f;
            if (aStart + win > numSamples || bStart + win > numSamples) return -1.0f;

            return normalisedCrossCorrelation (samples + aStart,
                                               samples + bStart,
                                               win);
        }
    } // namespace

    // -------------------------------------------------------------------------
    // LoopDetector::analyze
    // -------------------------------------------------------------------------
    std::vector<LoopRegion> LoopDetector::analyze (const float*    samples,
                                                   int             numSamples,
                                                   const Settings& s,
                                                   ProgressFn      onProgress,
                                                   const CancelFlag* cancel) const
    {
        std::vector<LoopRegion> results;

        if (samples == nullptr || numSamples <= 0 || s.sampleRate <= 0.0f)
            return results;

        // ---------------------------------------------------------------------
        // Translate ms → samples and clamp.
        // ---------------------------------------------------------------------
        const auto secondsToSamples = [sr = s.sampleRate] (float ms)
        {
            return static_cast<int> (std::lround (static_cast<double> (ms) * 0.001 * sr));
        };

        const int minLoop  = std::max (s.minWindowSamples, secondsToSamples (s.minLoopMs));
        const int maxLoop  = std::max (minLoop + 1, secondsToSamples (s.maxLoopMs));
        const int dedupe   = std::max (1, secondsToSamples (s.dedupeWindowMs));

        const int boundary = static_cast<int> (std::floor (static_cast<double> (numSamples)
                                                           * std::clamp (s.boundaryFraction,
                                                                         0.0f, 0.49f)));
        const int searchStart = boundary;
        const int searchEnd   = numSamples - boundary;
        if (searchEnd - searchStart < minLoop + 4)
            return results;

        // ---------------------------------------------------------------------
        // Step 1 — rising zero crossings inside the safe window.
        // ---------------------------------------------------------------------
        const auto crossings = findRisingZeroCrossings (samples, searchStart, searchEnd);
        const int  numCrossings = static_cast<int> (crossings.size());

        if (numCrossings < 2)
            return results;

        // Local period at each crossing = distance to next crossing.
        std::vector<int> periods (static_cast<size_t> (numCrossings), 0);
        for (int i = 0; i + 1 < numCrossings; ++i)
            periods[(size_t) i] = crossings[(size_t) i + 1] - crossings[(size_t) i];
        // Last crossing has no successor — leave its period at 0; it can't
        // be used as either an S or E (we require period > 0 below).

        // ---------------------------------------------------------------------
        // Step 2 + 3 — generate candidate pairs and score them.
        // ---------------------------------------------------------------------
        struct Candidate
        {
            int   s;
            int   e;
            float score;
        };
        std::vector<Candidate> candidates;
        candidates.reserve (1024);

        const float tol  = std::max (0.0f, s.periodTolerance);
        const int progressStep = std::max (1, numCrossings / 20);

        // Track cumulative search progress for the callback.
        for (int i = 0; i < numCrossings; ++i)
        {
            if (cancel != nullptr && cancel->load (std::memory_order_relaxed))
                break;

            if (onProgress && (i % progressStep) == 0)
                onProgress (static_cast<float> (i) / static_cast<float> (numCrossings));

            const int periodS = periods[(size_t) i];
            if (periodS <= 0)
                continue;

            const int sIdx = crossings[(size_t) i];

            // Window size = max(minWindow, local period). Same window for
            // both S and E so we compare like-for-like.
            const int win = std::max (s.minWindowSamples, periodS);

            // Find the index range of crossings whose distance from S falls
            // within [minLoop, maxLoop] using two binary searches.
            const auto lo = std::lower_bound (crossings.begin() + i + 1, crossings.end(),
                                              sIdx + minLoop);
            const auto hi = std::upper_bound (crossings.begin() + i + 1, crossings.end(),
                                              sIdx + maxLoop);

            for (auto it = lo; it != hi; ++it)
            {
                const int j = static_cast<int> (it - crossings.begin());
                const int periodE = periods[(size_t) j];
                if (periodE <= 0)
                    continue;

                // Period match within tolerance.
                const float ratio = static_cast<float> (std::abs (periodE - periodS))
                                  / static_cast<float> (periodS);
                if (ratio > tol)
                    continue;

                const int eIdx  = *it;
                const float sc  = scorePair (samples, numSamples, sIdx, eIdx, win);
                if (sc >= s.scoreThreshold)
                    candidates.push_back ({ sIdx, eIdx, sc });
            }
        }

        if (onProgress)
            onProgress (1.0f);

        if (candidates.empty())
            return results;

        // ---------------------------------------------------------------------
        // Step 4 — sort by score desc, then dedupe.
        // ---------------------------------------------------------------------
        std::sort (candidates.begin(), candidates.end(),
                   [] (const Candidate& a, const Candidate& b)
                   { return a.score > b.score; });

        const int crossfadeSamples = std::max (1, secondsToSamples (s.crossfadeMs));

        results.reserve (static_cast<size_t> (s.maxResults));
        for (const auto& c : candidates)
        {
            if (static_cast<int> (results.size()) >= s.maxResults)
                break;

            bool collides = false;
            for (const auto& r : results)
            {
                if (std::abs (c.s - r.startSample) < dedupe
                 || std::abs (c.e - r.endSample)   < dedupe)
                {
                    collides = true;
                    break;
                }
            }
            if (collides)
                continue;

            LoopRegion lr;
            lr.startSample  = c.s;
            lr.endSample    = c.e;
            lr.score        = c.score;
            lr.durationMs   = static_cast<float> (c.e - c.s) * 1000.0f / s.sampleRate;
            // The crossfade buffer itself is built on demand (see
            // buildCrossfadeTail) — flag that this region *should* use one.
            lr.hasCrossfade = c.score < s.crossfadeBelowScore
                            && (c.e - c.s) > crossfadeSamples * 2;
            results.push_back (lr);
        }

        return results;
    }

    // -------------------------------------------------------------------------
    // LoopDetector::buildCrossfadeTail
    // -------------------------------------------------------------------------
    std::vector<float> LoopDetector::buildCrossfadeTail (const float*      samples,
                                                         int               numSamples,
                                                         const LoopRegion& region,
                                                         int               crossfadeSamples)
    {
        std::vector<float> out;
        if (samples == nullptr || numSamples <= 0 || ! region.isValid())
            return out;

        const int N = region.lengthSamples();
        const int K = std::clamp (crossfadeSamples, 1, N / 2);
        out.resize (static_cast<size_t> (K), 0.0f);

        const int s = region.startSample;
        const int e = region.endSample;

        if (s < 0 || e > numSamples)
            return out;

        // Linearly fade out the tail of the loop while fading in the head,
        // so playback wraps from "end of K-tail = head of region" back to
        // sample S without a discontinuity.
        for (int i = 0; i < K; ++i)
        {
            const float a = static_cast<float> (i) / static_cast<float> (K - 1 > 0 ? K - 1 : 1);
            const float tail = samples[e - K + i];
            const float head = samples[s + i];
            out[(size_t) i]  = (1.0f - a) * tail + a * head;
        }

        return out;
    }
}
