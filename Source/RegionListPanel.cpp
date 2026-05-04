#include "RegionListPanel.h"
#include "PluginProcessor.h"

namespace lf
{
    // =========================================================================
    // RegionRow — one item in the scrolling list.
    // =========================================================================
    class RegionListPanel::RegionRow  : public juce::Component
    {
    public:
        RegionRow (int            idx,
                   LoopRegion     reg,
                   const std::vector<float>& mono,
                   double         /*sampleRate*/,
                   const Callbacks& cb)
            : index (idx),
              region (reg),
              monoSamples (mono),
              callbacks (cb)
        {
        }

        void setSelected (bool s) { selected = s; repaint(); }

        // ---------------------------------------------------------------------
        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds();
            g.setColour (selected ? theme::surface.brighter (0.15f) : theme::surface);
            g.fillRoundedRectangle (bounds.toFloat().reduced (4.0f, 3.0f),
                                    (float) theme::cornerRadius);
            g.setColour (selected ? theme::accent : theme::border);
            g.drawRoundedRectangle (bounds.toFloat().reduced (4.0f, 3.0f),
                                    (float) theme::cornerRadius, 1.0f);

            const auto col = theme::regionColour (index);

            // Color swatch
            const auto swatch = juce::Rectangle<int> (12, bounds.getY() + 10, 14, 14);
            g.setColour (col);
            g.fillRoundedRectangle (swatch.toFloat(), 3.0f);

            g.setColour (theme::textPrimary);
            g.setFont (theme::body());
            g.drawText (juce::String (index + 1),
                        juce::Rectangle<int> (32, bounds.getY() + 6, 24, 18),
                        juce::Justification::centredLeft);

            g.setColour (theme::textSecondary);
            g.setFont (theme::label());
            g.drawText (juce::String (region.durationMs, 1) + " ms",
                        juce::Rectangle<int> (60, bounds.getY() + 6, 80, 18),
                        juce::Justification::centredLeft);

            // Mini waveform
            const auto wfArea = juce::Rectangle<int> (60, bounds.getY() + 26,
                                                     bounds.getWidth() - 24, 22);
            drawMiniWave (g, wfArea, col);

            // Crossfade indicator
            if (region.hasCrossfade)
            {
                g.setColour (theme::warning);
                g.setFont (juce::Font (9.0f));
                g.drawText ("xfade",
                            juce::Rectangle<int> (bounds.getRight() - 60, bounds.getY() + 6,
                                                  44, 14),
                            juce::Justification::centredRight);
            }
        }

        void resized() override {}

        void mouseEnter (const juce::MouseEvent&) override
        {
            if (callbacks.onHover) callbacks.onHover (index);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            if (callbacks.onSelect) callbacks.onSelect (index);
        }

    private:
        void drawMiniWave (juce::Graphics& g, juce::Rectangle<int> area,
                           juce::Colour col)
        {
            if (! region.isValid() || monoSamples.empty()) return;

            const int  start = juce::jlimit (0, (int) monoSamples.size() - 1, region.startSample);
            const int  end   = juce::jlimit (start + 1, (int) monoSamples.size(), region.endSample);
            const int  len   = end - start;

            const int   W = area.getWidth();
            const float midY = (float) area.getCentreY();
            const float amp  = (float) area.getHeight() * 0.45f;

            juce::Path p;
            for (int x = 0; x < W; ++x)
            {
                const int i0 = start + (x       * len) / std::max (1, W);
                const int i1 = start + ((x + 1) * len) / std::max (1, W);
                float maxAbs = 0.0f, signedV = 0.0f;
                for (int k = i0; k < i1; ++k)
                {
                    const float s = monoSamples[(size_t) k];
                    if (std::abs (s) > maxAbs) { maxAbs = std::abs (s); signedV = s; }
                }
                const float y = midY - signedV * amp;
                if (x == 0) p.startNewSubPath ((float) (area.getX() + x), y);
                else        p.lineTo          ((float) (area.getX() + x), y);
            }

            g.setColour (col.withAlpha (0.85f));
            g.strokePath (p, juce::PathStrokeType (1.0f));
        }

        int  index;
        LoopRegion region;
        const std::vector<float>& monoSamples;
        Callbacks callbacks;
        bool selected { false };
    };

    // =========================================================================
    // RegionListPanel
    // =========================================================================
    RegionListPanel::RegionListPanel (LoopFinderProcessor& proc)
        : processor (proc)
    {
        addAndMakeVisible (titleLabel);
        titleLabel.setText ("Loop Regions", juce::dontSendNotification);
        titleLabel.setFont (theme::heading());
        titleLabel.setColour (juce::Label::textColourId, theme::textPrimary);

        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&listContainer, false);
        viewport.setScrollBarsShown (true, false);

        refresh();
    }

    RegionListPanel::~RegionListPanel() = default;

    void RegionListPanel::setCallbacks (Callbacks c)
    {
        callbacks = std::move (c);
        rebuildRows();
    }

    void RegionListPanel::refresh()
    {
        rebuildRows();
        repaint();
    }

    void RegionListPanel::paint (juce::Graphics& g)
    {
        g.fillAll (theme::background);

        g.setColour (theme::border);
        g.drawVerticalLine (0, 0.0f, (float) getHeight());
    }

    void RegionListPanel::resized()
    {
        auto b = getLocalBounds().reduced (8);
        auto top = b.removeFromTop (24);
        titleLabel.setBounds (top);

        b.removeFromTop (6);
        viewport.setBounds (b);

        const int rowH = 60;
        listContainer.setSize (viewport.getMaximumVisibleWidth(),
                               std::max (1, (int) rows.size()) * (rowH + 4));

        for (size_t i = 0; i < rows.size(); ++i)
            rows[i]->setBounds (0, (int) i * (rowH + 4),
                                listContainer.getWidth(), rowH);
    }

    void RegionListPanel::rebuildRows()
    {
        rows.clear();
        listContainer.removeAllChildren();

        const auto regs = processor.getRegions();
        const int  selected = processor.getSelectedRegion();
        const auto& mono    = processor.getFileManager().getMonoMix();
        const double sr     = processor.getFileManager().getMetadata().sampleRate;

        for (int i = 0; i < (int) regs.size(); ++i)
        {
            auto row = std::make_unique<RegionRow> (i, regs[(size_t) i], mono, sr, callbacks);
            row->setSelected (i == selected);
            listContainer.addAndMakeVisible (row.get());
            rows.push_back (std::move (row));
        }

        resized();
    }
}
