#pragma once

#include <cstdint>

namespace lf
{
    /** A detected (or user-edited) loop region within an audio file.
     *
     *  Sample positions refer to the analysed audio's native sample rate.
     */
    struct LoopRegion
    {
        int   startSample  { 0 };
        int   endSample    { 0 };
        float score        { 0.0f }; // normalised cross-correlation, [0, 1]
        float durationMs   { 0.0f };
        bool  hasCrossfade { false };

        constexpr int  lengthSamples() const noexcept { return endSample - startSample; }
        constexpr bool isValid()       const noexcept { return endSample > startSample; }
    };
}
