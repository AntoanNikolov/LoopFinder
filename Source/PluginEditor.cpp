#include "PluginEditor.h"
#include "LoopExporter.h"

namespace lf
{
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;

    // =========================================================================
    LoopFinderEditor::LoopFinderEditor (LoopFinderProcessor& p)
        : juce::AudioProcessorEditor (&p),
          proc (p),
          waveform (p),
          regionList (p),
          keyboard (p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        // Add ~84px of extra height to fit the on-screen MIDI keyboard.
        setSize (theme::defaultWidth, theme::defaultHeight + 84);
        setResizable (false, false);

        // Header
        addAndMakeVisible (titleLabel);
        titleLabel.setText ("LoopFinder", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));
        titleLabel.setColour (juce::Label::textColourId, theme::textPrimary);

        addAndMakeVisible (loadBtn);
        loadBtn.onClick = [this] { doLoadFile(); };

        addAndMakeVisible (analyzeBtn);
        analyzeBtn.onClick = [this] { doAnalyze(); };

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

        addAndMakeVisible (loopBtn);
        loopBtn.setToggleState (proc.getPlayback().isLoopEnabled(), juce::dontSendNotification);
        loopBtn.onClick = [this] { proc.getPlayback().setLoopEnabled (loopBtn.getToggleState()); };

        addAndMakeVisible (volumeLabel);
        volumeLabel.setText ("Vol", juce::dontSendNotification);
        volumeLabel.setFont (theme::label());
        volumeLabel.setColour (juce::Label::textColourId, theme::textSecondary);

        addAndMakeVisible (volumeSlider);
        volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 18);
        volumeSlider.setRange (-60.0, 6.0, 0.1);
        volumeSlider.setTextValueSuffix (" dB");
        volumeAttach = std::make_unique<SliderAttach> (proc.getApvts(), "gainDb", volumeSlider);

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
        rootValueLabel.setFont (theme::label());
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
        keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId,        juce::Colour (0xFFE6E6E6));
        keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId,        juce::Colour (0xFF1F1F1F));
        keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, theme::border);
        keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                            theme::accent.withAlpha (0.4f));
        keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,
                            theme::accent.withAlpha (0.7f));
        keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, theme::textSecondary);

        // Status
        addAndMakeVisible (fileInfoLabel);
        fileInfoLabel.setFont (theme::label());
        fileInfoLabel.setColour (juce::Label::textColourId, theme::textSecondary);

        addAndMakeVisible (messageLabel);
        messageLabel.setFont (theme::label());
        messageLabel.setColour (juce::Label::textColourId, theme::warning);
        messageLabel.setJustificationType (juce::Justification::centredRight);

        addAndMakeVisible (progressBar);
        progressBar.setVisible (false);

        // Main panels
        addAndMakeVisible (waveform);
        WaveformDisplay::Callbacks wcb;
        wcb.onRegionSelected   = [this] (int i) { selectRegion (i); };
        wcb.onRegionEdited     = [this] (int i, int s, int e) { editRegion (i, s, e); };
        wcb.onRegionAuditioned = [this] (int i) { auditionRegion (i); };
        waveform.setCallbacks (wcb);

        addAndMakeVisible (regionList);
        RegionListPanel::Callbacks rcb;
        rcb.onSelect    = [this] (int i) { selectRegion (i); };
        rcb.onExport    = [this] (int i) { doExport (i); };
        rcb.onExportAll = [this]         { doExportAll(); };
        rcb.onHover     = [] (int i) { /* could highlight in waveform — not yet */ juce::ignoreUnused (i); };
        regionList.setCallbacks (rcb);

        proc.addLoopFinderListener (this);
        startTimerHz (15); // refresh progress / playhead state

        updateFileInfo();
        updateTransportEnablement();
    }

    LoopFinderEditor::~LoopFinderEditor()
    {
        stopTimer();
        proc.removeLoopFinderListener (this);
        if (currentChooser) currentChooser.reset();
    }

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------
    void LoopFinderEditor::paint (juce::Graphics& g)
    {
        g.fillAll (theme::background);

        // Header background
        const int headerH   = 40;
        const int keyboardH = 84;
        const int footerH   = 84;

        g.setColour (theme::surface);
        g.fillRect (juce::Rectangle<int> (0, 0, getWidth(), headerH));
        g.setColour (theme::border);
        g.drawHorizontalLine (headerH, 0.0f, (float) getWidth());

        // Footer background (above the keyboard)
        const int footerY = getHeight() - keyboardH - footerH;
        g.setColour (theme::surface);
        g.fillRect (juce::Rectangle<int> (0, footerY, getWidth(), footerH));
        g.setColour (theme::border);
        g.drawHorizontalLine (footerY, 0.0f, (float) getWidth());

        // Keyboard background separator
        g.drawHorizontalLine (getHeight() - keyboardH, 0.0f, (float) getWidth());
    }

    void LoopFinderEditor::resized()
    {
        constexpr int headerH   = 40;
        constexpr int footerH   = 84;
        constexpr int keyboardH = 84;
        constexpr int rightW    = 240;

        auto bounds = getLocalBounds();

        // Header
        auto header = bounds.removeFromTop (headerH).reduced (10, 6);
        titleLabel.setBounds (header.removeFromLeft (160));
        header.removeFromLeft (8);
        analyzeBtn.setBounds (header.removeFromRight (90).reduced (0, 2));
        loadBtn   .setBounds (header.removeFromRight (90).reduced (0, 2));

        // Bottom: on-screen MIDI keyboard.
        keyboard.setBounds (bounds.removeFromBottom (keyboardH).reduced (8, 6));

        // Footer
        auto footer = bounds.removeFromBottom (footerH).reduced (10, 8);

        // Row 1 — transport
        auto row1 = footer.removeFromTop (24);
        playBtn .setBounds (row1.removeFromLeft (88));
        row1.removeFromLeft (4);
        stopBtn .setBounds (row1.removeFromLeft (76));
        row1.removeFromLeft (8);
        loopBtn .setBounds (row1.removeFromLeft (60));
        row1.removeFromLeft (8);
        volumeLabel.setBounds  (row1.removeFromLeft (28));
        volumeSlider.setBounds (row1);

        // Row 2 — root note
        footer.removeFromTop (4);
        auto row2 = footer.removeFromTop (22);
        rootLabel.setBounds      (row2.removeFromLeft (40));
        rootValueLabel.setBounds (row2.removeFromRight (90));
        row2.removeFromRight (6);
        rootSlider.setBounds (row2);

        // Row 3 — file info / messages
        auto row3 = footer;
        fileInfoLabel.setBounds (row3.removeFromLeft (row3.getWidth() / 2));
        messageLabel.setBounds  (row3);

        progressBar.setBounds (juce::Rectangle<int> (10,
                                                     getHeight() - keyboardH - footerH - 6,
                                                     getWidth() - 20, 4));

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
        {
            messageLabel.setText (proc.getFileManager().getMetadata().lastError,
                                  juce::dontSendNotification);
        }
        else
        {
            messageLabel.setText ({}, juce::dontSendNotification);
        }
    }

    // -------------------------------------------------------------------------
    // Listener
    // -------------------------------------------------------------------------
    void LoopFinderEditor::loopFinderStateChanged()
    {
        updateFileInfo();
        updateTransportEnablement();
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
                messageLabel.setText (
                    "No seamless loop points found — try a longer sustained section.",
                    juce::dontSendNotification);
            else
                messageLabel.setText ({}, juce::dontSendNotification);
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
                safe->messageLabel.setText (safe->proc.getFileManager().getMetadata().lastError,
                                            juce::dontSendNotification);
            else
                safe->messageLabel.setText ({}, juce::dontSendNotification);
        });
    }

    void LoopFinderEditor::doAnalyze()
    {
        if (! proc.getFileManager().isLoaded())
        {
            messageLabel.setText ("Load an audio file first.", juce::dontSendNotification);
            return;
        }
        messageLabel.setText ("Analysing…", juce::dontSendNotification);
        progressBar.setVisible (true);
        proc.startAnalysis();
    }

    void LoopFinderEditor::doExport (int regionIndex)
    {
        const auto regs = proc.getRegions();
        if (regionIndex < 0 || regionIndex >= (int) regs.size()) return;

        const auto base = proc.getFileManager().getMetadata().filename
                            .upToLastOccurrenceOf (".", false, false);
        const auto suggested = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                  .getChildFile (base + "_loop_"
                                              + juce::String (regionIndex + 1) + ".wav");

        currentChooser = std::make_unique<juce::FileChooser> (
            "Export loop region as WAV", suggested, "*.wav");

        const auto flags = juce::FileBrowserComponent::saveMode
                         | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting;

        juce::Component::SafePointer<LoopFinderEditor> safe (this);
        currentChooser->launchAsync (flags,
            [safe, regionIndex, regs] (const juce::FileChooser& fc)
            {
                if (safe == nullptr) return;
                const auto out = fc.getResult();
                if (out == juce::File()) return;

                LoopExporter::Options opts;
                opts.bitDepth = LoopExporter::BitDepth::int16;
                opts.applyCrossfade  = true;
                opts.crossfadeSamples = (int) std::lround (
                    0.002 * safe->proc.getFileManager().getMetadata().sampleRate);

                const auto r = LoopExporter::exportRegion (
                    safe->proc.getFileManager().getAudioBuffer(),
                    safe->proc.getFileManager().getMetadata().sampleRate,
                    regs[(size_t) regionIndex],
                    out, opts);

                safe->messageLabel.setText (r.ok ? r.message : "Export failed: " + r.message,
                                            juce::dontSendNotification);
            });
    }

    void LoopFinderEditor::doExportAll()
    {
        const auto regs = proc.getRegions();
        if (regs.empty()) return;

        currentChooser = std::make_unique<juce::FileChooser> (
            "Choose folder for batch export",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory));

        juce::Component::SafePointer<LoopFinderEditor> safe (this);
        currentChooser->launchAsync (
            juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectDirectories,
            [safe, regs] (const juce::FileChooser& fc)
            {
                if (safe == nullptr) return;
                const auto folder = fc.getResult();
                if (folder == juce::File()) return;

                LoopExporter::Options opts;
                opts.bitDepth        = LoopExporter::BitDepth::int16;
                opts.applyCrossfade  = true;
                opts.crossfadeSamples = (int) std::lround (
                    0.002 * safe->proc.getFileManager().getMetadata().sampleRate);

                const auto base = safe->proc.getFileManager().getMetadata().filename
                                    .upToLastOccurrenceOf (".", false, false);
                const auto results = LoopExporter::exportAll (
                    safe->proc.getFileManager().getAudioBuffer(),
                    safe->proc.getFileManager().getMetadata().sampleRate,
                    regs, folder, base, opts);

                int ok = 0;
                for (const auto& r : results) if (r.ok) ++ok;
                safe->messageLabel.setText ("Exported " + juce::String (ok) + " / "
                                            + juce::String ((int) results.size()) + " regions to "
                                            + folder.getFileName(),
                                            juce::dontSendNotification);
            });
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

    void LoopFinderEditor::updateFileInfo()
    {
        const auto& m = proc.getFileManager().getMetadata();
        if (! m.isLoaded)
        {
            fileInfoLabel.setText ("File: (none loaded)", juce::dontSendNotification);
            return;
        }
        const juce::String s = "File: " + m.filename
                            + "  •  " + juce::String ((int) m.sampleRate) + " Hz"
                            + "  •  " + juce::String (m.lengthSeconds, 2) + " s"
                            + "  •  " + juce::String (m.bitDepth) + "-bit";
        fileInfoLabel.setText (s, juce::dontSendNotification);

        if (m.isMissing)
            messageLabel.setText ("Linked file is missing — please re-link via Load File.",
                                  juce::dontSendNotification);
    }

    void LoopFinderEditor::updateTransportEnablement()
    {
        const bool hasFile = proc.getFileManager().isLoaded();
        playBtn.setEnabled (hasFile);
        stopBtn.setEnabled (hasFile);
        loopBtn.setToggleState (proc.getPlayback().isLoopEnabled(), juce::dontSendNotification);
        analyzeBtn.setEnabled (hasFile && ! proc.isAnalysing());
    }
}
