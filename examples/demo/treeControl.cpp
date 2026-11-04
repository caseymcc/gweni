#include "demo/treeControl.h"
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/treeControl.h"

namespace gweni
{

GWENI_CONTROL_CONSTRUCTOR(TreeControlDemo)
{
    setLayout(new controls::VerticalLayout());

    {
        controls::TreeControl *ctrl=newChild<controls::TreeControl>();
        ctrl->addNode("Node One");
        controls::TreeNode *pNode=ctrl->addNode("Node Two");
        pNode->addNode("Node Two Inside");
        pNode->addNode("Eyes");
        pNode->addNode("Brown")->addNode("Node Two Inside")->addNode("Eyes")->addNode("Brown");
        pNode->addNode("More");
        pNode->addNode("Nodes");
        ctrl->addNode("Node Three");
        ctrl->setBounds(30, 30, 200, 200);
        ctrl->expandAll();
    }
    {
        controls::TreeControl *ctrl=newChild<controls::TreeControl>();
        ctrl->allowMultiSelect(true);
        ctrl->addNode("Node One");
        controls::TreeNode *pNode=ctrl->addNode("Node Two");
        pNode->addNode("Node Two Inside");
        pNode->addNode("Eyes");
        controls::TreeNode *pNodeTwo=pNode->addNode("Brown")->addNode("Node Two Inside")->addNode("Eyes");
        pNodeTwo->addNode("Brown");
        pNodeTwo->addNode("Green");
        pNodeTwo->addNode("Slime");
        pNodeTwo->addNode("Grass");
        pNodeTwo->addNode("Pipe");
        pNode->addNode("More");
        pNode->addNode("Nodes");
        ctrl->addNode("Node Three");
        ctrl->setBounds(240, 30, 200, 200);
        ctrl->expandAll();
    }
}

}//namespace gweni
