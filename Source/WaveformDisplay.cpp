#include "WaveformDisplay.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

namespace lf
{
    // =========================================================================
    WaveformDisplay::WaveformDisplay (LoopFinderProcessor& proc)
        : processor (proc),
          thumbnail (512, processor.getFileManager().getFormatManager(),
                     processor.getThumbCache())
    {
        setOpaque (true);
        setInterceptsMouseClicks (true, true);
        setMouseCursor (juce::MouseCursor::NormalCursor);
        thumbnail.addChangeListener (this);

        startTimerHz (30); // for playhead refresh

        refreshSource();
        refreshRegions();
    }

    WaveformDisplay::~WaveformDisplay()
    {
        thumbnail.removeChangeListener (this);
        stopTimer();
    }

    void WaveformDisplay::setCallbacks (Callbacks c) { callbacks = std::move (c); }

    void WaveformDisplay::refreshRegions()
    {
        regions         = processor.getRegions();
        selectedRegion  = processor.getSelectedRegion();
        repaint();
    }

    void WaveformDisplay::refreshSource()
    {
        thumbnail.clear();
        thumbnailReady = false;
        invalidateWaveformCache();

        const auto& meta = processor.getFileManager().getMetadata();
        if (! meta.isLoaded)
        {
            totalSamples = 0;
            sourceSampleRate = 44100.0;
            zeroCrossings.clear();
            zoom = 1.0; viewStart = 0.0;
            repaint();
            return;
        }

        sourceSampleRate = meta.sampleRate;
        totalSamples     = static_cast<int> (meta.numSamples);

        // Feed the AudioBuffer into the thumbnail.
        const auto& buf = processor.getFileManager().getAudioBuffer();
        thumbnail.reset (buf.getNumChannels(), sourceSampleRate, buf.getNumSamples());
        thumbnail.addBlock (0, buf, 0, buf.getNumSamples());

        zoom      = 1.0;
        viewStart = 0.0;
        rebuildZeroCrossings();
        clampView();
        repaint();
    }

    void WaveformDisplay::changeListenerCallback (juce::ChangeBroadcaster* src)
    {
        if (src == &thumbnail)
        {
            thumbnailReady = thumbnail.isFullyLoaded()
                          || thumbnail.getNumSamplesFinished() > 0;
            invalidateWaveformCache();
            repaint();
        }
    }

    void WaveformDisplay::timerCallback()
    {
        // Trigger a repaint of the playhead area. The static waveform is
        // cached, so this is cheap (just a blit + a few overlay primitives).
        repaint();
    }

    // -------------------------------------------------------------------------
    // Layout / paint
    // -------------------------------------------------------------------------
    void WaveformDisplay::resized()
    {
        clampView();
        invalidateWaveformCache();
    }

    juce::Rectangle<int> WaveformDisplay::getMainArea() const
    {
        return getLocalBounds().removeFromTop (
            getHeight() - overviewHeight - rulerHeight);
    }

    juce::Rectangle<int> WaveformDisplay::getRulerArea() const
    {
        auto b = getLocalBounds();
        b.removeFromTop (b.getHeight() - overviewHeight - rulerHeight);
        return b.removeFromTop (rulerHeight);
    }

    juce::Rectangle<int> WaveformDisplay::getOverviewArea() const
    {
        return getLocalBounds().removeFromBottom (overviewHeight);
    }

    void WaveformDisplay::paint (juce::Graphics& g)
    {
        g.fillAll (theme::background);

        const auto main     = getMainArea();
        const auto ruler    = getRulerArea();
        const auto overview = getOverviewArea();

        if (totalSamples <= 0)
        {
            g.setColour (theme::textSecondary);
            g.setFont (theme::body());
            g.drawText ("Drag and drop an audio file here, or use Load File",
                        main, juce::Justification::centred);
            return;
        }

        // ---- static waveform layer (cached) ----
        const CacheKey desired { totalSamples, zoom, viewStart,
                                 main.getWidth(), main.getHeight(), thumbnailReady };
        if (waveformCacheDirty
            || ! waveformCache.isValid()
            || lastRenderedKey != desired)
        {
            renderWaveformCache (main);
            lastRenderedKey   = desired;
            waveformCacheDirty = false;
        }

        if (waveformCache.isValid())
            g.drawImage (waveformCache,
                         (float) main.getX(),       (float) main.getY(),
                         (float) main.getWidth(),   (float) main.getHeight(),
                         0, 0,
                         waveformCache.getWidth(),  waveformCache.getHeight(),
                         false);

        // ---- live overlays ----
        drawRegions   (g, main);
        drawPlayhead  (g, main);
        drawTimeRuler (g, ruler);
        drawOverview  (g, overview);
        drawDragLabel (g);
    }

    void WaveformDisplay::renderWaveformCache (juce::Rectangle<int> area)
    {
        if (area.getWidth() <= 0 || area.getHeight() <= 0)
        {
            waveformCache = {};
            return;
        }

        // Use a hi-DPI backing image so the rendering is crisp on Retina /
        // scaled Windows displays. JUCE's Graphics keeps anti-aliasing on
        // by default for path fills/strokes.
        float scale = 1.0f;
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            scale = juce::jmax (1.0f, (float) d->scale);
        waveformCache = juce::Image (juce::Image::ARGB,
                                     juce::roundToInt ((float) area.getWidth()  * scale),
                                     juce::roundToInt ((float) area.getHeight() * scale),
                                     true);
        juce::Graphics g (waveformCache);
        g.addTransform (juce::AffineTransform::scale (scale));

        const juce::Rectangle<int> local (0, 0, area.getWidth(), area.getHeight());

        if (samplesPerPixel() < 4.0)
            drawWaveformHighZoom (g, local);
        else
            drawWaveformLowZoom  (g, local);
    }

    void WaveformDisplay::drawWaveformLowZoom (juce::Graphics& g, juce::Rectangle<int> area)
    {
        const auto& mono = processor.getFileManager().getMonoMix();
        if (mono.empty()) return;

        // Build a per-pixel-column min / max envelope from the mono samples.
        // Drawing a single filled+stroked path produces a smooth, anti-aliased
        // waveform — much cleaner than JUCE's default thumbnail bar rendering.
        const int   W    = area.getWidth();
        const float midY = (float) area.getCentreY();
        const float amp  = (float) area.getHeight() * 0.45f;
        const double spp = samplesPerPixel();
        const int   N    = (int) mono.size();

        std::vector<float> mins (W, 0.0f), maxs (W, 0.0f);
        for (int x = 0; x < W; ++x)
        {
            const double s0 = viewStart + (double) x * spp;
            const double s1 = s0 + spp;
            const int i0 = juce::jlimit (0, N, (int) std::floor (s0));
            const int i1 = juce::jlimit (i0 + 1, N, (int) std::ceil (s1));
            if (i0 >= N || i1 <= i0) continue;

            float mn = mono[(size_t) i0];
            float mx = mn;
            for (int k = i0 + 1; k < i1; ++k)
            {
                const float v = mono[(size_t) k];
                if (v < mn) mn = v;
                if (v > mx) mx = v;
            }
            mins[(size_t) x] = mn;
            maxs[(size_t) x] = mx;
        }

        // Filled envelope: trace top, then back along bottom, close.
        juce::Path filled;
        filled.preallocateSpace (W * 6);
        filled.startNewSubPath ((float) area.getX(),
                                midY - maxs[0] * amp);
        for (int x = 1; x < W; ++x)
            filled.lineTo ((float) (area.getX() + x), midY - maxs[(size_t) x] * amp);
        for (int x = W - 1; x >= 0; --x)
            filled.lineTo ((float) (area.getX() + x), midY - mins[(size_t) x] * amp);
        filled.closeSubPath();

        g.setColour (theme::waveformFill.withAlpha (0.85f));
        g.fillPath (filled);

        // Outline the top and bottom envelopes for crisper edges. Thin (0.8 px)
        // stroke + anti-aliasing avoids the chunky look of bar rendering.
        juce::Path top, bot;
        top.preallocateSpace (W * 3);
        bot.preallocateSpace (W * 3);
        top.startNewSubPath ((float) area.getX(), midY - maxs[0] * amp);
        bot.startNewSubPath ((float) area.getX(), midY - mins[0] * amp);
        for (int x = 1; x < W; ++x)
        {
            top.lineTo ((float) (area.getX() + x), midY - maxs[(size_t) x] * amp);
            bot.lineTo ((float) (area.getX() + x), midY - mins[(size_t) x] * amp);
        }

        g.setColour (theme::waveformOutline);
        const juce::PathStrokeType stroke (0.9f,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded);
        g.strokePath (top, stroke);
        g.strokePath (bot, stroke);
    }

    void WaveformDisplay::drawWaveformHighZoom (juce::Graphics& g, juce::Rectangle<int> area)
    {
        const auto& mono = processor.getFileManager().getMonoMix();
        if (mono.empty()) return;

        const float midY = (float) area.getCentreY();
        const float ampScale = (float) area.getHeight() * 0.45f;
        const double spp = samplesPerPixel();
        const int   N    = (int) mono.size();

        // Sample at sub-pixel resolution (2x) so the path is smoother when
        // zoomed in close to the sample level. JUCE will anti-alias the path.
        const int   subSteps = 2;
        const double subSpp  = spp / (double) subSteps;
        const int    W       = area.getWidth();

        juce::Path path;
        path.preallocateSpace (W * subSteps * 3);
        bool started = false;

        for (int x = 0; x < W * subSteps; ++x)
        {
            const double sampleIdx = viewStart + (double) x * subSpp;
            const int    i = (int) std::floor (sampleIdx);
            if (i < 0 || i >= N) continue;

            float v;
            if (spp <= 1.0)
            {
                const float frac = (float) (sampleIdx - i);
                const float a = mono[(size_t) i];
                const float b = (i + 1 < N) ? mono[(size_t) (i + 1)] : a;
                v = a + (b - a) * frac;
            }
            else
            {
                const int end = juce::jlimit (i + 1, N,
                                              i + juce::jmax (1, (int) std::ceil (subSpp)));
                float maxAbs = 0.0f, signedV = 0.0f;
                for (int k = i; k < end; ++k)
                {
                    const float s = mono[(size_t) k];
                    if (std::abs (s) > maxAbs) { maxAbs = std::abs (s); signedV = s; }
                }
                v = signedV;
            }

            const float xPx = (float) area.getX() + (float) x / (float) subSteps;
            const float y   = midY - v * ampScale;
            if (! started) { path.startNewSubPath (xPx, y); started = true; }
            else            path.lineTo            (xPx, y);
        }

        g.setColour (theme::waveformOutline);
        g.strokePath (path,
                      juce::PathStrokeType (1.4f,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    }

    void WaveformDisplay::drawRegions (juce::Graphics& g, juce::Rectangle<int> area)
    {
        for (int i = 0; i < (int) regions.size(); ++i)
        {
            const auto& r = regions[(size_t) i];
            const int x0 = sampleToPixel (r.startSample);
            const int x1 = sampleToPixel (r.endSample);
            if (x1 < area.getX() || x0 > area.getRight()) continue;

            const auto col = theme::regionColour (i);
            const bool sel = (i == selectedRegion);

            // Translucent body
            g.setColour (col.withAlpha (sel ? 0.45f : 0.30f));
            g.fillRect (juce::Rectangle<int> (x0, area.getY(),
                                              std::max (1, x1 - x0), area.getHeight()));

            // Boundaries
            g.setColour (col.withAlpha (1.0f));
            g.fillRect (juce::Rectangle<int> (x0 - 1, area.getY(), 2, area.getHeight()));
            g.fillRect (juce::Rectangle<int> (x1 - 1, area.getY(), 2, area.getHeight()));

            // Number badge
            g.setColour (col.darker (0.4f));
            const auto badge = juce::Rectangle<int> (x0 + 4, area.getY() + 4, 18, 16);
            g.fillRoundedRectangle (badge.toFloat(), 3.0f);
            g.setColour (theme::textPrimary);
            g.setFont (theme::label());
            g.drawText (juce::String (i + 1), badge, juce::Justification::centred);
        }
    }

    void WaveformDisplay::drawPlayhead (juce::Graphics& g, juce::Rectangle<int> area)
    {
        if (! processor.getPlayback().isPlaying()) return;
        const int pos = processor.getPlayback().getPlayheadSourcePos();
        const int px  = sampleToPixel (pos);
        if (px < area.getX() || px > area.getRight()) return;
        g.setColour (theme::playhead);
        g.fillRect (juce::Rectangle<int> (px, area.getY(), 1, area.getHeight()));
    }

    void WaveformDisplay::drawTimeRuler (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setColour (theme::surface);
        g.fillRect (area);
        g.setColour (theme::border);
        g.drawHorizontalLine (area.getY(), (float) area.getX(), (float) area.getRight());

        if (totalSamples <= 0) return;

        // Choose a tick interval based on samplesPerPixel.
        const double sppx = samplesPerPixel();
        const double secondsPerPixel = sppx / sourceSampleRate;
        const double targetPxBetween = 80.0;
        const double targetSec = targetPxBetween * secondsPerPixel;

        // Snap to a "nice" step from a fixed list.
        constexpr double steps[] {
            0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01, 0.02, 0.05,
            0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0
        };
        double step = steps[std::size (steps) - 1];
        for (auto s : steps) if (s >= targetSec) { step = s; break; }

        g.setColour (theme::textSecondary);
        g.setFont (juce::Font (10.0f));

        const double startSec = viewStart / sourceSampleRate;
        const double endSec   = startSec + area.getWidth() * secondsPerPixel;
        const double firstTick = std::floor (startSec / step) * step;

        for (double t = firstTick; t <= endSec; t += step)
        {
            const int px = sampleToPixel (static_cast<int> (t * sourceSampleRate));
            if (px < area.getX() || px > area.getRight()) continue;
            g.fillRect ((float) px, (float) area.getY(), 1.0f, 5.0f);

            juce::String label;
            if (step >= 1.0)         label = juce::String (t, 1) + "s";
            else if (step >= 0.001)  label = juce::String ((int) std::round (t * 1000.0)) + "ms";
            else                     label = juce::String (t * 1000.0, 2) + "ms";

            g.drawText (label,
                        juce::Rectangle<int> (px + 3, area.getY() + 3, 60, area.getHeight() - 4),
                        juce::Justification::centredLeft, false);
        }
    }

    void WaveformDisplay::drawOverview (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setColour (theme::surface);
        g.fillRect (area);
        g.setColour (theme::border);
        g.drawRect (area);

        if (! thumbnailReady) return;

        const auto inner = area.reduced (4, 6);
        g.setColour (theme::waveformFill.withAlpha (0.7f));
        thumbnail.drawChannels (g, inner, 0.0,
                                static_cast<double> (totalSamples) / sourceSampleRate,
                                0.95f);

        // Region marks
        for (int i = 0; i < (int) regions.size(); ++i)
        {
            const auto& r = regions[(size_t) i];
            const auto col = theme::regionColour (i);
            const auto x0 = juce::jmap ((double) r.startSample, 0.0, (double) totalSamples,
                                        (double) inner.getX(), (double) inner.getRight());
            const auto x1 = juce::jmap ((double) r.endSample,   0.0, (double) totalSamples,
                                        (double) inner.getX(), (double) inner.getRight());
            g.setColour (col.withAlpha (0.55f));
            g.fillRect (juce::Rectangle<float> ((float) x0, (float) inner.getY(),
                                                std::max (1.0f, (float) (x1 - x0)),
                                                (float) inner.getHeight()));
        }

        // Viewport rectangle showing what the main area is currently displaying.
        const auto vx0 = juce::jmap (viewStart,
                                     0.0, (double) totalSamples,
                                     (double) inner.getX(), (double) inner.getRight());
        const auto vx1 = juce::jmap (viewStart + getMainArea().getWidth() * samplesPerPixel(),
                                     0.0, (double) totalSamples,
                                     (double) inner.getX(), (double) inner.getRight());
        g.setColour (theme::accent.withAlpha (0.18f));
        g.fillRect (juce::Rectangle<float> ((float) vx0, (float) inner.getY(),
                                            std::max (2.0f, (float) (vx1 - vx0)),
                                            (float) inner.getHeight()));
        g.setColour (theme::accent);
        g.drawRect (juce::Rectangle<float> ((float) vx0, (float) inner.getY(),
                                            std::max (2.0f, (float) (vx1 - vx0)),
                                            (float) inner.getHeight()), 1.0f);
    }

    void WaveformDisplay::drawDragLabel (juce::Graphics& g)
    {
        if (dragMode != DragMode::DragLeftEdge && dragMode != DragMode::DragRightEdge)
            return;

        const auto x = sampleToPixel (currentDragSample);
        const auto y = lastMousePos.getY();
        const auto timeMs = (double) currentDragSample * 1000.0 / sourceSampleRate;

        const juce::String text =
            "sample " + juce::String (currentDragSample) +
            "  •  " + juce::String (timeMs, 2) + " ms";

        g.setFont (theme::label());
        const auto w = g.getCurrentFont().getStringWidth (text) + 12;
        const auto rect = juce::Rectangle<int> (x + 8, y - 24, w, 18)
                            .constrainedWithin (getLocalBounds());

        g.setColour (theme::surface.withAlpha (0.95f));
        g.fillRoundedRectangle (rect.toFloat(), 3.0f);
        g.setColour (theme::accent);
        g.drawRoundedRectangle (rect.toFloat(), 3.0f, 1.0f);
        g.setColour (theme::textPrimary);
        g.drawText (text, rect, juce::Justification::centred);
    }

    // -------------------------------------------------------------------------
    // Coordinate helpers
    // -------------------------------------------------------------------------
    double WaveformDisplay::samplesPerPixel() const
    {
        const auto w = getMainArea().getWidth();
        if (w <= 0 || totalSamples <= 0 || zoom <= 0.0) return 1.0;
        return ((double) totalSamples / (double) w) / zoom;
    }

    int WaveformDisplay::pixelToSample (int x) const
    {
        const auto main = getMainArea();
        return (int) std::round (viewStart + (x - main.getX()) * samplesPerPixel());
    }

    int WaveformDisplay::sampleToPixel (int s) const
    {
        const auto main = getMainArea();
        const double spp = samplesPerPixel();
        if (spp <= 0.0) return main.getX();
        return (int) std::round (main.getX() + ((double) s - viewStart) / spp);
    }

    void WaveformDisplay::clampView()
    {
        if (totalSamples <= 0) { viewStart = 0.0; return; }
        const double visible = getMainArea().getWidth() * samplesPerPixel();
        const double maxStart = std::max (0.0, (double) totalSamples - visible);
        viewStart = std::clamp (viewStart, 0.0, maxStart);
    }

    void WaveformDisplay::zoomAt (int mouseX, double newZoom)
    {
        newZoom = std::clamp (newZoom, 1.0, maxZoom);
        if (juce::approximatelyEqual (newZoom, zoom)) return;
        const auto main = getMainArea();
        const double sampleUnderMouse = viewStart
                                       + (mouseX - main.getX()) * samplesPerPixel();
        zoom = newZoom;
        const double newSpp = samplesPerPixel();
        viewStart = sampleUnderMouse - (mouseX - main.getX()) * newSpp;
        clampView();
        invalidateWaveformCache();
        repaint();
    }

    // -------------------------------------------------------------------------
    // Hit testing
    // -------------------------------------------------------------------------
    WaveformDisplay::Hit WaveformDisplay::hitTestAt (juce::Point<int> pos) const
    {
        if (! getMainArea().contains (pos))
            return { HitKind::None, -1 };

        // Boundaries take priority over body.
        for (int i = 0; i < (int) regions.size(); ++i)
        {
            const auto& r = regions[(size_t) i];
            const int xL = sampleToPixel (r.startSample);
            const int xR = sampleToPixel (r.endSample);
            if (std::abs (pos.x - xL) <= edgeGrabPixels) return { HitKind::RegionLeft,  i };
            if (std::abs (pos.x - xR) <= edgeGrabPixels) return { HitKind::RegionRight, i };
        }
        for (int i = 0; i < (int) regions.size(); ++i)
        {
            const auto& r = regions[(size_t) i];
            const int xL = sampleToPixel (r.startSample);
            const int xR = sampleToPixel (r.endSample);
            if (pos.x >= xL && pos.x <= xR) return { HitKind::RegionBody, i };
        }
        return { HitKind::EmptyArea, -1 };
    }

    // -------------------------------------------------------------------------
    // Mouse
    // -------------------------------------------------------------------------
    void WaveformDisplay::mouseMove (const juce::MouseEvent& e)
    {
        lastMousePos = e.getPosition();
        const auto hit = hitTestAt (e.getPosition());
        switch (hit.kind)
        {
            case HitKind::RegionLeft:
            case HitKind::RegionRight: setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
            case HitKind::RegionBody:  setMouseCursor (juce::MouseCursor::PointingHandCursor);    break;
            case HitKind::EmptyArea:   setMouseCursor (zoom > 1.01 ? juce::MouseCursor::DraggingHandCursor
                                                                   : juce::MouseCursor::NormalCursor); break;
            case HitKind::None:        setMouseCursor (juce::MouseCursor::NormalCursor); break;
        }

        if (totalSamples > 0)
        {
            const int s = pixelToSample (e.x);
            setTooltip ("sample " + juce::String (s)
                       + "  •  " + juce::String (s * 1000.0 / sourceSampleRate, 2) + " ms");
        }
    }

    void WaveformDisplay::mouseDown (const juce::MouseEvent& e)
    {
        lastMousePos = e.getPosition();
        clickWasOnRegion = false;
        clickRegionIndex = -1;

        const auto hit = hitTestAt (e.getPosition());
        switch (hit.kind)
        {
            case HitKind::RegionLeft:
                dragMode = DragMode::DragLeftEdge;
                dragRegionIndex = hit.regionIndex;
                dragOriginalEdge = regions[(size_t) hit.regionIndex].startSample;
                currentDragSample = dragOriginalEdge;
                break;

            case HitKind::RegionRight:
                dragMode = DragMode::DragRightEdge;
                dragRegionIndex = hit.regionIndex;
                dragOriginalEdge = regions[(size_t) hit.regionIndex].endSample;
                currentDragSample = dragOriginalEdge;
                break;

            case HitKind::RegionBody:
                clickWasOnRegion = true;
                clickRegionIndex = hit.regionIndex;
                dragMode = DragMode::None;
                break;

            case HitKind::EmptyArea:
                dragMode        = DragMode::PanView;
                dragStartMouseX = e.x;
                dragStartView   = viewStart;
                break;

            case HitKind::None:
                dragMode = DragMode::None;
                break;
        }
    }

    void WaveformDisplay::mouseDrag (const juce::MouseEvent& e)
    {
        lastMousePos = e.getPosition();

        switch (dragMode)
        {
            case DragMode::PanView:
            {
                const double dx = (double) (e.x - dragStartMouseX);
                viewStart = dragStartView - dx * samplesPerPixel();
                clampView();
                invalidateWaveformCache();
                repaint();
                break;
            }
            case DragMode::DragLeftEdge:
            case DragMode::DragRightEdge:
            {
                int newSample = pixelToSample (e.x);
                newSample = std::clamp (newSample, 0, totalSamples);
                // Snap to nearest zero crossing within ±5ms.
                const int snapWindow = (int) std::round (0.005 * sourceSampleRate);
                const int snapped    = snapToZeroCrossing (newSample, snapWindow);
                if (snapped >= 0) newSample = snapped;
                currentDragSample = newSample;
                clickWasOnRegion = false;
                repaint();
                break;
            }
            case DragMode::None:
                if (e.getDistanceFromDragStart() > 4)
                    clickWasOnRegion = false;
                break;
        }
    }

    void WaveformDisplay::mouseUp (const juce::MouseEvent&)
    {
        if (clickWasOnRegion && clickRegionIndex >= 0 && callbacks.onRegionSelected)
            callbacks.onRegionSelected (clickRegionIndex);

        if ((dragMode == DragMode::DragLeftEdge || dragMode == DragMode::DragRightEdge)
            && dragRegionIndex >= 0
            && dragRegionIndex < (int) regions.size())
        {
            auto r = regions[(size_t) dragRegionIndex];
            if (dragMode == DragMode::DragLeftEdge)
                r.startSample = std::min (currentDragSample, r.endSample - 16);
            else
                r.endSample   = std::max (currentDragSample, r.startSample + 16);

            if (callbacks.onRegionEdited)
                callbacks.onRegionEdited (dragRegionIndex, r.startSample, r.endSample);
        }

        dragMode = DragMode::None;
        dragRegionIndex = -1;
        clickWasOnRegion = false;
        repaint();
    }

    void WaveformDisplay::mouseDoubleClick (const juce::MouseEvent& e)
    {
        const auto hit = hitTestAt (e.getPosition());
        if (hit.kind == HitKind::RegionBody && callbacks.onRegionAuditioned)
            callbacks.onRegionAuditioned (hit.regionIndex);
    }

    void WaveformDisplay::mouseWheelMove (const juce::MouseEvent& e,
                                          const juce::MouseWheelDetails& w)
    {
        const double factor = std::pow (1.18, w.deltaY * 4.0);
        zoomAt (e.x, zoom * factor);
    }

    // -------------------------------------------------------------------------
    // Zero-crossing snapping
    // -------------------------------------------------------------------------
    void WaveformDisplay::rebuildZeroCrossings()
    {
        zeroCrossings.clear();
        const auto& mono = processor.getFileManager().getMonoMix();
        if (mono.size() < 2) return;

        zeroCrossings.reserve (mono.size() / 64);
        for (int i = 1; i < (int) mono.size(); ++i)
            if (mono[(size_t) i] > 0.0f && mono[(size_t) i - 1] <= 0.0f)
                zeroCrossings.push_back (i);
    }

    int WaveformDisplay::snapToZeroCrossing (int sample, int withinSamples) const
    {
        if (zeroCrossings.empty() || withinSamples <= 0) return -1;

        auto it = std::lower_bound (zeroCrossings.begin(), zeroCrossings.end(), sample);
        int best = -1;
        int bestDist = withinSamples + 1;

        if (it != zeroCrossings.end())
        {
            const int d = std::abs (*it - sample);
            if (d <= withinSamples) { best = *it; bestDist = d; }
        }
        if (it != zeroCrossings.begin())
        {
            auto prev = std::prev (it);
            const int d = std::abs (*prev - sample);
            if (d <= withinSamples && d < bestDist) { best = *prev; }
        }
        return best;
    }
}
