#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>

namespace lf
{
    namespace
    {
        // -- APVTS layout -----------------------------------------------------
        juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
        {
            std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
            p.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "gainDb", 1 },
                "Output Volume",
                juce::NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f,
                juce::AudioParameterFloatAttributes()
                    .withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) {
                        return juce::String (v, 1) + " dB"; })));

            p.push_back (std::make_unique<juce::AudioParameterInt> (
                juce::ParameterID { "rootNote", 1 },
                "Root Note",
                0, 127, 60,
                juce::AudioParameterIntAttributes()
                    .withStringFromValueFunction ([] (int v, int) {
                        return juce::MidiMessage::getMidiNoteName (v, true, true, 4); })));

            return { p.begin(), p.end() };
        }
    }

    // =========================================================================
    LoopFinderProcessor::LoopFinderProcessor()
        : juce::AudioProcessor (BusesProperties()
              // No audio input — we are a sampler-style instrument.
              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "LoopFinderState", makeLayout())
    {
        // Forward parameters into the playback engine without allocating
        // a heap-owned listener. APVTS does not delete its listeners and
        // we own ours as long as the processor is alive.
        apvts.addParameterListener ("gainDb",   this);
        apvts.addParameterListener ("rootNote", this);

        if (auto* p = apvts.getRawParameterValue ("gainDb"))
            playback.setGainDb (p->load());
        if (auto* p = apvts.getRawParameterValue ("rootNote"))
            playback.setRootNote (static_cast<int> (p->load()));

        thumbThread.startThread (juce::Thread::Priority::low);

        regionsChangedNotifier.fn = [this] { listeners.call (&Listener::loopFinderRegionsChanged); };
        progressNotifier.fn       = [this] { listeners.call (&Listener::loopFinderAnalysisProgress,
                                                             analysisProgress.load()); };
        stateChangedNotifier.fn   = [this] { listeners.call (&Listener::loopFinderStateChanged); };
    }

    LoopFinderProcessor::~LoopFinderProcessor()
    {
        apvts.removeParameterListener ("gainDb",   this);
        apvts.removeParameterListener ("rootNote", this);
        cancelAnalysis();
        thumbThread.stopThread (1000);
    }

    void LoopFinderProcessor::parameterChanged (const juce::String& paramID,
                                                float newValue)
    {
        if      (paramID == "gainDb")   playback.setGainDb   (newValue);
        else if (paramID == "rootNote") playback.setRootNote (static_cast<int> (newValue));
    }

    // -------------------------------------------------------------------------
    // AudioProcessor lifecycle
    // -------------------------------------------------------------------------
    void LoopFinderProcessor::prepareToPlay (double sr, int blockSize)
    {
        playback.prepareToPlay (sr, blockSize);
        rebuildPlaybackSource();
    }

    void LoopFinderProcessor::releaseResources()
    {
        playback.releaseResources();
    }

    bool LoopFinderProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        // As an instrument plugin we have no audio input bus — only an
        // output. Accept mono or stereo on the output.
        const auto out = layouts.getMainOutputChannelSet();
        return out == juce::AudioChannelSet::mono()
            || out == juce::AudioChannelSet::stereo();
    }

    void LoopFinderProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
    {
        juce::ScopedNoDenormals noDenormals;

        // Merge any events posted by the on-screen MIDI keyboard into the
        // host-supplied MIDI buffer (`true` = inject as indirect events).
        keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

        playback.processBlock (buffer, midi);
    }

    juce::AudioProcessorEditor* LoopFinderProcessor::createEditor()
    {
        return new LoopFinderEditor (*this);
    }

    // -------------------------------------------------------------------------
    // State persistence
    // -------------------------------------------------------------------------
    void LoopFinderProcessor::getStateInformation (juce::MemoryBlock& destData)
    {
        juce::ValueTree root ("LoopFinder");

        // APVTS state (parameters)
        root.addChild (apvts.copyState(), -1, nullptr);

        // Audio file
        root.addChild (fileManager.toValueTree(), -1, nullptr);

        // Regions
        juce::ValueTree regionsNode ("Regions");
        regionsNode.setProperty ("selected", selectedRegion.load(), nullptr);
        {
            const juce::ScopedLock sl (regionsLock);
            for (const auto& r : regions)
            {
                juce::ValueTree n ("Region");
                n.setProperty ("start",       r.startSample,  nullptr);
                n.setProperty ("end",         r.endSample,    nullptr);
                n.setProperty ("score",       r.score,        nullptr);
                n.setProperty ("durationMs",  r.durationMs,   nullptr);
                n.setProperty ("hasCrossfade",r.hasCrossfade, nullptr);
                regionsNode.addChild (n, -1, nullptr);
            }
        }
        root.addChild (regionsNode, -1, nullptr);

        if (auto xml = root.createXml())
            copyXmlToBinary (*xml, destData);
    }

    void LoopFinderProcessor::setStateInformation (const void* data, int sizeInBytes)
    {
        std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
        if (xml == nullptr) return;
        const auto root = juce::ValueTree::fromXml (*xml);
        if (! root.hasType ("LoopFinder")) return;

        // Parameters
        if (auto state = root.getChildWithName (apvts.state.getType()); state.isValid())
            apvts.replaceState (state);

        // Audio file (must happen while suspended for clean buffer swap)
        suspendProcessing (true);
        fileManager.fromValueTree (root.getChildWithName ("AudioFile"));
        rebuildPlaybackSource();
        suspendProcessing (false);

        // Regions
        std::vector<LoopRegion> loaded;
        int selected = -1;
        if (auto regs = root.getChildWithName ("Regions"); regs.isValid())
        {
            selected = static_cast<int> (regs.getProperty ("selected", -1));
            for (auto child : regs)
            {
                if (! child.hasType ("Region")) continue;
                LoopRegion r;
                r.startSample  = static_cast<int>  (child.getProperty ("start"));
                r.endSample    = static_cast<int>  (child.getProperty ("end"));
                r.score        = static_cast<float>(static_cast<double> (child.getProperty ("score")));
                r.durationMs   = static_cast<float>(static_cast<double> (child.getProperty ("durationMs")));
                r.hasCrossfade = static_cast<bool> (child.getProperty ("hasCrossfade", false));
                loaded.push_back (r);
            }
        }
        {
            const juce::ScopedLock sl (regionsLock);
            regions = std::move (loaded);
        }
        selectedRegion.store (selected);
        applySelectedRegionToPlayback();

        notifyState();
        notifyRegions();
    }

    // -------------------------------------------------------------------------
    // Public API
    // -------------------------------------------------------------------------
    bool LoopFinderProcessor::loadFile (const juce::File& file)
    {
        cancelAnalysis();
        suspendProcessing (true);
        const bool ok = fileManager.loadFile (file);
        rebuildPlaybackSource();
        {
            const juce::ScopedLock sl (regionsLock);
            regions.clear();
        }
        selectedRegion.store (-1);
        playback.clearRegion();
        playback.releaseAllVoices();
        suspendProcessing (false);

        notifyState();
        notifyRegions();
        return ok;
    }

    void LoopFinderProcessor::rebuildPlaybackSource()
    {
        if (fileManager.isLoaded())
            playback.setSourceBuffer (&fileManager.getAudioBuffer(),
                                      fileManager.getMetadata().sampleRate);
        else
            playback.setSourceBuffer (nullptr, 44100.0);
    }

    std::vector<LoopRegion> LoopFinderProcessor::getRegions() const
    {
        const juce::ScopedLock sl (regionsLock);
        return regions;
    }

    void LoopFinderProcessor::setSelectedRegion (int idx)
    {
        selectedRegion.store (idx);
        applySelectedRegionToPlayback();
        notifyState();
    }

    void LoopFinderProcessor::updateRegion (int idx, int newStart, int newEnd)
    {
        {
            const juce::ScopedLock sl (regionsLock);
            if (idx < 0 || idx >= static_cast<int> (regions.size())) return;
            auto& r = regions[(size_t) idx];
            r.startSample = newStart;
            r.endSample   = newEnd;
            const double sr = fileManager.getMetadata().sampleRate > 0.0
                                ? fileManager.getMetadata().sampleRate : 44100.0;
            r.durationMs  = static_cast<float> ((r.endSample - r.startSample) * 1000.0 / sr);
        }
        if (idx == selectedRegion.load())
            applySelectedRegionToPlayback();
        notifyRegions();
    }

    void LoopFinderProcessor::clearRegions()
    {
        {
            const juce::ScopedLock sl (regionsLock);
            regions.clear();
        }
        selectedRegion.store (-1);
        playback.clearRegion();
        notifyRegions();
    }

    void LoopFinderProcessor::applySelectedRegionToPlayback()
    {
        const int idx = selectedRegion.load();
        const juce::ScopedLock sl (regionsLock);
        if (idx < 0 || idx >= static_cast<int> (regions.size()))
        {
            playback.clearRegion();
            return;
        }
        const auto& r = regions[(size_t) idx];
        const double sr = fileManager.getMetadata().sampleRate > 0.0
                            ? fileManager.getMetadata().sampleRate : 44100.0;
        const int K = r.hasCrossfade
                        ? juce::jmax (1, static_cast<int> (std::lround (0.002 * sr)))
                        : 0;
        playback.setRegion (r.startSample, r.endSample, K);
    }

    // -------------------------------------------------------------------------
    // Analysis
    // -------------------------------------------------------------------------
    void LoopFinderProcessor::startAnalysis()
    {
        if (! fileManager.isLoaded() || analysing.load())
            return;

        cancelAnalysis(); // ensure prior job is fully done
        analysisCancel.store (false);
        analysisProgress.store (0.0f);
        analysing.store (true);
        notifyState();

        analysisFuture = std::async (std::launch::async, [this] { runAnalysisJob(); });
    }

    void LoopFinderProcessor::cancelAnalysis()
    {
        analysisCancel.store (true);
        if (analysisFuture.valid())
        {
            try { analysisFuture.get(); }
            catch (...) {}
        }
        analysing.store (false);
    }

    void LoopFinderProcessor::runAnalysisJob()
    {
        try
        {
            LoopDetector::Settings s;
            s.sampleRate = static_cast<float> (fileManager.getMetadata().sampleRate);

            const auto& mono = fileManager.getMonoMix();
            auto found = detector.analyze (
                mono.data(),
                static_cast<int> (mono.size()),
                s,
                [this] (float p) { analysisProgress.store (p); progressNotifier.triggerAsyncUpdate(); },
                &analysisCancel);

            {
                const juce::ScopedLock sl (regionsLock);
                regions = std::move (found);
            }
            selectedRegion.store (regions.empty() ? -1 : 0);
        }
        catch (const std::exception&)
        {
            // Swallow — analysis must never crash the plugin.
            const juce::ScopedLock sl (regionsLock);
            regions.clear();
            selectedRegion.store (-1);
        }

        analysing.store (false);
        analysisProgress.store (1.0f);
        applySelectedRegionToPlayback();
        progressNotifier.triggerAsyncUpdate();
        regionsChangedNotifier.triggerAsyncUpdate();
        stateChangedNotifier.triggerAsyncUpdate();
    }

    // -------------------------------------------------------------------------
    // Listeners
    // -------------------------------------------------------------------------
    void LoopFinderProcessor::addLoopFinderListener    (Listener* l) { listeners.add (l); }
    void LoopFinderProcessor::removeLoopFinderListener (Listener* l) { listeners.remove (l); }

    void LoopFinderProcessor::notifyState()    { stateChangedNotifier.triggerAsyncUpdate(); }
    void LoopFinderProcessor::notifyRegions()  { regionsChangedNotifier.triggerAsyncUpdate(); }
    void LoopFinderProcessor::notifyProgress (float) { progressNotifier.triggerAsyncUpdate(); }
}

// =============================================================================
// JUCE plugin entry point
// =============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new lf::LoopFinderProcessor();
}
