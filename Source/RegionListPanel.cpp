#include "RegionListPanel.h"
#include "PluginProcessor.h"

#include <cmath>

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
            const auto card = getLocalBounds().toFloat().reduced (4.0f, 3.0f);
            const auto col  = theme::regionColour (index);

            auto bg = selected ? theme::surfaceRaised.brighter (0.08f)
                               : (hovered ? theme::surfaceRaised.brighter (0.03f)
                                          : theme::surfaceRaised);
            g.setColour (bg);
            g.fillRoundedRectangle (card, (float) theme::cornerRadius);
            g.setColour (selected ? col : theme::border);
            g.drawRoundedRectangle (card, (float) theme::cornerRadius, selected ? 1.4f : 1.0f);

            // Colour spine on the left edge.
            g.setColour (col);
            g.fillRoundedRectangle (card.getX() + 3.0f, card.getY() + 5.0f,
                                    3.5f, card.getHeight() - 10.0f, 1.75f);

            const int left = (int) card.getX() + 14;
            const int top  = (int) card.getY();

            g.setColour (theme::textPrimary);
            g.setFont (juce::Font (12.5f, juce::Font::bold));
            g.drawText ("Loop " + juce::String (index + 1),
                        juce::Rectangle<int> (left, top + 5, 70, 16),
                        juce::Justification::centredLeft);

            // Match quality, as a percentage.
            const int pct = (int) std::round (juce::jlimit (0.0f, 1.0f, region.score) * 100.0f);
            g.setColour (pct >= 95 ? theme::success : theme::textSecondary);
            g.setFont (theme::mono (10.0f));
            g.drawText (juce::String (pct) + "% match",
                        juce::Rectangle<int> (left + 66, top + 5, 76, 16),
                        juce::Justification::centredLeft);

            // Duration on the right.
            g.setColour (theme::textSecondary);
            g.setFont (theme::mono (10.0f));
            juce::String dur = region.durationMs >= 1000.0f
                                 ? juce::String (region.durationMs / 1000.0f, 2) + " s"
                                 : juce::String (region.durationMs, 0) + " ms";
            if (region.hasCrossfade) dur = "xf - " + dur;
            g.drawText (dur,
                        juce::Rectangle<int> ((int) card.getRight() - 92, top + 5, 84, 16),
                        juce::Justification::centredRight);

            // Mini waveform
            const auto wfArea = juce::Rectangle<int> (left, top + 26,
                                                      (int) card.getWidth() - 24, 22);
            drawMiniWave (g, wfArea, col);
        }

        void resized() override {}

        void mouseEnter (const juce::MouseEvent&) override
        {
            hovered = true;
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            repaint();
            if (callbacks.onHover) callbacks.onHover (index);
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            hovered = false;
            repaint();
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
        bool hovered  { false };
    };

    // =========================================================================
    // RegionListPanel
    // =========================================================================
    RegionListPanel::RegionListPanel (LoopFinderProcessor& proc)
        : processor (proc)
    {
        addAndMakeVisible (titleLabel);
        titleLabel.setText ("DETECTED LOOPS", juce::dontSendNotification);
        titleLabel.setFont (theme::heading());
        titleLabel.setColour (juce::Label::textColourId, theme::textSecondary);

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

        // Match the waveform's under-header inner shadow.
        {
            juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.25f), 0.0f, 0.0f,
                                         juce::Colours::transparentBlack, 0.0f, 9.0f, false);
            g.setGradientFill (shadow);
            g.fillRect (juce::Rectangle<int> (0, 0, getWidth(), 9));
        }

        if (rows.empty())
        {
            g.setColour (theme::textDim);
            g.setFont (theme::label());
            g.drawFittedText ("Press Analyze to detect\nseamless loops",
                              getLocalBounds().reduced (16).withTrimmedTop (40).withHeight (40),
                              juce::Justification::centredTop, 2);
        }
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
