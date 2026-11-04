#ifndef _gweni_exampleCommon_h_
#define _gweni_exampleCommon_h_

#include <gweni/align.h>
#include <gweni/controls/listBox.h>
#include <gweni/controls/dockBase.h>
#include <gweni/controls/statusBar.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/canvas.h>


namespace gweni
{

class ExampleWidow : public gweni::controls::DockBase
{
public:
    void render(sking::Base *skin) override;
            
private:
            
    void onCategorySelect(gweni::Event::Info info);
            
    controls::TabControl *m_tabs;
    controls::StatusBar *m_statusBar;
    unsigned int m_frames;
    float m_lastSecond;
};

}//namespace gweni

#endif//_gweni_exampleCommon_h_
