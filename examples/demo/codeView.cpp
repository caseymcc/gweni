#include "demo/codeView.h"
#include "gweni/controls/codeView.h"
#include "gweni/controls/label.h"

namespace gweni
{

const char *CodeViewDemo::diffText()
{
    return
        "diff --git a/prompts/coder.md.j2 b/prompts/coder.md.j2\n"
        "index 3f2a9c1..8d41e07 100644\n"
        "--- a/prompts/coder.md.j2\n"
        "+++ b/prompts/coder.md.j2\n"
        "@@ -1,9 +1,10 @@\n"
        " # Coder\n"
        " \n"
        " You are an autonomous coding agent working on {{ project }}.\n"
        " \n"
        "-Working directory: {{ workdir }}.\n"
        "+Working directory: {{ workdir }}, a git worktree on\n"
        "+branch `{{ branch }}`.\n"
        " \n"
        " ## Rules\n"
        " - Read the repository's AGENTS.md and follow it.\n"
        " - Commit your work with clear messages.\n"
        "@@ -14,8 +15,9 @@ ## Rules\n"
        " ## Finishing\n"
        " \n"
        "-Finish with `complete` or `block`.\n"
        "+Finish with `complete` or `block`, then a short\n"
        "+summary of what changed and anything left open.\n"
        " \n"
        " {% if steps %}\n"
        " ## The steps of this task\n"
        " {% for step in steps %}\n"
        " - [ ] {{ step.title }}\n"
        "@@ -30,7 +32,12 @@ ## The steps of this task\n"
        " {% endfor %}\n"
        " {% endif %}\n"
        " \n"
        "-## Pictures\n"
        "-Attach a screenshot when the change is visible.\n"
        "+## Pictures: show what you changed\n"
        "+\n"
        "+A finished task with a visible change comes with\n"
        "+before and after pictures on `complete`:\n"
        "+1. Capture **before** from the code without your change.\n"
        "+2. Capture **after** under exactly the same conditions.\n"
        "+3. Attach both, Before first.\n"
        " \n"
        " ## Feature";
}

const char *CodeViewDemo::configText()
{
    return
        "# agents/coder.yaml\n"
        "name: coder\n"
        "description: Implements a task on its branch.\n"
        "model: claude-opus-5-5\n"
        "effort: high\n"
        "tools:\n"
        "  - bash\n"
        "  - edit\n"
        "locks:\n"
        "  render: 1\n"
        "  quiet: false\n"
        "timeout_minutes: 120";
}

const char *CodeViewDemo::installText()
{
    return "./run.sh -b controltest";
}

// The diff fills the left column below its title, scrolling; the config is
// docked Top in the right column, as tall as its lines, and the command
// placed in a row below it, as tall and as wide as its line.
GWENI_CONTROL_CONSTRUCTOR(CodeViewDemo)
{
    setDock(DockPosition::Center);

    auto title=[](controls::Base *parent, const char *text)
        {
            controls::Label *label=parent->newChild<controls::Label>();

            label->setText(text);
            label->setDock(DockPosition::Top);
            label->setMargin(Margin(0, 0, 0, 4));
            return label;
        };

    controls::Base *left=newChild<controls::Base>("diff column");

    left->setDock(DockPosition::Left);
    left->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
    left->setWidth(DiffWidth);
    left->setMargin(Margin(10, 10, 0, 10));

    title(left, "prompts/coder.md.j2, v3 to v4");
    m_diff=left->newChild<controls::CodeView>("diff");
    m_diff->setDock(DockPosition::Center);
    m_diff->setFileName("coder.md.j2.diff");
    m_diff->setText(diffText());
    m_diff->setCopyButton(true);

    controls::Base *right=newChild<controls::Base>("right column");

    right->setDock(DockPosition::Center);
    right->setMargin(Margin(10, 10, 10, 10));

    title(right, "agents/coder.yaml");
    m_config=right->newChild<controls::CodeView>("config");
    m_config->setDock(DockPosition::Top);
    m_config->setLanguage("yaml");
    m_config->setLineNumbers(true);
    m_config->setText(configText());
    m_config->setCopyButton(true);

    controls::Label *install=title(right, "Install");

    install->setMargin(Margin(0, 12, 0, 4));
    // placed, not docked: as wide as its line
    controls::Base *row=right->newChild<controls::Base>("install row");

    row->setDock(DockPosition::Top);
    m_install=row->newChild<controls::CodeView>("install");
    m_install->setPos(0, 0);
    m_install->setLanguage("bash");
    m_install->setText(installText());
    m_install->setCopyButton(true);
}

}//namespace gweni
