#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>

namespace lf
{
    /** Polyphonic, MIDI-driven loop sample player.
     *
     *  Behaves like a one-zone sampler: each MIDI note triggers a voice that
     *  plays the currently-selected loop region, pitched up or down by the
     *  difference between the note number and the configured root note. While
     *  the note is held the voice loops the region (with the on-the-fly
     *  crossfade applied to the last K source samples for click-free wraps).
     *  On note-off, the voice enters a short linear release.
     *
     *  All cross-thread state is communicated via std::atomic — no mutex is
     *  ever taken on the audio thread. The source buffer pointer is set from
     *  the PluginProcessor only while processing is suspended, so the audio
     *  thread can rely on it being stable for the duration of any single
     *  processBlock call.
     */
    class PlaybackEngine
    {
    public:
        PlaybackEngine();

        // ---------------------------------------------------------------------
        // Lifecycle (called from PluginProcessor)
        // ---------------------------------------------------------------------
        void prepareToPlay (double hostSampleRate, int maxBlockSize);
        void releaseResources();

        /** Replace the source buffer pointer. MUST be called while the host
         *  has the processor suspended (or before the first processBlock).
         *  Pass nullptr to indicate "no file loaded". Active voices are
         *  killed on every swap to avoid reading freed memory.
         */
        void setSourceBuffer (const juce::AudioBuffer<float>* buf,
                              double sourceSampleRate) noexcept;

        // ---------------------------------------------------------------------
        // UI-thread state setters (lock-free)
        // ---------------------------------------------------------------------
        /** Schedule a new region for subsequent voices to play. Active voices
         *  finish on their existing region (so a held note isn't yanked). */
        void setRegion (int startSample, int endSample, int crossfadeSamples) noexcept;
        void clearRegion() noexcept;

        /** "Trigger" buttons — synthesize a MIDI note on/off at the root. */
        void triggerPreview()                     noexcept;
        void releasePreview()                     noexcept;

        void setLoopEnabled (bool shouldLoop)     noexcept;
        void setGainDb (float db)                 noexcept;
        void setRootNote (int midiNote)           noexcept;

        // ---------------------------------------------------------------------
        // UI-thread getters (lock-free)
        // ---------------------------------------------------------------------
        bool  isPlaying()      const noexcept; // true if any voice is sounding
        bool  isLoopEnabled()  const noexcept { return loopFlag.load (std::memory_order_acquire); }
        float getGainDb()      const noexcept { return gainDb.load   (std::memory_order_acquire); }
        int   getRootNote()    const noexcept { return rootNote.load (std::memory_order_acquire); }
        int   getPlayheadSourcePos() const noexcept { return displayPositionSamples.load (std::memory_order_acquire); }
        int   getActiveVoiceCount() const noexcept;

        // ---------------------------------------------------------------------
        // Audio thread
        // ---------------------------------------------------------------------
        /** Render into `buffer`, mixing active voices and consuming MIDI events
         *  in-line. Safe to call with an empty MidiBuffer.
         *  @return true if anything was written.
         */
        bool processBlock (juce::AudioBuffer<float>& buffer,
                           juce::MidiBuffer&         midi) noexcept;

        // Some things (e.g. the Standalone target's preview button) just want
        // an audio-only render. Equivalent to passing an empty MIDI buffer.
        bool processBlock (juce::AudioBuffer<float>& buffer) noexcept
        {
            juce::MidiBuffer empty;
            return processBlock (buffer, empty);
        }

        // ---------------------------------------------------------------------
        // Note triggering — invoked either by MIDI events on the audio thread
        // or by triggerPreview() from the UI thread.
        // ---------------------------------------------------------------------
        void handleNoteOn  (int midiNote, float velocity) noexcept;
        void handleNoteOff (int midiNote)                 noexcept;
        void releaseAllVoices()                           noexcept;

    private:
        // ---------------------------------------------------------------------
        // Voice
        // ---------------------------------------------------------------------
        struct Voice
        {
            std::atomic<bool> active   { false };
            bool   releasing          { false };
            int    midiNote           { -1 };
            int    age                { 0 };          // for voice stealing
            float  velocity           { 1.0f };
            double pitchRatio         { 1.0 };
            double positionInLoop     { 0.0 };        // in source samples
            int    activeStart        { 0 };
            int    activeEnd          { 0 };
            int    activeK            { 0 };
            float  envelope           { 0.0f };       // current AR amplitude
        };

        static constexpr int maxVoices = 16;
        std::array<Voice, maxVoices> voices {};

        // Voice management (audio-thread only)
        Voice* findVoiceForNote (int midiNote) noexcept;
        Voice* findFreeOrSteal  () noexcept;
        void   startVoice (Voice&, int midiNote, float velocity) noexcept;
        void   renderVoiceChunk (Voice&, juce::AudioBuffer<float>&,
                                 int startFrame, int numFrames, float blockGain) noexcept;
        float  readSample (int channel, double sourcePos) const noexcept;

        // Source data — raw pointer is safe because PluginProcessor swaps
        // it only while processing is suspended.
        const juce::AudioBuffer<float>* sourceBuffer { nullptr };
        double sourceSampleRate { 44100.0 };
        double hostSampleRate   { 44100.0 };

        // Pending region (UI → audio thread)
        std::atomic<int>  pendingStart      { 0 };
        std::atomic<int>  pendingEnd        { 0 };
        std::atomic<int>  pendingCrossfadeK { 0 };

        // Gain & loop settings
        std::atomic<bool>  loopFlag    { true  };
        std::atomic<float> gainDb      { 0.0f };
        std::atomic<int>   rootNote    { 60 };  // C4

        // Cross-thread "preview note" requests from the UI Play button.
        std::atomic<bool>  previewOnPending  { false };
        std::atomic<bool>  previewOffPending { false };

        // Smoothed master gain (audio-thread only)
        float smoothedGain { 1.0f };

        // Read by the UI for playhead drawing.
        std::atomic<int>   displayPositionSamples { 0 };

        // Envelope coefficients (computed from sample rate in prepareToPlay).
        float attackStep  { 0.005f };
        float releaseStep { 0.0005f };

        // Preview voice uses this sentinel note number so the Play/Stop UI
        // buttons can address it independently from real MIDI keys.
        static constexpr int previewNoteMagic = -1;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackEngine)
    };
}
