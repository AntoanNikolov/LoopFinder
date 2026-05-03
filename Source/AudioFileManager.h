#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>

namespace lf
{
    /** Owns the currently loaded audio file plus the decoded sample data.
     *
     *  Keeps both the original (potentially stereo) buffer for playback and
     *  a mono-mixdown buffer for analysis. State is serialisable to/from a
     *  `juce::ValueTree` so the PluginProcessor can persist it across
     *  DAW sessions.
     *
     *  Files larger than `maxEmbedBytes` are persisted by absolute path only;
     *  smaller files are embedded base64-encoded so projects survive moves.
     */
    class AudioFileManager
    {
    public:
        struct Metadata
        {
            juce::String filename;
            juce::String absolutePath;
            double       sampleRate    { 0.0 };
            double       lengthSeconds { 0.0 };
            int          bitDepth      { 0 };
            int          numChannels   { 0 };
            juce::int64  numSamples    { 0 };
            bool         isLoaded      { false };
            bool         isMissing     { false }; // true after restore if file path no longer exists
            juce::String lastError;
        };

        AudioFileManager();
        ~AudioFileManager();

        // ---------------------------------------------------------------------
        // Loading
        // ---------------------------------------------------------------------
        /** Synchronously load and decode an audio file.
         *  @return true on success. On failure call lastError() for details.
         */
        bool loadFile (const juce::File& file);

        /** Returns true if a file is currently loaded and decoded. */
        bool isLoaded() const noexcept              { return metadata.isLoaded; }
        const Metadata& getMetadata() const noexcept { return metadata; }

        /** Stereo (or multi-channel) decoded audio at the file's native rate. */
        const juce::AudioBuffer<float>& getAudioBuffer() const noexcept { return audioBuffer; }

        /** Mono mixdown used for analysis. */
        const std::vector<float>&       getMonoMix()      const noexcept { return monoMix; }

        // ---------------------------------------------------------------------
        // State serialisation
        // ---------------------------------------------------------------------
        /** Maximum file size to embed in serialised state (default 5 MB). */
        void setMaxEmbedBytes (juce::int64 bytes) noexcept { maxEmbedBytes = bytes; }

        /** Serialise the loaded file (path or embedded data) to a ValueTree. */
        juce::ValueTree toValueTree() const;

        /** Restore from a ValueTree previously produced by toValueTree(). */
        bool fromValueTree (const juce::ValueTree& tree);

        // ---------------------------------------------------------------------
        // Misc
        // ---------------------------------------------------------------------
        void clear();

        juce::AudioFormatManager& getFormatManager() noexcept { return formatManager; }

    private:
        bool decodeReader (juce::AudioFormatReader& reader,
                           const juce::File&        sourceFile);

        void rebuildMonoMix();

        juce::AudioFormatManager  formatManager;
        juce::AudioBuffer<float>  audioBuffer;
        std::vector<float>        monoMix;
        Metadata                  metadata;
        juce::int64               maxEmbedBytes { 5 * 1024 * 1024 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFileManager)
    };
}
