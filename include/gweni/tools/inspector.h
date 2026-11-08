/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_tools_inspector_h_
#define _gweni_tools_inspector_h_

#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/treeControl.h>
#include <gweni/controls/treeNode.h>
#include <gweni/controls/windowControl.h>

#include <vector>

namespace gweni
{
namespace tools
{

class Inspector;

//! A node of the Inspector's tree: one control, its children's nodes made only
//! when it is first opened (a placeholder node until then shows its toggle).
class GWENI_EXPORT InspectorNode: public controls::TreeNode
{
public:
    GWENI_CONTROL(InspectorNode, controls::TreeNode);
public:

    //! The control this node shows. Only compared, never followed, until the
    //! inspector has found it among its parent's children again.
    controls::Base *target() const { return m_target; }
    //! Its children's nodes are made (it was opened once).
    bool populated() const { return m_populated; }

    void open() override;
    void setSelected(bool b, bool fireEvents=true) override;

private:
    friend class Inspector;

    //! Shows target under inspector, with a placeholder child if it has children.
    void bind(Inspector *inspector, controls::Base *target);

    Inspector *m_inspector=nullptr;
    controls::Base *m_target=nullptr;
    bool m_populated=false;
};

//! The Inspector's highlight box: a frame drawn just outside the control the
//! inspector points at (the hovered node's, else the selected one). On the
//! canvas it is an overlay (Canvas::addOverlay), numbered above every other
//! control but the ToolTip box. It takes no input, and is excluded from the
//! frame counts like the inspector. Its renderer draws its four edges; it
//! moves, and so draws, only when its target's rect changes.
class GWENI_EXPORT InspectorHighlight: public controls::Base
{
public:
    GWENI_CONTROL(InspectorHighlight, controls::Base);
public:
    ~InspectorHighlight() override;

    //! The edges' thickness, in logical units, outside the target's rect.
    static constexpr int Edge=2;

private:
    friend class Inspector;
    Inspector *m_inspector=nullptr;
};

//! A window on a canvas showing what the canvas is made of and what its last
//! frame did (docs/architecture.md, "The inspector"):
//! - the control tree, a node per control (type, name, id), each node's
//!   children made when it is opened;
//! - the selected control's type, bounds, z and layers, primitive and text ids,
//!   pending state changes and hidden/disabled/clip state;
//! - the previous frame's counters: Canvas::previousFrameStats() (z renumber,
//!   animations, layout passes and entries, the controls rendered and their
//!   state changes) and the renderer's slots written and uploaded.
//!
//! It shows the app's work alone. Its window is setExcludedFromStats(), so
//! none of its own controls count in the canvas's counters, and the slots it
//! wrote itself are taken out of renderer::Base::writtenPrimitiveIds() and
//! writtenTextIds() by the ids its controls hold (skin::Control::appendIds).
//! Uploads cannot be split by id: they are shown for a frame the app wrote
//! slots in, as that frame's whole uploads, and as 0 for a frame only the
//! inspector wrote in. It refreshes in the update phase, from an animation of
//! its own (excluded too), and sets a label only when its string changes, so
//! over a settled app it settles: it shows 0, and the frames after write 0.
//!
//! The library binds no key: an app opens and closes it (open(), close(),
//! toggle()). Its close button deletes it.
class GWENI_EXPORT Inspector: public controls::WindowControl
{
public:
    GWENI_CONTROL(Inspector, controls::WindowControl);
public:
    ~Inspector() override;

    //! The canvas's inspector, made and shown if it has none.
    static Inspector *open(controls::Canvas *canvas);
    //! Deletes the canvas's inspector (delayed), if it has one.
    static void close(controls::Canvas *canvas);
    //! Opens the inspector, or closes it if it is open. Returns whether it is open.
    static bool toggle(controls::Canvas *canvas);
    //! The canvas's inspector, or null.
    static Inspector *find(controls::Canvas *canvas);

    //! Shows control's details and selects its node, opening the nodes of its
    //! ancestors so it has one; null selects nothing.
    void select(controls::Base *control);
    controls::Base *selected() const { return m_selected; }

    //! Reads the previous frame's counters and the selected control's state
    //! and sets what changed. Its animation calls it once a frame, at the start
    //! of the canvas's update phase; without GWENI_ANIMATE the app calls it.
    void refresh();

    controls::TreeControl *tree() const { return m_tree; }
    //! The node showing control, or null (not made, its parent never opened).
    InspectorNode *nodeFor(const controls::Base *control) const;
    //! The lines it shows, in order: the counters, then the details.
    std::vector<String> counterLines() const;
    std::vector<String> detailLines() const;
    //! The Label setText() calls refresh() made since the inspector was opened.
    size_t textSets() const { return m_textSets; }

    //! The highlight box, made with the inspector next to it (a child of its
    //! parent), and the control it frames now, or null.
    InspectorHighlight *highlight() const { return m_highlight; }
    controls::Base *highlightTarget() const { return m_highlightTarget; }

    //! The primitive and text ids the inspector's own controls hold, its
    //! highlight box's included.
    void ownIds(std::vector<size_t> &primitiveIds, std::vector<size_t> &textIds) const;

    //! The counter lines, one Label each.
    enum CounterLine
    {
        UpdateLine,
        LayoutLine,
        RedirtiedLine,
        RenderLine,
        ChangesLine,
        SlotsLine,
        UploadsLine,
        CounterLines
    };
    //! The detail lines, one Label each.
    enum DetailLine
    {
        NameLine,
        BoundsLine,
        ZLine,
        PrimitivesLine,
        TextIdsLine,
        PendingLine,
        StateLine,
        DetailLines
    };

private:
    friend class InspectorNode;
    friend class InspectorHighlight;

    void setLine(controls::Label *label, const String &text);
    void refreshCounters();
    void refreshDetails();
    //! Points the highlight box at the hovered node's control, else the
    //! selected one, and moves it to that control's rect.
    void refreshHighlight();
    //! Brings the tree's nodes in line with the canvas's tree.
    void syncTree();
    //! Makes node's (or, null, the tree's top-level) child nodes for parent's
    //! children, the targets in open being opened again.
    void populate(controls::TreeNode *node, controls::Base *parent,
        const std::vector<const controls::Base *> &open);
    void syncChildren(InspectorNode *node);
    //! Deletes node's child nodes, keeping the ids they held for the next
    //! refreshCounters() (their slots are cleared in the next flush).
    void deleteChildNodes(controls::TreeNode *node);
    //! The controls a node of parent lists: its actual children, less the
    //! inspector itself.
    void childrenOf(const controls::Base *parent, std::vector<controls::Base *> &out) const;
    void collectOpen(controls::TreeNode *node, std::vector<const controls::Base *> &out) const;
    //! Whether control is still on the canvas, by walking down from it.
    bool onCanvas(const controls::Base *control) const;

    controls::Canvas *m_canvas=nullptr;
    controls::TreeControl *m_tree=nullptr;
    controls::Base *m_panel=nullptr;
    std::vector<controls::Label *> m_counters;
    std::vector<controls::Label *> m_details;
    controls::Base *m_selected=nullptr;
    InspectorHighlight *m_highlight=nullptr;
    controls::Base *m_highlightTarget=nullptr;
    unsigned m_treeSerial=0;
    bool m_treeBuilt=false;
    size_t m_textSets=0;
    //! ids of its controls deleted since the last refreshCounters()
    std::vector<size_t> m_ownPrimitives;
    std::vector<size_t> m_ownTexts;
};

}//namespace tools
}//namespace gweni

#endif//_gweni_tools_inspector_h_
