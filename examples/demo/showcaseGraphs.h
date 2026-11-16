/*
  * Gweni demo
  * See license in gweni.h
 */

#ifndef _gweni_showcaseGraphs_example_h_
#define _gweni_showcaseGraphs_example_h_

#include "demo/showcase.h"

#include <gweni/controls/zoomPanel.h>

#include <functional>
#include <memory>

namespace gweni
{
namespace controls
{
class DragNumber;
class NodeGraph;
}
namespace nodegraphkit
{
class NodeGraphMenus;
}

//! The page's ZoomPanel, telling the page of each new zoom (the wheel's, a
//! setZoom()'s) so its zoom field follows it. It draws nothing, as a ZoomPanel.
class ShowcaseZoomPanel: public controls::ZoomPanel
{
public:
    GWENI_CONTROL(ShowcaseZoomPanel, controls::ZoomPanel);

    GWENI_DRAWS_NOTHING

public:
    std::function<void(float zoom)> zoomChanged;

protected:
    void spaceChanged(bool zoomed) override;
};

//! Canvas & graphs: a NodeGraph of five nodes linked in each style, with its
//! menus, and a ZoomPanel holding ordinary controls at 1.5x, with a zoom
//! field that follows the wheel and a Reset button.
class ShowcaseGraphsDemo: public controls::Base, public ShowcaseNotes
{
public:
    GWENI_CONTROL(ShowcaseGraphsDemo, controls::Base);

    ~ShowcaseGraphsDemo() override;

    const std::vector<String> &showcaseNotes() const override { return m_notes; }

    controls::NodeGraph *graph() const { return m_graph; }
    ShowcaseZoomPanel *zoomPanel() const { return m_panel; }
    controls::DragNumber *zoomField() const { return m_zoomField; }

    //! The zoom and pan the panel opens at, which Reset gives back.
    static constexpr float PanelZoom=1.5f;

private:
    void onZoomField(event::Info info);
    void onReset(event::Info info);

    std::vector<String> m_notes;
    controls::NodeGraph *m_graph=nullptr;
    std::unique_ptr<nodegraphkit::NodeGraphMenus> m_menus;
    ShowcaseZoomPanel *m_panel=nullptr;
    controls::DragNumber *m_zoomField=nullptr;
};

}//namespace gweni

#endif//_gweni_showcaseGraphs_example_h_
