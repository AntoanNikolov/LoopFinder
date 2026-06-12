#pragma once

#include <cmath>

namespace lf
{
    /** Result of a fundamental-frequency estimate. */
    struct PitchResult
    {
        float frequencyHz { 0.0f };   // <= 0 → nothing detected
        float midiNote    { -1.0f };  // fractional MIDI note (69 = A4 = 440 Hz)
        float clarity     { 0.0f };   // 0..1 — how periodic the signal was

        bool isValid() const noexcept { return frequencyHz > 0.0f; }

        int nearestMidiNote() const noexcept
        {
            return (int) std::lround (midiNote);
        }

        /** Cents offset from the nearest equal-tempered note, in [-50, +50]. */
        float centsOffset() const noexcept
        {
            return (midiNote - (float) nearestMidiNote()) * 100.0f;
        }
    };

    /** Pure C++17 monophonic pitch detector (no JUCE dependency).
     *
     *  Uses the normalised square difference function (NSDF, as in the
     *  McLeod Pitch Method): the analysis window is placed shortly after the
     *  loudest part of the file (skipping the attack transient), candidate
     *  lags are scored, and the first strong NSDF peak is refined with
     *  parabolic interpolation. Tuned for bass-heavy material like 808s
     *  (down to 25 Hz) but works on any sustained monophonic source.
     */
    class PitchDetector
    {
    public:
        struct Settings
        {
            float sampleRate       { 44100.0f };
            float minHz            { 25.0f };     // low enough for sub bass
            float maxHz            { 2000.0f };
            int   maxWindowSamples { 16384 };     // analysis window cap
            float minClarity       { 0.5f };      // reject noisy estimates
        };

        static PitchResult detect (const float* samples, int numSamples,
                                   const Settings& settings);
    };
}
