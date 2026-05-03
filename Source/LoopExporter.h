#pragma once

#include "LoopRegion.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <vector>

namespace lf
{
    /** Writes loop regions out to disk as standalone WAV files with a
     *  proper `smpl` chunk so samplers (Kontakt, Battery, FL Studio…)
     *  pick up the loop points automatically.
     *
     *  We do not rely on JUCE's WavAudioFormat for the metadata — we
     *  write the file ourselves so the smpl chunk is guaranteed to be
     *  positioned correctly.
     */
    class LoopExporter
    {
    public:
        enum class BitDepth : int
        {
            int16    = 16,
            float32  = 32,
        };

        struct Options
        {
            BitDepth bitDepth        { BitDepth::int16 };
            int      midiUnityNote   { 60 };           // middle C
            bool     applyCrossfade  { true };
            int      crossfadeSamples{ 0 };            // 0 → derived from region
        };

        struct Result
        {
            bool          ok { false };
            juce::String  message;
            juce::File    file;
        };

        /** Export a single region to `targetFile`. */
        static Result exportRegion (const juce::AudioBuffer<float>& source,
                                    double                          sampleRate,
                                    const LoopRegion&               region,
                                    const juce::File&               targetFile,
                                    const Options&                  opts);

        /** Export every region into `targetFolder` using `baseName` as
         *  the file prefix. Returns one Result per region.
         */
        static std::vector<Result> exportAll (const juce::AudioBuffer<float>& source,
                                              double                          sampleRate,
                                              const std::vector<LoopRegion>&  regions,
                                              const juce::File&               targetFolder,
                                              const juce::String&             baseName,
                                              const Options&                  opts);
    };
}
