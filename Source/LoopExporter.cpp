#include "LoopExporter.h"
#include "LoopDetector.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace lf
{
    namespace
    {
        // -- Little-endian writers ---------------------------------------------
        inline void writeU32LE (juce::MemoryOutputStream& s, uint32_t v)
        {
            const uint8_t b[4] {
                static_cast<uint8_t> ( v        & 0xFF),
                static_cast<uint8_t> ((v >>  8) & 0xFF),
                static_cast<uint8_t> ((v >> 16) & 0xFF),
                static_cast<uint8_t> ((v >> 24) & 0xFF),
            };
            s.write (b, 4);
        }

        inline void writeU16LE (juce::MemoryOutputStream& s, uint16_t v)
        {
            const uint8_t b[2] {
                static_cast<uint8_t> ( v       & 0xFF),
                static_cast<uint8_t> ((v >> 8) & 0xFF),
            };
            s.write (b, 2);
        }

        inline void writeI16LE (juce::MemoryOutputStream& s, int16_t v)
        {
            writeU16LE (s, static_cast<uint16_t> (v));
        }

        inline void writeFLE (juce::MemoryOutputStream& s, float f)
        {
            uint32_t bits;
            static_assert (sizeof (bits) == sizeof (f), "");
            std::memcpy (&bits, &f, sizeof (bits));
            writeU32LE (s, bits);
        }

        // -- Sample conversion -------------------------------------------------
        inline int16_t floatToInt16 (float v)
        {
            const float clipped = juce::jlimit (-1.0f, 1.0f, v);
            return static_cast<int16_t> (std::lround (clipped * 32767.0f));
        }

        // -- Apply crossfade tail to a region buffer in place -----------------
        void maybeCrossfade (juce::AudioBuffer<float>& regionBuf,
                             const juce::AudioBuffer<float>& source,
                             const LoopRegion& region,
                             const LoopExporter::Options& opts)
        {
            if (! opts.applyCrossfade || ! region.hasCrossfade)
                return;

            const int K = (opts.crossfadeSamples > 0
                            ? opts.crossfadeSamples
                            : juce::jmax (1, region.lengthSamples() / 100)); // ~1% fallback
            const int len = regionBuf.getNumSamples();
            if (K <= 1 || K > len / 2)
                return;

            for (int ch = 0; ch < regionBuf.getNumChannels(); ++ch)
            {
                const int srcCh = juce::jlimit (0, source.getNumChannels() - 1, ch);
                const float* src = source.getReadPointer (srcCh);
                float*       dst = regionBuf.getWritePointer (ch);

                for (int i = 0; i < K; ++i)
                {
                    const float a    = static_cast<float> (i) / static_cast<float> (K - 1);
                    const float tail = src[region.endSample - K + i];
                    const float head = src[region.startSample + i];
                    dst[len - K + i] = (1.0f - a) * tail + a * head;
                }
            }
        }
    } // namespace

    LoopExporter::Result LoopExporter::exportRegion (const juce::AudioBuffer<float>& source,
                                                     double                          sampleRate,
                                                     const LoopRegion&               region,
                                                     const juce::File&               targetFile,
                                                     const Options&                  opts)
    {
        Result r; r.file = targetFile;

        if (! region.isValid()
         || region.startSample < 0
         || region.endSample > source.getNumSamples())
        {
            r.message = "Invalid loop region";
            return r;
        }

        const int numCh = juce::jmax (1, source.getNumChannels());
        const int len   = region.lengthSamples();

        // Copy the region into a contiguous buffer.
        juce::AudioBuffer<float> region_buf (numCh, len);
        for (int ch = 0; ch < numCh; ++ch)
        {
            const int srcCh = juce::jlimit (0, source.getNumChannels() - 1, ch);
            region_buf.copyFrom (ch, 0, source, srcCh, region.startSample, len);
        }

        maybeCrossfade (region_buf, source, region, opts);

        // -----------------------------------------------------------------
        // Write WAV manually so we can control chunk ordering.
        // -----------------------------------------------------------------
        juce::MemoryBlock fileBytes;
        juce::MemoryOutputStream out (fileBytes, false);

        const bool     isFloat       = (opts.bitDepth == BitDepth::float32);
        const uint16_t bitsPerSample = isFloat ? 32u : 16u;
        const uint16_t blockAlign    = static_cast<uint16_t> (numCh * (bitsPerSample / 8));
        const uint32_t byteRate      = static_cast<uint32_t> (sampleRate) * blockAlign;
        const uint32_t dataSize      = static_cast<uint32_t> (len) * blockAlign;

        const uint32_t fmtChunkSize  = isFloat ? 18u : 16u; // floats need cbSize
        const uint32_t factChunkSize = isFloat ? 4u  : 0u;
        const uint32_t smplDataSize  = 36u + 24u;          // 1 loop point
        const uint32_t smplChunkSize = smplDataSize;

        // RIFF header (size patched at the end).
        out.write ("RIFF", 4);
        writeU32LE (out, 0); // placeholder
        out.write ("WAVE", 4);

        // fmt  chunk
        out.write ("fmt ", 4);
        writeU32LE (out, fmtChunkSize);
        writeU16LE (out, isFloat ? 0x0003 : 0x0001);
        writeU16LE (out, static_cast<uint16_t> (numCh));
        writeU32LE (out, static_cast<uint32_t> (sampleRate));
        writeU32LE (out, byteRate);
        writeU16LE (out, blockAlign);
        writeU16LE (out, bitsPerSample);
        if (isFloat)
            writeU16LE (out, 0); // cbSize

        // fact chunk for float WAVs
        if (isFloat)
        {
            out.write ("fact", 4);
            writeU32LE (out, factChunkSize);
            writeU32LE (out, static_cast<uint32_t> (len));
        }

        // data chunk
        out.write ("data", 4);
        writeU32LE (out, dataSize);
        for (int frame = 0; frame < len; ++frame)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float v = region_buf.getSample (ch, frame);
                if (isFloat) writeFLE (out, v);
                else         writeI16LE (out, floatToInt16 (v));
            }
        }
        // Pad to even byte boundary if needed
        if ((dataSize & 1u) != 0u)
            out.writeByte (0);

        // smpl chunk — loop points
        const uint32_t samplePeriod = static_cast<uint32_t> (
            std::llround (1.0e9 / juce::jmax (1.0, sampleRate)));

        out.write ("smpl", 4);
        writeU32LE (out, smplChunkSize);
        writeU32LE (out, 0);                                      // manufacturer
        writeU32LE (out, 0);                                      // product
        writeU32LE (out, samplePeriod);                           // sample period (ns)
        writeU32LE (out, static_cast<uint32_t> (opts.midiUnityNote));
        writeU32LE (out, 0);                                      // pitch fraction
        writeU32LE (out, 0);                                      // SMPTE format
        writeU32LE (out, 0);                                      // SMPTE offset
        writeU32LE (out, 1);                                      // num sample loops
        writeU32LE (out, 0);                                      // sampler-specific data size
        // Loop record (24 bytes)
        writeU32LE (out, 0);                                      // cue point ID
        writeU32LE (out, 0);                                      // type 0 = forward
        writeU32LE (out, 0);                                      // start sample (loop within file)
        writeU32LE (out, static_cast<uint32_t> (len - 1));        // end sample
        writeU32LE (out, 0);                                      // fraction
        writeU32LE (out, 0);                                      // play count (0 = infinite)

        out.flush();

        // Patch RIFF size = total file bytes minus 8.
        const auto totalSize = fileBytes.getSize();
        if (totalSize >= 8)
        {
            const uint32_t riffSize = static_cast<uint32_t> (totalSize - 8);
            auto* bytes = static_cast<uint8_t*> (fileBytes.getData());
            bytes[4] = static_cast<uint8_t> ( riffSize        & 0xFF);
            bytes[5] = static_cast<uint8_t> ((riffSize >>  8) & 0xFF);
            bytes[6] = static_cast<uint8_t> ((riffSize >> 16) & 0xFF);
            bytes[7] = static_cast<uint8_t> ((riffSize >> 24) & 0xFF);
        }

        if (! targetFile.getParentDirectory().createDirectory())
        {
            r.message = "Cannot create directory: "
                       + targetFile.getParentDirectory().getFullPathName();
            return r;
        }

        if (! targetFile.replaceWithData (fileBytes.getData(), fileBytes.getSize()))
        {
            r.message = "Failed to write file: " + targetFile.getFullPathName();
            return r;
        }

        r.ok      = true;
        r.message = "Wrote " + targetFile.getFileName();
        return r;
    }

    std::vector<LoopExporter::Result> LoopExporter::exportAll (
        const juce::AudioBuffer<float>& source,
        double                          sampleRate,
        const std::vector<LoopRegion>&  regions,
        const juce::File&               targetFolder,
        const juce::String&             baseName,
        const Options&                  opts)
    {
        std::vector<Result> all;
        all.reserve (regions.size());

        for (size_t i = 0; i < regions.size(); ++i)
        {
            const auto out = targetFolder.getChildFile (
                baseName + "_loop_" + juce::String ((int) (i + 1)) + ".wav");
            all.push_back (exportRegion (source, sampleRate, regions[i], out, opts));
        }
        return all;
    }
}
