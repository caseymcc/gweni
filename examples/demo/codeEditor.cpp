#include "demo/codeEditor.h"
#include "gweni/controls/codeEditor.h"
#include "gweni/controls/checkBox.h"
#include "gweni/controls/label.h"
#include "gweni/controls/textBox.h"

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
    case 11: return "      You work on task "+id+" of the gweni project: read its brief and the comments first, build"
        " in the container, run the tests that need a context under the render lock, and keep each change to"
        " the lines it needs.";
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

// A 5000-line YAML document in a CodeEditor filling the demo below a title
// and a wrap toggle, highlighted as YAML: only the lines on screen are
// controls, its line numbers in a gutter; each agent's prompt line is long
// enough to wrap.
GWENI_CONTROL_CONSTRUCTOR(CodeEditorDemo)
{
    setDock(DockPosition::Center);

    m_wrapToggle=newChild<controls::CheckBoxWithLabel>("wrap");
    m_wrapToggle->label()->setText("Wrap lines");
    m_wrapToggle->setPos(10, 8);
    m_wrapToggle->checkBox()->onCheckChanged.add(this, &CodeEditorDemo::onWrapChanged);

    controls::Label *title=newChild<controls::Label>();

    title->setText(std::to_string(LineCount)+" lines of highlighted YAML, a row for each one on screen");
    title->setPos(110, 10);

    m_editor=newChild<controls::CodeEditor>();
    m_editor->setDock(DockPosition::Center);
    m_editor->setMargin(Margin(10, 34, 10, 10));
    m_editor->setLineNumbers(true);
    m_editor->setLanguage("yaml");
    m_editor->setText(yamlDocument(LineCount));
}

void CodeEditorDemo::onWrapChanged(event::Info)
{
    m_editor->setWrap(m_wrapToggle->checkBox()->isChecked());
}

// A form of plain auto-height fields, as a dashboard's task form: a title
// TextBox, then a description and a comment, each a CodeEditor in its plain
// mode growing with its text between its least and most rows.
GWENI_CONTROL_CONSTRUCTOR(CodeEditorFormDemo)
{
    setDock(DockPosition::Center);

    controls::Base *form=newChild<controls::Base>("form");

    form->setDock(DockPosition::Left);
    form->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
    form->setWidth(FormWidth);
    form->setMargin(Margin(10, 10, 10, 10));

    auto label=[form](const char *text)
        {
            controls::Label *label=form->newChild<controls::Label>();

            label->setText(text);
            label->setDock(DockPosition::Top);
            label->setMargin(Margin(0, 8, 0, 2));
            return label;
        };
    auto field=[form](const char *name, int least, int most)
        {
            controls::CodeEditor *editor=form->newChild<controls::CodeEditor>(name);

            editor->setDock(DockPosition::Top);
            editor->setPlain(true);
            editor->setAutoHeight(least, most);
            return editor;
        };

    label("Title");
    m_title=form->newChild<controls::TextBox>("title");
    m_title->setDock(DockPosition::Top);
    m_title->setText("Wrap the code editor");

    label("Description: wraps, grows from 3 to 8 rows");
    m_description=field("description", 3, 8);
    m_description->setText("The prompt editor needs a wrap toggle, and the form's text areas an editor as tall"
        " as its text that wraps and behaves like a browser's textarea.\n\nTab moves the focus to the next"
        " field; every column is a cell of the mono face.");

    label("Comment: grows from 2 to 6 rows");
    m_comment=field("comment", 2, 6);

    controls::Label *note=label("Plain fields are monospace, as every CodeEditor.");

    note->setMargin(Margin(0, 12, 0, 0));
}

}//namespace gweni
