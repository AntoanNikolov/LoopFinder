#pragma once

#include "LoopRegion.h"
#include "Theme.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace lf
{
    class LoopFinderProcessor;

    /** Custom waveform component with detected-loop overlays, click-to-select,
     *  drag-to-edit boundaries (with zero-crossing snap) and a small overview
     *  strip showing the current zoomed-in viewport.
     *
     *  Communicates back to the rest of the plugin through std::function
     *  callbacks; it does not depend on the PluginProcessor implementation
     *  directly other than for read-only access to the source samples /
     *  thumbnail.
     */
    class WaveformDisplay  : public juce::Component,
                             public  juce::SettableTooltipClient,
                             private juce::Timer,
                             public  juce::ChangeListener
    {
    public:
        struct Callbacks
        {
            std::function<void (int regionIndex)> onRegionSelected;
            std::function<void (int regionIndex, int newStart, int newEnd)> onRegionEdited;
            std::function<void (int regionIndex)> onRegionAuditioned;
        };

        explicit WaveformDisplay (LoopFinderProcessor& proc);
        ~WaveformDisplay() override;

        void setCallbacks (Callbacks c);

        /** Re-read regions from the processor (call from editor on changes). */
        void refreshRegions();

        /** Re-read the loaded audio source from the processor. */
        void refreshSource();

        // ---------------------------------------------------------------------
        // Component overrides
        // ---------------------------------------------------------------------
        void paint (juce::Graphics&) override;
        void resized() override;

        void mouseMove        (const juce::MouseEvent&) override;
        void mouseDown        (const juce::MouseEvent&) override;
        void mouseDrag        (const juce::MouseEvent&) override;
        void mouseUp          (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove   (const juce::MouseEvent&,
                               const juce::MouseWheelDetails&) override;

        void changeListenerCallback (juce::ChangeBroadcaster*) override;

    private:
        // ---------------------------------------------------------------------
        // Internal helpers
        // ---------------------------------------------------------------------
        void timerCallback() override;

        // Geometry helpers
        juce::Rectangle<int> getMainArea()     const;
        juce::Rectangle<int> getOverviewArea() const;
        juce::Rectangle<int> getRulerArea()    const;

        // Sample / pixel conversions inside the main waveform area.
        double samplesPerPixel() const;
        int    pixelToSample (int x) const;
        int    sampleToPixel (int s) const;

        void clampView();
        void zoomAt (int mouseX, double newZoom);

        // Hit testing
        enum class HitKind { None, RegionLeft, RegionRight, RegionBody, EmptyArea };
        struct Hit
        {
            HitKind kind        { HitKind::None };
            int     regionIndex { -1 };
        };
        Hit hitTestAt (juce::Point<int> pos) const;

        // Drawing
        void drawWaveformLowZoom  (juce::Graphics&, juce::Rectangle<int> area);
        void drawWaveformHighZoom (juce::Graphics&, juce::Rectangle<int> area);
        void drawRegions          (juce::Graphics&, juce::Rectangle<int> area);
        void drawPlayhead         (juce::Graphics&, juce::Rectangle<int> area);
        void drawTimeRuler        (juce::Graphics&, juce::Rectangle<int> area);
        void drawOverview         (juce::Graphics&, juce::Rectangle<int> area);
        void drawDragLabel        (juce::Graphics&);

        // Cached waveform layer — re-rendered only when view / zoom / source
        // changes, so the playhead-driven 30Hz repaint stays cheap and the
        // displayed waveform is rock-steady (no per-frame re-binning).
        void invalidateWaveformCache() { waveformCacheDirty = true; }
        void renderWaveformCache (juce::Rectangle<int> area);

        // Zero-crossing snap support
        void rebuildZeroCrossings();
        int  snapToZeroCrossing (int sample, int withinSamples) const;

        // ---------------------------------------------------------------------
        // Data
        // ---------------------------------------------------------------------
        LoopFinderProcessor& processor;
        Callbacks callbacks;

        juce::AudioThumbnail thumbnail;
        bool thumbnailReady { false };

        std::vector<LoopRegion> regions;
        int  selectedRegion   { -1 };
        int  totalSamples     { 0 };
        double sourceSampleRate { 44100.0 };

        // Zoom & pan state.
        double zoom        { 1.0 };  // 1.0 → fits whole file; max 512.0
        double viewStart   { 0.0 };  // first visible sample (double for sub-sample pan)

        // Mouse interaction state.
        enum class DragMode { None, PanView, DragLeftEdge, DragRightEdge };
        DragMode dragMode  { DragMode::None };
        int      dragRegionIndex { -1 };
        int      dragStartMouseX { 0 };
        double   dragStartView   { 0.0 };
        int      dragOriginalEdge{ 0 };
        int      currentDragSample { 0 };

        bool clickWasOnRegion { false };
        int  clickRegionIndex { -1 };

        juce::Point<int> lastMousePos;

        // Cached zero crossings for snapping.
        std::vector<int> zeroCrossings;

        // Cached waveform image (only the static waveform layer — overlays
        // like regions and the playhead are drawn on top each frame).
        juce::Image waveformCache;
        bool        waveformCacheDirty { true };
        // The state used to render the current cache. When any of these
        // change we re-render; otherwise we just blit.
        struct CacheKey
        {
            int    totalSamples = -1;
            double zoom         = 0.0;
            double viewStart    = -1.0;
            int    width        = 0;
            int    height       = 0;
            bool   thumbReady   = false;
            bool operator!= (const CacheKey& o) const noexcept
            {
                return totalSamples != o.totalSamples
                    || zoom         != o.zoom
                    || viewStart    != o.viewStart
                    || width        != o.width
                    || height       != o.height
                    || thumbReady   != o.thumbReady;
            }
        };
        CacheKey lastRenderedKey {};

        static constexpr double maxZoom         = 512.0;
        static constexpr int    overviewHeight  = 36;
        static constexpr int    rulerHeight     = 18;
        static constexpr int    edgeGrabPixels  = 5;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
    };
}
