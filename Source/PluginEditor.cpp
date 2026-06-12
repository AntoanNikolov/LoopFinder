#include "PluginEditor.h"

namespace lf
{
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    namespace
    {
        constexpr int headerH   = 48;
        constexpr int footerH   = 92;
        constexpr int keyboardH = 84;
        constexpr int rightW    = 250;
    }

    // =========================================================================
    LoopFinderEditor::LoopFinderEditor (LoopFinderProcessor& p)
        : juce::AudioProcessorEditor (&p),
          proc (p),
          waveform (p),
          regionList (p),
          keyboard (p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        setLookAndFeel (&lookAndFeel);
        setSize (theme::defaultWidth, theme::defaultHeight + keyboardH);
        setResizable (false, false);

        // Header buttons
        addAndMakeVisible (loadBtn);
        loadBtn.onClick = [this] { doLoadFile(); };

        addAndMakeVisible (analyzeBtn);
        analyzeBtn.setColour (juce::TextButton::buttonColourId, theme::accent);
        analyzeBtn.setColour (juce::TextButton::textColourOffId, theme::background);
        analyzeBtn.setTooltip ("Search for seamless loop points. Drag on the waveform "
                               "first to limit the search to a highlighted section.");
        analyzeBtn.onClick = [this] { doAnalyze(); };

        // Detected key + tune-to-C
        addAndMakeVisible (keyBadge);
        keyBadge.setTooltip ("Detected key of the loaded sample (fundamental pitch).");

        addAndMakeVisible (tuneBtn);
        tuneBtn.setTooltip ("Retune the sample so its fundamental sits exactly on the "
                            "nearest C. Fine-adjust with the Tune slider below.");
        tuneBtn.onClick = [this]
        {
            const auto keyBefore = proc.getDetectedKeyText();
            const float cents = proc.tuneDetectedToC();
            if (! proc.hasDetectedKey()) return;
            if (std::abs (cents) < 0.5f)
                showMessage ("Sample is already on C.", theme::success);
            else
                showMessage ("Tuned " + keyBefore + " to C ("
                             + (cents > 0 ? "+" : "") + juce::String ((int) cents) + " ct).",
                             theme::success);
        };

        // Transport row — Preview button synthesizes a held note at the root
        // pitch via the playback engine's preview channel.
        addAndMakeVisible (playBtn);
        playBtn.onClick = [this] {
            if (proc.getSelectedRegion() < 0 && proc.getRegions().empty())
                proc.getPlayback().setRegion (0, (int) proc.getFileManager().getMetadata().numSamples, 0);
            proc.getPlayback().triggerPreview();
            updateTransportEnablement();
        };

        addAndMakeVisible (stopBtn);
        stopBtn.onClick = [this] { proc.getPlayback().releasePreview(); updateTransportEnablement(); };

        addAndMakeVisible (midiFromStartBtn);
        midiFromStartBtn.setTooltip (
            "When on, each MIDI trigger plays from the start of the file, then loops the region. "
            "Only one voice plays at a time so hits never stack.");
        midiFromStartAttach = std::make_unique<ButtonAttach> (proc.getApvts(), "midiFromStart", midiFromStartBtn);

        addAndMakeVisible (loopBtn);
        loopBtn.setToggleState (proc.getPlayback().isLoopEnabled(), juce::dontSendNotification);
        loopBtn.onClick = [this] { proc.getPlayback().setLoopEnabled (loopBtn.getToggleState()); };

        addAndMakeVisible (volumeLabel);
        volumeLabel.setText ("Vol", juce::dontSendNotification);
        volumeLabel.setFont (theme::label());
        volumeLabel.setColour (juce::Label::textColourId, theme::textSecondary);

        addAndMakeVisible (volumeSlider);
        volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
        volumeSlider.setRange (-60.0, 6.0, 0.1);
        // NB: no setTextValueSuffix — the parameter's text function already
        // appends the unit, so a slider suffix would print it twice.
        volumeAttach = std::make_unique<SliderAttach> (proc.getApvts(), "gainDb", volumeSlider);

        // Tune (cents) — double-click resets to 0
        addAndMakeVisible (tuneLabel);
        tuneLabel.setText ("Tune", juce::dontSendNotification);
        tuneLabel.setFont (theme::label());
        tuneLabel.setColour (juce::Label::textColourId, theme::textSecondary);

        addAndMakeVisible (tuneSlider);
        tuneSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        tuneSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
        tuneSlider.setRange (-1200.0, 1200.0, 1.0);
        tuneSlider.setDoubleClickReturnValue (true, 0.0);
        tuneSlider.setTooltip ("Global tuning in cents (100 ct = 1 semitone). "
                               "Double-click to reset.");
        tuneAttach = std::make_unique<SliderAttach> (proc.getApvts(), "tuneCents", tuneSlider);

        // Root-note row
        addAndMakeVisible (rootLabel);
        rootLabel.setText ("Root", juce::dontSendNotification);
        rootLabel.setFont (theme::label());
        rootLabel.setColour (juce::Label::textColourId, theme::textSecondary);

        addAndMakeVisible (rootSlider);
        rootSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        rootSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        rootSlider.setRange (0.0, 127.0, 1.0);
        rootAttach = std::make_unique<SliderAttach> (proc.getApvts(), "rootNote", rootSlider);

        addAndMakeVisible (rootValueLabel);
        rootValueLabel.setFont (theme::mono (11.0f));
        rootValueLabel.setColour (juce::Label::textColourId, theme::textPrimary);
        rootValueLabel.setJustificationType (juce::Justification::centredRight);
        rootSlider.onValueChange = [this] {
            const int n = (int) rootSlider.getValue();
            rootValueLabel.setText (juce::MidiMessage::getMidiNoteName (n, true, true, 4)
                                  + " (" + juce::String (n) + ")",
                                    juce::dontSendNotification);
            keyboard.repaint(); // re-highlight root key
        };
        rootSlider.onValueChange();

        // On-screen MIDI keyboard
        addAndMakeVisible (keyboard);
        keyboard.setLowestVisibleKey (36);  // C2
        keyboard.setKeyWidth (16.0f);
        keyboard.setOctaveForMiddleC (4);
        keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId,        juce::Colour (0xFFEDE6D8));
        keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId,        juce::Colour (0xFF221E19));
        keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, theme::border);
        keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                            theme::accent.withAlpha (0.35f));
        keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
                            theme::accent.withAlpha (0.7f));
        keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, theme::textDim);

        // Status
        addAndMakeVisible (fileInfoLabel);
        fileInfoLabel.setFont (theme::mono (10.5f));
        fileInfoLabel.setColour (juce::Label::textColourId, theme::textSecondary);

        addAndMakeVisible (messageLabel);
        messageLabel.setFont (theme::label());
        messageLabel.setColour (juce::Label::textColourId, theme::textSecondary);
        messageLabel.setJustificationType (juce::Justification::centredRight);

        addAndMakeVisible (progressBar);
        progressBar.setVisible (false);
        progressBar.setPercentageDisplay (false);

        // Main panels
        addAndMakeVisible (waveform);
        WaveformDisplay::Callbacks wcb;
        wcb.onRegionSelected      = [this] (int i) { selectRegion (i); };
        wcb.onRegionEdited        = [this] (int i, int s, int e) { editRegion (i, s, e); };
        wcb.onRegionAuditioned    = [this] (int i) { auditionRegion (i); };
        wcb.onSearchRangeChanged  = [this] (int s, int e) { searchRangeChanged (s, e); };
        waveform.setCallbacks (wcb);

        addAndMakeVisible (regionList);
        RegionListPanel::Callbacks rcb;
        rcb.onSelect = [this] (int i) { selectRegion (i); };
        rcb.onHover  = [] (int i) { juce::ignoreUnused (i); };
        regionList.setCallbacks (rcb);

        proc.addLoopFinderListener (this);
        startTimerHz (15); // refresh progress / playhead state

        updateFileInfo();
        updateTransportEnablement();
        keyBadge.setKeyText (proc.getDetectedKeyText());
        showHint();
    }

    LoopFinderEditor::~LoopFinderEditor()
    {
        stopTimer();
        proc.removeLoopFinderListener (this);
        if (currentChooser) currentChooser.reset();
        setLookAndFeel (nullptr);
    }

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------
    void LoopFinderEditor::paint (juce::Graphics& g)
    {
        g.fillAll (theme::background);

        // ---- Header bar (subtle vertical gradient + drop shadow) ----
        {
            juce::ColourGradient grad (theme::surface.brighter (0.05f), 0.0f, 0.0f,
                                       theme::surface,                  0.0f, (float) headerH,
                                       false);
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<int> (0, 0, getWidth(), headerH));
        }
        g.setColour (theme::border);
        g.drawHorizontalLine (headerH, 0.0f, (float) getWidth());

        // Wordmark: amber loop glyph + two-tone name.
        {
            const float chipS = 22.0f;
            const float chipX = 14.0f;
            const float chipY = (headerH - chipS) * 0.5f;

            g.setColour (theme::accent);
            g.fillRoundedRectangle (chipX, chipY, chipS, chipS, 6.0f);

            // Open circular arc — a "loop" mark.
            juce::Path arc;
            arc.addCentredArc (chipX + chipS * 0.5f, chipY + chipS * 0.5f,
                               6.0f, 6.0f, 0.0f,
                               0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
            g.setColour (theme::background);
            g.strokePath (arc, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

            auto title = juce::Font (17.0f, juce::Font::bold);
            title.setExtraKerningFactor (0.06f);
            g.setFont (title);

            const int textX = (int) (chipX + chipS) + 10;
            const int loopW = title.getStringWidth ("LOOP");
            g.setColour (theme::textPrimary);
            g.drawText ("LOOP",   textX, 0, loopW + 2, headerH, juce::Justification::centredLeft);
            g.setColour (theme::accent);
            g.drawText ("FINDER", textX + loopW + 1, 0, 120, headerH, juce::Justification::centredLeft);
        }

        // ---- Footer bar (above the keyboard) ----
        const int footerY = getHeight() - keyboardH - footerH;
        {
            juce::ColourGradient grad (theme::surface.brighter (0.04f), 0.0f, (float) footerY,
                                       theme::surface.darker (0.06f),   0.0f, (float) (footerY + footerH),
                                       false);
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<int> (0, footerY, getWidth(), footerH));
        }
        g.setColour (theme::border);
        g.drawHorizontalLine (footerY, 0.0f, (float) getWidth());

        // Keyboard well — darker, with an inner shadow at the top for depth.
        g.setColour (theme::backgroundDeep);
        g.fillRect (juce::Rectangle<int> (0, getHeight() - keyboardH, getWidth(), keyboardH));
        {
            const float ky = (float) (getHeight() - keyboardH);
            juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.30f), 0.0f, ky,
                                         juce::Colours::transparentBlack, 0.0f, ky + 9.0f, false);
            g.setGradientFill (shadow);
            g.fillRect (juce::Rectangle<int> (0, (int) ky, getWidth(), 9));
        }
        g.setColour (theme::border);
        g.drawHorizontalLine (getHeight() - keyboardH, 0.0f, (float) getWidth());
    }

    void LoopFinderEditor::resized()
    {
        auto bounds = getLocalBounds();

        // Header
        auto header = bounds.removeFromTop (headerH).reduced (12, 10);
        analyzeBtn.setBounds (header.removeFromRight (96));
        header.removeFromRight (8);
        loadBtn   .setBounds (header.removeFromRight (88));

        header.removeFromLeft (168);             // wordmark (painted)
        keyBadge.setBounds (header.removeFromLeft (118));
        header.removeFromLeft (8);
        tuneBtn.setBounds  (header.removeFromLeft (86));

        // Bottom: on-screen MIDI keyboard.
        keyboard.setBounds (bounds.removeFromBottom (keyboardH).reduced (10, 8));

        // Footer
        auto footer = bounds.removeFromBottom (footerH).reduced (12, 9);

        // Row 1 — transport
        auto row1 = footer.removeFromTop (26);
        playBtn .setBounds (row1.removeFromLeft (86));
        row1.removeFromLeft (6);
        stopBtn .setBounds (row1.removeFromLeft (74));
        row1.removeFromLeft (14);
        midiFromStartBtn.setBounds (row1.removeFromLeft (118));
        row1.removeFromLeft (10);
        loopBtn .setBounds (row1.removeFromLeft (68));
        row1.removeFromLeft (14);
        volumeLabel.setBounds  (row1.removeFromLeft (26));
        volumeSlider.setBounds (row1);

        // Row 2 — root note + tune
        footer.removeFromTop (5);
        auto row2 = footer.removeFromTop (22);
        rootLabel.setBounds  (row2.removeFromLeft (26));
        tuneSlider.setBounds (row2.removeFromRight (190));
        tuneLabel.setBounds  (row2.removeFromRight (34));
        row2.removeFromRight (10);
        rootValueLabel.setBounds (row2.removeFromRight (78));
        row2.removeFromRight (8);
        rootSlider.setBounds (row2);

        // Row 3 — file info / messages
        footer.removeFromTop (3);
        auto row3 = footer;
        fileInfoLabel.setBounds (row3.removeFromLeft (row3.getWidth() / 2));
        messageLabel.setBounds  (row3);

        progressBar.setBounds (juce::Rectangle<int> (12,
                                                     getHeight() - keyboardH - footerH - 7,
                                                     getWidth() - 24, 4));

        // Main split: waveform | region list
        regionList.setBounds (bounds.removeFromRight (rightW));
        waveform.setBounds   (bounds);
    }

    // -------------------------------------------------------------------------
    // Drag and drop
    // -------------------------------------------------------------------------
    bool LoopFinderEditor::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (const auto& f : files)
        {
            const juce::File file (f);
            const auto ext = file.getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".aif" || ext == ".aiff"
             || ext == ".flac" || ext == ".mp3")
                return true;
        }
        return false;
    }

    void LoopFinderEditor::filesDropped (const juce::StringArray& files, int, int)
    {
        if (files.isEmpty()) return;
        const juce::File file (files[0]);
        if (! proc.loadFile (file))
            showMessage (proc.getFileManager().getMetadata().lastError, theme::warning);
        else
            showHint();
    }

    // -------------------------------------------------------------------------
    // Listener
    // -------------------------------------------------------------------------
    void LoopFinderEditor::loopFinderStateChanged()
    {
        updateFileInfo();
        updateTransportEnablement();
        keyBadge.setKeyText (proc.getDetectedKeyText());
        waveform.refreshSource();
    }

    void LoopFinderEditor::loopFinderRegionsChanged()
    {
        waveform.refreshRegions();
        regionList.refresh();

        if (! proc.isAnalysing())
        {
            const auto regs = proc.getRegions();
            if (regs.empty() && proc.getFileManager().isLoaded())
            {
                if (proc.lastAnalysisUsedRange())
                    showMessage ("No clean loop found in the highlighted area - widen it or "
                                 "click the waveform to clear it.",
                                 theme::warning);
                else
                    showMessage ("No seamless loop points found - try a longer sustained section.",
                                 theme::warning);
            }
            else if (! regs.empty())
            {
                showMessage (juce::String ((int) regs.size())
                             + (regs.size() == 1 ? " loop found." : " loops found.")
                             + (proc.lastAnalysisUsedRange() ? " (searched highlighted area)" : ""),
                             theme::success);
            }
            else
            {
                showHint();
            }
        }
    }

    void LoopFinderEditor::loopFinderAnalysisProgress (float p)
    {
        progressValue = (double) p;
        progressBar.setVisible (proc.isAnalysing());
    }

    void LoopFinderEditor::timerCallback()
    {
        progressValue = (double) proc.getAnalysisProgress();
        progressBar.setVisible (proc.isAnalysing());
        // Sync transport button states with engine in case they change externally.
        updateTransportEnablement();
    }

    // -------------------------------------------------------------------------
    // Actions
    // -------------------------------------------------------------------------
    void LoopFinderEditor::doLoadFile()
    {
        currentChooser = std::make_unique<juce::FileChooser> (
            "Choose an audio file",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory),
            "*.wav;*.aif;*.aiff;*.flac;*.mp3");

        const auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles;

        juce::Component::SafePointer<LoopFinderEditor> safe (this);
        currentChooser->launchAsync (flags, [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            const auto file = fc.getResult();
            if (file == juce::File()) return;
            if (! safe->proc.loadFile (file))
                safe->showMessage (safe->proc.getFileManager().getMetadata().lastError,
                                   theme::warning);
            else
                safe->showHint();
        });
    }

    void LoopFinderEditor::doAnalyze()
    {
        if (! proc.getFileManager().isLoaded())
        {
            showMessage ("Load an audio file first.", theme::warning);
            return;
        }
        showMessage (proc.hasSearchRange() ? "Analysing highlighted area..." : "Analysing...",
                     theme::textSecondary);
        progressBar.setVisible (true);
        proc.startAnalysis();
    }

    void LoopFinderEditor::selectRegion (int idx)
    {
        proc.setSelectedRegion (idx);
        waveform.refreshRegions();
        regionList.refresh();
    }

    void LoopFinderEditor::auditionRegion (int idx)
    {
        selectRegion (idx);
        proc.getPlayback().triggerPreview();
        updateTransportEnablement();
    }

    void LoopFinderEditor::editRegion (int idx, int newStart, int newEnd)
    {
        proc.updateRegion (idx, newStart, newEnd);
        waveform.refreshRegions();
        regionList.refresh();
    }

    void LoopFinderEditor::searchRangeChanged (int startSample, int endSample)
    {
        proc.setSearchRange (startSample, endSample);

        if (proc.hasSearchRange())
        {
            const double sr = proc.getFileManager().getMetadata().sampleRate > 0.0
                                ? proc.getFileManager().getMetadata().sampleRate : 44100.0;
            const double s0 = startSample / sr;
            const double s1 = endSample   / sr;
            showMessage ("Search limited to " + juce::String (s0, 2) + "s - "
                         + juce::String (s1, 2) + "s. Press Analyze.",
                         theme::textSecondary);
            analyzeBtn.setButtonText ("Analyze Area");
        }
        else
        {
            analyzeBtn.setButtonText ("Analyze");
            showHint();
        }
    }

    void LoopFinderEditor::updateFileInfo()
    {
        const auto& m = proc.getFileManager().getMetadata();
        if (! m.isLoaded)
        {
            fileInfoLabel.setText ("No file loaded", juce::dontSendNotification);
            return;
        }
        const juce::String s = m.filename
                            + "  |  " + juce::String ((int) m.sampleRate) + " Hz"
                            + "  |  " + juce::String (m.lengthSeconds, 2) + " s"
                            + "  |  " + juce::String (m.bitDepth) + "-bit";
        fileInfoLabel.setText (s, juce::dontSendNotification);

        if (m.isMissing)
            showMessage ("Linked file is missing - please re-link via Load File.",
                         theme::warning);
    }

    void LoopFinderEditor::updateTransportEnablement()
    {
        const bool hasFile = proc.getFileManager().isLoaded();
        playBtn.setEnabled (hasFile);
        stopBtn.setEnabled (hasFile);
        midiFromStartBtn.setEnabled (hasFile);
        loopBtn.setToggleState (proc.getPlayback().isLoopEnabled(), juce::dontSendNotification);
        analyzeBtn.setEnabled (hasFile && ! proc.isAnalysing());
        tuneBtn.setEnabled (proc.hasDetectedKey());
        tuneSlider.setEnabled (hasFile);
    }

    void LoopFinderEditor::showMessage (const juce::String& text, juce::Colour colour)
    {
        messageLabel.setColour (juce::Label::textColourId, colour);
        messageLabel.setText (text, juce::dontSendNotification);
    }

    void LoopFinderEditor::showHint()
    {
        if (proc.getFileManager().isLoaded())
            showMessage ("Tip: drag on the waveform to choose where to search for loops.",
                         theme::textDim);
        else
            showMessage ({}, theme::textSecondary);
    }
}
