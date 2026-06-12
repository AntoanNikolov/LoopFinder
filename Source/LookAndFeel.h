#pragma once

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace lf
{
    /** Custom look-and-feel: rounded buttons, slim amber sliders, pill toggle
     *  switches and quiet borders. Applied by the editor; every widget colour
     *  can still be overridden per-component (e.g. the primary Analyze button).
     */
    class LoopFinderLookAndFeel  : public juce::LookAndFeel_V4
    {
    public:
        LoopFinderLookAndFeel()
        {
            setColour (juce::ResizableWindow::backgroundColourId, theme::background);

            setColour (juce::TextButton::buttonColourId,   theme::surfaceRaised);
            setColour (juce::TextButton::buttonOnColourId, theme::accent);
            setColour (juce::TextButton::textColourOffId,  theme::textPrimary);
            setColour (juce::TextButton::textColourOnId,   theme::background);
            setColour (juce::ComboBox::outlineColourId,    theme::border);

            setColour (juce::Slider::backgroundColourId,     theme::border);
            setColour (juce::Slider::trackColourId,          theme::accent);
            setColour (juce::Slider::thumbColourId,          theme::textPrimary);
            setColour (juce::Slider::textBoxTextColourId,    theme::textSecondary);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

            setColour (juce::Label::textColourId,        theme::textPrimary);
            setColour (juce::ToggleButton::textColourId, theme::textPrimary);
            setColour (juce::ToggleButton::tickColourId, theme::accent);

            setColour (juce::TooltipWindow::backgroundColourId, theme::surfaceRaised);
            setColour (juce::TooltipWindow::textColourId,       theme::textPrimary);
            setColour (juce::TooltipWindow::outlineColourId,    theme::border);

            setColour (juce::ScrollBar::thumbColourId, theme::border.brighter (0.4f));

            setColour (juce::ProgressBar::backgroundColourId, theme::surfaceRaised);
            setColour (juce::ProgressBar::foregroundColourId, theme::accent);

            setColour (juce::PopupMenu::backgroundColourId,            theme::surfaceRaised);
            setColour (juce::PopupMenu::textColourId,                  theme::textPrimary);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent.withAlpha (0.25f));
            setColour (juce::PopupMenu::highlightedTextColourId,       theme::textPrimary);

            setColour (juce::AlertWindow::backgroundColourId, theme::surface);
            setColour (juce::AlertWindow::textColourId,       theme::textPrimary);
            setColour (juce::AlertWindow::outlineColourId,    theme::border);
        }

        // ---------------------------------------------------------------------
        // Buttons
        // ---------------------------------------------------------------------
        void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                   const juce::Colour& backgroundColour,
                                   bool isHighlighted, bool isDown) override
        {
            auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
            const float radius = (float) theme::cornerRadius;

            auto base = backgroundColour;
            if (! button.isEnabled())   base = base.withMultipliedAlpha (0.45f);
            else if (isDown)            base = base.darker (0.18f);
            else if (isHighlighted)     base = base.brighter (0.10f);

            g.setColour (base);
            g.fillRoundedRectangle (bounds, radius);

            // Subtle top sheen so buttons read as physical, not flat.
            g.setColour (juce::Colours::white.withAlpha (isDown ? 0.0f : 0.045f));
            g.fillRoundedRectangle (bounds.withHeight (bounds.getHeight() * 0.5f), radius);

            const bool isPrimary = base.getPerceivedBrightness() > 0.5f;
            g.setColour (isPrimary ? base.darker (0.45f).withAlpha (0.65f)
                                   : theme::border);
            g.drawRoundedRectangle (bounds, radius, 1.0f);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int /*buttonHeight*/) override
        {
            return juce::Font (12.5f, juce::Font::bold);
        }

        // ---------------------------------------------------------------------
        // Toggle buttons → pill switches
        // ---------------------------------------------------------------------
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                               bool isHighlighted, bool /*isDown*/) override
        {
            const bool  on = button.getToggleState();
            const auto  bounds = button.getLocalBounds().toFloat();
            const float trackW = 30.0f, trackH = 16.0f;
            const float y = bounds.getCentreY() - trackH * 0.5f;

            auto track = juce::Rectangle<float> (0.0f, y, trackW, trackH);
            g.setColour (on ? theme::accent
                            : theme::surfaceRaised.brighter (isHighlighted ? 0.15f : 0.0f));
            g.fillRoundedRectangle (track, trackH * 0.5f);
            g.setColour (on ? theme::accent.darker (0.3f) : theme::border);
            g.drawRoundedRectangle (track.reduced (0.5f), trackH * 0.5f, 1.0f);

            const float knobD = trackH - 5.0f;
            const float knobX = on ? track.getRight() - knobD - 2.5f : track.getX() + 2.5f;
            g.setColour (on ? theme::background : theme::textSecondary);
            g.fillEllipse (knobX, y + 2.5f, knobD, knobD);

            g.setColour (button.findColour (juce::ToggleButton::textColourId)
                               .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
            g.setFont (theme::body());
            g.drawText (button.getButtonText(),
                        bounds.withTrimmedLeft (trackW + 7.0f).toNearestInt(),
                        juce::Justification::centredLeft);
        }

        // ---------------------------------------------------------------------
        // Sliders (horizontal linear only — that's all this plugin uses)
        // ---------------------------------------------------------------------
        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle style, juce::Slider& slider) override
        {
            if (style != juce::Slider::LinearHorizontal)
            {
                juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                        minSliderPos, maxSliderPos, style, slider);
                return;
            }

            const float cy = (float) y + (float) height * 0.5f;
            const float trackH = 4.0f;

            auto track = juce::Rectangle<float> ((float) x, cy - trackH * 0.5f,
                                                 (float) width, trackH);
            g.setColour (slider.findColour (juce::Slider::backgroundColourId));
            g.fillRoundedRectangle (track, trackH * 0.5f);

            auto filled = track.withWidth (juce::jmax (trackH, sliderPos - (float) x));
            g.setColour (slider.findColour (juce::Slider::trackColourId)
                               .withMultipliedAlpha (slider.isEnabled() ? 1.0f : 0.4f));
            g.fillRoundedRectangle (filled, trackH * 0.5f);

            const float thumbD = 13.0f;
            g.setColour (slider.findColour (juce::Slider::thumbColourId));
            g.fillEllipse (sliderPos - thumbD * 0.5f, cy - thumbD * 0.5f, thumbD, thumbD);
            g.setColour (theme::background);
            g.drawEllipse (sliderPos - thumbD * 0.5f, cy - thumbD * 0.5f, thumbD, thumbD, 1.5f);
        }

        int getSliderThumbRadius (juce::Slider&) override { return 7; }

        // ---------------------------------------------------------------------
        // Progress bar — slim amber strip
        // ---------------------------------------------------------------------
        void drawProgressBar (juce::Graphics& g, juce::ProgressBar& bar,
                              int width, int height, double progress,
                              const juce::String& /*textToShow*/) override
        {
            auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
            const float r = bounds.getHeight() * 0.5f;

            g.setColour (bar.findColour (juce::ProgressBar::backgroundColourId));
            g.fillRoundedRectangle (bounds, r);

            const float w = (float) juce::jlimit (0.0, 1.0, progress) * bounds.getWidth();
            if (w > r)
            {
                g.setColour (bar.findColour (juce::ProgressBar::foregroundColourId));
                g.fillRoundedRectangle (bounds.withWidth (w), r);
            }
        }

        // ---------------------------------------------------------------------
        // Tooltips
        // ---------------------------------------------------------------------
        void drawTooltip (juce::Graphics& g, const juce::String& text,
                          int width, int height) override
        {
            const auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height);
            g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
            g.fillRoundedRectangle (bounds, 4.0f);
            g.setColour (findColour (juce::TooltipWindow::outlineColourId));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

            g.setColour (findColour (juce::TooltipWindow::textColourId));
            g.setFont (theme::label());
            g.drawFittedText (text, juce::Rectangle<int> (0, 0, width, height).reduced (7, 3),
                              juce::Justification::centredLeft, 3);
        }

        juce::Rectangle<int> getTooltipBounds (const juce::String& text,
                                               juce::Point<int> screenPos,
                                               juce::Rectangle<int> parentArea) override
        {
            const auto font = theme::label();
            const int w = juce::jmin (320, font.getStringWidth (text) + 18);
            const int h = 22;
            return juce::Rectangle<int> (screenPos.x + 12, screenPos.y + 18, w, h)
                       .constrainedWithin (parentArea);
        }
    };
}
