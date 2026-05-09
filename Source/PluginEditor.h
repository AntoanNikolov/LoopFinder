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
    /** A TextButton that draws a vector play / stop / no icon to the left of
     *  its label. We render the glyph via juce::Path instead of relying on
     *  Unicode play / stop characters, which don't render reliably across
     *  platforms (Windows often shows tofu boxes for U+25B6 / U+25A0 because
     *  the system fallback font lacks those glyphs).
     */
    class TransportButton  : public juce::TextButton
    {
    public:
        enum class Icon { None, Play, Stop };

        TransportButton (Icon ic, const juce::String& label)
            : juce::TextButton (label), iconKind (ic) {}

        void paintButton (juce::Graphics& g,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override
        {
            auto& lf = getLookAndFeel();
            lf.drawButtonBackground (g, *this,
                findColour (getToggleState() ? buttonOnColourId : buttonColourId),
                shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

            const auto bounds = getLocalBounds().toFloat();
            constexpr float iconAreaW = 22.0f;

            // ----- icon -----
            if (iconKind != Icon::None)
            {
                juce::Path p;
                const float cx = 11.0f;
                const float cy = bounds.getCentreY();
                const float s  = 4.5f;

                if (iconKind == Icon::Play)
                {
                    // Right-pointing triangle, centred and visually balanced.
                    p.addTriangle (cx - s * 0.7f, cy - s,
                                   cx - s * 0.7f, cy + s,
                                   cx + s * 0.9f, cy);
                }
                else // Stop
                {
                    p.addRectangle (cx - s * 0.85f, cy - s * 0.85f,
                                    s * 1.7f,       s * 1.7f);
                }

                g.setColour (findColour (textColourOffId)
                                .withMultipliedAlpha (isEnabled() ? 1.0f : 0.5f));
                g.fillPath (p);
            }

            // ----- text (centred in the area to the right of the icon) -----
            const auto font = lf.getTextButtonFont (*this, getHeight());
            g.setFont (font);
            g.setColour (findColour (getToggleState() ? textColourOnId : textColourOffId)
                            .withMultipliedAlpha (isEnabled() ? 1.0f : 0.5f));

            const int yIndent  = juce::jmin (4, juce::roundToInt (getHeight() * 0.3f));
            const int textX    = (int) iconAreaW;
            const int textW    = getWidth() - textX - 6;
            if (textW > 0)
                g.drawFittedText (getButtonText(),
                                  textX, yIndent, textW, getHeight() - yIndent * 2,
                                  juce::Justification::centred, 2);
        }

    private:
        Icon iconKind { Icon::None };
    };

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
        void selectRegion (int idx);
        void auditionRegion (int idx);
        void editRegion (int idx, int newStart, int newEnd);

        void updateFileInfo();
        void updateTransportEnablement();

        // Renamed from `processor` to avoid shadowing
        // juce::AudioProcessorEditor::processor.
        LoopFinderProcessor& proc;

        // UI elements
        juce::Label       titleLabel;
        juce::TextButton  loadBtn     { "Load File" };
        juce::TextButton  analyzeBtn  { "Analyze"   };
        TransportButton   playBtn          { TransportButton::Icon::Play, "Preview" };
        TransportButton   stopBtn          { TransportButton::Icon::Stop, "Stop"    };
        juce::ToggleButton midiFromStartBtn { "From start" };
        juce::ToggleButton loopBtn          { "Loop" };
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

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   volumeAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   rootAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   midiFromStartAttach;

        std::unique_ptr<juce::FileChooser> currentChooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopFinderEditor)
    };
}
