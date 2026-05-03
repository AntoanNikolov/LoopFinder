#pragma once

#include "LoopRegion.h"

#include <atomic>
#include <functional>
#include <vector>

namespace lf
{
    /**
     *  Pure C++17 loop-point detector.
     *
     *  Has no JUCE dependency so it can be unit tested in isolation and
     *  reused outside of the plugin (e.g. in a CLI tool).
     *
     *  The algorithm is:
     *
     *   1. Find every rising zero-crossing in the input (ignoring the first
     *      and last `boundaryFraction` of the file).
     *   2. For every pair of crossings whose distance lies in the requested
     *      loop-length range and whose local period matches within tolerance,
     *      compute the normalised cross-correlation of a window centred on
     *      each crossing.
     *   3. Keep pairs scoring above `scoreThreshold`, sort by score, then
     *      remove pairs whose endpoints are within `dedupeWindowMs` of an
     *      already-selected pair.
     *   4. Return the top `maxResults` regions.
     *
     *  Crossfaded "tail" buffers for individual regions can be produced
     *  separately via `buildCrossfadeTail` and applied at playback time.
     */
    class LoopDetector
    {
    public:
        struct Settings
        {
            float sampleRate         { 44100.0f };
            float minLoopMs          { 50.0f };       // shortest acceptable loop
            float maxLoopMs          { 4000.0f };     // longest acceptable loop
            float scoreThreshold     { 0.85f };       // discard worse correlations
            float crossfadeBelowScore{ 0.95f };       // recommend xfade below this
            float crossfadeMs        { 2.0f };        // xfade length
            float dedupeWindowMs     { 10.0f };       // collision window
            float boundaryFraction   { 0.05f };       // 5% head/tail dead zone
            float periodTolerance    { 0.10f };       // ±10% period match
            int   minWindowSamples   { 64 };          // floor for correlation window
            int   maxResults         { 8 };           // max regions returned
        };

        using ProgressFn = std::function<void (float /* 0..1 */)>;
        using CancelFlag = std::atomic<bool>;

        /** Run the analysis. May be called from a background thread.
         *
         *  @param samples       Pointer to mono PCM data (interleaved mixes
         *                       must be downmixed by the caller).
         *  @param numSamples    Length of @p samples.
         *  @param settings      Algorithm tuning.
         *  @param onProgress    Optional progress callback (0..1). Called
         *                       periodically from the analysis thread.
         *  @param cancel        Optional cancellation flag — when set true
         *                       the analysis returns early with whatever
         *                       results it has collected so far.
         *  @return              At most settings.maxResults regions, sorted
         *                       by descending correlation score.
         */
        std::vector<LoopRegion> analyze (const float*    samples,
                                         int             numSamples,
                                         const Settings& settings,
                                         ProgressFn      onProgress = {},
                                         const CancelFlag* cancel    = nullptr) const;

        /** Build a crossfaded copy of the *tail* of a loop region.
         *
         *  The returned buffer has length `crossfadeSamples` and contains the
         *  blended samples that should replace the last `crossfadeSamples`
         *  of the loop on playback.
         */
        static std::vector<float> buildCrossfadeTail (const float*       samples,
                                                      int                numSamples,
                                                      const LoopRegion&  region,
                                                      int                crossfadeSamples);
    };
}
