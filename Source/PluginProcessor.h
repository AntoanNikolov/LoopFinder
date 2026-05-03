#pragma once

#include "AudioFileManager.h"
#include "LoopDetector.h"
#include "LoopRegion.h"
#include "PlaybackEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <future>
#include <memory>
#include <mutex>

namespace lf
{
    class LoopFinderProcessor  : public juce::AudioProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        // ---------------------------------------------------------------------
        // ctor / dtor
        // ---------------------------------------------------------------------
        LoopFinderProcessor();
        ~LoopFinderProcessor() override;

        // ---------------------------------------------------------------------
        // AudioProcessor required overrides
        // ---------------------------------------------------------------------
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return "LoopFinder"; }

        bool acceptsMidi()  const override { return true;  }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }

        int getNumPrograms()       override { return 1; }
        int getCurrentProgram()    override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

        // ---------------------------------------------------------------------
        // Public API for the editor
        // ---------------------------------------------------------------------
        AudioFileManager& getFileManager() noexcept { return fileManager; }
        PlaybackEngine&   getPlayback()    noexcept { return playback; }
        juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }
        juce::AudioThumbnailCache& getThumbCache() noexcept { return thumbnailCache; }
        juce::TimeSliceThread&     getThumbThread() noexcept { return thumbThread; }

        /** UI components (e.g. MidiKeyboardComponent) post note events to this
         *  shared state; we drain them into the audio buffer in processBlock.
         */
        juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

        bool loadFile (const juce::File& file);

        void startAnalysis();
        void cancelAnalysis();
        bool isAnalysing() const noexcept       { return analysing.load(); }
        float getAnalysisProgress() const noexcept { return analysisProgress.load(); }

        std::vector<LoopRegion> getRegions() const;
        int  getSelectedRegion() const noexcept { return selectedRegion.load(); }
        void setSelectedRegion (int idx);
        void updateRegion (int idx, int newStart, int newEnd);
        void clearRegions();

        struct Listener
        {
            virtual ~Listener() = default;
            virtual void loopFinderStateChanged() {}
            virtual void loopFinderRegionsChanged() {}
            virtual void loopFinderAnalysisProgress (float) {}
        };

        // NB: named *LoopFinderListener* (not addListener) so we don't
        // shadow juce::AudioProcessor::addListener (AudioProcessorListener*).
        void addLoopFinderListener    (Listener*);
        void removeLoopFinderListener (Listener*);

    private:
        // ---------------------------------------------------------------------
        // helpers
        // ---------------------------------------------------------------------
        void notifyState();
        void notifyRegions();
        void notifyProgress (float);

        void applySelectedRegionToPlayback();
        void rebuildPlaybackSource();

        void runAnalysisJob();

        // juce::AudioProcessorValueTreeState::Listener
        void parameterChanged (const juce::String& paramID, float newValue) override;

        // ---------------------------------------------------------------------
        // members
        // ---------------------------------------------------------------------
        juce::AudioProcessorValueTreeState apvts;

        AudioFileManager fileManager;
        LoopDetector     detector;
        PlaybackEngine   playback;

        // Receives note events from the on-screen MidiKeyboardComponent and
        // is merged with incoming DAW MIDI in processBlock.
        juce::MidiKeyboardState keyboardState;

        // Thumbnail support — owned here so the editor can re-create cheaply.
        juce::TimeSliceThread     thumbThread { "LoopFinderThumbThread" };
        juce::AudioThumbnailCache thumbnailCache { 4 };

        // Region storage — guarded by `regionsLock` for getters from the UI.
        mutable juce::CriticalSection regionsLock;
        std::vector<LoopRegion>       regions;
        std::atomic<int>              selectedRegion { -1 };

        // Background analysis bookkeeping.
        std::future<void>             analysisFuture;
        std::atomic<bool>             analysing        { false };
        std::atomic<bool>             analysisCancel   { false };
        std::atomic<float>            analysisProgress { 0.0f };

        // Listeners (UI-thread only).
        juce::ListenerList<Listener>  listeners;

        // For thread-safe notification from the analysis thread.
        struct AsyncUpdater  : public juce::AsyncUpdater
        {
            std::function<void()> fn;
            void handleAsyncUpdate() override { if (fn) fn(); }
        };
        AsyncUpdater regionsChangedNotifier;
        AsyncUpdater progressNotifier;
        AsyncUpdater stateChangedNotifier;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopFinderProcessor)
    };
}
