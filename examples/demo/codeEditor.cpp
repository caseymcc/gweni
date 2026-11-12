#include "demo/codeEditor.h"
#include "gweni/controls/codeEditor.h"
#include "gweni/controls/label.h"

#include <string>

namespace gweni
{

// One agent's block, line `i` of it: 16 lines an agent, numbered from the
// agent's index, so every line depends on its number alone.
static String agentLine(size_t agent, size_t i)
{
    const String id=std::to_string(agent);

    switch(i)
    {
    case 0: return "  - name: agent-"+id;
    case 1: return "    model: "+String((agent%3 == 0)?"opus":(agent%3 == 1)?"sonnet":"haiku");
    case 2: return "    host: worker-"+std::to_string(agent%7);
    case 3: return "    tags: [ui, text, layout-"+std::to_string(agent%5)+"]";
    case 4: return "    limits:";
    case 5: return "      tokens: "+std::to_string(8000+(agent*937)%24000);
    case 6: return "      minutes: "+std::to_string(10+agent%50);
    case 7: return "    locks:";
    case 8: return "      - render";
    case 9: return "      - quiet  # timed runs only";
    case 10: return "    prompt: |";
    case 11: return "      You work on task "+id+" of the gweni project.";
    case 12: return "      Read the brief first; keep the caf\xc3\xa9 open.";
    case 13: return "    enabled: "+String((agent%4 == 0)?"false":"true");
    case 14: return "    retries: "+std::to_string(agent%3);
    default: return "";
    }
}

String CodeEditorDemo::yamlDocument(size_t lines)
{
    String text;
    const char *header[]={"# gweni agents, generated", "project: gweni", "version: 3", "agents:"};
    const size_t headerLines=sizeof(header)/sizeof(header[0]);

    for(size_t line=0; line < lines; ++line)
    {
        if(line > 0)
            text+='\n';

        if(line < headerLines)
            text+=header[line];
        else
            text+=agentLine((line-headerLines)/16, (line-headerLines)%16);
    }
    return text;
}

// A 5000-line YAML document in a CodeEditor filling the demo below a title:
// only the lines on screen are controls, its line numbers in a gutter.
GWENI_CONTROL_CONSTRUCTOR(CodeEditorDemo)
{
    setDock(DockPosition::Center);

    controls::Label *title=newChild<controls::Label>();

    title->setText(std::to_string(LineCount)+" lines of YAML, a row for each one on screen");
    title->setPos(10, 10);

    m_editor=newChild<controls::CodeEditor>();
    m_editor->setDock(DockPosition::Center);
    m_editor->setMargin(Margin(10, 34, 10, 10));
    m_editor->setLineNumbers(true);
    m_editor->setText(yamlDocument(LineCount));
}

}//namespace gweni
