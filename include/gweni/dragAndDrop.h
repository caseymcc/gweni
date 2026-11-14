/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_draganddrop_h_
#define _gweni_draganddrop_h_

#include <sstream>

#include <gweni/skin.h>
#include <gweni/uiTypes.h>

namespace gweni
{
namespace draganddrop
{
//! The live drag's package, null when nothing is dragged.
extern GWENI_EXPORT Package *CurrentPackage;
//! The control the live drag came from: the pressed control, or for an item
//! drag (Package::sourceList set) that list, so a pooled row given another item
//! while the drag lasts does not take the drag with it.
extern GWENI_EXPORT gweni::controls::Base *SourceControl;
//! The control that accepts the live drag's package under the cursor, null over
//! none (the cursor is then cursor::No).
extern GWENI_EXPORT gweni::controls::Base *HoveredControl;

bool GWENI_EXPORT start(gweni::controls::Base *control, Package *package);

//! Whether a drag is live.
GWENI_EXPORT bool isDragging();

//! The canvas the live drag is on, null when nothing is dragged.
GWENI_EXPORT gweni::controls::Canvas *dragCanvas();

//! Ends the live drag with no drop: the target hears its hoverLeave and the
//! source its dragAndDrop_endDragging(false, ...); no handleDrop is called. A
//! press that has not started a drag yet is forgotten too. Returns whether a
//! drag was live.
GWENI_EXPORT bool cancel();

//! The cursor the drag core last asked the platform for while a drag was live
//! (cursor::No over nothing that accepts the package, cursor::Normal over a
//! target), cursor::Normal when nothing is dragged. Natively platform::setCursor
//! does nothing, so a test reads this.
GWENI_EXPORT int currentCursor();

//! A press (\p down) on \p hoveredControl is remembered as a drag's possible
//! start; a release ends the live drag, dropping it on the target under the
//! cursor or, over none, cancelling it. Returns true when the release ended a
//! drag (input does not deliver it then).
bool GWENI_EXPORT nnMouseButton(gweni::controls::Base *hoveredControl, int x, int y,
    bool down);

//! A mouse move on \p canvas: starts the drag of a pressed control moved far
//! enough, and while a drag is live finds the target under the cursor itself
//! (one hit test of the canvas) and tells it of the move. The input handler
//! leaves the hover where it is while a drag is live, so the controls the
//! cursor crosses take no hover look; it is hit-tested again when the drag ends.
void GWENI_EXPORT onMouseMoved(gweni::controls::Canvas *canvas, int x, int y);

//! The update phase of \p canvas: an item drag whose source list is no longer
//! visible is cancelled. Returns whether it cancelled one.
GWENI_EXPORT bool onCanvasThink(gweni::controls::Canvas *canvas);

//! \p control is being deleted. The live drag's source (or its source list)
//! deleted cancels the drag, its canvas deleted ends it with nothing called.
void GWENI_EXPORT controlDeleted(gweni::controls::Base *control);
}

}
#endif//_gweni_draganddrop_h_
