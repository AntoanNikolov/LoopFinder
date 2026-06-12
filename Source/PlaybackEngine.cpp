#include "PlaybackEngine.h"

#include <algorithm>
#include <cmath>

namespace lf
{
    PlaybackEngine::PlaybackEngine() = default;

    void PlaybackEngine::prepareToPlay (double rate, int /*maxBlockSize*/)
    {
        hostSampleRate = (rate > 0.0 ? rate : 44100.0);
        smoothedGain   = std::pow (10.0f, gainDb.load() / 20.0f);

        // Per-sample step for a 5 ms attack / 50 ms release.
        attackStep  = 1.0f / static_cast<float> (juce::jmax (1.0, hostSampleRate * 0.005));
        releaseStep = 1.0f / static_cast<float> (juce::jmax (1.0, hostSampleRate * 0.050));

        releaseAllVoices();
    }

    void PlaybackEngine::releaseResources()
    {
        releaseAllVoices();
    }

    void PlaybackEngine::setSourceBuffer (const juce::AudioBuffer<float>* buf,
                                          double sourceRate) noexcept
    {
        // We are guaranteed by PluginProcessor that processing is suspended
        // when this is called, so it's safe to mutate everything.
        releaseAllVoices();
        sourceBuffer     = buf;
        sourceSampleRate = (sourceRate > 0.0 ? sourceRate : 44100.0);
    }

    void PlaybackEngine::setRegion (int s, int e, int k) noexcept
    {
        pendingStart      .store (s, std::memory_order_release);
        pendingEnd        .store (e, std::memory_order_release);
        pendingCrossfadeK .store (std::max (0, k), std::memory_order_release);
    }

    void PlaybackEngine::clearRegion() noexcept       { setRegion (0, 0, 0); }
    void PlaybackEngine::triggerPreview() noexcept
    {
        previewOnPending.store (true, std::memory_order_release);
    }

    void PlaybackEngine::releasePreview() noexcept
    {
        previewOffPending.store (true, std::memory_order_release);
    }

    void PlaybackEngine::setMidiIntroFromFileStartEnabled (bool on) noexcept
    {
        midiIntroFromFileStartEnabled.store (on, std::memory_order_release);
    }

    void PlaybackEngine::setLoopEnabled (bool s) noexcept { loopFlag.store (s,                       std::memory_order_release); }
    void PlaybackEngine::setGainDb     (float db) noexcept { gainDb.store   (juce::jlimit (-60.0f, 6.0f, db), std::memory_order_release); }
    void PlaybackEngine::setRootNote   (int n)    noexcept { rootNote.store (juce::jlimit (0, 127, n),       std::memory_order_release); }

    bool PlaybackEngine::isPlaying() const noexcept
    {
        for (const auto& v : voices)
            if (v.active.load (std::memory_order_acquire))
                return true;
        return false;
    }

    int PlaybackEngine::getActiveVoiceCount() const noexcept
    {
        int n = 0;
        for (const auto& v : voices)
            if (v.active.load (std::memory_order_acquire))
                ++n;
        return n;
    }

    // -------------------------------------------------------------------------
    // Voice management (audio thread)
    // -------------------------------------------------------------------------
    PlaybackEngine::Voice* PlaybackEngine::findVoiceForNote (int midiNote) noexcept
    {
        for (auto& v : voices)
            if (v.active.load (std::memory_order_acquire) && v.midiNote == midiNote)
                return &v;
        return nullptr;
    }

    PlaybackEngine::Voice* PlaybackEngine::findFreeOrSteal() noexcept
    {
        // Prefer a free voice.
        for (auto& v : voices)
            if (! v.active.load (std::memory_order_acquire))
                return &v;

        // Otherwise steal the oldest releasing voice; failing that, the
        // overall oldest voice.
        Voice* victim = nullptr;
        int    bestAge = -1;
        for (auto& v : voices)
            if (v.releasing && v.age > bestAge) { victim = &v; bestAge = v.age; }
        if (victim != nullptr) return victim;

        for (auto& v : voices)
            if (v.age > bestAge) { victim = &v; bestAge = v.age; }
        return victim;
    }

    void PlaybackEngine::startVoice (Voice& v, int midiNote, float velocity,
                                     bool introFromFileStartRequested) noexcept
    {
        const int s = pendingStart.load (std::memory_order_acquire);
        const int e = pendingEnd.load   (std::memory_order_acquire);

        const int sourceLen = sourceBuffer != nullptr ? sourceBuffer->getNumSamples() : 0;
        v.activeStart = juce::jlimit (0, sourceLen, s);
        v.activeEnd   = (e > s && e <= sourceLen) ? e : sourceLen;
        v.activeK     = std::min (pendingCrossfadeK.load (std::memory_order_acquire),
                                  juce::jmax (0, (v.activeEnd - v.activeStart) / 2));
        v.midiNote    = midiNote;
        v.velocity    = juce::jlimit (0.0f, 1.0f, velocity);
        v.releasing   = false;
        v.envelope    = 0.0f;
        v.positionInLoop = 0.0;
        v.age         = 0;

        v.introFromFileStart = introFromFileStartRequested && (v.activeStart > 0);

        const int referenceNote = (midiNote == previewNoteMagic ? rootNote.load() : midiNote);
        const int delta = referenceNote - rootNote.load();
        const double tune = (double) tuneCents.load (std::memory_order_acquire) / 100.0;
        v.pitchRatio  = std::pow (2.0, (static_cast<double> (delta) + tune) / 12.0);
        v.active.store (true, std::memory_order_release);
    }

    void PlaybackEngine::handleNoteOn (int midiNote, float velocity) noexcept
    {
        if (sourceBuffer == nullptr || sourceBuffer->getNumSamples() == 0)
            return;

        const bool isPreview = (midiNote == previewNoteMagic);
        const bool midiIntro = (! isPreview)
                               && midiIntroFromFileStartEnabled.load (std::memory_order_acquire);

        if (midiIntro)
            releaseAllVoices();

        const bool introRequested = midiIntro;

        if (auto* existing = findVoiceForNote (midiNote))
        {
            startVoice (*existing, midiNote, velocity, introRequested);
            return;
        }
        if (auto* v = findFreeOrSteal())
            startVoice (*v, midiNote, velocity, introRequested);
    }

    void PlaybackEngine::handleNoteOff (int midiNote) noexcept
    {
        if (auto* v = findVoiceForNote (midiNote))
            v->releasing = true;
    }

    void PlaybackEngine::releaseAllVoices() noexcept
    {
        for (auto& v : voices)
        {
            v.active.store (false, std::memory_order_release);
            v.releasing = false;
            v.envelope  = 0.0f;
            v.positionInLoop = 0.0;
            v.introFromFileStart = false;
        }
    }

    // -------------------------------------------------------------------------
    // Sample reading
    // -------------------------------------------------------------------------
    float PlaybackEngine::readSample (int channel, double pos) const noexcept
    {
        if (sourceBuffer == nullptr) return 0.0f;
        const int n = sourceBuffer->getNumSamples();
        if (n <= 0) return 0.0f;

        if (pos < 0.0 || pos >= static_cast<double> (n) - 1.0)
            return 0.0f;

        const int   i0   = static_cast<int> (pos);
        const float frac = static_cast<float> (pos - static_cast<double> (i0));
        const int   ch   = juce::jlimit (0, sourceBuffer->getNumChannels() - 1, channel);

        const float* data = sourceBuffer->getReadPointer (ch);
        return data[i0] + (data[i0 + 1] - data[i0]) * frac;
    }

    // -------------------------------------------------------------------------
    // Per-voice rendering
    // -------------------------------------------------------------------------
    void PlaybackEngine::renderVoiceChunk (Voice& v,
                                           juce::AudioBuffer<float>& buffer,
                                           int startFrame, int numFrames,
                                           float blockGain) noexcept
    {
        if (! v.active.load (std::memory_order_acquire))
            return;

        const int srcLen = sourceBuffer != nullptr ? sourceBuffer->getNumSamples() : 0;
        const int s = juce::jlimit (0, srcLen, v.activeStart);
        const int e = (v.activeEnd > s && v.activeEnd <= srcLen) ? v.activeEnd : srcLen;
        const int loopLen = e - s;
        if (loopLen <= 1) { v.active.store (false, std::memory_order_release); return; }

        const int K     = std::min (v.activeK, loopLen / 2);
        const bool loop = loopFlag.load (std::memory_order_acquire);
        const int  numCh = buffer.getNumChannels();

        const double baseStep = (sourceSampleRate > 0.0 ? sourceSampleRate / hostSampleRate : 1.0);
        const double step     = baseStep * v.pitchRatio;

        const double tailStart = static_cast<double> (loopLen - K);

        for (int f = startFrame; f < startFrame + numFrames; ++f)
        {
            // Envelope step
            if (v.releasing)
            {
                v.envelope -= releaseStep;
                if (v.envelope <= 0.0f)
                {
                    v.envelope = 0.0f;
                    v.active.store (false, std::memory_order_release);
                    return;
                }
            }
            else if (v.envelope < 1.0f)
            {
                v.envelope = std::min (1.0f, v.envelope + attackStep);
            }

            const float voiceGain = v.envelope * v.velocity * blockGain;

            // ----- Intro: absolute file position until loop region start -----
            if (v.introFromFileStart)
            {
                double absPos = v.positionInLoop;
                const double loopBegin = static_cast<double> (v.activeStart);
                const double lastReadable = static_cast<double> (juce::jmax (0, srcLen - 1));

                if (absPos >= loopBegin)
                {
                    v.introFromFileStart = false;
                    v.positionInLoop = absPos - loopBegin;
                }
                else if (srcLen > 0 && absPos >= lastReadable)
                {
                    v.introFromFileStart = false;
                    v.positionInLoop = 0.0;
                }
                else
                {
                    for (int ch = 0; ch < numCh; ++ch)
                        buffer.addSample (ch, f, readSample (ch, absPos) * voiceGain);

                    v.positionInLoop += step;
                    continue;
                }
            }

            // ----- Loop region (relative positionInLoop) -----
            float blendAlpha = 0.0f;
            if (K > 0 && v.positionInLoop >= tailStart)
            {
                const double t = (v.positionInLoop - tailStart) / static_cast<double> (K);
                blendAlpha = static_cast<float> (juce::jlimit (0.0, 1.0, t));
            }

            for (int ch = 0; ch < numCh; ++ch)
            {
                float sample = readSample (ch, static_cast<double> (s) + v.positionInLoop);
                if (blendAlpha > 0.0f)
                {
                    const double headPos = static_cast<double> (s)
                                         + (v.positionInLoop - tailStart);
                    const float h = readSample (ch, headPos);
                    sample = (1.0f - blendAlpha) * sample + blendAlpha * h;
                }
                buffer.addSample (ch, f, sample * voiceGain);
            }

            v.positionInLoop += step;
            if (v.positionInLoop >= static_cast<double> (loopLen))
            {
                if (loop)
                {
                    // Skip-K wrap: the next sample after the blend continues
                    // naturally from sig[S+K], not sig[S].
                    v.positionInLoop -= static_cast<double> (loopLen - K);
                }
                else
                {
                    v.releasing = true;
                }
            }
        }

        ++v.age;
    }

    // -------------------------------------------------------------------------
    // Audio thread entry point
    // -------------------------------------------------------------------------
    bool PlaybackEngine::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&         midi) noexcept
    {
        const int numFrames = buffer.getNumSamples();
        const int numCh     = buffer.getNumChannels();
        if (numFrames <= 0 || numCh <= 0)
            return false;

        // Always clear — as a synth we own the output.
        buffer.clear();

        // Service preview-note requests from the UI thread.
        if (previewOnPending.exchange (false, std::memory_order_acq_rel))
            handleNoteOn (previewNoteMagic, 1.0f);
        if (previewOffPending.exchange (false, std::memory_order_acq_rel))
            handleNoteOff (previewNoteMagic);

        const float targetGainLin = std::pow (10.0f,
                                              gainDb.load (std::memory_order_acquire) / 20.0f);

        // Walk MIDI events, rendering voices in chunks between them.
        int lastSample = 0;
        for (const auto meta : midi)
        {
            const int evSample = juce::jlimit (0, numFrames, meta.samplePosition);
            const int chunk    = evSample - lastSample;
            if (chunk > 0)
                for (auto& v : voices)
                    renderVoiceChunk (v, buffer, lastSample, chunk, smoothedGain);
            lastSample = evSample;

            const auto& m = meta.getMessage();
            if (m.isNoteOn())
                handleNoteOn  (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff())
                handleNoteOff (m.getNoteNumber());
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                releaseAllVoices();
        }
        const int remaining = numFrames - lastSample;
        if (remaining > 0)
            for (auto& v : voices)
                renderVoiceChunk (v, buffer, lastSample, remaining, smoothedGain);

        // Smooth global gain at the block boundary (one update per block).
        smoothedGain += 0.25f * (targetGainLin - smoothedGain);

        // Publish a "playhead position" for the UI — pick the most recently
        // started active voice as a heuristic.
        int   pubPos = 0;
        bool  any    = false;
        for (const auto& v : voices)
        {
            if (v.active.load (std::memory_order_acquire))
            {
                any = true;
                pubPos = v.introFromFileStart
                             ? static_cast<int> (v.positionInLoop)
                             : v.activeStart + static_cast<int> (v.positionInLoop);
                break; // first match is fine for a status display
            }
        }
        displayPositionSamples.store (any ? pubPos : 0, std::memory_order_release);

        return any;
    }
}
