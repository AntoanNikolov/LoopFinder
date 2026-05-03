#pragma once

#include "LoopRegion.h"
#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace lf
{
    class LoopFinderProcessor;

    class RegionListPanel  : public juce::Component
    {
    public:
        struct Callbacks
        {
            std::function<void (int regionIdx)> onSelect;
            std::function<void (int regionIdx)> onExport;
            std::function<void (int regionIdx)> onHover;
            std::function<void ()>              onExportAll;
        };

        explicit RegionListPanel (LoopFinderProcessor& proc);
        ~RegionListPanel() override;

        void setCallbacks (Callbacks c);
        void refresh();

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        class RegionRow;

        LoopFinderProcessor& processor;
        Callbacks callbacks;

        juce::Viewport viewport;
        juce::Component listContainer;
        std::vector<std::unique_ptr<RegionRow>> rows;

        juce::TextButton exportAllBtn { "Export All" };
        juce::Label      titleLabel;

        void rebuildRows();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionListPanel)
    };
}
