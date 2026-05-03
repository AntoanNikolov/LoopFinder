#pragma once

#include "PluginProcessor.h"
#include "RegionListPanel.h"
#include "Theme.h"
#include "WaveformDisplay.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace lf
{
    class LoopFinderEditor  : public juce::AudioProcessorEditor,
                              public juce::FileDragAndDropTarget,
                              private juce::Timer,
                              public  LoopFinderProcessor::Listener
    {
    public:
        explicit LoopFinderEditor (LoopFinderProcessor&);
        ~LoopFinderEditor() override;

        // Component
        void paint (juce::Graphics&) override;
        void resized() override;

        // Drag and drop
        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void filesDropped (const juce::StringArray& files, int x, int y) override;

        // Processor listener
        void loopFinderStateChanged()      override;
        void loopFinderRegionsChanged()    override;
        void loopFinderAnalysisProgress (float) override;

    private:
        void timerCallback() override;

        // Actions
        void doLoadFile();
        void doAnalyze();
        void doExport (int regionIndex);
        void doExportAll();
        void selectRegion (int idx);
        void auditionRegion (int idx);
        void editRegion (int idx, int newStart, int newEnd);

        void updateFileInfo();
        void updateTransportEnablement();

        // Renamed from `processor` to avoid shadowing
        // juce::AudioProcessorEditor::processor.
        LoopFinderProcessor& proc;

        // UI elements
        juce::Label      titleLabel;
        juce::TextButton loadBtn      { "Load File" };
        juce::TextButton analyzeBtn   { "Analyze"   };
        juce::TextButton playBtn      { juce::String::fromUTF8 (u8"▶ Preview") };
        juce::TextButton stopBtn      { juce::String::fromUTF8 (u8"■ Stop") };
        juce::ToggleButton loopBtn    { "Loop" };
        juce::Slider     volumeSlider;
        juce::Label      volumeLabel;

        // Root-note ("which key plays the sample at its native pitch")
        juce::Label      rootLabel;
        juce::Slider     rootSlider;
        juce::Label      rootValueLabel;

        juce::Label      fileInfoLabel;
        juce::Label      messageLabel;     // inline error/info message

        // NB: progressValue MUST be declared before progressBar — the bar
        // holds a reference to it, and members initialise in declaration order.
        double progressValue { 0.0 };
        juce::ProgressBar progressBar { progressValue };

        WaveformDisplay  waveform;
        RegionListPanel  regionList;

        juce::MidiKeyboardComponent keyboard;

        juce::TooltipWindow tooltipWindow { this, 350 };

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rootAttach;

        std::unique_ptr<juce::FileChooser> currentChooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopFinderEditor)
    };
}
