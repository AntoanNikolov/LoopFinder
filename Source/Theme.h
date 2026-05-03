#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>

namespace lf::theme
{
    // -------------------------------------------------------------------------
    // Palette (matches the spec in the project brief)
    // -------------------------------------------------------------------------
    inline const juce::Colour background     { 0xFF1A1A1A };
    inline const juce::Colour surface        { 0xFF242424 };
    inline const juce::Colour border         { 0xFF383838 };
    inline const juce::Colour accent         { 0xFF4A9EFF };
    inline const juce::Colour textPrimary    { 0xFFF0F0F0 };
    inline const juce::Colour textSecondary  { 0xFF888888 };
    inline const juce::Colour success        { 0xFF4CAF82 };
    inline const juce::Colour warning        { 0xFFF0A040 };

    inline const juce::Colour waveformFill   { 0xFF888888 };
    inline const juce::Colour waveformOutline{ 0xFFA0A0A0 };
    inline const juce::Colour playhead       { 0xFFFFFFFF };

    // -------------------------------------------------------------------------
    // Eight visually distinct region colours (cycled through detected regions).
    // -------------------------------------------------------------------------
    inline const std::array<juce::Colour, 8> regionPalette
    {
        juce::Colour { 0xFF4A9EFF }, // blue
        juce::Colour { 0xFF4CAF82 }, // green
        juce::Colour { 0xFFF0A040 }, // orange
        juce::Colour { 0xFFE15CCB }, // pink
        juce::Colour { 0xFFB872FF }, // purple
        juce::Colour { 0xFFFFD75A }, // yellow
        juce::Colour { 0xFF5DD1D1 }, // teal
        juce::Colour { 0xFFFF6B6B }, // red
    };

    inline juce::Colour regionColour (int index) noexcept
    {
        const auto n = static_cast<int> (regionPalette.size());
        const auto i = ((index % n) + n) % n;
        return regionPalette[static_cast<std::size_t> (i)];
    }

    // -------------------------------------------------------------------------
    // Typography helpers (JUCE 7-compatible Font constructors)
    // -------------------------------------------------------------------------
    inline juce::Font heading()  { return juce::Font (15.0f, juce::Font::bold); }
    inline juce::Font body()     { return juce::Font (13.0f); }
    inline juce::Font label()    { return juce::Font (11.0f); }

    // -------------------------------------------------------------------------
    // Geometry
    // -------------------------------------------------------------------------
    inline constexpr int defaultWidth  = 780;
    inline constexpr int defaultHeight = 480;
    inline constexpr int cornerRadius  = 4;
}
