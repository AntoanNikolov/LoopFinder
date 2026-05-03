#include "AudioFileManager.h"

#include <juce_core/juce_core.h>

namespace lf
{
    namespace ids
    {
        static const juce::Identifier root          { "AudioFile" };
        static const juce::Identifier path          { "path" };
        static const juce::Identifier embedded      { "embedded" };
        static const juce::Identifier embeddedData  { "embeddedData" };
        static const juce::Identifier filename      { "filename" };
        static const juce::Identifier originalSize  { "originalSize" };
    }

    AudioFileManager::AudioFileManager()
    {
        formatManager.registerBasicFormats();
    }

    AudioFileManager::~AudioFileManager() = default;

    void AudioFileManager::clear()
    {
        audioBuffer.setSize (0, 0);
        monoMix.clear();
        metadata = {};
    }

    bool AudioFileManager::loadFile (const juce::File& file)
    {
        clear();

        if (! file.existsAsFile())
        {
            metadata.lastError  = "File does not exist: " + file.getFullPathName();
            metadata.isMissing  = true;
            return false;
        }

        std::unique_ptr<juce::AudioFormatReader> reader (
            formatManager.createReaderFor (file));

        if (reader == nullptr)
        {
            metadata.lastError = "Unsupported or corrupt audio file: " + file.getFileName();
            return false;
        }

        if (! decodeReader (*reader, file))
            return false;

        metadata.isLoaded     = true;
        metadata.absolutePath = file.getFullPathName();
        metadata.filename     = file.getFileName();
        metadata.isMissing    = false;
        metadata.lastError.clear();
        return true;
    }

    bool AudioFileManager::decodeReader (juce::AudioFormatReader& reader,
                                         const juce::File&        sourceFile)
    {
        const auto numChannels = static_cast<int> (reader.numChannels);
        const auto numSamples  = static_cast<int> (reader.lengthInSamples);
        if (numChannels <= 0 || numSamples <= 0)
        {
            metadata.lastError = "Audio file has zero channels or samples.";
            return false;
        }

        try
        {
            audioBuffer.setSize (numChannels, numSamples, false, true, false);
            if (! reader.read (&audioBuffer, 0, numSamples, 0, true, true))
            {
                metadata.lastError = "Failed to read audio data.";
                audioBuffer.setSize (0, 0);
                return false;
            }
        }
        catch (const std::exception& ex)
        {
            metadata.lastError = juce::String ("Decoding failed: ") + ex.what();
            audioBuffer.setSize (0, 0);
            return false;
        }

        metadata.sampleRate    = reader.sampleRate;
        metadata.lengthSeconds = static_cast<double> (numSamples) / reader.sampleRate;
        metadata.bitDepth      = static_cast<int> (reader.bitsPerSample);
        metadata.numChannels   = numChannels;
        metadata.numSamples    = numSamples;
        metadata.filename      = sourceFile.getFileName();
        metadata.absolutePath  = sourceFile.getFullPathName();

        rebuildMonoMix();
        return true;
    }

    void AudioFileManager::rebuildMonoMix()
    {
        const int n  = audioBuffer.getNumSamples();
        const int ch = audioBuffer.getNumChannels();
        monoMix.assign (static_cast<size_t> (n), 0.0f);

        if (n <= 0 || ch <= 0)
            return;

        const float gain = 1.0f / static_cast<float> (ch);
        for (int c = 0; c < ch; ++c)
        {
            const float* src = audioBuffer.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                monoMix[(size_t) i] += src[i] * gain;
        }
    }

    // -------------------------------------------------------------------------
    // Serialisation
    // -------------------------------------------------------------------------
    juce::ValueTree AudioFileManager::toValueTree() const
    {
        juce::ValueTree tree (ids::root);

        if (! metadata.isLoaded)
            return tree;

        tree.setProperty (ids::filename,     metadata.filename,     nullptr);
        tree.setProperty (ids::originalSize, (juce::int64) metadata.numSamples, nullptr);
        tree.setProperty (ids::path,         metadata.absolutePath, nullptr);

        const juce::File file { metadata.absolutePath };
        if (file.existsAsFile() && file.getSize() <= maxEmbedBytes)
        {
            juce::MemoryBlock raw;
            if (file.loadFileAsData (raw))
            {
                tree.setProperty (ids::embedded,     true, nullptr);
                tree.setProperty (ids::embeddedData, raw.toBase64Encoding(), nullptr);
            }
        }

        return tree;
    }

    bool AudioFileManager::fromValueTree (const juce::ValueTree& tree)
    {
        clear();
        if (! tree.hasType (ids::root))
            return false;

        const bool hasEmbed = static_cast<bool> (tree.getProperty (ids::embedded, false));
        if (hasEmbed)
        {
            const auto base64 = tree.getProperty (ids::embeddedData).toString();
            juce::MemoryBlock raw;
            if (raw.fromBase64Encoding (base64) && raw.getSize() > 0)
            {
                auto* mis = new juce::MemoryInputStream (raw, true);
                std::unique_ptr<juce::AudioFormatReader> reader (
                    formatManager.createReaderFor (std::unique_ptr<juce::InputStream> (mis)));

                if (reader != nullptr)
                {
                    juce::File faux { tree.getProperty (ids::path).toString() };
                    if (decodeReader (*reader, faux))
                    {
                        metadata.filename = tree.getProperty (ids::filename, faux.getFileName())
                                                .toString();
                        metadata.isLoaded = true;
                        return true;
                    }
                }
            }
        }

        const auto path = tree.getProperty (ids::path).toString();
        if (path.isNotEmpty())
        {
            juce::File file { path };
            if (file.existsAsFile())
                return loadFile (file);

            metadata.absolutePath = path;
            metadata.filename     = file.getFileName();
            metadata.isMissing    = true;
            metadata.lastError    = "Linked audio file is missing: " + path;
        }

        return false;
    }
}
