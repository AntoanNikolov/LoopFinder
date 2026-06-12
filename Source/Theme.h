#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>

namespace lf::theme
{
    // -------------------------------------------------------------------------
    // Palette — warm charcoal with an amber accent, in the spirit of classic
    // hardware samplers rather than a flat web dashboard.
    // -------------------------------------------------------------------------
    inline const juce::Colour background     { 0xFF181511 };   // deep warm black
    inline const juce::Colour backgroundDeep { 0xFF121009 };   // waveform well
    inline const juce::Colour surface        { 0xFF211D18 };   // header / footer bars
    inline const juce::Colour surfaceRaised  { 0xFF2B2620 };   // buttons, cards
    inline const juce::Colour border         { 0xFF3B342B };
    inline const juce::Colour borderSoft     { 0xFF2E2922 };

    inline const juce::Colour accent         { 0xFFE8A33D };   // amber
    inline const juce::Colour accentBright   { 0xFFFFC169 };

    inline const juce::Colour textPrimary    { 0xFFF1EBE0 };
    inline const juce::Colour textSecondary  { 0xFF9A9184 };
    inline const juce::Colour textDim        { 0xFF6E665A };

    inline const juce::Colour success        { 0xFF93BA77 };
    inline const juce::Colour warning        { 0xFFE0795C };

    inline const juce::Colour waveformFill   { 0xFF77705F };
    inline const juce::Colour waveformOutline{ 0xFFB5AB94 };
    inline const juce::Colour playhead       { 0xFFFFF6E3 };

    // -------------------------------------------------------------------------
    // Eight visually distinct region colours (cycled through detected regions).
    // Deliberately avoids the amber accent so detected loops never get
    // confused with the user's search highlight.
    // -------------------------------------------------------------------------
    inline const std::array<juce::Colour, 8> regionPalette
    {
        juce::Colour { 0xFF4FB8A8 }, // teal
        juce::Colour { 0xFF9D7BD8 }, // violet
        juce::Colour { 0xFF88B86E }, // green
        juce::Colour { 0xFF5E9ED6 }, // blue
        juce::Colour { 0xFFE0719A }, // rose
        juce::Colour { 0xFFD8C25A }, // gold
        juce::Colour { 0xFFE2745B }, // coral
        juce::Colour { 0xFF7FC4DC }, // ice
    };

    inline juce::Colour regionColour (int index) noexcept
    {
        const auto n = static_cast<int> (regionPalette.size());
        const auto i = ((index % n) + n) % n;
        return regionPalette[static_cast<std::size_t> (i)];
    }

    // -------------------------------------------------------------------------
    // Typography
    // -------------------------------------------------------------------------
    inline juce::Font heading()
    {
        auto f = juce::Font (13.0f, juce::Font::bold);
        f.setExtraKerningFactor (0.08f);
        return f;
    }

    inline juce::Font body()  { return juce::Font (13.0f); }
    inline juce::Font label() { return juce::Font (11.0f); }

    /** Monospaced font for time / sample readouts. */
    inline juce::Font mono (float height = 11.0f)
    {
        return juce::Font (juce::Font::getDefaultMonospacedFontName(),
                           height, juce::Font::plain);
    }

    // -------------------------------------------------------------------------
    // Geometry
    // -------------------------------------------------------------------------
    inline constexpr int defaultWidth  = 820;
    inline constexpr int defaultHeight = 500;
    inline constexpr int cornerRadius  = 6;
}
