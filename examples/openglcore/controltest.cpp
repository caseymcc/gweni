/*
 *  Gweni - single-control test harness
 *
 *  Drops ONE control (via its demo builder) directly on the canvas, so each
 *  control can be verified in isolation without the demo's dock/page layout.
 *
 *  Usage: ControlTest [options]
 *    --control <Name>   control demo to show (default ButtonDemo)
 *    --size WxH         canvas size in logical units (default 640x480); the
 *                       framebuffer is that times --scale
 *    --scale <s>        render at s device pixels per logical unit (default
 *                       1, or the GWENI_SCALE environment variable; the flag
 *                       beats it). The framebuffer and the PNGs are
 *                       round(size*s) device pixels; --move/--click and the
 *                       other scripted coordinates, the scenarios' fixed
 *                       sizes and --dump-bounds stay in logical units, so a
 *                       listing is the same at any scale. The interactive
 *                       window draws at s times the OS content scale, and
 *                       --size is its canvas in logical units.
 *    --frames N         frames to render before capturing (default 3); with
 *                       --shot, 0 renders none and the shot fails
 *    --shot <path>      render offscreen and write one PNG, then exit
 *    --shot-all         write one PNG per control demo, in this one run
 *    --shot-dir <dir>   directory for shots, created if missing (default /tmp)
 *    --headless         force the surfaceless EGL context (no display needed)
 *    --stats-overlay [on|off]  open the stats overlay (gweni::tools::StatsOverlay)
 *                       on the canvas at start-up, in the window or a capture
 *                       (on if neither word follows); a later
 *                       --stats-overlay off overrides an earlier one. A
 *                       capture shows it only when asked: its times are a
 *                       fixed clock's (capture::installFixedProfileClock),
 *                       so a shot of it is the same in every run
 *    --renderer gl|vulkan  the backend (default gl, or GWENI_RENDERER; the
 *                       flag beats it): OpenGLCore, or renderer::Vulkan on a
 *                       Vulkan device with no GL or EGL context. A Vulkan
 *                       capture is headless (with or without --headless, the
 *                       same), the window has a FIFO swapchain;
 *                       GWENI_VULKAN_VALIDATION=1 turns the validation layer
 *                       on, any message aborting
 *    --window-test <dir>  the window (--renderer vulkan), scripted: it presents
 *                       at --size, resizes itself to 5/4 and 4/5 of it and back,
 *                       iconifies and restores (a window manager must run),
 *                       writes each image it presented, read back from the
 *                       swapchain, to <dir>/window_<i>_<W>x<H>.png and
 *                       window_restored_<W>x<H>.png, prints the swapchain's
 *                       counts and fails if a resize made no new swapchain or
 *                       anything was drawn or presented while iconified. No
 *                       input is taken, the clock steps as in a capture;
 *                       window_vulkan (tests/windowVulkan.cmake) runs it
 *    --stats            print one key=value line per frame; freezes the clock.
 *                       prim_slots_written= and text_slots_written= count
 *                       the distinct primitive and glyph slots the frame's
 *                       flush uploads (vertexes or clip box): every draw,
 *                       hide, show or release that changed one since the
 *                       last flush, each slot once;
 *                       buffer_regrows= the vertex buffers and clip-box
 *                       textures the frame re-allocated and sent whole,
 *                       because the mirror outgrew them (geometrically, so
 *                       rarely);
 *                       prim_ids= counts the live primitive ids (handed out
 *                       and not released) after the frame; the id bank is
 *                       process-wide, so it covers every canvas;
 *                       visited= the controls the render phase rendered,
 *                       the queued ones alone (0 on a settled frame);
 *                       render_carried= the entries its drain left queued
 *                       for a later frame (controls off the canvas);
 *                       excluded_changed= the controls with a state change
 *                       in a setExcludedFromStats() subtree (the inspector),
 *                       which no other change count includes: a frame where
 *                       only they changed writes slots with controls_changed=0;
 *                       layout_visited= the controls the frame's layout
 *                       passes entered, one per calculateSize() and one per
 *                       arrange(), each axis counted (0 with no pass);
 *                       layout_children_scanned= the children those
 *                       passes' generic loops (Base::sizeOfChildren,
 *                       arrangeHorizontal/Vertical, arrange, doArrange)
 *                       went over, entered or not, hidden ones included;
 *                       measure_calls= the renderer measureText(),
 *                       lineHeight() and measureCarets() calls the
 *                       controls made since the last frame line (the
 *                       first: since the control was built), so the
 *                       frame's total: the scripted input
 *                       before it (a typed key re-wraps there), the update
 *                       phase and any render-phase layout;
 *                       invalidations= the Base::invalidate() calls made
 *                       over the same span, each step of one's walk up to
 *                       the canvas a call (controls::invalidateCalls(), none
 *                       from a setExcludedFromStats() subtree);
 *                       z_visited= the controls updateCanvas()'s z renumber
 *                       entered (0 unless the tree order changed);
 *                       anims_run= the animations its update phase ran
 *                       (its controls' active ones only);
 *                       layout_requests= the requested sizes or positions
 *                       recorded while a layout pass ran, and requested_by=
 *                       the first control that recorded one (Type#id, or
 *                       none): layout code computes bounds, so it is 0 for
 *                       every library control (docs/layout.md)
 *    --stats-clock frozen|step
 *                       whether the clock moves under --stats: frozen (the
 *                       default) steps it only for a scripted press, so an
 *                       idle frame is genuinely idle; step moves it by the
 *                       capture's 1/60 s per frame, as without --stats, so
 *                       the counters of something that waits on time (a
 *                       tooltip's hover delay, an animation) can be read:
 *                         --control ButtonDemo --stats --stats-clock step \
 *                           --frames 3 --move 34,114 --frames 40
 *                       (ButtonDemo's "With Tooltip" button is at 0,107 69x15
 *                       and "Toggle Me", which has one too, at 0,73 58x15,
 *                       whatever the canvas size)
 *    --require-one-pass fail the run (exit non-zero) if any frame's layout took
 *                       more than one update-phase pass or the render phase
 *                       laid out again; each such frame is named on stderr
 *                       with its control and the control that re-dirtied
 *                       layout. Works with any run that renders frames
 *                       (--control, --scenario, --check-teardown).
 *    --verify-layout    after every frame's layout, run one more pass that
 *                       enters every control and fail the run (exit non-zero)
 *                       if it moves, resizes or re-measures anything; each
 *                       difference is printed on stderr as
 *                         gweni: layout verify: <Type>#<id> <X|Y> <field>: ...
 *                       (the library's GWENI_LAYOUT_VERIFY=1 does the same in
 *                       any app; see gweni::controls::layoutVerifyEnabled()).
 *    --check-teardown   build, render and destroy every control (or only
 *                       --control's), then check that the canvas renders
 *                       byte-identical to the empty canvas it started as.
 *                       Exits non-zero on a mismatch and writes
 *                       teardown-reference.png and teardown-after.png to
 *                       --shot-dir, so the ghost can be looked at.
 *    --trace            log each scripted input on stderr with what it hit:
 *                       the hovered control as Type[child index], its
 *                       canvas bounds and "disabled", then its ancestors
 *    --list             print the control demo names and exit
 *    --help, -h         print the option list and exit
 *    --scenario <name>  per-control | resize | docked | scroll | interaction |
 *                       typing | clip | text-size | scale | tooltip |
 *                       context-menu | popup | virtual-list |
 *                       data-table-virtual | hide-show | demo-labels |
 *                       plot-histogram | plot-lines | inspector |
 *                       stats-overlay (see below)
 *    --dump-bounds [<path>]
 *                       print the laid-out control tree as text: one line per
 *                       control, depth-first in child order, indented by depth,
 *                       with its type, bounds, preferred size (pref=, the
 *                       getPreferredSize() the last layout measured; for a
 *                       control with a layout, its top-level LayoutItem's
 *                       plus its padding), dock, size flags and hidden
 *                       state. Every child is listed, private ones included:
 *                       a ScrollControl shows its inner panel (its rows under
 *                       it) and both scrollbars. An inner panel's line ends
 *                       in " inner". No path writes to stdout; a
 *                       path writes that file. Nothing pointer-, id- or
 *                       time-derived is printed, so two runs of the same
 *                       command give byte-identical listings.
 *
 *  Scripted input (repeatable, applied in the order written, and interleaved
 *  with --frames / --shot so one process run can capture a sequence):
 *    --move X,Y         move the mouse
 *    --press X,Y        move there and hold the left button down
 *    --release X,Y      move there and let the left button up
 *    --click X,Y        a whole left click (press then release)
 *    --double-click X,Y a left double click
 *    --press-right X,Y, --release-right X,Y, --click-right X,Y
 *                       the same with the right button (a right press opens
 *                       a context menu, Base::setContextMenu). Every button
 *                       goes through gweni::input::GLFW3's mouse-button
 *                       callback, as a window's does, and a press (a click's
 *                       included) steps the clock by 1 s, as --press does.
 *    --scroll DX,DY     mouse wheel (DX is ignored; gweni only takes the Y axis)
 *    --key <name>       a key: return, kp-enter, tab, space, escape, up, down,
 *                       left, right, home, end, backspace, delete, shift,
 *                       control, alt, command, f10, f11 and f12 (which, as in
 *                       the window, open and close the stats overlay, the
 *                       theme editor, saving to and loading from
 *                       gweni-theme.json in the cwd, and the inspector, and
 *                       are not passed on),
 *                       or a single printable character. A named key is pressed and released as
 *                       synthesised GLFW key events and a character is typed
 *                       as a GLFW char event, both through the same
 *                       gweni::input::GLFW3 translator the examples' callbacks
 *                       use. A chord joins modifiers (ctrl, shift, alt,
 *                       command) and a key with '+': --key ctrl+a, --key
 *                       shift+left, --key ctrl+shift+end. The modifiers are
 *                       pressed in order, the key goes down and up with their
 *                       mods bits set, then the modifiers come up in reverse.
 *                       A chord's key is a named key, a letter or a digit, and
 *                       is sent as a key event only, as GLFW sends no char for
 *                       Ctrl+letter.
 *    --type <utf8>      type the text: it is decoded as UTF-8 and each
 *                       codepoint sent as a GLFW char event. Until UnicodeChar
 *                       is widened the translator drops all but printable
 *                       ASCII, so only that arrives. To type a space, --type " ".
 *    --expect-text <utf8>
 *                       the keyboard-focused control's text has to be exactly
 *                       this (a TextBox's getText()). A mismatch prints both on
 *                       stderr and the run exits non-zero at the end; the
 *                       script carries on, so every bad step is reported.
 *
 *  Any of those switches puts the run in script mode: --frames N then means
 *  "render N frames now" and --shot P means "capture the framebuffer now", both
 *  at the point they appear. A script that does not start with --frames is
 *  settled first (3 frames), so its first input hits the laid-out controls, and
 *  a --shot straight after input renders one frame first, so it shows that
 *  input. A shot before any frame was rendered (--frames 0 --shot) is an error.
 *  Hovering and pressing ButtonDemo's "Event Tester" (its top row, x 0..69):
 *
 *    --control ButtonDemo --move 30,5 --shot hover.png \
 *                         --press 30,5 --shot press.png
 *
 *  Use --press (not --click) to photograph a held-down button: a --click is down
 *  and up again, so by the next frame the control is only hovered.
 *
 *  Typing into TextBoxDemo's first box (at 640x480 its centre is 100,10) and
 *  checking the result:
 *
 *    --control TextBoxDemo --frames 3 --click 100,10 --frames 2 \
 *        --type hello --expect-text hello --key backspace --expect-text hell \
 *        --key ctrl+a --key ctrl+c --key end --key ctrl+v --expect-text hellhell
 *
 *  Scenarios:
 *    per-control  the single-control shots (same as --shot-all)
 *    resize       one control (default Demo) at 640x480, 1024x768 and
 *                 640x480 again, then once more on a new canvas built directly
 *                 at 640x480 (resize-00-fresh-640x480.png). The three 640x480
 *                 states are compared cheapest first - the listings, then the
 *                 renderer mirror id by id (shot 1 against shot 3), then the
 *                 pixels - and one line reports them:
 *                   resize control=X listing_settled=yes|no mirror_settled=yes|no
 *                          shrink_settled=yes|no fresh_matches=yes|no
 *                 listing_settled and shrink_settled compare shot 3 with shot 1
 *                 (listing, pixels), fresh_matches the fresh shot with both
 *                 (listing and pixels). Each mismatch is described on stderr:
 *                 the first differing listing line with its control path, the
 *                 differing primitive/text ids with their owning control, the
 *                 differing pixel count. Exits non-zero if any check is no.
 *                 The clock is frozen as under --stats, so an animation (an
 *                 indeterminate ProgressBar) is at the same place in every shot.
 *                 Every mirror comparison (here, scale and hide-show) takes
 *                 each z by its rank among the z values drawn: the canvas's
 *                 labels depend on the edits that led to a state (a Text line
 *                 made again takes a z in a gap), its depth order does not.
 *    docked       the whole Demo (dock layout + status bar) at 1024x768
 *    demo-labels  the whole Demo at --size (the README hero's 1024x768 in its
 *                 CTest runs), shot as demo-labels.png. Each demo-list
 *                 entry's label Text has to fit its room
 *                 (Demo::listEntryFits: from the Text's left edge to the
 *                 button's inner right edge, cut by each clipping control up
 *                 to the list and by the list's vertical scrollbar while it is
 *                 shown). One line
 *                   demo-labels entries=N clipped=N
 *                 and one stderr line per clipped entry, by its key:
 *                   ControlTest: demo list label clipped: <key> ("<label>"
 *                   <width> wide, <room> room)
 *                 Exits non-zero if any entry is clipped.
 *    scroll       the five overflowing demos, idle and after a scripted scroll
 *    interaction  hover, press, disabled and focus have to repaint, checked on
 *                 the pixels inside the control's own bounds, and one line
 *                 reports each check as yes/no:
 *                   ButtonDemo: idle, hover and press of the first Button
 *                   differ pairwise, moving off and setDisabled(false) give
 *                   the idle look back, setDisabled(true) changes it; the
 *                   hover frame writes 1..4 primitive slots and changes at
 *                   most 4 controls; a settled frame with no input changes
 *                   nothing. ButtonDemo's toggle: a click turns it on and
 *                   changes no pixel outside it, its release marking nothing
 *                   outside it and writing its one slot; moved off, it still
 *                   looks unlike off, and a second click gives the frame
 *                   before the first back. TextBoxDemo: clicking TextBox 1
 *                   changes it, and once the mouse has moved on and clicked
 *                   TextBox 2, TextBox 1 looks as it did before any click.
 *                   TreeControlDemo: collapsing a node repaints and moves the
 *                   node below it up. SelectableDemo: hovering a free row
 *                   changes it and moving off gives its idle look back, a
 *                   click selects it and a second one deselects it back to
 *                   that look; with group row A selected and the mouse on row
 *                   B, the click on B changes pixels inside A and B only, and
 *                   that frame changes only A, B and their labels
 *                   (background=2), with at most 2 primitive slots and their
 *                   glyphs written (a line reports the counts).
 *                   CheckBoxDemo: hovering the first CheckBox changes it and
 *                   moving off gives it back, a click checks it and a second
 *                   one gives the unchecked look back; hovering the
 *                   CheckBoxWithLabel's label lights its box, and two clicks
 *                   on the label check and uncheck it; the disabled third box
 *                   looks unlike the first, and hovering or clicking it
 *                   changes nothing; each hover frame writes 1..4 primitive
 *                   slots and changes at most 4 controls. WindowDemo:
 *                   the close button of the window "Normal Window" opens
 *                   draws something at rest, hover and press change its
 *                   pixels and moving off gives the rest look back, the hover
 *                   frame writes 1..2 primitive slots, a press let go off it
 *                   leaves the window open and the button at rest, and a
 *                   click closes the window with the live primitive ids back
 *                   where they were before it opened. SliderDemo: each frame
 *                   of a thumb drag, a unit at a time along the track, then
 *                   in larger steps along it and off it, changes the thumb
 *                   only and moves it by exactly the mouse's step, and the
 *                   release off it takes its hover away. SliderTextDemo:
 *                   dragging the first slider's thumb a unit at a time, a
 *                   frame that keeps the
 *                   "%.0f" value text writes no glyph slot and at most the
 *                   thumb's primitive slot, and one that changes it no more
 *                   glyph slots than the new string's (a line lists them).
 *                   TabCloseDemo: hovering an unselected closable tab shows
 *                   its close button, changing no pixel outside the tab and
 *                   writing at most the tab's and the button's slots, moving
 *                   off gives the frame back, and a click on the button
 *                   closes the tab, the selection kept and its ids given
 *                   back. TabOverflowDemo: a press on the first TabControl's
 *                   right scroll button scrolls its tabs, the frame leaving
 *                   nothing pending outside the strip and the buttons,
 *                   writing no more than their primitive slots and the tabs'
 *                   glyphs and changing no pixel outside the TabControl; the
 *                   left one gives the first look back; settled frames write
 *                   nothing, with the tab list closed and open; the list
 *                   button opens one row per tab, and choosing the tenth
 *                   selects it, closes the list and shows the whole tab.
 *                   TabControlDemo, its last TabControl: hovering an
 *                   unselected tab writes its one primitive slot and its own
 *                   glyphs alone, changing no pixel outside it, and moving off
 *                   gives the frame back; a click on it selects it, every
 *                   frame until settled writing no slot outside the two tabs
 *                   and their pages, never the page frame's, and changing no
 *                   pixel outside the TabControl; selecting the first again
 *                   gives the frame back.
 *                   PropertiesDemo: the top Properties is its rows'
 *                   height and the PropertyTree runs from one layout
 *                   spacing below it to the demo's bottom; hovering the
 *                   first PropertyRow's value changes the row's pixels
 *                   (column fill, lines, label colour) and none outside it,
 *                   every change of that frame in the row but the left
 *                   control's; moving onto its label marks nothing on the
 *                   row; moving off gives its rest look back; a click on the
 *                   value shows the Editing look, unlike the hover's, and
 *                   dropping the focus gives the rest look back.
 *                   ColorEditDemo: typing into the RGBA
 *                   ColorEdit's R field (Backspace, then a digit) leaves
 *                   nothing pending outside the swatch and that field and
 *                   writes no more than their slots and the field's glyphs,
 *                   the swatch taking the value; a settled frame writes
 *                   nothing with the picker closed and open, a click on the
 *                   swatch opening it below and Escape closing it (a line
 *                   lists the edit frames). DataTableDemo: hovering an
 *                   unselected row leaves nothing pending outside it, writes
 *                   no more than its primitive slots and its cells' glyphs and
 *                   changes no pixel outside it, moving off gives the frame
 *                   back, and a click on the Number header sorts the rows by
 *                   number in one layout pass; settled frames write nothing
 *                   (a line gives the hover's and the sort's counts).
 *                   DragNumberDemo: every frame of a drag of the integer
 *                   field (horizontal, vertical and Alt tenth-unit steps)
 *                   leaves nothing pending outside the field, one that keeps
 *                   the shown value writes no slot and one that changes it at
 *                   most the field's slot and the new string's glyphs; the
 *                   release and a settled frame write nothing; a double-click,
 *                   "7" and Return set it to 7 (a line lists the drag
 *                   frames). ShowcaseInputDemo: its first DragNumber
 *                   (docked Top, Elastic) double-clicked, "42" typed and
 *                   Escape, then Return and Return: the box covers the field,
 *                   no other control moves, and each open and close frame
 *                   lays out in one pass entering only the field's subtree
 *                   and its ancestors, changing no pixel outside the field
 *                   and the control that had the focus (a line per frame).
 *                   VectorFieldDemo: every frame of a drag of the
 *                   tagged float3's Y component leaves nothing pending and no
 *                   pixel changed outside that field, writes at most its slot
 *                   and the new string's glyphs (none when the string stays)
 *                   and fires onChanged once, for component 1, per change;
 *                   a settled frame writes nothing and Tab moves the focus to
 *                   the Z field (a line lists the drag frames). Exits
 *                   non-zero if any check is no. The clock is frozen as
 *                   under --stats.
 *    typing       the keyboard on TextBoxDemo, every key a synthesised GLFW
 *                 event through gweni::input::GLFW3: click into the first,
 *                 empty, box, type "a" and Backspace, each of which has to
 *                 change nothing outside that box and rewrite only its text
 *                 run (the lines
 *                   typing first-char|last-char controls_changed=N geometry=N
 *                          text=N prim_slots_written=N text_slots_written=N
 *                 give their counters); then click it again,
 *                 Ctrl+A and type "hello" (shot typing-hello.png), then
 *                 Backspace, Left, Delete, Home, End, Right, Shift+Left,
 *                 Ctrl+A/C/X/V, a Ctrl+letter with no shortcut, Escape, Ctrl
 *                 held while the window loses focus, Tab to the second
 *                 box; digits and letters into the TextBoxNumeric; Return and keypad Enter in the
 *                 TextBoxMultiline. Each step is followed by a frame and checks
 *                 the focused control's text; a failure is named on stderr with
 *                 the expected and actual text. Then one keystroke into a
 *                 settled box has to rewrite one control's text run and the
 *                 frame after it upload nothing; the line
 *                   typing keystroke controls_changed=N text=N
 *                          prim_slots_written=N text_slots_written=N upload_bytes=N
 *                 gives its counters, and
 *                   typing checks=N failed=N
 *                 the total. Exits non-zero if any check fails. The clock is
 *                 frozen, so key repeat (wall-time based) is not covered.
 *    clip         children cut at their clipping parent's edges, on the bare
 *                 canvas, each frame compared with the one with less in it:
 *                 a ScrollControl with no bars and content past its right and
 *                 bottom edges (a fill, a Button, a CheckBox, a Label) leaves
 *                 every pixel outside it as on the empty canvas, and once the
 *                 content is moved below it the frame is the empty canvas
 *                 again. A Window in a 400x300 parent, dragged by its title
 *                 bar past the parent's bottom-right and then top-left edges,
 *                 leaves everything outside the parent as on the empty
 *                 canvas; dragged right out of the parent the frame is the
 *                 parent alone, and dragged back it looks as it started. A
 *                 drag step changes controls in the Window's subtree only,
 *                 and a settled frame changes and uploads nothing. A Window
 *                 on the canvas itself, dragged past the canvas's own top-left
 *                 corner (to -28,-8), leaves everything outside it as on the
 *                 empty canvas, and its on-canvas part is the same Window
 *                 drawn whole at (60,56), shifted. One line
 *                   clip scroll_cut=yes|no ... canvas_back=yes|no
 *                 reports the checks. Exits non-zero if any check is no.
 *    text-size    a Label in OpenSans 24 px beside an 11 px one and a 48 px one,
 *                 then its font changed to 36 px at runtime through
 *                 Label::setFont(face, size, bold), then the Label deleted
 *                 (text-size-24.png, text-size-36.png, text-size-deleted.png).
 *                 All three sizes are drawn from the face's one SDF atlas, so the
 *                 change has to upload nothing to a font atlas and write no more
 *                 glyph slots in a frame than the Label's run holds; the Label
 *                 has to grow and repaint, and a frame after it write nothing.
 *                 The 48 px Label has to draw the same pixels after the change
 *                 and after the deletion, both of which free the Label's font,
 *                 and the atlas has to keep its GL name:
 *                   text-size from=24 to=36 sdf=yes atlas_upload_calls=N
 *                          text_slots_written=N run_glyphs=N ... same_atlas=yes
 *                 Exits non-zero if any of it fails, or if GWENI_SDF_TEXT_MIN puts
 *                 either size below the SDF threshold.
 *    plot-histogram  PlotHistogramDemo's 32-bar series, the clock frozen: a
 *                 sample given a new height (bar 4, to the middle of the
 *                 range) has to write exactly that bar's primitive slot and
 *                 change that control alone, a hair more (the same height)
 *                 nothing, and the highest value doubled (a rescale of the
 *                 automatic range) more than one bar's slot and no more than
 *                 every bar's; the mouse onto bar 10 one slot (its hover look),
 *                 a move inside it nothing, onto bar 11 the two bars' slots,
 *                 with "11: <value>" as the tooltip; every settled frame
 *                 nothing; the demo gives every primitive id back
 *                 (plot-histogram-hover.png). One line
 *                   plot-histogram bars=32 sample_prim_slots=1 ... ids_back=yes
 *                 Exits non-zero if any check is no.
 *    plot-lines   PlotLinesDemo's two series, the clock frozen: a sample of
 *                 the 64-sample sine moved (sample 20, to 0) has to write at
 *                 least one and at most 4 primitive slots (segments 18..21,
 *                 the two it ends and the two drawn with it as their
 *                 neighbour) and change that control alone; a frame time
 *                 past the fixed range raised further (still on its top edge)
 *                 nothing; the sine's highest value doubled (a rescale of the
 *                 automatic range) more than 4 slots and no more than every
 *                 segment's; a push() onto the frame times no more than every
 *                 segment's, with no layout pass; the mouse near sample 10
 *                 no slot, with "10: <value>" as the tooltip; every settled
 *                 frame nothing; the demo gives every primitive id back
 *                 (plot-lines.png). One line
 *                   plot-lines segments=63 sample_prim_slots=4 ... ids_back=yes
 *                 Exits non-zero if any check is no.
 *    inspector    Demo, the clock frozen, settled; F12 (scripted, through the
 *                 same key handler as the window's) opens the inspector
 *                 (gweni::tools::Inspector) over it. It has to settle within 6
 *                 frames to one that writes no primitive or glyph slot at all,
 *                 its own included, while showing 0 slots written, 0 controls
 *                 rendered and 0 layout passes, and the frame after it must
 *                 write nothing either. The mouse onto a demo-list button: that
 *                 frame writes some slots, the next frame's inspector shows
 *                 exactly that frame's primitive and glyph slots, and within 4
 *                 frames it shows 0 again and a frame writes nothing. F12 again
 *                 closes it, giving back every primitive id it took
 *                 (inspector.png, with it open). One line
 *                   inspector settle_frames=2 settled_zero=yes ... ids_back=yes
 *                 Exits non-zero if any check is no.
 *    theme        Demo, the clock frozen: the textured skin's theme changed
 *                 (surface colours, a radius, a border width, text colours and
 *                 the scroll bar thickness) and applied live
 *                 (tools::ThemeEditor::applyToCanvas); the frame after the one
 *                 drawing it writes nothing, and the pixels are those of a Demo
 *                 built under the changed theme on a new canvas. Then the
 *                 theme is saved (TexturedSkin::saveThemeFile, to
 *                 theme-saved.json in the shot dir),
 *                 the defaults given back live (the first Demo's pixels again)
 *                 and the file loaded (loadThemeFile, the changed values and
 *                 pixels again). Shots theme-00-default.png ...
 *                 theme-04-loaded.png. One line
 *                   theme edit_prim_slots=N ... load_matches=yes
 *                 Exits non-zero if any check is no.
 *    stats-overlay  Demo, the clock stepping, with a toggle Button of its own
 *                 on the canvas under where the overlay goes; F10 (scripted,
 *                 through the window's key handler) opens the stats overlay
 *                 (gweni::tools::StatsOverlay) over it. Once it has refreshed
 *                 twice, over the next two refresh windows: every frame between
 *                 two refreshes writes no slot, a refresh frame writes only
 *                 slots under the overlay's own ids, and each refresh shows
 *                 what this harness counted for the frames of its window
 *                 (previousFrameStats() and the renderer's frameStats(), the
 *                 overlay's own slots taken out: the times, the most
 *                 rendered, changed, layout passes and entries, z visited,
 *                 animations, app slots, upload bytes and draws). The mouse onto
 *                 a demo-list entry: the next refresh shows that frame's
 *                 rendered controls and slots, the one after it 0 again. The
 *                 mouse onto its "interval" name (from its value, which the
 *                 mouse passes through): after the canvas's tooltip delay its
 *                 tooltip shows as the canvas's last child, above the
 *                 overlay, inside the canvas and clear of the name, the frame
 *                 writing only the overlay's and the box's slots; the next
 *                 refresh shows 0 rendered, changed, slots, passes and z, the
 *                 frames until the one after write nothing, and back on the
 *                 value the frame before is given back (the plots left out).
 *                 The mouse onto the Button through the overlay (under a
 *                 value or a plot, no name) hovers it, and a click toggles it.
 *                 F10 again closes it, giving back every primitive and text
 *                 id it took, the canvas's ToolTip box's left out
 *                 (stats-overlay.png, with it open). Then the overlay and the
 *                 Inspector (F12) open together, the overlay first: after
 *                 every frame the harness counts the app's slots of it (those
 *                 under no id a tool held after the frame before or after
 *                 it), and the Inspector must show exactly the frame before's
 *                 and each overlay refresh the most of its window's, frames
 *                 only a tool wrote in included (the Inspector opening, its
 *                 node for Demo opened, the mouse over its nodes, the overlay
 *                 closed and opened again under it, the Inspector closed under
 *                 the overlay); the overlay's ids row is the same with the
 *                 Inspector open as without; a hover on a demo-list entry
 *                 shows in both, then 0; and closing both gives back every id
 *                 they took. One line
 *                   stats-overlay quiet_frames=N quiet_written=0 ... ids_back=yes
 *                 Exits non-zero if any check is no. controltest_stats_overlay
 *                 (tests/statsOverlay.cmake) also holds the --stats lines of
 *                 Demo with --stats-overlay to the ones without it.
 *    scale        one control (default Demo) on a 640x480 canvas at scale 1,
 *                 then Canvas::setScale(2) with the framebuffer resized to
 *                 1280x960, then back to 1 (scale-01-1x.png, scale-02-2x.png,
 *                 scale-03-1x.png), then a new canvas built directly at scale
 *                 2 (scale-00-fresh-2x.png). It sets both scales itself, so
 *                 --scale is ignored. One line reports the checks:
 *                   scale control=X listing_settled=yes|no mirror_settled=yes|no
 *                          pixels_settled=yes|no fresh_matches=yes|no
 *                 listing_settled, mirror_settled and pixels_settled compare
 *                 shot 3 with shot 1 (listing, renderer mirror id by id,
 *                 pixels), fresh_matches the fresh shot with shot 2 (listing
 *                 and pixels). Mismatches are described as for resize. Exits
 *                 non-zero if any check is no. The clock is frozen, as for
 *                 resize.
 *    tooltip      the canvas's tooltip on ButtonDemo, on the stepped clock
 *                 (moving even under --stats): resting on "With Tooltip" shows
 *                 nothing outside the button until the canvas's delay has
 *                 passed, then a ToolTip box below the button, inside the
 *                 canvas and above every other control; a settled frame and a
 *                 move inside the button write nothing; moving straight to
 *                 "Toggle Me" moves the same box there at once and changes
 *                 only the box and the two buttons; moving off hides it and
 *                 the frame is the one before hovering; a press hides it for
 *                 good until the mouse has left, a --click (the move and the
 *                 press in one frame) included; a button in the canvas's
 *                 bottom-right corner gets its tooltip above it, inside the
 *                 canvas; and with a Menu open over where the box goes, the
 *                 box's pixels are the ones it has with no Menu. One line
 *                   tooltip waits=yes|no shown=yes|no ... over_menu=yes|no
 *                 reports the checks. Exits non-zero if any check is no.
 *    context-menu Base::setContextMenu on ContextMenuDemo, right-clicks
 *                 through the GLFW translator, on the stepped clock: a
 *                 right-click in the Document box opens its menu with its
 *                 top-left at the cursor, and that frame changes nothing
 *                 visible outside the menu and the box; a settled frame with
 *                 it open writes nothing; a left click outside and Escape
 *                 each close it back to the frame before opening; in the
 *                 Corner box near the bottom-right corner the menu opens
 *                 flipped to the cursor's left and above it, inside the
 *                 canvas; a right-click in the other box closes one menu and
 *                 opens the other; a disabled item does nothing and an
 *                 enabled one is chosen (the demo's label names it) and
 *                 closes it; the Help button's tooltip draws over an open
 *                 menu; hiding or deleting the box closes its menu (deleting
 *                 it deletes the menu it created). One line
 *                   context-menu opens_at_cursor=yes|no ... deleted_owner_closes=yes|no
 *                 reports the checks. Exits non-zero if any check is no.
 *    popup        controls::Popup on PopupDemo, clicks through the GLFW
 *                 translator, on the stepped clock: a click on Options opens
 *                 its popup below it, and that frame changes no pixel and
 *                 marks no visible control outside the popup and the button;
 *                 a settled frame with it open writes nothing; a click
 *                 outside closes it back to the idle frame; a second click on
 *                 Options closes it (the button toggles it); Escape closes it
 *                 back to the frame before it opened; the Corner button's
 *                 popup opens above it with their right edges aligned, inside
 *                 the canvas; opening Options closes it; the popup's ComboBox
 *                 opens its list over the popup, Escape closes the list
 *                 alone and choosing an item keeps the popup open; the Help
 *                 button's tooltip draws over the open popup; hiding Options
 *                 closes it. One line
 *                   popup opens_below=yes|no ... hidden_anchor_closes=yes|no
 *                 reports the checks. Exits non-zero if any check is no.
 *    virtual-list VirtualListDemo with its list at 1 000 and then 100 000
 *                 items, the clock frozen: a settled frame, a one-row scroll
 *                 from the top (scrollByRows) and a wheel notch over the list
 *                 (three rows), each followed by settled frames, then the last
 *                 item scrolled into view (virtual-list-1000-end.png,
 *                 virtual-list-100000-end.png). One line per count
 *                   virtual-list items=N rows=N subtree=N prim_ids=N
 *                          idle_slots=0 ... row_prim_slots=N row_text_slots=N
 *                          row_visited=N row_z_visited=0 ... ids_left=0
 *                 gives the counters, and one line
 *                   virtual-list same_cost=yes|no settled=yes|no
 *                          rows_only=yes|no ids_back=yes|no
 *                 the checks: the pool, the subtree, the ids and the one-row
 *                 scroll's counters are the same at both counts (the wheel's
 *                 slots too, less the thumb's: in the list, which fills the
 *                 canvas's height (a 436-unit viewport at 640x480), its three
 *                 rows move the bar's 8 px thumb one pixel at 1 000 items
 *                 and the 6 px one not at all at 100 000, thumb_moved=1
 *                 and 0);
 *                 settled frames write and change nothing;
 *                 each scroll writes no more than its shown rows' primitive
 *                 slots and glyphs, enters no more than the list's subtree
 *                 and the two controls above it, renumbers no z and lays out
 *                 in one pass with no request; the demo gives every primitive
 *                 id back. Exits non-zero if any check is no.
 *    data-table-virtual
 *                 DataTableVirtualDemo with its source-mode DataTable at
 *                 1 000 and then 100 000 records, the clock frozen: a settled
 *                 frame, a one-row scroll from the top and a wheel notch over
 *                 the body, a click on the Score header (the demo sorts its
 *                 records and rebinds) and the Name column's grip dragged 30
 *                 units, each followed by settled frames
 *                 (data-table-virtual-1000.png, data-table-virtual-100000.png
 *                 after the drag). One line per count
 *                   data-table-virtual items=N rows=N subtree=N prim_ids=N
 *                          idle_slots=0 ... row_prim_slots=N ... sort_passes=1
 *                          ... drag_passes=1 ... ids_left=0
 *                 gives the counters, and one line
 *                   data-table-virtual same_cost=yes|no settled=yes|no
 *                          rows_only=yes|no sort_one_pass=yes|no
 *                          drag_one_pass=yes|no ids_back=yes|no
 *                 the checks: the pool, the table's subtree, the ids and the
 *                 one-row scroll's counters are the same at both counts (the
 *                 wheel's slots too, less a thumb that moved at one count
 *                 only); settled frames write and change nothing; each
 *                 scroll writes no more than its shown rows' primitive slots
 *                 and glyphs, enters no more than the table's subtree and the
 *                 two controls above it, renumbers no z and lays out in one
 *                 pass; the click sorts the rows on screen by score and the
 *                 drag gives the column its width, each in one pass with no
 *                 z renumber or layout request; the demo gives every
 *                 primitive id back. Exits non-zero if any check is no.
 *    hide-show    one control (default ButtonDemo) built into a host page, a
 *                 plain Base docked to fill the canvas as PageControl::addPage
 *                 makes the shell's pages, the clock frozen. On a new canvas
 *                 the empty host (hide-show-00-empty.png), the
 *                 demo built shown, the reference (hide-show-01-shown.png),
 *                 the host hidden (hide-show-02-hidden.png) and shown again
 *                 (hide-show-03-shown-again.png); then, on another new
 *                 canvas, the demo built into a host hidden already, as every
 *                 shell page from index 1 on is
 *                 (hide-show-04-built-hidden.png), and the host shown
 *                 (hide-show-05-built-hidden-shown.png). A hidden control
 *                 loses the keyboard focus, so the control that held it in
 *                 the reference gets it back once shown, as the shell's
 *                 showcaseShown() gives it. One line
 *                   hide-show control=X hide_local=yes|no hide_marks=yes|no
 *                          ... round_trip=yes|no built_hidden=yes|no
 *                          hidden_empty=yes|no
 *                 reports the checks: each hide or show frame has nothing
 *                 pending outside the host (_local) and marks
 *                 StateChange_Visibility on exactly the controls whose
 *                 visible() changed, besides what that frame's layout hid or
 *                 showed itself (_marks); every settled frame, hidden or
 *                 shown, changes and writes nothing (_settles); the live
 *                 primitive and text ids are back after the round trip
 *                 (ids_kept); shot 3 is shot 1 (round_trip: listing, renderer
 *                 mirror id by id, pixels) and so is shot 5 (built_hidden:
 *                 listing, pixels and the mirror slot by owner, since a demo
 *                 built hidden may take its ids in another order, a hidden
 *                 slot counting as hidden whatever else it holds); shots 2
 *                 and 4 are shot 0 (hidden_empty). For Demo it then selects
 *                 PropertiesDemo and ComboBoxDemo in the shell, pages built
 *                 hidden (hide-show-shell-<page>.png): at least 1 and 3
 *                 ComboBoxes drawn, inside the canvas, none with its list
 *                 open (shell_<page>). Exits non-zero if any check is no.
 *
 *  Every scenario (and --shot-all) takes a listing per shot when
 *  --dump-bounds is on, named after that shot with a .txt extension, so the
 *  shot and its listing sit side by side in --shot-dir - resize yields four
 *  (the fresh one included) and per-control 28. The resize checks take their
 *  listings in memory whether or not --dump-bounds is on; it only writes them
 *  out. A scenario names its own listings, so --dump-bounds
 *  there takes no path.
 *
 *  All of them, in the build container, from the repo root (a non-zero exit
 *  from resize, interaction, typing, clip, text-size or scale is their
 *  assertion, so it is reported rather than aborting the loop):
 *
 *    for s in per-control resize docked scroll interaction typing clip text-size scale tooltip; do \
 *      ./build_linux_x64_debug/examples/openglcore/ControlTest \
 *          --headless --scenario $s --shot-dir /tmp/shots/$s; \
 *      echo "$s exit=$?"; \
 *    done
 *
 *  With no --shot/--shot-all/--stats/--scenario and no scripted input it opens
 *  the interactive window, as before, on the real platform clock.
 *
 *  A control type with no skin renderer fails the run, in every mode: when
 *  Base::setSkin() finds that a control's renderer lookup ended at the skin's
 *  Base entry (and the type does not opt out with drawsNothing()), it prints
 *    gweni: no skin renderer for <TypeName>
 *  on stderr, once per type, and before exiting ControlTest prints
 *    ControlTest: N control type(s) with no skin renderer: <TypeName> ...
 *  and exits non-zero (gweni::controls::typesWithoutSkinRenderer()). A control
 *  added, removed, reordered or deleted during a render walk is printed by the
 *  library as
 *    gweni: <Type>#<id> <operation> during the render walk (rendering <Type>#<id>)
 *  and fails the run the same way, after
 *    ControlTest: N tree change(s) during the render walk: [...] ...
 *  (gweni::controls::renderWalkMutations()).
 *
 *  A missing resource directory, skin config (DefaultSkin.json) or default font
 *  is reported on stderr and drawn degraded: every surface magenta, the text in
 *  the library's built-in OpenSans. The window keeps running. Any other mode
 *  writes what it drew and exits 2 ("resources incomplete") unless it failed
 *  (exit 1 wins); a clean run exits 0.
 *
 *  Captures install a stepped clock (gweni::platform::setTimeSource), so a shot
 *  depends on the frame number and not on how fast the process ran, and a fixed
 *  profile clock (gweni::capture::installFixedProfileClock), so the frame times
 *  the library records are the same in every run too. --stats
 *  freezes the per-frame step instead, so an idle frame is genuinely idle and
 *  the counters settle to zero; --stats-clock step keeps it moving.
 */

#include <gweni/platforms/renderers/glApi.h>
#include <gweni/platforms/renderers/slotRenderer.h>
#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/windowControl.h>
#include <gweni/controls/checkBox.h>
#include <gweni/controls/label.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/button.h>
#include <gweni/controls/selectable.h>
#include <gweni/controls/toolTip.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/slider.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/scrollBarButton.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/virtualList.h>
#include <gweni/controls/dataTable.h>
#include <gweni/controls/properties.h>
#include <gweni/tools/inspector.h>
#include <gweni/tools/statsOverlay.h>
#include <gweni/tools/themeEditor.h>
#include <GLFW/glfw3.h>
#include <gweni/utils/filesystem.h>
#include "capture.h"
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <demo/button.h>
#include <demo/demo.h>
#include <demo/checkBox.h>
#include <demo/collapsibleList.h>
#include <demo/colorPicker.h>
#include <demo/comboBox.h>
#include <demo/contextMenu.h>
#include <demo/popup.h>
#include <demo/crossSplitter.h>
#include <demo/dataTable.h>
#include <demo/dataTableVirtual.h>
#include <demo/groupBox.h>
#include <demo/imagePanel.h>
#include <demo/label.h>
#include <demo/labelMultiline.h>
#include <demo/layoutDemo.h>
#include <demo/listBox.h>
#include <demo/menuStrip.h>
#include <demo/numeric.h>
#include <demo/pageControl.h>
#include <demo/plotHistogram.h>
#include <demo/plotLines.h>
#include <demo/inspector.h>
#include <gweni/controls/plotHistogram.h>
#include <gweni/controls/plotLines.h>
#include <demo/progressBar.h>
#include <demo/properties.h>
#include <demo/radioButton.h>
#include <demo/scrollControl.h>
#include <demo/selectable.h>
#include <demo/separator.h>
#include <demo/showcaseButtons.h>
#include <demo/showcaseContainers.h>
#include <demo/showcaseInput.h>
#include <demo/showcaseDisplay.h>
#include <demo/showcaseWindows.h>
#include <demo/slider.h>
#include <demo/sliderText.h>
#include <demo/dragNumber.h>
#include <gweni/controls/dragNumber.h>
#include <demo/vectorField.h>
#include <gweni/controls/vectorField.h>
#include <demo/statsOverlay.h>
#include <demo/statusBar.h>
#include <demo/tabClose.h>
#include <demo/colorEdit.h>
#include <gweni/controls/colorButton.h>
#include <gweni/controls/colorEdit.h>
#include <demo/tabControl.h>
#include <demo/tabOverflow.h>
#include <demo/treeControl.h>
#include <demo/textBox.h>
#include <demo/textDemo.h>
#include <demo/textSizes.h>
#include <demo/themeEditor.h>
#include <demo/unicodeText.h>
#include <demo/virtualList.h>
#include <demo/window.h>
#include <demo/windowButtons.h>

// A capture's clock: one fixed step per rendered frame, so every run sees the
// same times. 1/60 s is the step, i.e. a nominal 60fps.
static const float g_frameStep=1.0f/60.0f;

// Every scripted press also steps the clock, by more than inputHandler's 0.5s
// double-click window (source/inputHandler.cpp), so that two --clicks in a row
// are two clicks however fast the process runs. --double-click is the only way
// to ask for a double click: it issues its second press with no step in
// between, i.e. 0s apart, which is inside any window.
static const float g_pressStep=1.0f;

static float g_clockSeconds=0.0f;

static float steppedClock()
{
    return g_clockSeconds;
}

static gweni::input::GLFW3 g_glfwInput;
static gweni::renderer::Base *g_renderer=nullptr;
static gweni::controls::Canvas *g_canvas=nullptr;
static float g_manualScale=1.0f;        // --scale or GWENI_SCALE

// --stats-overlay: every canvas createCanvas() makes opens the stats overlay.
static bool g_statsOverlay=false;

// The app's own keys, before the canvas sees them: F12 opens and closes the
// inspector, F11 the theme editor, F10 the stats overlay (the library binds no
// key).
static bool appKey(int key, int action)
{
    if(((key != GLFW_KEY_F12) && (key != GLFW_KEY_F11) && (key != GLFW_KEY_F10)) || !g_canvas)
        return false;
    if(action != GLFW_PRESS)
        return true;
    if(key == GLFW_KEY_F12)
        gweni::tools::Inspector::toggle(g_canvas);
    else if(key == GLFW_KEY_F10)
        gweni::tools::StatsOverlay::toggle(g_canvas);
    else if(gweni::tools::ThemeEditor::toggle(g_canvas))
        gweni::tools::ThemeEditor::find(g_canvas)->setThemePath("gweni-theme.json");
    return true;
}

static void key_callback(GLFWwindow*, int key, int scancode, int action, int mods)
{
    if(appKey(key, action))
        return;
    const gweni::input::GLFW3::KeyEvent e={key,scancode,action,mods};
    g_glfwInput.processKeyEvent(e);
}
static void char_callback(GLFWwindow*, unsigned int codepoint)
{   g_glfwInput.processCharEvent(codepoint); }
static void window_focus_callback(GLFWwindow*, int focused)
{   g_glfwInput.processFocus(focused == GLFW_TRUE); }
static void cursor_pos_callback(GLFWwindow*, double x, double y)
{   g_glfwInput.processMouseMove(x,y); }
static void cursor_enter_callback(GLFWwindow*, int entered)
{   g_glfwInput.processCursorEnter(entered == GLFW_TRUE); }
static void mouse_button_callback(GLFWwindow*, int b, int a, int m)
{   g_glfwInput.processMouseButtons(b,a,m); }
static void scroll_callback(GLFWwindow*, double x, double y)
{   g_glfwInput.processScroll(x, y); }
// The framebuffer is in device pixels and the canvas in logical units, at the
// window's content scale times --scale; the cursor is mapped the same way.
static void fitCanvas(GLFWwindow *window)
{
    if(g_renderer && g_canvas)
        gweni::capture::fitCanvasToWindow(window, g_manualScale, *g_renderer, *g_canvas, g_glfwInput);
}
static void framebuffer_size_callback(GLFWwindow *window, int, int)
{   fitCanvas(window); }
static void content_scale_callback(GLFWwindow *window, float, float)
{   fitCanvas(window); }

#define GWENI_CONTROL_DEMOS(X) \
    X(ShowcaseContainersDemo) X(ShowcaseButtonsDemo) X(ShowcaseInputDemo) \
    X(ShowcaseDisplayDemo) X(ShowcaseWindowsDemo) \
    X(ButtonDemo) X(LabelDemo) X(LabelMultilineDemo) X(ProgressBarDemo) \
    X(GroupBoxDemo) X(ImagePanelDemo) X(StatusBarDemo) X(SeparatorDemo) X(PlotHistogramDemo) X(PlotLinesDemo) X(ComboBoxDemo) \
    X(TextBoxDemo) X(ListBoxDemo) X(SelectableDemo) X(VirtualListDemo) X(DataTableDemo) X(DataTableVirtualDemo) X(ContextMenuDemo) X(PopupDemo) X(CrossSplitterDemo) X(RadioButtonDemo) \
    X(CheckBoxDemo) X(NumericDemo) X(SliderDemo) X(SliderTextDemo) X(DragNumberDemo) X(VectorFieldDemo) X(MenuStripDemo) \
    X(WindowDemo) X(WindowButtonsDemo) X(TreeControlDemo) X(PropertiesDemo) X(TabControlDemo) X(TabCloseDemo) X(TabOverflowDemo) \
    X(ScrollControlDemo) X(PageControlDemo) X(CollapsibleListDemo) X(ColorPickerDemo) X(ColorEditDemo) \
    X(UnicodeTextDemo) X(TextSizesDemo) X(TextDemo) X(LayoutDemo) X(InspectorDemo) X(ThemeEditorDemo) \
    X(StatsOverlayDemo)

static const char *const g_controlNames[]=
{
#define GWENI_NAME(N) #N,
    GWENI_CONTROL_DEMOS(GWENI_NAME)
#undef GWENI_NAME
};

// The whole demo application, not one of its pages: the dock layout, the demo
// list, the output ListBox and the status bar. Kept out of GWENI_CONTROL_DEMOS
// so --shot-all and --scenario per-control stay the single-control shots.
static const char *const g_dockedControlName="Demo";

// Builds the demo as a child of `parent`: the canvas, or (hide-show) a page
// that may be hidden already, as PageControl::addPage builds the shell's.
static gweni::controls::Base *createControl(gweni::controls::Base *parent, const std::string &which)
{
    if(which == g_dockedControlName) return parent->newChild<gweni::Demo>();
#define TRY(N) if(which==#N) return parent->newChild<gweni::N>();
    GWENI_CONTROL_DEMOS(TRY)
#undef TRY
    return nullptr;
}

static bool isKnownControl(const std::string &which)
{
    if(which == g_dockedControlName) return true;
    for(const char *name : g_controlNames)
        if(which == name) return true;
    return false;
}

static void printControlNames(std::ostream &out)
{
    out<<"Available controls:\n";
    for(const char *name : g_controlNames)
        out<<"  "<<name<<"\n";
    out<<"  "<<g_dockedControlName<<" (the whole docked demo)\n";
}

// The scripted input switches are listed briefly: they only make sense
// together, and the file header documents them in full.
static void printUsage(std::ostream &out)
{
    out<<"Usage: ControlTest [options]\n"
         "  --control <Name>   control demo to show (default ButtonDemo)\n"
         "  --size WxH         canvas size in logical units (default 640x480)\n"
         "  --scale <s>        device pixels per logical unit (default 1, or\n"
         "                     GWENI_SCALE); the framebuffer and PNGs are\n"
         "                     round(size*s), coordinates and listings logical\n"
         "  --frames N         frames to render before capturing (default 3)\n"
         "  --shot <path>      render offscreen and write one PNG, then exit\n"
         "  --shot-all         write one PNG per control demo, in this one run\n"
         "  --shot-dir <dir>   directory for shots (default /tmp)\n"
         "  --headless         force the surfaceless EGL context\n"
         "  --stats-overlay [on|off]  open the stats overlay at start-up (on if\n"
         "                     neither follows; a later one overrides it)\n"
       <<gweni::capture::RendererHelp<<
         "  --window-test <dir>  the Vulkan window, scripted: resized, iconified and\n"
         "                     restored, each presented image written to <dir>\n"
         "  --stats            print one key=value line per frame; prim_slots_written=\n"
         "                     and text_slots_written= are the distinct slots its\n"
         "                     flush uploads (a draw, hide, show or release each)\n"
         "                     layout_visited= the controls the layout entered,\n"
         "                     layout_children_scanned= the children its loops\n"
         "                     went over, entered or not\n"
         "  --stats-clock frozen|step  under --stats, keep the clock still (the\n"
         "                     default) or step it 1/60 s per frame, so a\n"
         "                     tooltip's delay or an animation runs\n"
         "  --require-one-pass fail if a frame's layout takes more than one pass\n"
         "  --verify-layout    check every frame's layout against a full pass\n"
         "                     (also GWENI_LAYOUT_VERIFY=1)\n"
         "  --check-teardown   check the canvas renders empty again once every\n"
         "                     control has been destroyed; --control narrows it\n"
         "  --trace            log each scripted input on stderr\n"
         "  --scenario <name>  per-control | resize | docked | scroll | interaction\n"
         "                     | typing | clip | text-size | scale | tooltip\n"
         "                     | context-menu | popup | virtual-list\n"
         "                     | data-table-virtual | hide-show | demo-labels\n"
         "                     | plot-histogram | plot-lines | inspector\n"
         "                     | stats-overlay;\n"
         "                     resize prints listing_settled, mirror_settled,\n"
         "                     shrink_settled and fresh_matches, interaction one\n"
         "                     yes/no per repaint check, typing its keyboard\n"
         "                     checks, clip one yes/no per clipping check,\n"
         "                     text-size what a 24 -> 36 px\n"
         "                     Label font change wrote, scale whether a 1 -> 2 -> 1\n"
         "                     setScale round trip came back, tooltip one yes/no per\n"
         "                     tooltip check, context-menu one per context-menu\n"
         "                     check, popup one per popup check, virtual-list\n"
         "                     whether 1 000 and 100 000 items cost the same,\n"
         "                     data-table-virtual the same for a DataTable's\n"
         "                     records (and a sort and a column drag in one pass),\n"
         "                     hide-show whether a demo hidden and shown again, and\n"
         "                     one built hidden and shown, are the one built shown,\n"
         "                     demo-labels whether every demo-list label fits,\n"
         "                     plot-histogram whether a changed sample wrote its\n"
         "                     bar's slot alone, plot-lines whether it wrote the\n"
         "                     four segments around it, inspector whether the\n"
         "                     inspector over Demo settles to 0 and shows a hover's\n"
         "                     slots, stats-overlay whether the stats overlay\n"
         "                     writes only its own slots, on a refresh, and shows\n"
         "                     what the frames did; all seventeen fail on a no\n"
         "  --dump-bounds [<path>]  print the laid-out control tree as text,\n"
         "                     private children and hidden state included, an\n"
         "                     inner panel marked \" inner\", with each control's\n"
         "                     bounds and preferred size (pref=)\n"
         "  --resources <dir>  load the skin, fonts and textures from <dir> (relative\n"
         "                     to the working directory, or absolute) instead of\n"
         "                     the resources/ next to the executable\n"
         "  --list             print the control demo names and exit\n"
         "Scripted input, applied in order; --frames N and --shot then render and\n"
         "capture where they appear. A script not starting with --frames is settled\n"
         "first (3 frames), a --shot straight after input renders one frame first,\n"
         "and a shot before any frame was rendered is an error:\n"
         "  --move/--press/--release/--click/--double-click X,Y, --scroll DX,DY\n"
         "  --press-right/--release-right/--click-right X,Y  the right button\n"
         "  --key <name>       a named key (backspace, left, home, ...), a single\n"
         "                     character, or a chord such as ctrl+a or shift+left,\n"
         "                     sent as GLFW events through the GLFW translator\n"
         "  --type <utf8>      type the text, one GLFW char event per codepoint\n"
         "  --expect-text <utf8>  fail the run unless the focused control's\n"
         "                     text is exactly this\n"
         "Exit status (every mode but the window): 0 success, 1 failure, 2 run\n"
         "from incomplete resources (a missing resource dir, skin config or\n"
         "default font, each reported on stderr and drawn degraded: magenta\n"
         "surfaces, the built-in font).\n"
         "The header of controltest.cpp documents them in full.\n";
}

//-----------------------------------------------------------------------------

using gweni::capture::ensureDirectory;
using gweni::capture::writeRendererPng;
using gweni::capture::writeRgbaPng;

// The control that first re-dirtied layout in a frame, as Type#id, or "none".
static std::string redirtiedBy(const gweni::controls::LayoutStats &layout)
{
    if(!layout.redirtiedType)
        return "none";
    return std::string(layout.redirtiedType)+"#"+std::to_string(layout.redirtiedId);
}

// The control that first recorded a request during a pass in a frame, as
// Type#id, or "none" (LayoutStats::requests).
static std::string requestedBy(const gweni::controls::LayoutStats &layout)
{
    if(!layout.requestedType)
        return "none";
    return std::string(layout.requestedType)+"#"+std::to_string(layout.requestedId);
}

// measureTextCalls() and invalidateCalls() after the last frame line, or when
// the frames started: measure_calls= and invalidations= are every measure and
// invalidation since, the input before the frame included.
static size_t g_measuresAtFrame=0;
static size_t g_invalidationsAtFrame=0;

static void restartFrameCounts()
{
    g_measuresAtFrame=gweni::controls::measureTextCalls();
    g_invalidationsAtFrame=gweni::controls::invalidateCalls();
}

// One grep-able line per frame, frame index first. The renderer numbers are read
// after renderCanvas(), so they describe the frame just flushed.
static void printFrameStats(int frame, const std::string &control,
    const gweni::controls::ChangeStats &changes,
    const gweni::renderer::SlotRenderer::FrameStats &gpu,
    const gweni::controls::LayoutStats &layout,
    const gweni::controls::UpdateStats &update)
{
    const size_t measures=gweni::controls::measureTextCalls()-g_measuresAtFrame;
    const size_t invalidations=gweni::controls::invalidateCalls()-g_invalidationsAtFrame;

    restartFrameCounts();
    std::printf("frame=%d control=%s controls_changed=%zu created=%zu visibility=%zu"
        " geometry=%zu background=%zu text=%zu prim_slots_written=%zu text_slots_written=%zu"
        " upload_calls=%zu upload_bytes=%zu mirror_bytes=%zu draw_calls=%zu"
        " atlas_upload_calls=%zu atlas_upload_bytes=%zu buffer_regrows=%zu"
        " layout_passes=%d redirtied_by=%s render_layout=%d layout_requests=%d requested_by=%s"
        " layout_visited=%zu layout_children_scanned=%zu measure_calls=%zu invalidations=%zu"
        " visited=%zu render_carried=%zu z_visited=%zu anims_run=%zu prim_ids=%zu excluded_changed=%zu\n",
        frame, control.c_str(),
        changes.controls, changes.created, changes.visibility,
        changes.geometry, changes.background, changes.text,
        gpu.primitiveSlotsWritten, gpu.textSlotsWritten,
        gpu.uploadCalls, gpu.uploadBytes, gpu.mirrorBytes, gpu.drawCalls,
        gpu.atlasUploadCalls, gpu.atlasUploadBytes, gpu.bufferRegrows,
        layout.passes, redirtiedBy(layout).c_str(), layout.renderLayout ? 1 : 0,
        layout.requests, requestedBy(layout).c_str(),
        layout.visited, layout.childrenScanned, measures, invalidations,
        changes.visited, changes.renderCarried, update.zVisited, update.animsRun,
        gweni::primitiveIdCount()-gweni::releasedPrimitiveIdCount(), changes.excluded);
    std::fflush(stdout);
}

static bool parseSize(const std::string &value, gweni::Point &size)
{
    const size_t sep=value.find_first_of("xX");
    if(sep == std::string::npos) return false;

    const int w=std::atoi(value.substr(0, sep).c_str());
    const int h=std::atoi(value.substr(sep+1).c_str());
    if(w<=0 || h<=0) return false;

    size=gweni::Point(w, h);
    return true;
}

// A logical point or size in device pixels, snapped as the renderer snaps an
// edge.
static gweni::Point devicePixels(const gweni::Point &size, float scale)
{
    return gweni::Point(gweni::renderer::Base::snap(float(size.x), scale),
        gweni::renderer::Base::snap(float(size.y), scale));
}

static bool parsePoint(const std::string &value, int &x, int &y)
{
    const size_t sep=value.find(',');
    if(sep == std::string::npos) return false;

    x=std::atoi(value.substr(0, sep).c_str());
    y=std::atoi(value.substr(sep+1).c_str());
    return true;
}

// The canvas every capture draws on. The fill comes from the theme, so a
// "theme" block in resources/DefaultSkin.json moves it too.
static gweni::controls::Canvas *createCanvas(gweni::skin::Base *skin, const gweni::Point &size,
    float scale)
{
    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);
    canvas->setScale(scale);
    canvas->setSize(size.x, size.y);
    canvas->setDrawBackground(true);
    canvas->setBackgroundColor(gweni::skin::textured::tokyonight::Canvas.top);
    if(g_statsOverlay)
        gweni::tools::StatsOverlay::open(canvas);
    return canvas;
}

//-----------------------------------------------------------------------------
// Capture session: everything a scenario or a scripted run drives.
//-----------------------------------------------------------------------------

// --frames' default, and the frames a script that does not start with --frames
// settles before its first step.
static constexpr int kSettleFrames=3;

struct Session
{
    gweni::renderer::SlotRenderer *renderer=nullptr;
    gweni::controls::Canvas *canvas=nullptr;
    gweni::capture::Device *device=nullptr; // the offscreen target's
    gweni::Color background;            // what each frame clears it to
    gweni::Point size;                  // the canvas, logical units
    float scale=1.0f;                   // --scale
    gweni::Point pixels;                // the framebuffer, device pixels
    bool stats=false;
    bool statsClockSteps=false;         // --stats-clock step
    bool freezeClock=false;             // as under --stats, without the lines
    bool requireOnePass=false;          // --require-one-pass
    int multiPassFrames=0;              // frames that broke it
    int expectFailures=0;               // --expect-text mismatches
    bool trace=false;
    bool writePng=true;
    int frames=kSettleFrames;           // --frames: the scenarios' settle count
    std::string label;                  // the control --stats lines name
    int frame=0;
    gweni::Point mouse=gweni::Point(-1, -1);
    bool dumpBounds=false;              // --dump-bounds
    bool dumpPerShot=false;             // one listing per shot, named after it
    std::string dumpPath;               // --dump-bounds <path>; empty is stdout
};

//-----------------------------------------------------------------------------
// --dump-bounds: the laid-out tree as text
//-----------------------------------------------------------------------------

// Walks the children actually parented to each control, not getChildren(),
// which skips past an inner panel to the children placed in it: so a
// ScrollControl lists its inner panel (with the rows under it) and both of its
// scrollbars, hidden or not. Those private parts are where a layout that does
// not come back usually shows, and the inner panel is what a scroll moves.
// That is the tree layout and render walk too. The inner panel's line ends in
// " inner", so the listing still shows where getChildren() would have gone.
// pref= is the control's getPreferredSize(), so a preferred size that does not
// come back shows too; a nested LayoutItem is not a control and has no line of
// its own.
static void writeControlLine(std::ostream &out, gweni::controls::Base *control, int depth,
    bool inner=false)
{
    const gweni::Rect &bounds=control->getBounds();
    const gweni::Size &preferred=control->getPreferredSize();
    const gweni::SizeFlags flags=control->getSizeFlags();

    out<<std::string(static_cast<size_t>(depth)*2, ' ')
       <<control->getTypeName()
       <<" bounds=("<<bounds.x<<", "<<bounds.y<<", "<<bounds.w<<", "<<bounds.h<<")"
       <<" pref=("<<preferred.width<<", "<<preferred.height<<")"
       <<" dock="<<gweni::getDockPositionName(control->getDock())
       <<" size="<<gweni::sizeFlagName(flags.horizontal)
       <<","<<gweni::sizeFlagName(flags.vertical)
       <<" hidden="<<(control->hidden() ? "yes" : "no")
       <<" visible="<<(control->visible() ? "yes" : "no")
       <<(inner ? " inner" : "")
       <<"\n";

    for(auto &&child : control->getActualChildren())
        writeControlLine(out, child, depth+1, child == control->getInnerPanel());
}

static void writeBoundsListing(std::ostream &out, const Session &s)
{
    out<<"# control="<<s.label<<" size="<<s.size.x<<"x"<<s.size.y<<"\n";
    writeControlLine(out, s.canvas, 0);
}

static std::string boundsListing(const Session &s)
{
    std::ostringstream out;
    writeBoundsListing(out, s);
    return out.str();
}

// A control's place in the tree, as listed: each step is its type and its index
// among its parent's children, e.g. Canvas/Demo[0]/DockBase[1]/Text[0]. That
// names one control even where its siblings share its type.
static std::string childPath(const std::string &parent, gweni::controls::Base *control, size_t index)
{
    return parent+"/"+control->getTypeName()+"["+std::to_string(index)+"]";
}

// The same path read back out of a listing, for its line `lineIndex` (0-based,
// header included), from the indentation alone.
static std::string listingPathAt(const std::string &listing, size_t lineIndex)
{
    std::vector<std::string> stack;     // path up to each depth
    std::vector<size_t> childCount;     // children seen so far at each depth
    std::istringstream in(listing);
    std::string line;

    for(size_t i=0; std::getline(in, line) && i <= lineIndex; ++i)
    {
        if(line.empty() || line[0] == '#')
            continue;

        const size_t indent=line.find_first_not_of(' ');
        const size_t depth=indent/2;
        const std::string type=line.substr(indent, line.find(' ', indent)-indent);

        stack.resize(depth);
        childCount.resize(depth+1, 0);

        if(depth == 0)
            stack.push_back(type);
        else
            stack.push_back(stack[depth-1]+"/"+type+"["+std::to_string(childCount[depth]++)+"]");
        childCount.resize(depth+2, 0);
        childCount[depth+1]=0;
    }
    return stack.empty() ? std::string("(header)") : stack.back();
}

// The listing beside a shot: the same path with a .txt extension.
static std::string boundsPathFor(const std::string &shotPath)
{
    const size_t dot=shotPath.rfind('.');
    const size_t slash=shotPath.find_last_of('/');
    if(dot != std::string::npos && (slash == std::string::npos || dot>slash))
        return shotPath.substr(0, dot)+".txt";
    return shotPath+".txt";
}

static bool dumpBoundsTo(Session &s, const std::string &path)
{
    if(path.empty())
    {
        writeBoundsListing(std::cout, s);
        std::cout.flush();
        return true;
    }

    if(!ensureDirectory(fs::path(path).parent_path()))
        return false;

    std::ofstream out(path, std::ios::binary|std::ios::trunc);
    if(!out)
    {
        std::cerr<<"ControlTest: cannot write "<<path<<"\n";
        return false;
    }
    writeBoundsListing(out, s);
    out.close();

    std::cout<<"wrote "<<path<<"\n";
    return true;
}

// --require-one-pass: a frame whose layout did not converge in its first pass
// is named on stderr and fails the run at exit.
static void checkOnePass(Session &s, const std::string &frame)
{
    const gweni::controls::LayoutStats &layout=s.canvas->getLayoutStats();
    if(!s.requireOnePass || (layout.passes <= 1 && !layout.renderLayout))
        return;

    ++s.multiPassFrames;
    std::cerr<<"ControlTest: layout took more than one pass: control="<<s.label
             <<" frame="<<frame<<" layout_passes="<<layout.passes
             <<" render_layout="<<(layout.renderLayout ? 1 : 0)
             <<" redirtied_by="<<redirtiedBy(layout)
             <<(layout.capped ? " (capped)" : "")<<"\n";
}

static void renderFrames(Session &s, int count)
{
    for(int i=0; i<count; ++i)
    {
        gweni::controls::resetChangeStats();

        s.canvas->updateCanvas();
        s.renderer->clearTarget(s.background);
        s.canvas->renderCanvas();

        if(s.stats)
            printFrameStats(s.frame, s.label, gweni::controls::getChangeStats(),
                s.renderer->getFrameStats(), s.canvas->getLayoutStats(),
                s.canvas->getUpdateStats());
        checkOnePass(s, std::to_string(s.frame));

        ++s.frame;

        // --stats wants a frame with no input to be genuinely idle, so there the
        // clock only moves when a scripted press moves it, unless
        // --stats-clock step asked for the capture's step.
        if((!s.stats || s.statsClockSteps) && !s.freezeClock)
            g_clockSeconds+=g_frameStep;
    }
}

static bool shoot(Session &s, const std::string &path)
{
    // Nothing has drawn into the framebuffer yet: the PNG would be all zeros.
    if(s.frame == 0)
    {
        std::cerr<<"ControlTest: --shot "<<path<<" before any frame was rendered\n";
        return false;
    }
    if(s.dumpBounds && s.dumpPerShot && !dumpBoundsTo(s, boundsPathFor(path)))
        return false;
    if(!s.writePng)
        return true;
    if(!ensureDirectory(fs::path(path).parent_path()))
        return false;
    if(!writeRendererPng(*s.renderer, s.pixels.x, s.pixels.y, path))
        return false;

    std::cout<<"wrote "<<path<<"\n";
    return true;
}

// The whole of framebuffer_size_callback, called directly: no window manager is
// involved, so a scripted resize is just these two plus a new offscreen target.
// The size is logical; the target is that at the session's scale.
static bool resizeSession(Session &s, int width, int height)
{
    s.size=gweni::Point(width, height);
    s.pixels=devicePixels(s.size, s.scale);

    if(!s.device->resizeTarget(s.pixels))
        return false;

    s.renderer->setViewRect(gweni::Rect(gweni::Point(0, 0), s.pixels));
    s.canvas->setSize(width, height);
    return true;
}

// A runtime scale change, as the content-scale callback makes one: the canvas
// keeps its logical size, and the target is that at the new scale.
static bool rescaleSession(Session &s, float scale)
{
    s.scale=scale;
    s.pixels=devicePixels(s.size, s.scale);

    if(!s.device->resizeTarget(s.pixels))
        return false;

    s.renderer->setViewRect(gweni::Rect(gweni::Point(0, 0), s.pixels));
    s.canvas->setScale(scale);
    return true;
}

//-----------------------------------------------------------------------------
// Scripted input. The mouse goes in through Canvas's IInputEvent methods, which
// are the same gweni::input::onMouseMoved/onMouseClicked entry points the GLFW
// backend uses. The keyboard goes one step further out: synthesised GLFW key
// and char events through gweni::input::GLFW3, so its translation is covered
// too. There is no event loop behind either.
//-----------------------------------------------------------------------------

// --trace, on stderr so stdout stays parseable. Scripted coordinates are blind
// without it: this is how you tell "my 40,6 missed the button" from "the button
// did not repaint".
//
// Each control on the hovered chain is named as its type and its index among
// its parent's actual children, or among its parent's inner panel's children
// when it was placed there (the order --dump-bounds lists them in), so two
// same-type siblings are told apart: hovered=CheckBox[2] (0,38 15x15) disabled
// < CheckBoxDemo[0] < Canvas. The hovered control also gets its canvas bounds,
// and "disabled" when it ignores input.
static std::string traceName(gweni::controls::Base *control)
{
    std::string name=control->getTypeName();
    const gweni::controls::Base *parent=control->getParent();

    const auto indexIn=[control](const gweni::controls::Base *holder)
    {
        size_t index=0;

        for(const gweni::controls::Base *child : holder->getActualChildren())
        {
            if(child == control)
                return std::to_string(index);
            index++;
        }
        return std::string();
    };

    if(parent)
    {
        std::string index=indexIn(parent);

        if(index.empty() && parent->getInnerPanel())
            index=indexIn(parent->getInnerPanel());
        if(!index.empty())
            name+="["+index+"]";
    }
    return name;
}

static void trace(const Session &s, const std::string &what)
{
    if(!s.trace)
        return;
    gweni::controls::Base *hovered=gweni::HoveredControl;

    std::cerr<<"trace "<<what
             <<" clock="<<g_clockSeconds
             <<" hovered=";
    if(hovered)
    {
        const gweni::Point at=hovered->localPosToCanvas();

        std::cerr<<traceName(hovered)
                 <<" ("<<at.x<<","<<at.y<<" "<<hovered->getWidth()<<"x"<<hovered->getHeight()<<")";
        if(hovered->isDisabled())
            std::cerr<<" disabled";
    }
    else
        std::cerr<<"(none)";
    for(gweni::controls::Base *p=hovered?hovered->getParent():nullptr; p; p=p->getParent())
        std::cerr<<" < "<<traceName(p);
    std::cerr<<" focus="<<(gweni::KeyboardFocus
        ? gweni::KeyboardFocus->getTypeName() : "(none)")<<"\n";
}

static void moveMouse(Session &s, int x, int y)
{
    const gweni::Point to(x, y);
    const gweni::Point delta=(s.mouse.x<0 && s.mouse.y<0)
        ? gweni::Point(0, 0)
        : gweni::Point(to.x-s.mouse.x, to.y-s.mouse.y);

    s.mouse=to;
    s.canvas->inputMouseMoved(to.x, to.y, delta.x, delta.y);
    trace(s, "move "+std::to_string(to.x)+","+std::to_string(to.y));
}

static gweni::input::GLFW3 &glfwInput(Session &s);

// A button through gweni::input::GLFW3's mouse-button callback, as the
// examples' window delivers it: GLFW_MOUSE_BUTTON_LEFT (0) or _RIGHT (1).
static void mouseDown(Session &s, int x, int y, bool stepClock, int button=GLFW_MOUSE_BUTTON_LEFT)
{
    if(stepClock)
        g_clockSeconds+=g_pressStep;
    moveMouse(s, x, y);
    glfwInput(s).processMouseButtons(button, GLFW_PRESS, 0);
    trace(s, button == GLFW_MOUSE_BUTTON_RIGHT ? "right down" : "down");
}

static void mouseUp(Session &s, int x, int y, int button=GLFW_MOUSE_BUTTON_LEFT)
{
    moveMouse(s, x, y);
    glfwInput(s).processMouseButtons(button, GLFW_RELEASE, 0);
    trace(s, button == GLFW_MOUSE_BUTTON_RIGHT ? "right up" : "up");
}

static void clickMouse(Session &s, int x, int y, int button=GLFW_MOUSE_BUTTON_LEFT)
{
    mouseDown(s, x, y, true, button);
    mouseUp(s, x, y, button);
}

static void doubleClickMouse(Session &s, int x, int y)
{
    clickMouse(s, x, y);
    mouseDown(s, x, y, false);  // 0s after the first: inside any double-click window
    mouseUp(s, x, y);
}

// Named keys as the GLFW key codes a keyboard sends, so --key goes through
// gweni::input::GLFW3's translator exactly as the examples' key callback does.
static const struct { const char *name; int key; } g_namedKeys[]=
{
    {"return", GLFW_KEY_ENTER},          {"backspace", GLFW_KEY_BACKSPACE},
    {"delete", GLFW_KEY_DELETE},         {"left", GLFW_KEY_LEFT},
    {"right", GLFW_KEY_RIGHT},           {"shift", GLFW_KEY_LEFT_SHIFT},
    {"tab", GLFW_KEY_TAB},               {"space", GLFW_KEY_SPACE},
    {"home", GLFW_KEY_HOME},             {"end", GLFW_KEY_END},
    {"control", GLFW_KEY_LEFT_CONTROL},  {"up", GLFW_KEY_UP},
    {"down", GLFW_KEY_DOWN},             {"escape", GLFW_KEY_ESCAPE},
    {"alt", GLFW_KEY_LEFT_ALT},          {"command", GLFW_KEY_LEFT_SUPER},
    {"kp-enter", GLFW_KEY_KP_ENTER},     {"f10", GLFW_KEY_F10},     {"f11", GLFW_KEY_F11},
    {"f12", GLFW_KEY_F12},
};

// The modifiers a --key chord may hold, with the mods bit GLFW reports for them.
static const struct { const char *name; int key; int mod; } g_chordModifiers[]=
{
    {"ctrl", GLFW_KEY_LEFT_CONTROL, GLFW_MOD_CONTROL},
    {"control", GLFW_KEY_LEFT_CONTROL, GLFW_MOD_CONTROL},
    {"shift", GLFW_KEY_LEFT_SHIFT, GLFW_MOD_SHIFT},
    {"alt", GLFW_KEY_LEFT_ALT, GLFW_MOD_ALT},
    {"command", GLFW_KEY_LEFT_SUPER, GLFW_MOD_SUPER},
};

// The GLFW translator, pointed at this session's canvas. Looked up per event,
// since the resize scenario replaces the canvas part way through.
static gweni::input::GLFW3 &glfwInput(Session &s)
{
    g_glfwInput.initialize(s.canvas);
    return g_glfwInput;
}

static std::string glfwKeyName(int key)
{
    for(const auto &named : g_namedKeys)
        if(named.key == key)
            return named.name;
    if(key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        return std::string(1, static_cast<char>(key));
    if(key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        return std::string(1, static_cast<char>(key));
    return std::to_string(key);
}

// One synthesised GLFW key event, as glfwSetKeyCallback would deliver it.
static void glfwKey(Session &s, int key, int action, int mods)
{
    const gweni::input::GLFW3::KeyEvent event={key, 0, action, mods};
    if(!appKey(key, action))
        glfwInput(s).processKeyEvent(event);
    trace(s, std::string("key ")+glfwKeyName(key)+(action == GLFW_PRESS ? " press" : " release")
        +" mods="+std::to_string(mods));
}

// One typed codepoint, as glfwSetCharCallback would deliver it.
static void glfwChar(Session &s, unsigned int codepoint)
{
    const bool used=glfwInput(s).processCharEvent(codepoint);
    char hex[16];
    std::snprintf(hex, sizeof(hex), "U+%04X", codepoint);
    trace(s, std::string("char ")+hex+(used ? " consumed" : " ignored"));
}

// A chord's last part: a named key or a single letter or digit. GLFW names a
// letter by its uppercase ASCII code, and a digit by its own.
static bool parseKeyPart(const std::string &part, int &key)
{
    for(const auto &named : g_namedKeys)
    {
        if(part == named.name)
        {
            key=named.key;
            return true;
        }
    }

    if(part.size() == 1 && std::isalpha(static_cast<unsigned char>(part[0])))
    {
        key=GLFW_KEY_A+(std::toupper(static_cast<unsigned char>(part[0]))-'A');
        return true;
    }
    if(part.size() == 1 && part[0] >= '0' && part[0] <= '9')
    {
        key=GLFW_KEY_0+(part[0]-'0');
        return true;
    }
    return false;
}

// --key: a single printable character is typed (the char callback); anything
// else is a key, or a chord of modifiers and a key joined by '+', sent as GLFW
// key events: each modifier pressed in order, the key pressed and released with
// every modifier's mods bit set, then the modifiers released in reverse. A
// chord's key goes only down the key path, as on a keyboard, where GLFW sends
// no char for Ctrl+letter.
static bool sendKey(Session &s, const std::string &name)
{
    if(name.size() == 1 && name[0]>=' ' && name[0]<'\x7f')
    {
        glfwChar(s, static_cast<unsigned char>(name[0]));
        return true;
    }

    std::vector<std::string> parts;
    for(size_t from=0;;)
    {
        const size_t plus=name.find('+', from);
        parts.push_back(name.substr(from, plus == std::string::npos ? std::string::npos : plus-from));
        if(plus == std::string::npos)
            break;
        from=plus+1;
    }

    std::vector<int> modifierKeys;
    std::vector<int> modifierBits;
    for(size_t i=0; i+1 < parts.size(); ++i)
    {
        bool found=false;
        for(const auto &modifier : g_chordModifiers)
        {
            if(parts[i] != modifier.name)
                continue;
            modifierKeys.push_back(modifier.key);
            modifierBits.push_back(modifier.mod);
            found=true;
            break;
        }
        if(!found)
        {
            std::cerr<<"ControlTest: --key "<<name<<": "<<parts[i]
                     <<" is not a modifier (ctrl, shift, alt, command)\n";
            return false;
        }
    }

    int key=0;
    if(!parseKeyPart(parts.back(), key))
    {
        std::cerr<<"ControlTest: unknown key name: "<<name<<"\n";
        return false;
    }

    int mods=0;
    for(size_t i=0; i < modifierKeys.size(); ++i)
    {
        glfwKey(s, modifierKeys[i], GLFW_PRESS, mods);
        mods|=modifierBits[i];
    }

    glfwKey(s, key, GLFW_PRESS, mods);
    glfwKey(s, key, GLFW_RELEASE, mods);

    for(size_t i=modifierKeys.size(); i-- > 0;)
    {
        glfwKey(s, modifierKeys[i], GLFW_RELEASE, mods);
        mods&=~modifierBits[i];
    }
    return true;
}

// UTF-8 to codepoints. A malformed sequence (a stray continuation byte, a
// truncated, overlong or surrogate encoding) decodes to U+FFFD and decoding
// resumes at the next byte.
static std::vector<unsigned int> decodeUtf8(const std::string &text)
{
    std::vector<unsigned int> codepoints;
    const size_t size=text.size();

    for(size_t i=0; i < size;)
    {
        const unsigned char lead=static_cast<unsigned char>(text[i]);
        size_t length=0;
        unsigned int codepoint=0;
        unsigned int minimum=0;

        if(lead < 0x80){ length=1; codepoint=lead; }
        else if((lead & 0xE0) == 0xC0){ length=2; codepoint=lead & 0x1F; minimum=0x80; }
        else if((lead & 0xF0) == 0xE0){ length=3; codepoint=lead & 0x0F; minimum=0x800; }
        else if((lead & 0xF8) == 0xF0){ length=4; codepoint=lead & 0x07; minimum=0x10000; }

        bool valid=length != 0 && i+length <= size;
        for(size_t j=1; valid && j < length; ++j)
        {
            const unsigned char next=static_cast<unsigned char>(text[i+j]);
            if((next & 0xC0) != 0x80)
                valid=false;
            else
                codepoint=(codepoint << 6) | (next & 0x3F);
        }
        if(valid && (codepoint < minimum || codepoint > 0x10FFFF
            || (codepoint >= 0xD800 && codepoint <= 0xDFFF)))
            valid=false;

        if(valid)
        {
            codepoints.push_back(codepoint);
            i+=length;
        }
        else
        {
            codepoints.push_back(0xFFFD);
            ++i;
        }
    }
    return codepoints;
}

// --type: the text one codepoint at a time through the char callback's path.
static void typeText(Session &s, const std::string &text)
{
    for(unsigned int codepoint : decodeUtf8(text))
        glfwChar(s, codepoint);
}

// The keyboard-focused control's text, if it has one (a Label, which every
// TextBox is).
static bool focusedText(std::string &text)
{
    const gweni::controls::Label *label=dynamic_cast<const gweni::controls::Label *>(gweni::KeyboardFocus);
    if(!label)
        return false;
    text=label->getText();
    return true;
}

// The text as a quoted C string, so a newline or an empty string shows.
static std::string quotedText(const std::string &text)
{
    std::string out="\"";
    for(char c : text)
    {
        if(c == '\n') out+="\\n";
        else if(c == '\t') out+="\\t";
        else if(c == '"' || c == '\\'){ out+='\\'; out+=c; }
        else out+=c;
    }
    return out+"\"";
}

// --expect-text: the focused control's text has to be exactly this. A mismatch
// names both on stderr and fails the run at exit, and the script carries on, so
// one run reports every step that went wrong.
static bool expectText(Session &s, const std::string &expected)
{
    std::string actual;
    const bool hasText=focusedText(actual);
    const bool match=hasText && actual == expected;
    trace(s, "expect-text "+quotedText(expected)+(match ? " ok" : " MISMATCH"));
    if(match)
        return true;

    ++s.expectFailures;
    if(!hasText)
        std::cerr<<"ControlTest: --expect-text "<<quotedText(expected)<<": the keyboard focus ("
                 <<(gweni::KeyboardFocus ? gweni::KeyboardFocus->getTypeName() : "none")
                 <<") has no text\n";
    else
        std::cerr<<"ControlTest: --expect-text mismatch: expected "<<quotedText(expected)
                 <<", the focused "<<gweni::KeyboardFocus->getTypeName()<<" has "<<quotedText(actual)<<"\n";
    return false;
}

static bool scrollWheel(Session &s, int delta)
{
    const bool used=s.canvas->inputMouseWheel(delta);
    trace(s, std::string("wheel ")+std::to_string(delta)+(used?" consumed":" ignored"));
    return used;
}

// Forgetting the hover between controls, so one capture entry cannot leave a
// control hovered for the next one.
static void forgetMouse(Session &s)
{
    if(s.mouse.x<0 && s.mouse.y<0)
        return;
    moveMouse(s, -1, -1);
    s.mouse=gweni::Point(-1, -1);
}

static gweni::controls::Base *addControl(Session &s, const std::string &name)
{
    forgetMouse(s);
    g_clockSeconds=0.0f;
    s.frame=0;
    restartFrameCounts();
    s.label=name;

    gweni::controls::Base *control=createControl(s.canvas, name);
    if(control)
        control->setDock(gweni::DockPosition::Center);
    return control;
}

// Goes through the canvas so preDelete() releases the control's primitives; a
// bare delete leaves the renderer holding them.
static void dropControl(Session &s, gweni::controls::Base *control)
{
    control->delayedDelete();
    s.canvas->updateCanvas();
    checkOnePass(s, "drop");
}

//-----------------------------------------------------------------------------
// Scripted runs
//-----------------------------------------------------------------------------
enum class StepKind { Move, Down, Up, Click, DoubleClick, DownRight, UpRight, ClickRight, Scroll, Key, Type,
    ExpectText, Frames, Shot };

struct Step
{
    StepKind kind;
    int x=0, y=0;
    std::string text;
};

static std::string shotPathIn(const std::string &dir, const std::string &name)
{
    if(name.find('/') != std::string::npos)     // already a path, leave it alone
        return name;
    return dir.empty() ? name : dir+"/"+name;
}

// A script runs its steps as written, with two frames of its own: one settle
// before a script that does not start with --frames, so its first input hits a
// laid-out tree as it would in an app (one that does start with --frames is left
// as it is, so its captures do not move), and one frame before a --shot that
// follows input with none in between, so the shot shows that input. A shot
// before any frame at all (--frames 0) is refused by shoot().
static bool runSteps(Session &s, const std::vector<Step> &steps, const std::string &shotDir)
{
    if(!steps.empty() && steps.front().kind != StepKind::Frames)
        renderFrames(s, kSettleFrames);

    bool inputSinceFrame=false;     // --expect-text, --frames and --shot are not input
    for(const Step &step : steps)
    {
        if(step.kind != StepKind::ExpectText && step.kind != StepKind::Frames
            && step.kind != StepKind::Shot)
            inputSinceFrame=true;

        switch(step.kind)
        {
        case StepKind::Move: moveMouse(s, step.x, step.y); break;
        case StepKind::Down: mouseDown(s, step.x, step.y, true); break;
        case StepKind::Up: mouseUp(s, step.x, step.y); break;
        case StepKind::Click: clickMouse(s, step.x, step.y); break;
        case StepKind::DoubleClick: doubleClickMouse(s, step.x, step.y); break;
        case StepKind::DownRight: mouseDown(s, step.x, step.y, true, GLFW_MOUSE_BUTTON_RIGHT); break;
        case StepKind::UpRight: mouseUp(s, step.x, step.y, GLFW_MOUSE_BUTTON_RIGHT); break;
        case StepKind::ClickRight: clickMouse(s, step.x, step.y, GLFW_MOUSE_BUTTON_RIGHT); break;
        case StepKind::Scroll: (void)scrollWheel(s, step.y); break;
        case StepKind::Key: if(!sendKey(s, step.text)) return false; break;
        case StepKind::Type: typeText(s, step.text); break;
        case StepKind::ExpectText: (void)expectText(s, step.text); break;
        case StepKind::Frames:
            renderFrames(s, step.x);
            if(step.x > 0)
                inputSinceFrame=false;
            break;
        case StepKind::Shot:
            if(inputSinceFrame)
            {
                renderFrames(s, 1);
                inputSinceFrame=false;
            }
            if(!shoot(s, shotPathIn(shotDir, step.text)))
                return false;
            break;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
// Scenarios
//-----------------------------------------------------------------------------
enum class Scenario { None, PerControl, Resize, Docked, Scroll, Interaction, Typing, Clip, TextSize, Scale,
    ToolTip, ContextMenu, Popup, VirtualList, DataTableVirtual, HideShow, DemoLabels, PlotHistogram,
    PlotLines, Inspector, Theme, StatsOverlay };

static bool parseScenario(const std::string &name, Scenario &scenario)
{
    if(name == "per-control") scenario=Scenario::PerControl;
    else if(name == "resize") scenario=Scenario::Resize;
    else if(name == "docked") scenario=Scenario::Docked;
    else if(name == "scroll") scenario=Scenario::Scroll;
    else if(name == "interaction") scenario=Scenario::Interaction;
    else if(name == "typing") scenario=Scenario::Typing;
    else if(name == "clip") scenario=Scenario::Clip;
    else if(name == "text-size") scenario=Scenario::TextSize;
    else if(name == "scale") scenario=Scenario::Scale;
    else if(name == "tooltip") scenario=Scenario::ToolTip;
    else if(name == "context-menu") scenario=Scenario::ContextMenu;
    else if(name == "popup") scenario=Scenario::Popup;
    else if(name == "virtual-list") scenario=Scenario::VirtualList;
    else if(name == "data-table-virtual") scenario=Scenario::DataTableVirtual;
    else if(name == "hide-show") scenario=Scenario::HideShow;
    else if(name == "demo-labels") scenario=Scenario::DemoLabels;
    else if(name == "plot-histogram") scenario=Scenario::PlotHistogram;
    else if(name == "plot-lines") scenario=Scenario::PlotLines;
    else if(name == "inspector") scenario=Scenario::Inspector;
    else if(name == "theme") scenario=Scenario::Theme;
    else if(name == "stats-overlay") scenario=Scenario::StatsOverlay;
    else return false;
    return true;
}

static bool runPerControlScenario(Session &s, const std::string &shotDir)
{
    bool ok=true;
    for(const char *name : g_controlNames)
    {
        gweni::controls::Base *control=addControl(s, name);
        renderFrames(s, s.frames);
        if(!shoot(s, shotDir+"/"+name+".png"))
            ok=false;
        dropControl(s, control);
    }
    return ok;
}

// Which control owns each primitive and text id, by its listing path. Only the
// textured skin's Text renderer holds text ids; every other one holds primitive
// ids.
struct IdOwners
{
    std::map<size_t, std::string> primitives;
    std::map<size_t, std::string> texts;
};

static void collectIdOwners(gweni::controls::Base *control, const std::string &path, IdOwners &owners)
{
    if(const gweni::skin::Control *skinControl=control->getSkinControl())
    {
        const bool text=dynamic_cast<const gweni::skin::textured::Text *>(skinControl) != nullptr;
        for(size_t id : skinControl->getPrimitives())
            (text ? owners.texts : owners.primitives)[id]=path;
    }

    size_t index=0;
    for(auto &&child : control->getActualChildren())
        collectIdOwners(child, childPath(path, child, index++), owners);
}

// One 640x480 state of the resize scenario, as its three checks compare it.
struct ResizeState
{
    std::string name;                   // the shot's file name, for messages
    std::string listing;                // --dump-bounds' listing, always taken
    std::vector<uint8_t> pixels;
    gweni::renderer::SlotRenderer::MirrorSnapshot mirror;
    IdOwners owners;                    // who held each id, to name a mismatch
};

// Replaces every z in the mirror, a primitive vertex's and a glyph vertex's,
// by its rank among the z values drawn (slots and runs not hidden), so two
// states compare equal when they draw in the same depth order. The canvas's z
// labels depend on the order of past edits (Canvas::ZGap: a control added
// between two others takes a z in the gap, one moved takes new ones), so a
// state reached by a round trip draws the same at other z values. A z drawn
// nowhere (a hidden slot's stale bytes) maps between the ranks around it.
static void rankMirrorDepths(gweni::renderer::SlotRenderer::MirrorSnapshot &mirror)
{
    // SlotRenderer::Vertex: uint16_t style, int16_t x, y, then uint16_t z
    const size_t primitiveZOffset=3*sizeof(uint16_t);
    const size_t glyphZOffset=offsetof(ftgl::glyph_vertex_t, z);
    const size_t glyphBytes=sizeof(ftgl::glyph_vertex_t);

    auto primitiveHidden=[](const std::vector<uint8_t> &bytes)
    {
        const size_t vertexBytes=bytes.size()/4;
        for(size_t vertex=0; vertex<4; ++vertex)
        {
            uint16_t style;
            std::memcpy(&style, &bytes[vertex*vertexBytes], sizeof(style));
            if(style != 0)
                return false;
        }
        return true;
    };

    std::vector<double> drawn;
    for(const auto &entry : mirror.primitives)
    {
        const std::vector<uint8_t> &bytes=entry.second;
        if((bytes.size()%4 != 0) || primitiveHidden(bytes))
            continue;
        for(size_t vertex=0; vertex<4; ++vertex)
        {
            uint16_t z;
            std::memcpy(&z, &bytes[vertex*(bytes.size()/4)+primitiveZOffset], sizeof(z));
            drawn.push_back(z);
        }
    }
    for(const auto &entry : mirror.texts)
    {
        const std::vector<uint8_t> &bytes=entry.second;
        if(bytes.empty() || (bytes[0] != 0))
            continue;
        for(size_t offset=1; offset+glyphBytes <= bytes.size(); offset+=glyphBytes)
        {
            float z;
            std::memcpy(&z, &bytes[offset+glyphZOffset], sizeof(z));
            drawn.push_back(z);
        }
    }
    std::sort(drawn.begin(), drawn.end());
    drawn.erase(std::unique(drawn.begin(), drawn.end()), drawn.end());

    auto rank=[&](double z)
    {
        const auto at=std::lower_bound(drawn.begin(), drawn.end(), z);
        const size_t index=(size_t)(at-drawn.begin());
        return 2*index+((at != drawn.end()) && (*at == z) ? 0 : 1);
    };

    for(auto &entry : mirror.primitives)
    {
        std::vector<uint8_t> &bytes=entry.second;
        if(bytes.size()%4 != 0)
            continue;
        for(size_t vertex=0; vertex<4; ++vertex)
        {
            uint8_t *field=&bytes[vertex*(bytes.size()/4)+primitiveZOffset];
            uint16_t z;
            std::memcpy(&z, field, sizeof(z));
            const uint16_t ranked=(uint16_t)rank(z);
            std::memcpy(field, &ranked, sizeof(ranked));
        }
    }
    for(auto &entry : mirror.texts)
    {
        std::vector<uint8_t> &bytes=entry.second;
        for(size_t offset=1; offset+glyphBytes <= bytes.size(); offset+=glyphBytes)
        {
            float z;
            std::memcpy(&z, &bytes[offset+glyphZOffset], sizeof(z));
            const float ranked=(float)rank(z);
            std::memcpy(&bytes[offset+glyphZOffset], &ranked, sizeof(ranked));
        }
    }
}

static void captureResizeState(Session &s, const std::string &path, ResizeState &state)
{
    state.name=fs::path(path).filename().string();
    state.listing=boundsListing(s);
    s.renderer->readPixels(s.pixels.x, s.pixels.y, state.pixels);
    state.mirror=s.renderer->snapshotMirror();
    rankMirrorDepths(state.mirror);
    collectIdOwners(s.canvas, s.canvas->getTypeName(), state.owners);
}

// Prints the first line where two listings part, with the control it lists.
static bool listingsMatch(const ResizeState &a, const ResizeState &b)
{
    if(a.listing == b.listing)
        return true;

    std::istringstream left(a.listing), right(b.listing);
    std::string leftLine, rightLine;
    size_t line=0;

    for(;; ++line)
    {
        const bool haveLeft=static_cast<bool>(std::getline(left, leftLine));
        const bool haveRight=static_cast<bool>(std::getline(right, rightLine));
        if(!haveLeft) leftLine="(end of listing)";
        if(!haveRight) rightLine="(end of listing)";
        if(leftLine != rightLine || !haveLeft || !haveRight)
            break;
    }

    const size_t linesInA=static_cast<size_t>(std::count(a.listing.begin(), a.listing.end(), '\n'));
    const std::string &listing=(line < linesInA) ? a.listing : b.listing;
    std::cerr<<"ControlTest: listing of "<<b.name<<" differs from "<<a.name
             <<" first at line "<<line+1<<", control "<<listingPathAt(listing, line)<<"\n"
             <<"  "<<a.name<<": "<<leftLine<<"\n"
             <<"  "<<b.name<<": "<<rightLine<<"\n";
    return false;
}

// The largest x/y move of any glyph vertex of a text run that kept its glyph
// count: a run that was moved and moved back by `x += delta` (text defect B)
// differs only there, by float rounding.
static float largestGlyphMove(const std::vector<uint8_t> &before, const std::vector<uint8_t> &after)
{
    float largest=0.0f;
    const size_t count=(before.size()-1)/sizeof(ftgl::glyph_vertex_t);
    for(size_t i=0; i<count; ++i)
    {
        ftgl::glyph_vertex_t a, b;
        std::memcpy(&a, before.data()+1+i*sizeof(a), sizeof(a));
        std::memcpy(&b, after.data()+1+i*sizeof(b), sizeof(b));
        largest=std::max(largest, std::max(std::fabs(a.x-b.x), std::fabs(a.y-b.y)));
    }
    return largest;
}

static size_t reportMirrorKind(const char *kind, const ResizeState &beforeState,
    const std::map<size_t, std::vector<uint8_t>> &before, const ResizeState &afterState,
    const std::map<size_t, std::vector<uint8_t>> &after, const std::map<size_t, std::string> &owners,
    bool text)
{
    static const size_t maxReported=10;
    size_t reported=0;

    std::vector<size_t> ids;
    for(const auto &entry : before) ids.push_back(entry.first);
    for(const auto &entry : after)
        if(before.find(entry.first) == before.end()) ids.push_back(entry.first);
    std::sort(ids.begin(), ids.end());

    size_t differing=0;
    for(size_t id : ids)
    {
        const auto was=before.find(id);
        const auto now=after.find(id);
        std::string how;

        if(was == before.end()) how="live only in "+afterState.name;
        else if(now == after.end()) how="live only in "+beforeState.name;
        else if(was->second == now->second) continue;
        else if(was->second.size() != now->second.size()) how="contents differ (size)";
        else if(text && was->second[0] != now->second[0]) how="hidden state differs";
        else if(text)
            how="glyph vertexes differ, largest x/y move "+std::to_string(largestGlyphMove(was->second, now->second));
        else how="slot bytes differ";

        ++differing;
        if(reported++ == maxReported)
            std::cerr<<"  ... (only the first "<<maxReported<<" "<<kind<<" ids are shown)\n";
        if(reported > maxReported)
            continue;

        const auto owner=owners.find(id);
        std::cerr<<"  "<<kind<<" id="<<id<<" "<<how<<", owner "
                 <<(owner != owners.end() ? owner->second : std::string("(not found)"))<<"\n";
    }
    return differing;
}

// The renderer's CPU mirror, id by id, of an earlier state against a later one
// (before the grow against after the shrink). Owners are named from the tree
// as it stood in the later one.
static bool mirrorsMatch(const ResizeState &before, const ResizeState &after)
{
    if(before.mirror.primitives == after.mirror.primitives && before.mirror.texts == after.mirror.texts)
        return true;

    const IdOwners &owners=after.owners;
    std::cerr<<"ControlTest: renderer mirror of "<<after.name
             <<" differs from "<<before.name<<":\n";
    const size_t primitives=reportMirrorKind("primitive", before, before.mirror.primitives,
        after, after.mirror.primitives, owners.primitives, false);
    const size_t texts=reportMirrorKind("text", before, before.mirror.texts,
        after, after.mirror.texts, owners.texts, true);
    std::cerr<<"  "<<primitives<<" primitive ids and "<<texts<<" text ids differ\n";
    return false;
}

static bool pixelsMatch(const ResizeState &a, const ResizeState &b)
{
    if(a.pixels == b.pixels)
        return true;

    size_t differing=0;
    for(size_t i=0; i<a.pixels.size() && i<b.pixels.size(); i+=4)
        if(std::memcmp(&a.pixels[i], &b.pixels[i], 4) != 0)
            ++differing;
    std::cerr<<"ControlTest: "<<b.name<<" differs from "<<a.name<<" in "
             <<differing<<" pixels\n";
    return false;
}

// Replaces the session's canvas with a new one, built directly at the
// session's size. The old one goes first, so the new one gets the same
// (lowest-first) primitive ids a canvas built at start-up got.
static void replaceCanvas(Session &s)
{
    forgetMouse(s);
    gweni::skin::Base *skin=s.canvas->getSkin();
    delete s.canvas;
    s.canvas=createCanvas(skin, s.size, s.scale);
    g_canvas=s.canvas;
}

// Three shots around a grow-then-shrink, plus a reference from a canvas built
// directly at 640x480 once the cycle is over. All three 640x480 states have to
// be the same, checked cheapest first: the listings, then the renderer mirror
// id by id (before the grow against after the shrink), then the pixels.
// Anything that does not re-settle on shrink shows up here and nowhere else in
// the harness.
static bool runResizeScenario(Session &s, const std::string &controlName,
    const std::string &shotDir, bool &settled)
{
    const std::string fresh=shotDir+"/resize-00-fresh-640x480.png";
    const std::string first=shotDir+"/resize-01-640x480.png";
    const std::string grown=shotDir+"/resize-02-1024x768.png";
    const std::string shrunk=shotDir+"/resize-03-640x480.png";

    ResizeState freshState, firstState, shrunkState;

    // the shots are compared with each other, so nothing may move between them
    // but what the resize moves: an animation stands still
    s.freezeClock=true;

    if(!resizeSession(s, 640, 480))
        return false;

    gweni::controls::Base *control=addControl(s, controlName);

    renderFrames(s, s.frames);
    if(!shoot(s, first)) return false;
    captureResizeState(s, first, firstState);

    if(!resizeSession(s, 1024, 768)) return false;
    renderFrames(s, s.frames);
    if(!shoot(s, grown)) return false;

    if(!resizeSession(s, 640, 480)) return false;
    renderFrames(s, s.frames);
    if(!shoot(s, shrunk)) return false;
    captureResizeState(s, shrunk, shrunkState);

    dropControl(s, control);

    replaceCanvas(s);
    control=addControl(s, controlName);
    renderFrames(s, s.frames);
    if(!shoot(s, fresh)) return false;
    captureResizeState(s, fresh, freshState);
    dropControl(s, control);

    // Every comparison runs, so a failing run names all that differs; each
    // message says which pair it is about.
    const bool listingSettled=listingsMatch(firstState, shrunkState);
    bool freshMatches=listingsMatch(freshState, firstState);
    freshMatches=listingsMatch(freshState, shrunkState) && freshMatches;

    const bool mirrorSettled=mirrorsMatch(firstState, shrunkState);

    const bool shrinkSettled=pixelsMatch(firstState, shrunkState);
    freshMatches=pixelsMatch(freshState, firstState) && freshMatches;
    freshMatches=pixelsMatch(freshState, shrunkState) && freshMatches;

    settled=listingSettled && mirrorSettled && shrinkSettled && freshMatches;

    std::cout<<"resize control="<<controlName
             <<" listing_settled="<<(listingSettled?"yes":"no")
             <<" mirror_settled="<<(mirrorSettled?"yes":"no")
             <<" shrink_settled="<<(shrinkSettled?"yes":"no")
             <<" fresh_matches="<<(freshMatches?"yes":"no")<<"\n";
    if(!settled)
        std::cerr<<"ControlTest: the layout did not come back to its 640x480 state\n";
    return true;
}

// Three shots around a 1 -> 2 -> 1 Canvas::setScale, plus a reference from a
// canvas built directly at scale 2. A scale change lays the whole tree out
// again and re-emits every primitive, so the last 1x state has to be the first
// one byte for byte (listing, renderer mirror, pixels), and the 2x one what a
// start at --scale 2 draws.
static bool runScaleScenario(Session &s, const std::string &controlName,
    const std::string &shotDir, bool &settled)
{
    const std::string fresh=shotDir+"/scale-00-fresh-2x.png";
    const std::string first=shotDir+"/scale-01-1x.png";
    const std::string doubled=shotDir+"/scale-02-2x.png";
    const std::string back=shotDir+"/scale-03-1x.png";

    ResizeState freshState, firstState, doubledState, backState;

    // as for resize: an animation stands still between the shots
    s.freezeClock=true;

    if(!rescaleSession(s, 1.0f) || !resizeSession(s, 640, 480))
        return false;

    gweni::controls::Base *control=addControl(s, controlName);

    renderFrames(s, s.frames);
    if(!shoot(s, first)) return false;
    captureResizeState(s, first, firstState);

    if(!rescaleSession(s, 2.0f)) return false;
    renderFrames(s, s.frames);
    if(!shoot(s, doubled)) return false;
    captureResizeState(s, doubled, doubledState);

    if(!rescaleSession(s, 1.0f)) return false;
    renderFrames(s, s.frames);
    if(!shoot(s, back)) return false;
    captureResizeState(s, back, backState);

    dropControl(s, control);

    // built at scale 2 from the start, onto a 2x target
    if(!rescaleSession(s, 2.0f)) return false;
    replaceCanvas(s);
    control=addControl(s, controlName);
    renderFrames(s, s.frames);
    if(!shoot(s, fresh)) return false;
    captureResizeState(s, fresh, freshState);
    dropControl(s, control);

    const bool listingSettled=listingsMatch(firstState, backState);
    const bool mirrorSettled=mirrorsMatch(firstState, backState);
    const bool pixelsSettled=pixelsMatch(firstState, backState);
    bool freshMatches=listingsMatch(doubledState, freshState);
    freshMatches=pixelsMatch(doubledState, freshState) && freshMatches;

    settled=listingSettled && mirrorSettled && pixelsSettled && freshMatches;

    std::cout<<"scale control="<<controlName
             <<" listing_settled="<<(listingSettled?"yes":"no")
             <<" mirror_settled="<<(mirrorSettled?"yes":"no")
             <<" pixels_settled="<<(pixelsSettled?"yes":"no")
             <<" fresh_matches="<<(freshMatches?"yes":"no")<<"\n";
    if(!settled)
        std::cerr<<"ControlTest: the canvas did not come back from a 1 -> 2 -> 1 scale change\n";
    return true;
}

// The shell's demo list, laid out at --size: each entry's label has to fit
// the room it is drawn in (Demo::listEntryFits), or it is clipped, as the
// README hero's "ShowcaseContainersD" was.
static bool runDemoLabelsScenario(Session &s, const std::string &shotDir, bool &passed)
{
    gweni::controls::Base *control=addControl(s, g_dockedControlName);
    renderFrames(s, s.frames);
    const bool ok=shoot(s, shotDir+"/demo-labels.png");

    gweni::Demo *demo=gweni::gweni_cast<gweni::Demo>(control);
    size_t entries=0;
    size_t clipped=0;

    if(demo)
    {
        for(const gweni::Demo::ListEntryFit &fit : demo->listEntryFits())
        {
            ++entries;
            if(fit.textWidth <= fit.room)
                continue;

            ++clipped;
            std::cerr<<"ControlTest: demo list label clipped: "<<fit.key<<" (\""<<fit.label<<"\" "
                     <<fit.textWidth<<" wide, "<<fit.room<<" room)\n";
        }
    }
    else
        std::cerr<<"ControlTest: demo-labels: the "<<g_dockedControlName<<" control is not a gweni::Demo\n";

    std::cout<<"demo-labels entries="<<entries<<" clipped="<<clipped<<"\n";
    passed=demo && entries > 0 && clipped == 0;
    dropControl(s, control);
    return ok;
}

static bool runDockedScenario(Session &s, const std::string &shotDir)
{
    if(!resizeSession(s, 1024, 768))
        return false;

    gweni::controls::Base *control=addControl(s, g_dockedControlName);
    renderFrames(s, s.frames);
    const bool ok=shoot(s, shotDir+"/docked-1024x768.png");
    dropControl(s, control);
    return ok;
}

// The demos that overflow their own bounds, idle and scrolled. Content is cut
// at each clipping control's edges, so the scrolled shots show what is left
// inside the viewports.
static const char *const g_scrollDemos[]=
{
    "ScrollControlDemo", "PageControlDemo", "ListBoxDemo",
    "TreeControlDemo", "PropertiesDemo"
};

// ScrollControl::onMouseWheeled nudges by delta/60 of a nudge-per-content-size
// fraction, so one notch moves almost nothing; this is a deliberate shove.
static const int g_scrollNotch=-30;
static const int g_scrollNotches=10;

static void collectScrollControls(gweni::controls::Base *control,
    std::vector<gweni::controls::ScrollControl *> &found)
{
    if(control->hidden())
        return;
    if(auto *scroll=dynamic_cast<gweni::controls::ScrollControl *>(control))
        found.push_back(scroll);
    // The actual children: getChildren() would skip a ScrollControl that is
    // its parent's inner panel (a TreeControl's).
    for(auto &&child : control->getActualChildren())
        collectScrollControls(child, found);
}

// Canvas::inputMouseWheel only ever reaches the hovered control, and a demo's
// scrollable regions are small and nowhere near the canvas centre, so aim at
// each one in turn rather than guessing a coordinate.
static int scrollEveryRegion(Session &s, gweni::controls::Base *control)
{
    std::vector<gweni::controls::ScrollControl *> regions;
    collectScrollControls(control, regions);

    int consumed=0;
    for(gweni::controls::ScrollControl *region : regions)
    {
        // Several demos lay their scroll regions out far wider than the canvas,
        // so the region's own centre can be off-screen and hover nothing: aim at
        // the middle of the part that is actually on the canvas.
        const gweni::Point origin=region->localPosToCanvas();
        const int left=std::max(0, origin.x);
        const int top=std::max(0, origin.y);
        const int right=std::min(s.size.x-1, origin.x+region->getSize().x);
        const int bottom=std::min(s.size.y-1, origin.y+region->getSize().y);
        if(left>right || top>bottom)
            continue;

        moveMouse(s, (left+right)/2, (top+bottom)/2);

        bool moved=false;
        for(int i=0; i<g_scrollNotches; ++i)
            moved=scrollWheel(s, g_scrollNotch) || moved;
        if(moved)
            ++consumed;
    }

    std::cout<<"scroll control="<<s.label<<" regions="<<regions.size()
             <<" scrolled="<<consumed<<"\n";
    return consumed;
}

static bool runScrollScenario(Session &s, const std::string &shotDir)
{
    bool ok=true;
    for(const char *name : g_scrollDemos)
    {
        gweni::controls::Base *control=addControl(s, name);

        renderFrames(s, s.frames);
        if(!shoot(s, shotDir+"/scroll-"+name+"-idle.png"))
            ok=false;

        scrollEveryRegion(s, control);

        renderFrames(s, s.frames);
        if(!shoot(s, shotDir+"/scroll-"+name+"-scrolled.png"))
            ok=false;

        dropControl(s, control);
    }
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario plot-histogram: PlotHistogramDemo's 32-bar series changed and
// hovered
//
// One primitive slot per bar, redrawn only where a bar changed: a sample whose
// bar gets a new height writes that bar's slot alone, one whose bar keeps its
// height writes and changes nothing, and a rescale writes the bars that moved.
// The hovered bar is drawn in its hover look: moving onto a bar writes its slot,
// a move inside it nothing, a move to the next bar the two bars' slots.
//-----------------------------------------------------------------------------

namespace
{

struct PlotStep
{
    size_t primSlots=0;
    size_t textSlots=0;
    size_t changed=0;       // controls with a state change in the frame
};

}//namespace

static size_t livePrimitiveIds();

static PlotStep plotFrame(Session &s)
{
    renderFrames(s, 1);

    PlotStep step;
    const gweni::renderer::SlotRenderer::FrameStats &gpu=s.renderer->getFrameStats();

    step.primSlots=gpu.primitiveSlotsWritten;
    step.textSlots=gpu.textSlotsWritten;
    step.changed=gweni::controls::getChangeStats().controls;
    return step;
}

static void printPlotStep(const char *name, const PlotStep &step)
{
    std::cout<<" "<<name<<"_prim_slots="<<step.primSlots<<" "<<name<<"_text_slots="<<step.textSlots
             <<" "<<name<<"_changed="<<step.changed;
}

static bool runPlotHistogramScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    const size_t idsBefore=livePrimitiveIds();
    gweni::controls::Base *control=addControl(s, "PlotHistogramDemo");
    auto *demo=gweni::gweni_cast<gweni::PlotHistogramDemo>(control);

    if(!demo || !demo->series())
    {
        std::cerr<<"ControlTest: plot-histogram: PlotHistogramDemo has no series\n";
        return false;
    }

    gweni::controls::PlotHistogram *plot=demo->series();

    s.label="plot-histogram";
    renderFrames(s, s.frames);

    const size_t bars=plot->getValueCount();
    PlotStep idle=plotFrame(s);
    size_t idleSlots=idle.primSlots+idle.textSlots;
    size_t idleChanged=idle.changed;

    auto settle=[&]()
    {
        const PlotStep step=plotFrame(s);

        idleSlots+=step.primSlots+step.textSlots;
        idleChanged+=step.changed;
    };

    // A sample inside the automatic range given a new height: bar 4, raised to
    // the middle of the range, below the highest value.
    const size_t bar=4;
    const float middle=(plot->getScaleMin()+plot->getScaleMax())/2.0f;
    const int heightBefore=plot->getBarRect(bar).h;

    plot->setValue(bar, middle);

    const bool newHeight=plot->getBarRect(bar).h != heightBefore;
    const PlotStep sample=plotFrame(s);
    settle();

    // the same bar a hair higher, still the same height: nothing
    const float range=plot->getScaleMax()-plot->getScaleMin();
    const int heightAfter=plot->getBarRect(bar).h;

    plot->setValue(bar, middle+range*0.001f);

    const bool sameHeight=plot->getBarRect(bar).h == heightAfter;
    const PlotStep keep=plotFrame(s);
    settle();

    // the highest value doubled: the automatic range rescales the bars
    size_t highest=0;
    for(size_t i=1; i < bars; ++i)
    {
        if(plot->getValues()[i] > plot->getValues()[highest])
            highest=i;
    }

    const float top=plot->getValues()[highest];

    plot->setValue(highest, top*2.0f);
    const PlotStep rescale=plotFrame(s);
    settle();
    plot->setValue(highest, top);
    renderFrames(s, 1);
    settle();

    // the mouse onto bar 10, inside it, then onto bar 11
    const gweni::Rect area=plot->getPlotBounds();
    const gweni::Point origin=plot->localPosToCanvas();
    auto barPoint=[&](size_t index, int dx, int y)
    {
        return gweni::Point(origin.x+plot->getBarRect(index).x+dx, origin.y+y);
    };

    gweni::Point at=barPoint(10, 1, area.y+area.h-2);
    moveMouse(s, at.x, at.y);
    const PlotStep hover=plotFrame(s);
    const int hoveredFirst=plot->getHoveredBar();
    settle();

    at=barPoint(10, 3, area.y+2);
    moveMouse(s, at.x, at.y);
    const PlotStep inside=plotFrame(s);
    settle();

    at=barPoint(11, 1, area.y+area.h-2);
    moveMouse(s, at.x, at.y);
    const PlotStep next=plotFrame(s);
    const int hoveredNext=plot->getHoveredBar();
    const std::string toolTip=plot->getToolTipText();
    settle();

    const bool shot=shoot(s, shotDir+"/plot-histogram-hover.png");

    forgetMouse(s);
    renderFrames(s, 1);
    settle();

    dropControl(s, control);
    renderFrames(s, 1);

    const size_t idsLeft=livePrimitiveIds()-idsBefore;

    const bool oneSlot=newHeight && sample.primSlots == 1 && sample.textSlots == 0 && sample.changed == 1;
    const bool nothingKept=sameHeight && keep.primSlots == 0 && keep.textSlots == 0 && keep.changed == 0;
    const bool rescaled=rescale.primSlots > 1 && rescale.primSlots <= bars && rescale.textSlots == 0;
    const bool hovered=hoveredFirst == 10 && hover.primSlots == 1 && hover.textSlots == 0
        && inside.primSlots == 0 && inside.textSlots == 0 && inside.changed == 0
        && hoveredNext == 11 && next.primSlots == 2 && next.textSlots == 0 && toolTip.rfind("11: ", 0) == 0;
    const bool settled=idleSlots == 0 && idleChanged == 0;
    const bool idsBack=idsLeft == 0;

    passed=oneSlot && nothingKept && rescaled && hovered && settled && idsBack;

    std::cout<<"plot-histogram bars="<<bars;
    printPlotStep("sample", sample);
    printPlotStep("same_height", keep);
    printPlotStep("rescale", rescale);
    printPlotStep("hover", hover);
    printPlotStep("inside", inside);
    printPlotStep("next", next);
    std::cout<<" idle_slots="<<idleSlots<<" idle_changed="<<idleChanged<<" ids_left="<<idsLeft
             <<" one_slot="<<(oneSlot?"yes":"no")<<" same_height_nothing="<<(nothingKept?"yes":"no")
             <<" rescaled="<<(rescaled?"yes":"no")<<" hovered="<<(hovered?"yes":"no")
             <<" settled="<<(settled?"yes":"no")<<" ids_back="<<(idsBack?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: plot-histogram: "
                 <<(!oneSlot ? "a sample with a new height did not write exactly its bar's slot"
                     : !nothingKept ? "a sample that kept its height wrote or changed something"
                     : !rescaled ? "a rescale did not redraw the bars that moved"
                     : !hovered ? "the hover wrote other than the hovered bars' slots"
                     : !settled ? "a settled frame wrote slots or changed controls"
                     : "the demo did not give its primitive ids back")<<"\n";
    return shot;
}

//-----------------------------------------------------------------------------
// --scenario inspector: gweni::tools::Inspector over the settled Demo
//
// The inspector shows the previous frame's counters with its own controls'
// work and slots taken out, and sets a label only when its string changes, so
// over a settled app it settles too: it shows 0, and the frames after it
// write nothing at all, its own slots included (docs/architecture.md -> "The
// inspector").
//-----------------------------------------------------------------------------

// The first shown demo-list entry (a CategoryButton, which lights up while
// hovered) under control whose centre is outside avoid.
static gweni::controls::Base *findListEntry(gweni::controls::Base *control, const gweni::Rect &avoid)
{
    if(!control->visible())
        return nullptr;
    if(std::strcmp(control->getTypeName(), "CategoryButton") == 0)
    {
        const gweni::Point at=control->localPosToCanvas();
        const gweni::Point centre(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
        const bool inside=centre.x >= avoid.x && centre.x < avoid.x+avoid.w
            && centre.y >= avoid.y && centre.y < avoid.y+avoid.h;

        if(!inside && control->getWidth() > 0 && control->getHeight() > 0)
            return control;
    }
    for(gweni::controls::Base *child : control->getActualChildren())
    {
        if(gweni::controls::Base *found=findListEntry(child, avoid))
            return found;
    }
    return nullptr;
}

static bool runInspectorScenario(Session &s, const std::string &shotDir, bool &passed)
{
    using gweni::tools::Inspector;

    s.freezeClock=true;

    gweni::controls::Base *control=addControl(s, g_dockedControlName);

    if(!control)
        return false;
    s.label="inspector";
    renderFrames(s, std::max(s.frames, 5));

    // Demo leaves canvas-level Menus behind when it goes: the inspector's ids
    // are counted from here to its close
    const size_t idsBefore=livePrimitiveIds();

    // F12, as the window's key callback would deliver it
    glfwKey(s, GLFW_KEY_F12, GLFW_PRESS, 0);
    glfwKey(s, GLFW_KEY_F12, GLFW_RELEASE, 0);

    Inspector *inspector=Inspector::find(s.canvas);

    if(!inspector)
    {
        std::cerr<<"ControlTest: inspector: F12 opened no inspector\n";
        return false;
    }

    const gweni::renderer::SlotRenderer::FrameStats &gpu=s.renderer->getFrameStats();
    const std::string zeroSlots="slots written: 0 primitive, 0 text";
    const std::string zeroRender="render: 0 rendered, 0 changed";

    // frames until one writes nothing at all
    auto settle=[&](int most)
    {
        for(int frame=1; frame <= most; ++frame)
        {
            renderFrames(s, 1);
            if(gpu.primitiveSlotsWritten == 0 && gpu.textSlotsWritten == 0)
                return frame;
        }
        return -1;
    };

    const int settleFrames=settle(6);
    const std::vector<gweni::String> settledLines=inspector->counterLines();
    const bool settledZero=settledLines[Inspector::SlotsLine] == zeroSlots
        && settledLines[Inspector::RenderLine] == zeroRender
        && settledLines[Inspector::LayoutLine].rfind("layout: 0 passes", 0) == 0;
    renderFrames(s, 1);
    const size_t afterSlots=gpu.primitiveSlotsWritten+gpu.textSlotsWritten;

    const bool shot=shoot(s, shotDir+"/inspector.png");

    // a hover in the app: shown on the next frame, then 0 again
    const gweni::Point at=inspector->localPosToCanvas();
    gweni::controls::Base *button=findListEntry(control,
        gweni::Rect(at.x, at.y, inspector->getWidth(), inspector->getHeight()));
    size_t hoverPrim=0;
    size_t hoverText=0;
    std::string shownHover;
    int backFrames=-1;
    std::string backLine;

    if(button)
    {
        const gweni::Point origin=button->localPosToCanvas();

        moveMouse(s, origin.x+button->getWidth()/2, origin.y+button->getHeight()/2);
        renderFrames(s, 1);
        hoverPrim=gpu.primitiveSlotsWritten;
        hoverText=gpu.textSlotsWritten;
        renderFrames(s, 1);
        shownHover=inspector->counterLines()[Inspector::SlotsLine];
        backFrames=settle(4);
        backLine=inspector->counterLines()[Inspector::SlotsLine];
    }

    const std::string expectHover="slots written: "+std::to_string(hoverPrim)+" primitive, "
        +std::to_string(hoverText)+" text";

    // F12 again closes it
    forgetMouse(s);
    glfwKey(s, GLFW_KEY_F12, GLFW_PRESS, 0);
    glfwKey(s, GLFW_KEY_F12, GLFW_RELEASE, 0);
    renderFrames(s, 2);

    const bool closed=Inspector::find(s.canvas) == nullptr;
    const size_t idsLeft=livePrimitiveIds()-idsBefore;

    dropControl(s, control);
    renderFrames(s, 1);

    const bool settles=settleFrames > 0 && settledZero && afterSlots == 0;
    const bool hoverShown=button && (hoverPrim+hoverText) > 0 && shownHover == expectHover;
    const bool back=backFrames > 0 && backLine == zeroSlots;
    const bool idsBack=idsLeft == 0;

    passed=settles && hoverShown && back && closed && idsBack;

    std::cout<<"inspector settle_frames="<<settleFrames<<" settled_zero="<<(settledZero?"yes":"no")
             <<" after_slots="<<afterSlots<<" hover_prim_slots="<<hoverPrim<<" hover_text_slots="<<hoverText
             <<" hover_shown="<<(hoverShown?"yes":"no")<<" back_frames="<<backFrames
             <<" back_zero="<<(back?"yes":"no")<<" closed="<<(closed?"yes":"no")
             <<" ids_left="<<idsLeft<<" ids_back="<<(idsBack?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: inspector: "
                 <<(!settles ? "it did not settle to a frame writing nothing while showing 0 ("
                        +settledLines[Inspector::SlotsLine]+", "+settledLines[Inspector::RenderLine]+")"
                     : !hoverShown ? "the hover frame's slots were not shown (expected \""+expectHover
                        +"\", shown \""+shownHover+"\")"
                     : !back ? "it did not come back to 0 after the hover (\""+backLine+"\")"
                     : !closed ? "F12 did not close it"
                     : "the inspector did not give its primitive ids back")<<"\n";
    return shot;
}

//-----------------------------------------------------------------------------
// --scenario stats-overlay: gweni::tools::StatsOverlay over the settled Demo
//
// The overlay gathers every frame and refreshes what it shows every
// StatsOverlay::RefreshSeconds, setting only labels whose text changed, with
// its own controls' work and slots left out, so over a settled app only its
// refreshes write, and only its own slots; what it shows is what the frames of
// its window counted (docs/architecture.md -> "The stats overlay").
//
// The demo shell's "Stats" button follows it: on once F10 has opened it and
// off once F10 has closed it, the overlay sitting Inset above the shell's
// status bar, clear of the button. Then a click on the button opens it, F10
// closes and opens it, and a click closes it: after each the button is on
// exactly while it is open and it was told once (StatsOverlay::
// onOpenChangedCaller); a click frame writes only the button's slots and the
// overlay's and lays out the canvas alone, and the close gives back its ids.
//
// Then the Inspector and the overlay open together: each leaves out every
// tool's slots, the other's and those either gave back included
// (Canvas::excludedSlots()), so a frame only a tool wrote in reads 0 in both.
//-----------------------------------------------------------------------------

// What one frame left behind, read after it: the canvas's counts of the frame
// before it (what the overlay gathered in this frame's update phase), and the
// renderer's of this one.
struct OverlayFrame
{
    gweni::controls::CanvasFrameStats previous;
    gweni::renderer::FrameCounts counts;
    std::vector<size_t> primitives;
    std::vector<gweni::renderer::WrittenText> texts;
    size_t refreshes=0;
    gweni::tools::StatsOverlay::Window shown;
};

// The window a refresh in frame `refresh` showed, from the frames recorded
// since the refresh in frame `start`: the overlay's thinks in frames
// start+1..refresh, each gathering the canvas's counts of the frame before it
// and that frame's renderer counts, its own slots (by id) taken out.
static gweni::tools::StatsOverlay::Window expectedWindow(const std::vector<OverlayFrame> &frames,
    size_t start, size_t refresh, const std::vector<size_t> &ownPrimitives, const std::vector<size_t> &ownTexts)
{
    gweni::tools::StatsOverlay::Window window;

    for(size_t k=start+1; k <= refresh; ++k)
    {
        const gweni::controls::CanvasFrameStats &frame=frames[k].previous;
        const gweni::controls::FrameTimes &times=frame.times;
        const OverlayFrame &drawn=frames[k-1];

        window.frame.add(times.startMicros-frames[k-1].previous.times.startMicros);
        window.update.add(times.updateMicros);
        window.render.add(times.renderMicros-times.flushMicros);
        window.flush.add(times.flushMicros);
        window.cpu.add(times.updateMicros+times.renderMicros);
        window.rendered=std::max(window.rendered, frame.render.visited);
        window.changed=std::max(window.changed, frame.render.controls);
        window.layoutPasses=std::max(window.layoutPasses, size_t(std::max(0, frame.layout.passes)));
        window.layoutEntries=std::max(window.layoutEntries, frame.layout.visited);
        window.zVisited=std::max(window.zVisited, frame.update.zVisited);
        window.animsRun=std::max(window.animsRun, frame.update.animsRun);

        size_t primitives=0;
        size_t glyphs=0;

        for(size_t id : drawn.primitives)
        {
            if(!std::binary_search(ownPrimitives.begin(), ownPrimitives.end(), id))
                ++primitives;
        }
        for(const gweni::renderer::WrittenText &text : drawn.texts)
        {
            if(!std::binary_search(ownTexts.begin(), ownTexts.end(), text.textId))
                glyphs+=text.glyphSlots;
        }
        window.primitiveSlots=std::max(window.primitiveSlots, primitives);
        window.glyphSlots=std::max(window.glyphSlots, glyphs);
        window.slots=std::max(window.slots, primitives+glyphs);
        if(primitives+glyphs != 0)
            window.uploadBytes=std::max(window.uploadBytes, drawn.counts.uploadBytes);
        window.drawCalls=std::max(window.drawCalls, drawn.counts.drawCalls);
        window.counted=drawn.counts.counted;
        ++window.frames;
    }
    return window;
}

static bool sameRange(const gweni::tools::StatsOverlay::TimeRange &a, const gweni::tools::StatsOverlay::TimeRange &b)
{
    return a.min == b.min && a.max == b.max && a.sum == b.sum && a.count == b.count;
}

// The first field two windows differ in, or empty.
static std::string windowDifference(const gweni::tools::StatsOverlay::Window &a,
    const gweni::tools::StatsOverlay::Window &b)
{
    if(a.frames != b.frames) return "frames "+std::to_string(a.frames)+" vs "+std::to_string(b.frames);
    if(!sameRange(a.frame, b.frame)) return "frame time";
    if(!sameRange(a.cpu, b.cpu)) return "cpu time";
    if(!sameRange(a.update, b.update)) return "update time";
    if(!sameRange(a.render, b.render)) return "render time";
    if(!sameRange(a.flush, b.flush)) return "flush time";

    const struct { const char *name; size_t a, b; } counts[]=
    {
        {"rendered", a.rendered, b.rendered}, {"changed", a.changed, b.changed},
        {"layout passes", a.layoutPasses, b.layoutPasses}, {"layout entries", a.layoutEntries, b.layoutEntries},
        {"z visited", a.zVisited, b.zVisited}, {"anims", a.animsRun, b.animsRun},
        {"primitive slots", a.primitiveSlots, b.primitiveSlots}, {"glyph slots", a.glyphSlots, b.glyphSlots},
        {"slots", a.slots, b.slots}, {"upload bytes", a.uploadBytes, b.uploadBytes},
        {"draws", a.drawCalls, b.drawCalls},
    };

    for(const auto &count : counts)
    {
        if(count.a != count.b)
            return std::string(count.name)+" "+std::to_string(count.a)+" vs "+std::to_string(count.b);
    }
    return a.counted == b.counted ? "" : "counted";
}

static size_t liveTextIds();
static std::vector<uint8_t> framePixels(const Session &s);
static size_t differencesIn(const Session &s, const std::vector<uint8_t> &a,
    const std::vector<uint8_t> &b, const gweni::Rect &rect);
static size_t differencesOutsideRects(const Session &s, const std::vector<uint8_t> &a,
    const std::vector<uint8_t> &b, const std::vector<gweni::Rect> &rects);
static int highestZOutside(gweni::controls::Base *control, gweni::controls::Base *box);
static bool insideRect(const gweni::Rect &inner, const gweni::Rect &outer);
static std::string rectText(const gweni::Rect &r);

static bool runStatsOverlayScenario(Session &s, const std::string &shotDir, bool &passed)
{
    using gweni::tools::StatsOverlay;

    gweni::controls::Base *control=addControl(s, g_dockedControlName);

    if(!control)
        return false;
    s.label="stats-overlay";

    // an app control where the overlay goes, in the canvas's bottom-right corner
    gweni::controls::Button *target=s.canvas->newChild<gweni::controls::Button>("under-overlay");

    target->setText("Under");
    target->setIsToggle(true);
    target->setBounds(s.size.x-StatsOverlay::Inset-120, s.size.y-StatsOverlay::Inset-60, 80, 24);
    renderFrames(s, std::max(s.frames, 5));

    // Demo leaves canvas-level Menus behind when it goes: the overlay's ids
    // are counted from here to its close
    const size_t idsBefore=livePrimitiveIds();
    const size_t textIdsBefore=liveTextIds();

    // F10, as the window's key callback would deliver it
    glfwKey(s, GLFW_KEY_F10, GLFW_PRESS, 0);
    glfwKey(s, GLFW_KEY_F10, GLFW_RELEASE, 0);

    StatsOverlay *overlay=StatsOverlay::find(s.canvas);

    if(!overlay)
    {
        std::cerr<<"ControlTest: stats-overlay: F10 opened no stats overlay\n";
        return false;
    }

    // The demo shell's Stats button and its status bar: the overlay opened by
    // F10 shows on the button, and sits Inset above the bar, right-aligned.
    gweni::Demo *shell=gweni::gweni_cast<gweni::Demo>(control);
    gweni::controls::Button *statsButton=shell ? shell->statsButton() : nullptr;
    gweni::controls::Base *statusBar=shell ? shell->statusBar() : nullptr;

    if(!statsButton || !statusBar)
    {
        std::cerr<<"ControlTest: stats-overlay: the shell has no Stats button\n";
        return false;
    }

    const bool buttonMade=statsButton->getText() == "Stats" && statsButton->isToggle()
        && statsButton->getToolTipText() == "Stats overlay (F10)"
        && statsButton->getParent() == statusBar && statsButton->getDock() == gweni::DockPosition::Right;
    bool buttonOnF10=false;
    bool aboveBar=false;
    std::string overlayPlace;
    auto barTop=[&]() { return statusBar->localPosToCanvas().y; };
    auto rectOf=[](gweni::controls::Base *c)
    {
        const gweni::Point at=c->localPosToCanvas();
        return gweni::Rect(at.x, at.y, c->getWidth(), c->getHeight());
    };
    auto overlaps=[](const gweni::Rect &a, const gweni::Rect &b)
    {
        return a.x < b.x+b.w && b.x < a.x+a.w && a.y < b.y+b.h && b.y < a.y+a.h;
    };

    std::vector<OverlayFrame> frames;
    auto frame=[&]()
    {
        renderFrames(s, 1);

        OverlayFrame record;

        record.previous=s.canvas->previousFrameStats();
        record.counts=s.renderer->frameStats();
        record.primitives=s.renderer->writtenPrimitiveIds();
        record.texts=s.renderer->writtenTextIds();
        record.refreshes=overlay->refreshes();
        record.shown=overlay->shownWindow();
        frames.push_back(record);
    };
    // frames until the next refresh has run; the index of its frame
    auto untilRefresh=[&]()
    {
        const size_t refreshes=overlay->refreshes();

        for(int i=0; i < 100 && overlay->refreshes() == refreshes; ++i)
            frame();
        return frames.size()-1;
    };

    std::vector<size_t> ownPrimitives;
    std::vector<size_t> ownTexts;

    // past the windows its opening is in
    frame();
    buttonOnF10=statsButton->getToggleState();
    {
        const gweni::Rect placed=rectOf(overlay);

        aboveBar=placed.y+placed.h == barTop()-StatsOverlay::Inset
            && placed.x+placed.w == s.canvas->getWidth()-StatsOverlay::Inset
            && !overlaps(placed, rectOf(statsButton));
        overlayPlace="overlay ("+std::to_string(placed.x)+", "+std::to_string(placed.y)+", "
            +std::to_string(placed.w)+", "+std::to_string(placed.h)+"), bar top "+std::to_string(barTop());
    }
    untilRefresh();
    size_t start=untilRefresh();
    overlay->ownIds(ownPrimitives, ownTexts);
    std::sort(ownPrimitives.begin(), ownPrimitives.end());
    std::sort(ownTexts.begin(), ownTexts.end());

    // two whole windows, every frame checked
    int quietFrames=0;
    size_t quietWritten=0;
    int refreshFrames=0;
    size_t foreignWritten=0;
    std::string shownDiff;

    for(int window=0; window < 2; ++window)
    {
        const size_t refresh=untilRefresh();

        for(size_t k=start+1; k <= refresh; ++k)
        {
            const OverlayFrame &f=frames[k];

            if(k < refresh)
            {
                ++quietFrames;
                quietWritten+=f.counts.primitiveSlotsWritten+f.counts.textSlotsWritten;
                continue;
            }
            ++refreshFrames;
            for(size_t id : f.primitives)
            {
                if(!std::binary_search(ownPrimitives.begin(), ownPrimitives.end(), id))
                    ++foreignWritten;
            }
            for(const gweni::renderer::WrittenText &text : f.texts)
            {
                if(!std::binary_search(ownTexts.begin(), ownTexts.end(), text.textId))
                    foreignWritten+=text.glyphSlots;
            }
        }

        const std::string diff=windowDifference(frames[refresh].shown,
            expectedWindow(frames, start, refresh, ownPrimitives, ownTexts));

        if(shownDiff.empty() && !diff.empty())
            shownDiff="window "+std::to_string(window)+": "+diff;
        start=refresh;
    }

    const bool shot=shoot(s, shotDir+"/stats-overlay.png");

    // a hover in the app, in the middle of a window: on the next refresh, then 0
    const gweni::Point at=overlay->localPosToCanvas();
    const gweni::Rect overlayRect(at.x, at.y, overlay->getWidth(), overlay->getHeight());
    gweni::controls::Base *entry=findListEntry(control, overlayRect);
    size_t hoverSlots=0;
    StatsOverlay::Window hoverShown;
    StatsOverlay::Window backShown;
    std::string hoverDiff;

    if(entry)
    {
        const gweni::Point origin=entry->localPosToCanvas();

        for(int i=0; i < 4; ++i)
            frame();
        moveMouse(s, origin.x+entry->getWidth()/2, origin.y+entry->getHeight()/2);
        frame();
        hoverSlots=frames.back().counts.primitiveSlotsWritten+frames.back().counts.textSlotsWritten;

        const size_t refresh=untilRefresh();

        hoverShown=overlay->shownWindow();
        hoverDiff=windowDifference(hoverShown, expectedWindow(frames, start, refresh, ownPrimitives, ownTexts));
        untilRefresh();
        backShown=overlay->shownWindow();
    }

    // A name cell's tooltip: from a point under the "interval" row's first
    // value, which passes through to the app, onto the name. After the
    // canvas's delay the box shows the name's tooltip, as the canvas's last
    // child, numbered above the overlay and inside the canvas, clear of the
    // name; the frame that shows it writes the overlay's and the box's slots
    // alone; the refresh after it shows 0 (none of it is the app's), and the
    // frames until the next write nothing; and back on the point, the frame
    // before the hover is given back, but for the plots, whose history has
    // moved on.
    gweni::controls::Label *intervalName=nullptr;

    for(gweni::controls::Label *name : overlay->nameCells())
    {
        if(name->getText() == "interval")
            intervalName=name;
    }

    bool nameValuePasses=false;
    bool nameHovered=false;
    bool nameTipShown=false;
    std::string nameTipPlace;
    size_t nameTipForeign=0;
    size_t nameTipDrawn=0;
    StatsOverlay::Window nameTipWindow;
    bool nameTipZero=false;
    size_t nameTipQuiet=0;
    bool nameTipHidden=false;
    size_t nameTipBack=0;

    if(intervalName)
    {
        const gweni::Rect nameRect=rectOf(intervalName);
        const gweni::Point onName(nameRect.x+nameRect.w/2, nameRect.y+nameRect.h/2);
        const gweni::Point onValue(nameRect.x+nameRect.w+20, onName.y);

        nameValuePasses=overlay->getControlAt(onValue.x-at.x, onValue.y-at.y) == nullptr
            && overlay->getControlAt(onName.x-at.x, onName.y-at.y) == intervalName;

        // settled on the value, the overlay showing it
        moveMouse(s, onValue.x, onValue.y);
        frame();
        for(int i=0; i < 3; ++i)
            untilRefresh();

        const std::vector<uint8_t> before=framePixels(s);

        moveMouse(s, onName.x, onName.y);
        frame();
        nameHovered=gweni::HoveredControl == intervalName;
        for(int i=0; i < 120 && !s.canvas->toolTipShown(); ++i)
            frame();

        gweni::controls::ToolTip *box=s.canvas->toolTipControl();

        if(box && s.canvas->toolTipShown() && s.canvas->toolTipOwner() == intervalName)
        {
            const gweni::Rect boxRect=rectOf(box);

            nameTipShown=s.canvas->getChildren().back() == box
                && box->getZIndex() > highestZOutside(overlay, nullptr)
                && insideRect(boxRect, gweni::Rect(0, 0, s.canvas->getWidth(), s.canvas->getHeight()))
                && !overlaps(boxRect, nameRect) && overlaps(boxRect, overlayRect)
                && box->isExcludedFromStats();
            nameTipPlace="box "+rectText(boxRect)+", name "+rectText(nameRect)+", overlay "+rectText(overlayRect)
                +", box z "+std::to_string(box->getZIndex())+", overlay's highest "
                +std::to_string(highestZOutside(overlay, nullptr));

            // the overlay's ids, the box's included now
            std::vector<size_t> tipPrimitives;
            std::vector<size_t> tipTexts;

            overlay->ownIds(tipPrimitives, tipTexts);
            std::sort(tipPrimitives.begin(), tipPrimitives.end());
            std::sort(tipTexts.begin(), tipTexts.end());
            for(size_t id : frames.back().primitives)
            {
                if(!std::binary_search(tipPrimitives.begin(), tipPrimitives.end(), id))
                    ++nameTipForeign;
            }
            for(const gweni::renderer::WrittenText &text : frames.back().texts)
            {
                if(!std::binary_search(tipTexts.begin(), tipTexts.end(), text.textId))
                    nameTipForeign+=text.glyphSlots;
            }
            nameTipDrawn=differencesIn(s, before, framePixels(s), boxRect);

            // the window the box was shown in, then the one after it
            const size_t shownAt=untilRefresh();

            nameTipWindow=overlay->shownWindow();
            nameTipZero=nameTipWindow.rendered == 0 && nameTipWindow.changed == 0
                && nameTipWindow.slots == 0 && nameTipWindow.layoutPasses == 0
                && nameTipWindow.zVisited == 0;

            const size_t next=untilRefresh();

            for(size_t k=shownAt+1; k < next; ++k)
                nameTipQuiet+=frames[k].counts.primitiveSlotsWritten+frames[k].counts.textSlotsWritten;
        }

        moveMouse(s, onValue.x, onValue.y);
        frame();
        nameTipHidden=!s.canvas->toolTipShown();
        for(int i=0; i < 3; ++i)
            untilRefresh();
        nameTipBack=differencesOutsideRects(s, before, framePixels(s),
            {rectOf(overlay->cpuPlot()), rectOf(overlay->slotsPlot())});
    }

    const bool nameTipOk=intervalName && nameValuePasses && nameHovered && nameTipShown && nameTipForeign == 0
        && nameTipDrawn > 0 && nameTipZero && nameTipQuiet == 0 && nameTipHidden && nameTipBack == 0;

    // through the overlay onto the Button under it: hovered, and a click toggles it
    const gweni::Point targetAt=target->localPosToCanvas();
    const gweni::Point targetCentre(targetAt.x+target->getWidth()/2, targetAt.y+target->getHeight()/2);
    // under the raised overlay but under none of its name cells (a value or a
    // plot), and clear of the status bar under it
    const bool underOverlay=targetCentre.x >= overlayRect.x && targetCentre.x < overlayRect.x+overlayRect.w
        && targetCentre.y >= overlayRect.y && targetCentre.y < overlayRect.y+overlayRect.h
        && overlay->getControlAt(targetCentre.x-at.x, targetCentre.y-at.y) == nullptr
        && !overlaps(rectOf(target), rectOf(statusBar));

    moveMouse(s, targetCentre.x, targetCentre.y);
    frame();

    const bool hoveredThrough=gweni::HoveredControl == target;

    clickMouse(s, targetCentre.x, targetCentre.y);
    frame();

    const bool clickedThrough=underOverlay && hoveredThrough && target->getToggleState();

    // F10 again closes it
    forgetMouse(s);
    glfwKey(s, GLFW_KEY_F10, GLFW_PRESS, 0);
    glfwKey(s, GLFW_KEY_F10, GLFW_RELEASE, 0);
    renderFrames(s, 2);

    const bool closed=StatsOverlay::find(s.canvas) == nullptr;
    const bool buttonOffF10=!statsButton->getToggleState();
    // the ToolTip box the name's tooltip made is the canvas's, and stays
    std::vector<size_t> boxPrimitives;
    std::vector<size_t> boxTexts;

    if(gweni::controls::ToolTip *box=s.canvas->toolTipControl())
        gweni::tools::appendSubtreeIds(box, boxPrimitives, boxTexts);

    const size_t idsLeft=livePrimitiveIds()-idsBefore-boxPrimitives.size();
    const size_t textIdsLeft=liveTextIds()-textIdsBefore-boxTexts.size();

    // The Stats button: a click opens it, F10 closes and opens it again, a
    // click closes it. After each, the button is on exactly while the overlay
    // is open, the overlay was told once (it never opens or closes twice), and
    // a click frame writes only the button's slots and the overlay's (and the
    // ToolTip box's, renumbered after the overlay), and
    // its layout pass enters the canvas alone (its four entries, X and Y,
    // counted as the canvas's: the overlay added or taken out is a change of
    // the canvas's children, as for F10), not the shell.
    struct OpenCounter: public gweni::event::Handler
    {
        void onOpenChanged(gweni::event::Info info) { if(info.control == canvas) ++calls; }
        gweni::controls::Canvas *canvas=nullptr;
        int calls=0;
    } counter;

    counter.canvas=s.canvas;
    StatsOverlay::onOpenChangedCaller.add(&counter, &OpenCounter::onOpenChanged);

    const gweni::Rect buttonRect=rectOf(statsButton);
    const gweni::Point buttonCentre(buttonRect.x+buttonRect.w/2, buttonRect.y+buttonRect.h/2);
    // the button's own ids, and the overlay's while one is open
    std::vector<size_t> allowedPrimitives;
    std::vector<size_t> allowedTexts;
    auto allow=[&](gweni::controls::Base *c)
    {
        if(StatsOverlay *open=gweni::gweni_cast<StatsOverlay>(c))
            open->ownIds(allowedPrimitives, allowedTexts);
        else
            gweni::tools::appendSubtreeIds(c, allowedPrimitives, allowedTexts);
        std::sort(allowedPrimitives.begin(), allowedPrimitives.end());
        std::sort(allowedTexts.begin(), allowedTexts.end());
    };
    size_t clickForeign=0;
    // the most entries one click frame's layout counted
    size_t clickLayoutEntries=0;
    size_t clickWritten=0;
    int stepsInStep=0;
    std::string stepFailure;
    // after `what`: the button follows the overlay, and the overlay was told once
    auto inStep=[&](const char *what, bool open, int callsBefore)
    {
        const bool found=StatsOverlay::find(s.canvas) != nullptr;

        if(found == open && statsButton->getToggleState() == open && counter.calls == callsBefore+1)
            ++stepsInStep;
        else if(stepFailure.empty())
            stepFailure=std::string(what)+": overlay "+(found?"open":"closed")+", button "
                +(statsButton->getToggleState()?"on":"off")+", told "
                +std::to_string(counter.calls-callsBefore)+" time(s)";
    };
    // a click on the Stats button, its frame's writes checked
    auto clickStats=[&](const char *what, bool opens)
    {
        const int callsBefore=counter.calls;

        allowedPrimitives.clear();
        allowedTexts.clear();
        allow(statsButton);
        // the canvas's ToolTip box (made by the "interval" tooltip above),
        // which the canvas keeps after its overlays: an overlay added or taken
        // out moves its z, and its text's glyphs are written again
        if(gweni::controls::ToolTip *box=s.canvas->toolTipControl())
            allow(box);
        if(StatsOverlay *open=StatsOverlay::find(s.canvas))
            allow(open);
        clickMouse(s, buttonCentre.x, buttonCentre.y);
        renderFrames(s, 1);
        if(StatsOverlay *open=StatsOverlay::find(s.canvas))
            allow(open);
        clickLayoutEntries=std::max(clickLayoutEntries, s.canvas->getLayoutStats().visited);

        const gweni::renderer::FrameCounts counts=s.renderer->frameStats();

        clickWritten+=counts.primitiveSlotsWritten+counts.textSlotsWritten;
        for(size_t id : s.renderer->writtenPrimitiveIds())
        {
            if(!std::binary_search(allowedPrimitives.begin(), allowedPrimitives.end(), id))
                ++clickForeign;
        }
        for(const gweni::renderer::WrittenText &text : s.renderer->writtenTextIds())
        {
            if(!std::binary_search(allowedTexts.begin(), allowedTexts.end(), text.textId))
                clickForeign+=text.glyphSlots;
        }
        inStep(what, opens, callsBefore);
    };
    auto pressF10=[&](const char *what, bool opens)
    {
        const int callsBefore=counter.calls;

        glfwKey(s, GLFW_KEY_F10, GLFW_PRESS, 0);
        glfwKey(s, GLFW_KEY_F10, GLFW_RELEASE, 0);
        renderFrames(s, 1);
        inStep(what, opens, callsBefore);
    };

    const size_t buttonIdsBefore=livePrimitiveIds();
    const size_t buttonTextIdsBefore=liveTextIds();

    // onto the button first: the hover is its own frame, not the click's
    moveMouse(s, buttonCentre.x, buttonCentre.y);
    renderFrames(s, 1);

    const bool buttonHovered=gweni::HoveredControl == statsButton;

    clickStats("the first click", true);
    // the overlay opened by the click is placed as F10's was
    const bool clickAboveBar=StatsOverlay::find(s.canvas)
        && rectOf(StatsOverlay::find(s.canvas)).y+rectOf(StatsOverlay::find(s.canvas)).h
            == barTop()-StatsOverlay::Inset;
    pressF10("F10 after the click", false);
    pressF10("F10 again", true);
    // the mouse never left the button: still hovered, through the overlay
    const bool hoveredOpen=gweni::HoveredControl == statsButton;
    clickStats("the second click", false);
    renderFrames(s, 2);

    const size_t buttonIdsLeft=livePrimitiveIds()-buttonIdsBefore;
    const size_t buttonTextIdsLeft=liveTextIds()-buttonTextIdsBefore;

    StatsOverlay::onOpenChangedCaller.removeHandler(&counter);
    forgetMouse(s);

    // The Inspector (F12) and the overlay (F10) open together over the
    // settled Demo: each takes every tool's slots out of the renderer's, the
    // other's too, and the ids they give back (the canvas's one record of
    // them, Canvas::excludedSlots()). After every frame k this harness counts
    // the app's slots of frame k: those written under no id a tool held after
    // frame k-1 (so ids a tool gave back before or during k) or after k (ids
    // made in k). The Inspector open in frame k+1 must show exactly frame k's,
    // and each overlay refresh the most of its window's frames, so a frame
    // only a tool wrote in reads 0 in the other.
    using gweni::tools::Inspector;
    using gweni::tools::InspectorNode;

    renderFrames(s, 3);

    const size_t bothIdsBefore=livePrimitiveIds();
    const size_t bothTextIdsBefore=liveTextIds();

    struct BothFrame
    {
        size_t appPrimitives=0;
        size_t appGlyphs=0;
        bool toolOnly=false;     //!< a tool wrote slots in it, the app none
    };
    std::vector<BothFrame> both;
    std::vector<size_t> heldPrimitives;
    std::vector<size_t> heldTexts;
    // the tools' ids now: those of the ones open, none closing (each close
    // below is a key pressed after this was read, deleted in the next frame)
    auto toolIds=[&](std::vector<size_t> &primitives, std::vector<size_t> &texts)
    {
        primitives.clear();
        texts.clear();
        if(StatsOverlay *open=StatsOverlay::find(s.canvas))
            open->ownIds(primitives, texts);
        if(Inspector *open=Inspector::find(s.canvas))
            open->ownIds(primitives, texts);
    };
    toolIds(heldPrimitives, heldTexts);

    // the overlay checked: its refreshes so far, the frame of its last one
    // (none since it opened: its first window is not checked)
    StatsOverlay *bothOverlay=nullptr;
    size_t bothRefreshes=0;
    long windowStart=-1;
    // what the checks saw
    int inspectorChecked=0;
    int inspectorToolOnly=0;
    int windowsChecked=0;
    int windowsToolOnly=0;
    std::string bothFailure;
    auto failBoth=[&](const std::string &what) { if(bothFailure.empty()) bothFailure=what; };
    auto slotsLine=[](size_t primitives, size_t glyphs)
    {
        return "slots written: "+std::to_string(primitives)+" primitive, "+std::to_string(glyphs)+" text";
    };

    auto bothFrame=[&]()
    {
        // the Inspector's refresh in this frame's update phase reads the last one's
        Inspector *inspector=Inspector::find(s.canvas);

        renderFrames(s, 1);

        std::vector<size_t> nowPrimitives;
        std::vector<size_t> nowTexts;

        toolIds(nowPrimitives, nowTexts);

        std::vector<size_t> primitives=heldPrimitives;
        std::vector<size_t> texts=heldTexts;

        primitives.insert(primitives.end(), nowPrimitives.begin(), nowPrimitives.end());
        texts.insert(texts.end(), nowTexts.begin(), nowTexts.end());
        std::sort(primitives.begin(), primitives.end());
        std::sort(texts.begin(), texts.end());

        BothFrame record;
        size_t written=0;

        for(size_t id : s.renderer->writtenPrimitiveIds())
        {
            ++written;
            if(!std::binary_search(primitives.begin(), primitives.end(), id))
                ++record.appPrimitives;
        }
        for(const gweni::renderer::WrittenText &text : s.renderer->writtenTextIds())
        {
            written+=text.glyphSlots;
            if(!std::binary_search(texts.begin(), texts.end(), text.textId))
                record.appGlyphs+=text.glyphSlots;
        }
        record.toolOnly=written > 0 && record.appPrimitives+record.appGlyphs == 0;
        heldPrimitives=nowPrimitives;
        heldTexts=nowTexts;

        const size_t k=both.size();

        if(inspector && inspector == Inspector::find(s.canvas) && k > 0)
        {
            const BothFrame &last=both[k-1];
            const std::string expect=slotsLine(last.appPrimitives, last.appGlyphs);
            const std::string shown=inspector->counterLines()[Inspector::SlotsLine];

            ++inspectorChecked;
            if(last.toolOnly)
                ++inspectorToolOnly;
            if(shown != expect)
                failBoth("frame "+std::to_string(k)+": the Inspector showed \""+shown+"\", the app wrote \""
                    +expect+"\" in the frame before");
        }

        StatsOverlay *overlay=StatsOverlay::find(s.canvas);

        if(overlay != bothOverlay)
        {
            bothOverlay=overlay;
            bothRefreshes=overlay ? overlay->refreshes() : 0;
            windowStart=-1;
        }
        else if(overlay && overlay->refreshes() != bothRefreshes)
        {
            bothRefreshes=overlay->refreshes();
            if(windowStart >= 0)
            {
                size_t most[2]={0, 0};
                bool toolOnly=false;

                for(size_t j=size_t(windowStart); j < k; ++j)
                {
                    most[0]=std::max(most[0], both[j].appPrimitives);
                    most[1]=std::max(most[1], both[j].appGlyphs);
                    toolOnly=toolOnly || both[j].toolOnly;
                }
                ++windowsChecked;
                if(toolOnly)
                    ++windowsToolOnly;

                const StatsOverlay::Window &shown=overlay->shownWindow();

                if(shown.primitiveSlots != most[0] || shown.glyphSlots != most[1])
                    failBoth("frame "+std::to_string(k)+": the overlay showed "+std::to_string(shown.primitiveSlots)
                        +" primitive and "+std::to_string(shown.glyphSlots)+" glyph slots, its window's app most "
                        +std::to_string(most[0])+" and "+std::to_string(most[1]));
            }
            windowStart=long(k);
        }
        both.push_back(record);
    };
    // frames until the overlay's next refresh
    auto bothRefresh=[&]()
    {
        StatsOverlay *overlay=StatsOverlay::find(s.canvas);
        const size_t refreshes=overlay ? overlay->refreshes() : 0;

        for(int i=0; i < 100 && overlay && overlay->refreshes() == refreshes; ++i)
            bothFrame();
    };
    auto key=[&](int glfwKeyCode)
    {
        glfwKey(s, glfwKeyCode, GLFW_PRESS, 0);
        glfwKey(s, glfwKeyCode, GLFW_RELEASE, 0);
    };

    // the overlay alone: its ids row
    key(GLFW_KEY_F10);
    bothFrame();
    bothRefresh();
    bothRefresh();

    const size_t aloneIds=StatsOverlay::find(s.canvas) ? StatsOverlay::find(s.canvas)->shownPrimitiveIds() : 0;
    const size_t aloneTextIds=StatsOverlay::find(s.canvas) ? StatsOverlay::find(s.canvas)->shownTextIds() : 0;

    // the Inspector too: its opening frames are the Inspector's writes alone
    key(GLFW_KEY_F12);

    Inspector *bothInspector=Inspector::find(s.canvas);

    bothFrame();
    bothRefresh();
    bothRefresh();

    StatsOverlay *withOverlay=StatsOverlay::find(s.canvas);
    const size_t withIds=withOverlay ? withOverlay->shownPrimitiveIds() : 0;
    const size_t withTextIds=withOverlay ? withOverlay->shownTextIds() : 0;

    // the Inspector writes alone: Demo's node opened (its placeholder node
    // deleted, the ids given back), the mouse over its node and a child's
    if(InspectorNode *node=bothInspector ? bothInspector->nodeFor(control) : nullptr)
    {
        node->open();
        bothFrame();
        bothFrame();
        moveMouse(s, rectOf(node->GetButton()).x+8, rectOf(node->GetButton()).y+rectOf(node->GetButton()).h/2);
        bothFrame();
        for(gweni::controls::Base *child : control->getActualChildren())
        {
            if(InspectorNode *childNode=bothInspector->nodeFor(child))
            {
                const gweni::Rect title=rectOf(childNode->GetButton());

                moveMouse(s, title.x+8, title.y+title.h/2);
                bothFrame();
                break;
            }
        }
        // and closed again
        node->close();
        bothFrame();
        forgetMouse(s);
        bothFrame();
    }
    else
        failBoth("the Inspector has no node for the demo");
    bothRefresh();
    bothRefresh();

    // a hover in the app, in the middle of a window: in the Inspector on the
    // next frame and the overlay on its next refresh (both by the checks
    // above), then 0 in both
    bool bothHoverShown=false;
    bool bothHoverBack=false;

    if(bothInspector && withOverlay)
    {
        const gweni::Rect inspectorRect=rectOf(bothInspector);
        const gweni::Rect overlayRect=rectOf(withOverlay);
        gweni::controls::Base *entry=findListEntry(control, inspectorRect);

        if(entry && !overlaps(rectOf(entry), overlayRect))
        {
            const gweni::Rect at=rectOf(entry);

            for(int i=0; i < 4; ++i)
                bothFrame();
            moveMouse(s, at.x+at.w/2, at.y+at.h/2);
            bothFrame();

            const BothFrame hover=both.back();

            bothFrame();

            const std::string inspectorHover=bothInspector->counterLines()[Inspector::SlotsLine];

            bothRefresh();

            const StatsOverlay::Window overlayHover=withOverlay->shownWindow();

            bothRefresh();
            bothHoverShown=hover.appPrimitives+hover.appGlyphs > 0
                && inspectorHover == slotsLine(hover.appPrimitives, hover.appGlyphs)
                && overlayHover.slots > 0;
            bothHoverBack=withOverlay->shownWindow().slots == 0
                && bothInspector->counterLines()[Inspector::SlotsLine] == slotsLine(0, 0);
            forgetMouse(s);
            bothRefresh();
            bothRefresh();
        }
    }

    // the overlay closed and opened again under the Inspector, then the
    // Inspector closed under the overlay, then the overlay closed
    key(GLFW_KEY_F10);
    for(int i=0; i < 4; ++i)
        bothFrame();
    key(GLFW_KEY_F10);
    bothFrame();
    bothRefresh();
    bothRefresh();
    key(GLFW_KEY_F12);
    bothFrame();
    bothRefresh();
    bothRefresh();

    const bool bothInspectorClosed=Inspector::find(s.canvas) == nullptr;

    key(GLFW_KEY_F10);
    renderFrames(s, 2);

    const bool bothClosed=bothInspectorClosed && StatsOverlay::find(s.canvas) == nullptr;
    const size_t bothIdsLeft=livePrimitiveIds()-bothIdsBefore;
    const size_t bothTextIdsLeft=liveTextIds()-bothTextIdsBefore;

    target->delayedDelete();
    dropControl(s, control);
    renderFrames(s, 1);

    const bool quiet=quietFrames > 0 && quietWritten == 0;
    const bool ownOnly=refreshFrames == 2 && foreignWritten == 0;
    const bool shownMatch=shownDiff.empty();
    const bool hoverShownOk=entry && hoverSlots > 0 && hoverDiff.empty() && hoverShown.rendered > 0
        && hoverShown.slots > 0;
    const bool hoverBack=entry && backShown.rendered == 0 && backShown.slots == 0;
    const bool idsBack=idsLeft == 0 && textIdsLeft == 0;

    const bool buttonSynced=buttonMade && buttonOnF10 && buttonOffF10;
    const bool buttonSteps=buttonHovered && hoveredOpen && stepsInStep == 4 && clickAboveBar;
    const bool clickOwnOnly=clickForeign == 0 && clickWritten > 0 && clickLayoutEntries <= 4;
    const bool buttonIdsBack=buttonIdsLeft == 0 && buttonTextIdsLeft == 0;

    const bool bothIdsRow=withOverlay && aloneIds == withIds && aloneTextIds == withTextIds;
    // each check ran, on frames only a tool wrote in too
    const bool bothCounted=bothFailure.empty() && inspectorToolOnly > 0 && windowsToolOnly > 0;
    const bool bothIdsBack=bothClosed && bothIdsLeft == 0 && bothTextIdsLeft == 0;

    passed=quiet && ownOnly && shownMatch && hoverShownOk && hoverBack && nameTipOk && clickedThrough && closed && idsBack
        && aboveBar && buttonSynced && buttonSteps && clickOwnOnly && buttonIdsBack
        && bothCounted && bothIdsRow && bothHoverShown && bothHoverBack && bothIdsBack;

    std::cout<<"stats-overlay quiet_frames="<<quietFrames<<" quiet_written="<<quietWritten
             <<" refresh_frames="<<refreshFrames<<" foreign_written="<<foreignWritten
             <<" shown_match="<<(shownMatch?"yes":"no")<<" hover_slots="<<hoverSlots
             <<" hover_shown="<<(hoverShownOk?"yes":"no")<<" hover_back="<<(hoverBack?"yes":"no")
             <<" name_tip_drawn="<<nameTipDrawn<<" name_tip_foreign="<<nameTipForeign
             <<" name_tip_window="<<nameTipWindow.rendered<<"/"<<nameTipWindow.changed<<"/"<<nameTipWindow.slots
             <<" name_tip_back="<<nameTipBack<<" name_tip="<<(nameTipOk?"yes":"no")
             <<" click_through="<<(clickedThrough?"yes":"no")<<" closed="<<(closed?"yes":"no")
             <<" ids_left="<<idsLeft<<" text_ids_left="<<textIdsLeft<<" ids_back="<<(idsBack?"yes":"no")
             <<" above_bar="<<(aboveBar?"yes":"no")<<" button_synced="<<(buttonSynced?"yes":"no")
             <<" button_steps="<<stepsInStep<<" click_written="<<clickWritten<<" click_foreign="<<clickForeign
             <<" click_layout_entries="<<clickLayoutEntries
             <<" button_ids_back="<<(buttonIdsBack?"yes":"no")
             <<" both_inspector_frames="<<inspectorChecked<<" both_inspector_tool_only="<<inspectorToolOnly
             <<" both_windows="<<windowsChecked<<" both_windows_tool_only="<<windowsToolOnly
             <<" both_counted="<<(bothCounted?"yes":"no")<<" both_ids_row="<<(bothIdsRow?"yes":"no")
             <<" both_hover_shown="<<(bothHoverShown?"yes":"no")<<" both_hover_back="<<(bothHoverBack?"yes":"no")
             <<" both_ids_back="<<(bothIdsBack?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: stats-overlay: "
                 <<(!quiet ? "a frame between two refreshes wrote "+std::to_string(quietWritten)+" slots"
                     : !ownOnly ? "a refresh frame wrote "+std::to_string(foreignWritten)+" slots not its own"
                     : !shownMatch ? "it showed other counts than its window's ("+shownDiff+")"
                     : !hoverShownOk ? "the hover was not shown on the next refresh ("
                        +(hoverDiff.empty() ? std::string("no entry or nothing shown") : hoverDiff)+")"
                     : !hoverBack ? "it did not show 0 again on the refresh after the hover"
                     : !nameTipOk ? (!intervalName ? std::string("it has no \"interval\" name cell")
                        : !nameValuePasses ? std::string("its \"interval\" name is not hit, or its value is")
                        : !nameHovered ? std::string("the mouse on \"interval\" did not hover it")
                        : !nameTipShown ? "the \"interval\" tooltip is not the canvas's last child above the"
                            " overlay, inside the canvas and clear of the name, excluded from the counts ("
                            +(nameTipPlace.empty() ? std::string("not shown") : nameTipPlace)+")"
                        : nameTipForeign != 0 ? "the frame that showed the \"interval\" tooltip wrote "
                            +std::to_string(nameTipForeign)+" slots not the overlay's or the box's"
                        : nameTipDrawn == 0 ? std::string("the \"interval\" tooltip drew nothing")
                        : !nameTipZero ? "the refresh after the \"interval\" tooltip showed it (rendered "
                            +std::to_string(nameTipWindow.rendered)+", changed "+std::to_string(nameTipWindow.changed)
                            +", slots "+std::to_string(nameTipWindow.slots)+", passes "
                            +std::to_string(nameTipWindow.layoutPasses)+", z "+std::to_string(nameTipWindow.zVisited)+")"
                        : nameTipQuiet != 0 ? "a frame with the \"interval\" tooltip shown wrote "
                            +std::to_string(nameTipQuiet)+" slots"
                        : !nameTipHidden ? std::string("moving off \"interval\" did not hide its tooltip")
                        : "moving off \"interval\" changed "+std::to_string(nameTipBack)
                            +" pixels from the frame before its hover, outside the plots")
                     : !clickedThrough ? "the mouse did not reach the Button under it (under a value or a plot)"
                     : !closed ? "F10 did not close it"
                     : !idsBack ? "it did not give its ids back"
                     : !aboveBar ? "it is not Inset above the status bar, right-aligned, clear of the Stats button ("
                        +overlayPlace+")"
                     : !buttonMade ? "the Stats button is not a toggle labelled \"Stats\" with the tooltip"
                        " \"Stats overlay (F10)\", docked Right in the status bar"
                     : !buttonSynced ? "the Stats button did not follow F10 (on "+std::string(buttonOnF10?"yes":"no")
                        +", off "+(buttonOffF10?"yes":"no")+")"
                     : !buttonSteps ? (!stepFailure.empty() ? stepFailure
                        : !buttonHovered || !hoveredOpen ? std::string("the Stats button was not hovered")
                        : std::string("the overlay a click opened is not above the status bar"))
                     : !clickOwnOnly ? "a click on the Stats button wrote "+std::to_string(clickForeign)
                        +" slots not the button's or the overlay's ("+std::to_string(clickWritten)
                        +" in all), or its layout entered more than the canvas ("
                        +std::to_string(clickLayoutEntries)+" entries)"
                     : !buttonIdsBack ? "the overlay the Stats button opened did not give its ids back"
                     : !bothCounted ? "with the Inspector open too: "+(!bothFailure.empty() ? bothFailure
                        : "no frame only a tool wrote in was checked ("+std::to_string(inspectorToolOnly)
                            +" in the Inspector, "+std::to_string(windowsToolOnly)+" overlay windows)")
                     : !bothIdsRow ? "the overlay's ids row changed with the Inspector open ("+std::to_string(aloneIds)
                        +"/"+std::to_string(aloneTextIds)+" alone, "+std::to_string(withIds)+"/"
                        +std::to_string(withTextIds)+" with it)"
                     : !bothHoverShown ? std::string("with both open, a hover was not shown in both")
                     : !bothHoverBack ? std::string("with both open, they did not show 0 again after the hover")
                     : std::string("with both open, closing them did not give their ids back"))<<"\n";
    return shot;
}

//-----------------------------------------------------------------------------
// --scenario theme: the textured skin's theme changed live, saved and loaded
//
// tools::ThemeEditor::applyToCanvas() redraws a canvas with the theme as it is
// now, so a theme changed under a laid-out Demo must draw what a Demo built
// under it draws; giving the old values back must draw the first Demo again;
// and the theme saved (TexturedSkin::saveThemeFile), the defaults restored and
// the file loaded (loadThemeFile) must draw the changed theme again, byte for
// byte (docs/architecture.md -> "The theme editor").
//-----------------------------------------------------------------------------

static bool runThemeScenario(Session &s, const std::string &shotDir, bool &passed)
{
    using gweni::skin::textured::TexturedSkin;
    using gweni::tools::ThemeEditor;

    s.freezeClock=true;

    TexturedSkin *skin=dynamic_cast<TexturedSkin *>(s.canvas->getSkin());

    if(!skin)
    {
        std::cerr<<"ControlTest: theme: the canvas's skin is not the textured skin\n";
        return false;
    }

    const int settleFrames=std::max(s.frames, 5);
    const TexturedSkin::ThemeValues defaults=skin->captureTheme();
    const gweni::renderer::SlotRenderer::FrameStats &gpu=s.renderer->getFrameStats();
    // next to the shots, where it is kept to be looked at
    const fs::path themePath=fs::path(shotDir)/"theme-saved.json";

    // several surfaces (colours, a radius, a border width), text colours and a
    // metric, as a theme editor's rows would set them
    TexturedSkin::ThemeValues changed=defaults;

    changed.surfaces.at("ButtonNormal").top=gweni::Color(0x3d, 0x59, 0xa1);
    changed.surfaces.at("ButtonNormal").bottom=gweni::Color(0x2c, 0x3d, 0x6b);
    changed.surfaces.at("CategoryList").top=gweni::Color(0x24, 0x1b, 0x2e);
    changed.surfaces.at("CategoryHeader").border=gweni::Color(0xe0, 0xaf, 0x68);
    changed.surfaces.at("TabActive").radius=2.0f;
    changed.surfaces.at("TextBox").borderWidth=2.0f;
    changed.surfaces.at("ScrollThumb").top=gweni::Color(0x9e, 0xce, 0x6a, 200);
    changed.textColors.at("LabelDefault")=gweni::Color(0xe0, 0xaf, 0x68);
    changed.textColors.at("ButtonNormal")=gweni::Color(0x7d, 0xcf, 0xff);
    changed.metrics.at("ScrollBarThickness")=16;

    gweni::controls::Base *control=addControl(s, g_dockedControlName);

    if(!control)
        return false;
    s.label="theme";
    renderFrames(s, settleFrames);

    std::vector<unsigned char> original, edited, fresh, reverted, loaded;

    s.renderer->readPixels(s.pixels.x, s.pixels.y, original);
    bool shot=shoot(s, shotDir+"/theme-00-default.png");

    // a change applied live: one frame draws it, the next writes nothing
    skin->applyTheme(changed);
    ThemeEditor::applyToCanvas(s.canvas, true);
    renderFrames(s, 1);
    const size_t editPrim=gpu.primitiveSlotsWritten;
    const size_t editText=gpu.textSlotsWritten;
    renderFrames(s, 1);
    const size_t settledSlots=gpu.primitiveSlotsWritten+gpu.textSlotsWritten;
    s.renderer->readPixels(s.pixels.x, s.pixels.y, edited);
    shot=shoot(s, shotDir+"/theme-01-edited.png") && shot;

    // the theme saved, as the editor's Save writes it
    const bool saved=ensureDirectory(shotDir) && skin->saveThemeFile(themePath.string());

    // a Demo built under the changed theme on a new canvas
    dropControl(s, control);
    replaceCanvas(s);
    control=addControl(s, g_dockedControlName);
    if(!control)
        return false;
    renderFrames(s, settleFrames);
    s.renderer->readPixels(s.pixels.x, s.pixels.y, fresh);
    shot=shoot(s, shotDir+"/theme-02-fresh.png") && shot;

    // the defaults given back live: the Demo built under them
    skin->applyTheme(defaults);
    ThemeEditor::applyToCanvas(s.canvas, true);
    renderFrames(s, settleFrames);
    const bool defaultsBack=skin->captureTheme() == defaults;
    s.renderer->readPixels(s.pixels.x, s.pixels.y, reverted);
    shot=shoot(s, shotDir+"/theme-03-reverted.png") && shot;

    // the saved file loaded live: the changed theme again
    const bool loadedFile=saved && skin->loadThemeFile(themePath.string());
    const bool valuesBack=skin->captureTheme() == changed;

    ThemeEditor::applyToCanvas(s.canvas, true);
    renderFrames(s, settleFrames);
    s.renderer->readPixels(s.pixels.x, s.pixels.y, loaded);
    shot=shoot(s, shotDir+"/theme-04-loaded.png") && shot;

    skin->applyTheme(defaults);
    ThemeEditor::applyToCanvas(s.canvas, true);
    dropControl(s, control);
    renderFrames(s, 1);

    auto same=[](const std::vector<unsigned char> &a, const std::vector<unsigned char> &b,
        const char *what)
    {
        if(a == b)
            return true;

        size_t differing=0;

        for(size_t i=0; i+3 < a.size() && i+3 < b.size(); i+=4)
            if(std::memcmp(&a[i], &b[i], 4) != 0)
                ++differing;
        std::cerr<<"ControlTest: theme: "<<what<<" in "<<differing<<" pixels\n";
        return false;
    };

    const bool editShows=edited != original;
    const bool liveMatchesFresh=same(edited, fresh, "the theme changed live differs from a Demo built under it");
    const bool revertMatches=same(reverted, original, "the defaults given back differ from the first Demo");
    const bool loadMatches=same(loaded, edited, "the theme loaded from the file differs from the one saved");
    const bool settles=settledSlots == 0;

    passed=editShows && liveMatchesFresh && revertMatches && loadMatches && settles
        && saved && loadedFile && valuesBack && defaultsBack;

    std::cout<<"theme edit_prim_slots="<<editPrim<<" edit_text_slots="<<editText
             <<" settled_slots="<<settledSlots<<" edit_shows="<<(editShows?"yes":"no")
             <<" live_matches_fresh="<<(liveMatchesFresh?"yes":"no")
             <<" revert_matches="<<(revertMatches?"yes":"no")
             <<" saved="<<(saved?"yes":"no")<<" loaded="<<(loadedFile?"yes":"no")
             <<" values_back="<<(valuesBack?"yes":"no")<<" defaults_back="<<(defaultsBack?"yes":"no")
             <<" load_matches="<<(loadMatches?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: theme: "
                 <<(!editShows ? "the changed theme drew nothing new"
                     : !settles ? "the frame after the change still wrote slots"
                     : !saved ? "the theme could not be saved"
                     : !loadedFile ? "the saved theme could not be loaded"
                     : !valuesBack ? "the loaded theme's values are not the saved ones"
                     : !defaultsBack ? "the defaults did not come back"
                     : "a capture did not match (above)")<<"\n";
    return shot;
}

//-----------------------------------------------------------------------------
// --scenario plot-lines: PlotLinesDemo's two series changed and hovered
//
// One primitive slot per segment, redrawn only where a segment's endpoints or
// neighbours moved: a sample whose point moves writes the four segments around
// it, the two it ends and the two drawn with it as their neighbour (at most;
// the renderer counts a slot only if its bytes differ), one whose point
// stays (clamped to the range's edge) nothing, a rescale every segment that
// moved, and a push() onto a scrolling series no more than every segment,
// with no layout. A hover has no look of its own: it writes nothing and sets
// the nearest sample's tooltip.
//-----------------------------------------------------------------------------

static bool runPlotLinesScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    const size_t idsBefore=livePrimitiveIds();
    gweni::controls::Base *control=addControl(s, "PlotLinesDemo");
    auto *demo=gweni::gweni_cast<gweni::PlotLinesDemo>(control);

    if(!demo || !demo->series() || !demo->captioned())
    {
        std::cerr<<"ControlTest: plot-lines: PlotLinesDemo has no series\n";
        return false;
    }

    gweni::controls::PlotLines *plot=demo->series();
    gweni::controls::PlotLines *frames=demo->captioned();

    s.label="plot-lines";
    renderFrames(s, s.frames);

    // read now: both series go with the demo before the checks
    const size_t segments=plot->getSegmentCount();
    const size_t frameSegments=frames->getSegmentCount();
    PlotStep idle=plotFrame(s);
    size_t idleSlots=idle.primSlots+idle.textSlots;
    size_t idleChanged=idle.changed;

    auto settle=[&]()
    {
        const PlotStep step=plotFrame(s);

        idleSlots+=step.primSlots+step.textSlots;
        idleChanged+=step.changed;
    };

    // sample 20 of the sine, inside the automatic range, moved to 0
    const size_t sample=20;
    const float yBefore=plot->getSamplePoint(sample).y;

    plot->setValue(sample, 0.0f);

    const bool moved=plot->getSamplePoint(sample).y != yBefore;
    const PlotStep change=plotFrame(s);
    settle();

    // a frame time past the fixed range raised further: still on its top edge
    size_t past=0;

    while((past < frames->getValueCount()) && !(frames->getValues()[past] > frames->getScaleMax()))
        ++past;

    const bool hasPast=past < frames->getValueCount();
    PlotStep keep;

    if(hasPast)
    {
        frames->setValue(past, frames->getValues()[past]*2.0f);
        keep=plotFrame(s);
        settle();
    }

    // the sine's highest value doubled: the automatic range rescales it
    size_t highest=0;
    for(size_t i=1; i < plot->getValueCount(); ++i)
    {
        if(plot->getValues()[i] > plot->getValues()[highest])
            highest=i;
    }

    const float top=plot->getValues()[highest];

    plot->setValue(highest, top*2.0f);
    const PlotStep rescale=plotFrame(s);
    settle();
    plot->setValue(highest, top);
    renderFrames(s, 1);
    settle();

    // a new frame time pushed onto the scrolling series: no layout
    frames->push(21.5f);
    const PlotStep push=plotFrame(s);
    const int pushPasses=s.canvas->getLayoutStats().passes;
    settle();

    // the mouse near sample 10 of the sine
    const gweni::Rect area=plot->getPlotBounds();
    const gweni::Point origin=plot->localPosToCanvas();
    const gweni::Point at(origin.x+int(plot->getSamplePoint(10).x), origin.y+area.y+area.h/2);

    moveMouse(s, at.x, at.y);
    const PlotStep hover=plotFrame(s);
    const int hovered=plot->getHoveredSample();
    const std::string toolTip=plot->getToolTipText();
    settle();

    const bool shot=shoot(s, shotDir+"/plot-lines.png");

    forgetMouse(s);
    renderFrames(s, 1);
    settle();

    dropControl(s, control);
    renderFrames(s, 1);

    const size_t idsLeft=livePrimitiveIds()-idsBefore;

    const bool fourSegments=moved && change.primSlots >= 1 && change.primSlots <= 4 && change.textSlots == 0
        && change.changed == 1;
    const bool nothingKept=hasPast && keep.primSlots == 0 && keep.textSlots == 0 && keep.changed == 0;
    const bool rescaled=rescale.primSlots > 4 && rescale.primSlots <= segments && rescale.textSlots == 0;
    const bool pushed=push.primSlots >= 1 && push.primSlots <= frameSegments && push.textSlots == 0
        && push.changed == 1 && pushPasses == 0;
    const bool hoverNothing=hovered == 10 && hover.primSlots == 0 && hover.textSlots == 0
        && toolTip.rfind("10: ", 0) == 0;
    const bool settled=idleSlots == 0 && idleChanged == 0;
    const bool idsBack=idsLeft == 0;

    passed=fourSegments && nothingKept && rescaled && pushed && hoverNothing && settled && idsBack;

    std::cout<<"plot-lines segments="<<segments;
    printPlotStep("sample", change);
    printPlotStep("same_y", keep);
    printPlotStep("rescale", rescale);
    printPlotStep("push", push);
    printPlotStep("hover", hover);
    std::cout<<" push_layout_passes="<<pushPasses<<" idle_slots="<<idleSlots<<" idle_changed="<<idleChanged
             <<" ids_left="<<idsLeft<<" four_segments="<<(fourSegments?"yes":"no")
             <<" same_y_nothing="<<(nothingKept?"yes":"no")<<" rescaled="<<(rescaled?"yes":"no")
             <<" pushed="<<(pushed?"yes":"no")<<" hover_nothing="<<(hoverNothing?"yes":"no")
             <<" settled="<<(settled?"yes":"no")<<" ids_back="<<(idsBack?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: plot-lines: "
                 <<(!fourSegments ? "a moved sample did not write at most the four segments around it"
                     : !nothingKept ? "a sample that kept its point wrote or changed something"
                     : !rescaled ? "a rescale did not redraw the segments that moved"
                     : !pushed ? "a push wrote more than the series' segments, or laid out"
                     : !hoverNothing ? "the hover wrote slots or set the wrong tooltip"
                     : !settled ? "a settled frame wrote slots or changed controls"
                     : "the demo did not give its primitive ids back")<<"\n";
    return shot;
}

//-----------------------------------------------------------------------------
// --scenario text-size: a Label's font size changed at runtime
//
// Both sizes are at or above OpenGLCore's SDF threshold (by default), so both are
// drawn from the face's one SDF atlas: the change has to rasterise and upload no
// glyph, and rewrite only that Label's own glyph slots.
//-----------------------------------------------------------------------------

static const int g_textSizeFrom=24;
static const int g_textSizeTo=36;
static const int g_textSizeKept=48;

static std::vector<uint8_t> regionPixels(const Session &s, gweni::controls::Base *control);

static gweni::Font openSansAt(int size)
{
    gweni::Font font;
    font.facename="OpenSans";
    font.size=static_cast<float>(size);
    return font;
}

static bool runTextSizeScenario(Session &s, const std::string &shotDir, bool &passed)
{
    const char *const sample="Sphinx gyp 0123456789 &?!";

    forgetMouse(s);
    g_clockSeconds=0.0f;
    s.frame=0;
    restartFrameCounts();
    s.label="text-size";

    // a Label in the skin's default font beside it, whose run must not be touched
    gweni::controls::Label *other=s.canvas->newChild<gweni::controls::Label>();
    other->setAlignment(gweni::Alignment::Absolute);
    other->setText("unchanged 11 px label");
    other->sizeToContents();
    other->setPos(20, 8);

    // Sizes are set through Label::setFont(face, size, bold), which frees the
    // Label's previous font: each size of the face shares the one SDF atlas, so
    // that must leave the other sizes drawing.
    gweni::controls::Label *label=s.canvas->newChild<gweni::controls::Label>();
    label->setAlignment(gweni::Alignment::Absolute);
    label->setFont("OpenSans", g_textSizeFrom, false);
    label->setText(sample);
    label->sizeToContents();
    label->setPos(20, 30);

    // another size of the same face below it, which neither the change nor the
    // changed Label's deletion may touch
    gweni::controls::Label *kept=s.canvas->newChild<gweni::controls::Label>();
    kept->setAlignment(gweni::Alignment::Absolute);
    kept->setFont("OpenSans", g_textSizeKept, false);
    kept->setText(sample);
    kept->sizeToContents();
    kept->setPos(20, 100);

    // the extra frame leaves every glyph the first frames needed on the GPU
    renderFrames(s, s.frames+1);

    const std::string fromShot=shotDir+"/text-size-"+std::to_string(g_textSizeFrom)+".png";
    const std::string toShot=shotDir+"/text-size-"+std::to_string(g_textSizeTo)+".png";
    const std::string deletedShot=shotDir+"/text-size-deleted.png";
    std::vector<uint8_t> fromPixels, toPixels;

    bool ok=shoot(s, fromShot);
    s.renderer->readPixels(s.size.x, s.size.y, fromPixels);

    const gweni::Point fromSize=label->getSize();
    const std::vector<uint8_t> keptPixels=regionPixels(s, kept);
    const unsigned int atlas=s.renderer->getFontAtlasHandle(openSansAt(g_textSizeKept));

    label->setFont("OpenSans", g_textSizeTo, false);
    label->sizeToContents();

    // The change and however many frames it takes to settle: the most glyph slots
    // any one of them writes, and everything they upload to the font atlases.
    size_t atlasUploadCalls=0, atlasUploadBytes=0;
    size_t textSlotsWritten=0, primSlotsWritten=0, uploadBytes=0;

    for(int i=0; i<s.frames; ++i)
    {
        renderFrames(s, 1);

        const gweni::renderer::SlotRenderer::FrameStats &gpu=s.renderer->getFrameStats();
        atlasUploadCalls+=gpu.atlasUploadCalls;
        atlasUploadBytes+=gpu.atlasUploadBytes;
        textSlotsWritten=std::max(textSlotsWritten, gpu.textSlotsWritten);
        primSlotsWritten+=gpu.primitiveSlotsWritten;
        uploadBytes+=gpu.uploadBytes;
    }

    // and one more with nothing to do, which has to write nothing
    renderFrames(s, 1);
    const size_t idleSlots=s.renderer->getFrameStats().textSlotsWritten
        +s.renderer->getFrameStats().primitiveSlotsWritten;

    ok=shoot(s, toShot) && ok;
    s.renderer->readPixels(s.size.x, s.size.y, toPixels);

    const gweni::Point toSize=label->getSize();
    const bool keptAfterChange=(regionPixels(s, kept) == keptPixels);

    // the changed Label goes, freeing its font a second time
    label->delayedDelete();
    label=nullptr;
    for(int i=0; i<s.frames; ++i)
    {
        renderFrames(s, 1);
        atlasUploadCalls+=s.renderer->getFrameStats().atlasUploadCalls;
        atlasUploadBytes+=s.renderer->getFrameStats().atlasUploadBytes;
    }
    ok=shoot(s, deletedShot) && ok;

    const bool keptAfterDelete=(regionPixels(s, kept) == keptPixels);
    const bool sameAtlas=(atlas != 0) && (s.renderer->getFontAtlasHandle(openSansAt(g_textSizeKept)) == atlas);

    // the sample is one glyph slot per codepoint, spaces included
    const size_t glyphs=std::strlen(sample);

    const bool sdf=(s.renderer->getTextMode(openSansAt(g_textSizeFrom)) == gweni::renderer::TextMode::Sdf)
        && (s.renderer->getTextMode(openSansAt(g_textSizeTo)) == gweni::renderer::TextMode::Sdf)
        && (s.renderer->getTextMode(openSansAt(g_textSizeKept)) == gweni::renderer::TextMode::Sdf);
    const bool noAtlasUpload=(atlasUploadCalls == 0);
    const bool onlyThatRun=(textSlotsWritten > 0) && (textSlotsWritten <= glyphs);
    const bool grew=(toSize.x > fromSize.x) && (toSize.y > fromSize.y);
    const bool repainted=(fromPixels != toPixels);
    const bool settled=(idleSlots == 0);
    const bool keptDrawing=keptAfterChange && keptAfterDelete && sameAtlas;

    passed=sdf && noAtlasUpload && onlyThatRun && grew && repainted && settled && keptDrawing;

    std::cout<<"text-size from="<<g_textSizeFrom<<" to="<<g_textSizeTo
             <<" sdf="<<(sdf?"yes":"no")
             <<" atlas_upload_calls="<<atlasUploadCalls<<" atlas_upload_bytes="<<atlasUploadBytes
             <<" text_slots_written="<<textSlotsWritten<<" run_glyphs="<<glyphs
             <<" prim_slots_written="<<primSlotsWritten<<" upload_bytes="<<uploadBytes
             <<" size="<<fromSize.x<<"x"<<fromSize.y<<"->"<<toSize.x<<"x"<<toSize.y
             <<" repainted="<<(repainted?"yes":"no")
             <<" idle_slots_written="<<idleSlots
             <<" kept="<<g_textSizeKept
             <<" kept_after_change="<<(keptAfterChange?"yes":"no")
             <<" kept_after_delete="<<(keptAfterDelete?"yes":"no")
             <<" same_atlas="<<(sameAtlas?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: text-size: the "<<g_textSizeFrom<<" -> "<<g_textSizeTo
                 <<" px change "<<(!sdf ? "is not in the SDF range (GWENI_SDF_TEXT_MIN?)"
                     : !noAtlasUpload ? "uploaded to a font atlas"
                     : !onlyThatRun ? "wrote glyph slots outside the Label's run"
                     : !grew ? "did not grow the Label"
                     : !repainted ? "did not repaint"
                     : !settled ? "left a frame that still writes slots"
                     : !sameAtlas ? "or the Label's deletion replaced the face's SDF atlas"
                     : "or the Label's deletion changed the "+std::to_string(g_textSizeKept)+" px Label")<<"\n";

    other->delayedDelete();
    kept->delayedDelete();
    s.canvas->updateCanvas();
    checkOnePass(s, "drop");
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario interaction: hover, press, disabled and focus repaint
//
// Every interaction state change ends in Base::redraw(), which has to mark the
// control dirty or the skin never re-picks its look. Each check compares the
// pixels inside one control's laid-out bounds, so an unrelated pixel elsewhere
// (a label colour, a caret) cannot make it pass.
//-----------------------------------------------------------------------------

// The first visible control of exactly this type, depth-first in child order,
// i.e. the first one --dump-bounds lists.
static gweni::controls::Base *findControl(gweni::controls::Base *control, const char *type, int skip=0)
{
    std::vector<gweni::controls::Base *> stack(1, control);
    while(!stack.empty())
    {
        gweni::controls::Base *at=stack.back();
        stack.pop_back();
        if(at->hidden())
            continue;
        if(std::strcmp(at->getTypeName(), type) == 0 && skip-- == 0)
            return at;

        const auto &children=at->getActualChildren();
        for(auto it=children.rbegin(); it != children.rend(); ++it)
            stack.push_back(*it);
    }
    return nullptr;
}

static gweni::Point centreOf(gweni::controls::Base *control)
{
    const gweni::Point origin=control->localPosToCanvas();
    return gweni::Point(origin.x+control->getWidth()/2, origin.y+control->getHeight()/2);
}

// The RGBA pixels inside a control's bounds (snapped to device pixels as the
// renderer snaps them), clipped to the framebuffer.
static std::vector<uint8_t> regionPixels(const Session &s, gweni::controls::Base *control)
{
    std::vector<uint8_t> frame;
    s.renderer->readPixels(s.pixels.x, s.pixels.y, frame);

    const gweni::Point origin=control->localPosToCanvas();
    const gweni::Point topLeft=devicePixels(origin, s.scale);
    const gweni::Point bottomRight=devicePixels(origin+control->getSize(), s.scale);
    const int left=std::max(0, topLeft.x);
    const int top=std::max(0, topLeft.y);
    const int right=std::min(s.pixels.x, bottomRight.x);
    const int bottom=std::min(s.pixels.y, bottomRight.y);

    std::vector<uint8_t> region;
    for(int y=top; y<bottom; ++y)
    {
        const auto row=frame.begin()+(static_cast<size_t>(y)*s.pixels.x+left)*4;
        region.insert(region.end(), row, row+static_cast<size_t>(std::max(0, right-left))*4);
    }
    return region;
}

// Renders the frames a step needs to settle and returns the control's pixels;
// a shot is written for the record.
static std::vector<uint8_t> settledRegion(Session &s, gweni::controls::Base *control,
    const std::string &shotDir, const std::string &name, bool &ok)
{
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-"+name+".png"))
        ok=false;
    return regionPixels(s, control);
}

// One named assertion: a line on stderr when it fails, and a yes/no for the
// summary line.
struct InteractionChecks
{
    std::string scenario="interaction";     // names the failures on stderr
    std::vector<std::pair<std::string, bool>> results;

    void check(const std::string &name, bool passed, const std::string &why)
    {
        results.emplace_back(name, passed);
        if(!passed)
            std::cerr<<"ControlTest: "<<scenario<<" "<<name<<" failed: "<<why<<"\n";
    }

    bool allPassed() const
    {
        for(const auto &result : results)
            if(!result.second)
                return false;
        return true;
    }
};

// A hover transition dirties the control left (here ButtonDemo, which owns no
// primitive), the Button entered and the Button's label Text. Only the two
// Button-side ones can write a primitive slot, and a button's background is a
// single RoundedPrimitive slot. 4 leaves room for a left control that is a
// button too, plus an overlay primitive each, while anything that dirties
// ancestors or the whole tree (19 controls here) is far over it.
static const size_t g_hoverBound=4;

static void checkButton(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "ButtonDemo");
    renderFrames(s, s.frames);

    gweni::controls::Base *button=findControl(demo, "Button");
    if(!button)
    {
        checks.check("button_found", false, "ButtonDemo has no visible Button");
        dropControl(s, demo);
        return;
    }

    const gweni::Point on=centreOf(button);
    const gweni::Point off(s.size.x-1, s.size.y-1);    // ButtonDemo, no button

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> idle=settledRegion(s, button, shotDir, "button-idle", ok);

    // The hover frame alone, so its counters are the transition's.
    moveMouse(s, on.x, on.y);
    renderFrames(s, 1);
    const gweni::controls::ChangeStats hoverChanges=gweni::controls::getChangeStats();
    const size_t hoverSlots=s.renderer->getFrameStats().primitiveSlotsWritten;
    const std::vector<uint8_t> hover=settledRegion(s, button, shotDir, "button-hover", ok);

    mouseDown(s, on.x, on.y, true);
    const std::vector<uint8_t> press=settledRegion(s, button, shotDir, "button-press", ok);

    // Let go off the button, so it is not clicked. The release hit-tests the
    // hover again, so no further move is needed for its idle look.
    mouseUp(s, off.x, off.y);
    const std::vector<uint8_t> released=settledRegion(s, button, shotDir, "button-released", ok);

    // A frame with no input once everything has settled must do nothing.
    renderFrames(s, 1);
    const gweni::controls::ChangeStats idleChanges=gweni::controls::getChangeStats();
    const gweni::renderer::SlotRenderer::FrameStats idleGpu=s.renderer->getFrameStats();

    button->setDisabled(true);
    const std::vector<uint8_t> disabled=settledRegion(s, button, shotDir, "button-disabled", ok);
    button->setDisabled(false);
    const std::vector<uint8_t> enabled=settledRegion(s, button, shotDir, "button-enabled", ok);

    dropControl(s, demo);

    checks.check("button_hover", hover != idle, "hovering the Button left its pixels as they were idle");
    checks.check("button_press", press != idle && press != hover,
        "pressing the Button did not change its pixels from idle and hover");
    checks.check("button_unhover", released == idle,
        "moving off and releasing did not bring the Button back to its idle look");
    checks.check("hover_bounded",
        hoverChanges.background >= 1 && hoverChanges.background <= g_hoverBound
        && hoverSlots >= 1 && hoverSlots <= g_hoverBound && hoverChanges.controls <= g_hoverBound,
        "the hover frame had controls_changed="+std::to_string(hoverChanges.controls)
        +" background="+std::to_string(hoverChanges.background)
        +" prim_slots_written="+std::to_string(hoverSlots)
        +"; want background and prim_slots_written in 1.."+std::to_string(g_hoverBound)
        +" and controls_changed at most "+std::to_string(g_hoverBound));
    checks.check("idle_zero",
        idleChanges.controls == 0 && idleGpu.primitiveSlotsWritten == 0 && idleGpu.textSlotsWritten == 0,
        "a settled frame with no input had controls_changed="+std::to_string(idleChanges.controls)
        +" prim_slots_written="+std::to_string(idleGpu.primitiveSlotsWritten)
        +" text_slots_written="+std::to_string(idleGpu.textSlotsWritten));
    checks.check("button_disabled", disabled != idle && enabled == idle,
        "setDisabled(true) did not change the Button, or setDisabled(false) did not restore it");
}

static size_t differencesOutside(const Session &s, const std::vector<uint8_t> &a,
    const std::vector<uint8_t> &b, gweni::controls::Base *control);
static std::vector<uint8_t> framePixels(const Session &s);
static void countPendingOutside(gweni::controls::Base *control,
    const std::vector<gweni::controls::Base *> &roots, size_t &count);

// ButtonDemo's toggle ("Toggle Me"): a click turns it on, which changes its
// pixels and no pixel outside it, the release leaving nothing pending outside
// it and writing its one primitive slot; on, it looks unlike off with the
// mouse moved away too, and a second click gives the whole frame before the
// first back.
static void checkToggle(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "ButtonDemo");
    renderFrames(s, s.frames);

    gweni::controls::Button *toggle=nullptr;
    for(int i=0; !toggle; ++i)
    {
        gweni::controls::Base *found=findControl(demo, "Button", i);
        if(!found)
            break;
        auto *button=gweni::gweni_cast<gweni::controls::Button>(found);
        if(button && button->isToggle())
            toggle=button;
    }
    if(!toggle || toggle->getToggleState())
    {
        checks.check("toggle_found", false, "ButtonDemo has no visible toggle Button that is off");
        dropControl(s, demo);
        return;
    }

    const gweni::Point on=centreOf(toggle);
    const gweni::Point off(s.size.x-1, s.size.y-1);    // ButtonDemo, no button

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> offLook=settledRegion(s, toggle, shotDir, "toggle-off", ok);
    const std::vector<uint8_t> offFrame=framePixels(s);

    mouseDown(s, on.x, on.y, true);
    renderFrames(s, s.frames);
    // the release alone: what it marks, then its frame's writes
    mouseUp(s, on.x, on.y);
    const bool turnedOn=toggle->getToggleState();
    size_t pending=0;
    countPendingOutside(s.canvas, {toggle}, pending);
    renderFrames(s, 1);
    const size_t releaseSlots=s.renderer->getFrameStats().primitiveSlotsWritten;

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> onLook=settledRegion(s, toggle, shotDir, "toggle-on", ok);
    const size_t onOutside=differencesOutside(s, offFrame, framePixels(s), toggle);

    clickMouse(s, on.x, on.y);
    renderFrames(s, s.frames);
    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> offAgain=settledRegion(s, toggle, shotDir, "toggle-off-again", ok);
    const bool frameBack=framePixels(s) == offFrame;
    const bool turnedOff=!toggle->getToggleState();

    dropControl(s, demo);

    checks.check("toggle_on", turnedOn && onLook != offLook && onOutside == 0,
        std::string(turnedOn ? "" : "a click did not turn the toggle on; ")
        +"on and moved off, the toggle "+(onLook != offLook ? "looks unlike off" : "looks as it did off")
        +", with "+std::to_string(onOutside)+" pixels changed outside it");
    checks.check("toggle_release_alone", pending == 0 && releaseSlots == 1,
        "the release that turned the toggle on left "+std::to_string(pending)
        +" controls pending outside it and wrote "+std::to_string(releaseSlots)
        +" primitive slots; want 0 and 1");
    checks.check("toggle_off", turnedOff && offAgain == offLook && frameBack,
        std::string(turnedOff ? "" : "a second click did not turn the toggle off; ")
        +(frameBack ? "" : "off again, the frame is not the one before the first click"));
}

static void checkFocusHandOff(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "TextBoxDemo");
    renderFrames(s, s.frames);

    gweni::controls::Base *first=findControl(demo, "TextBox");
    gweni::controls::Base *second=findControl(demo, "TextBox", 1);
    if(!first || !second)
    {
        checks.check("textbox_found", false, "TextBoxDemo has fewer than two visible TextBoxes");
        dropControl(s, demo);
        return;
    }

    const std::vector<uint8_t> unfocused=settledRegion(s, first, shotDir, "textbox-unfocused", ok);

    // The mouse is moved onto TextBox 2 before the click, so TextBox 1 has its
    // mouse-leave repaint behind it and only the focus hand-off can change it.
    const gweni::Point firstAt=centreOf(first);
    const gweni::Point secondAt=centreOf(second);
    clickMouse(s, firstAt.x, firstAt.y);
    moveMouse(s, secondAt.x, secondAt.y);
    const std::vector<uint8_t> focused=settledRegion(s, first, shotDir, "textbox-focused", ok);

    clickMouse(s, secondAt.x, secondAt.y);
    const std::vector<uint8_t> handedOff=settledRegion(s, first, shotDir, "textbox-handoff", ok);

    dropControl(s, demo);

    checks.check("textbox_focus", focused != unfocused,
        "clicking TextBox 1 did not change its pixels");
    checks.check("focus_handoff", handedOff == unfocused,
        "after clicking TextBox 2, TextBox 1 does not look unfocused again");
}

// Walks the actual children, since a TreeNode's getParent() is the control it
// was added to and not the inner panel that holds it and its siblings.
static bool findOpenNode(gweni::controls::Base *control, gweni::controls::Base *&toggle,
    gweni::controls::Base *&below)
{
    const auto &children=control->getActualChildren();
    for(auto it=children.begin(); it != children.end(); ++it)
    {
        gweni::controls::Base *child=*it;
        if(child->hidden())
            continue;

        auto next=std::next(it);
        if(std::strcmp(child->getTypeName(), "TreeNode") == 0 && next != children.end())
        {
            for(auto &&part : child->getActualChildren())
            {
                if(!part->hidden() && std::strcmp(part->getTypeName(), "OpenToggleButton") == 0)
                {
                    toggle=part;
                    below=*next;
                    return true;
                }
            }
        }
        if(findOpenNode(child, toggle, below))
            return true;
    }
    return false;
}

// The layout path (invalidate / setBounds' StateChange_Geometry) is separate
// from redraw(), and is what moves the nodes below a collapsed one.
static void checkTreeCollapse(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "TreeControlDemo");
    renderFrames(s, s.frames);

    // The first node with a toggle on show and a sibling below it.
    gweni::controls::Base *toggle=nullptr;
    gweni::controls::Base *below=nullptr;
    findOpenNode(demo, toggle, below);
    if(!below)
    {
        checks.check("tree_found", false, "TreeControlDemo has no open node with a node below it");
        dropControl(s, demo);
        return;
    }

    std::vector<uint8_t> before, after;
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tree-expanded.png"))
        ok=false;
    s.renderer->readPixels(s.pixels.x, s.pixels.y, before);
    const gweni::Rect belowBefore=below->getBounds();

    const gweni::Point at=centreOf(toggle);
    clickMouse(s, at.x, at.y);
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tree-collapsed.png"))
        ok=false;
    s.renderer->readPixels(s.pixels.x, s.pixels.y, after);
    const gweni::Rect belowAfter=below->getBounds();

    dropControl(s, demo);

    checks.check("tree_repaint", before != after, "collapsing a tree node left the frame as it was");
    checks.check("tree_layout", belowAfter.y < belowBefore.y,
        "the node below the collapsed one stayed at y="+std::to_string(belowBefore.y)
        +" (now y="+std::to_string(belowAfter.y)+")");
}

static void countPending(gweni::controls::Base *control, size_t &count);
static std::vector<uint8_t> framePixels(const Session &s);

// The pixels of two frames that differ outside both controls' bounds (snapped
// to device pixels as the renderer snaps them), and inside each.
struct SplitDifferences
{
    size_t outside=0;
    size_t inFirst=0;
    size_t inSecond=0;
};

static SplitDifferences differencesSplit(const Session &s, const std::vector<uint8_t> &a,
    const std::vector<uint8_t> &b, gweni::controls::Base *first, gweni::controls::Base *second)
{
    auto deviceRect=[&s](gweni::controls::Base *control)
    {
        const gweni::Point origin=control->localPosToCanvas();
        const gweni::Point topLeft=devicePixels(origin, s.scale);
        const gweni::Point bottomRight=devicePixels(origin+control->getSize(), s.scale);
        return gweni::Rect(topLeft.x, topLeft.y, bottomRight.x-topLeft.x, bottomRight.y-topLeft.y);
    };
    auto inside=[](const gweni::Rect &r, int x, int y)
    {
        return x >= r.x && x < r.x+r.w && y >= r.y && y < r.y+r.h;
    };
    const gweni::Rect firstRect=deviceRect(first);
    const gweni::Rect secondRect=deviceRect(second);

    SplitDifferences counts;
    for(int y=0; y<s.pixels.y; ++y)
    {
        for(int x=0; x<s.pixels.x; ++x)
        {
            const size_t at=(static_cast<size_t>(y)*s.pixels.x+x)*4;
            if(std::memcmp(&a[at], &b[at], 4) == 0)
                continue;
            if(inside(firstRect, x, y))
                ++counts.inFirst;
            else if(inside(secondRect, x, y))
                ++counts.inSecond;
            else
                ++counts.outside;
        }
    }
    return counts;
}

// SelectableDemo: a free row's hover comes and goes, a click selects it and a
// second one deselects it, back to its first look; in the group, a click on B
// while A is selected changes A's and B's pixels only, and the frame changes
// and writes only those two rows (and their labels).
static void checkSelectable(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "SelectableDemo");
    renderFrames(s, s.frames);

    // the demo's first free row starts unselected, and its group is the four
    // rows after the six free ones
    auto *row=gweni::gweni_cast<gweni::controls::Selectable>(findControl(demo, "Selectable"));
    auto *first=gweni::gweni_cast<gweni::controls::Selectable>(findControl(demo, "Selectable", 6));
    auto *second=gweni::gweni_cast<gweni::controls::Selectable>(findControl(demo, "Selectable", 8));
    if(!row || !first || !second || !first->getGroup() || first->getGroup() != second->getGroup())
    {
        checks.check("selectable_found", false,
            "SelectableDemo has no free row and two rows of one group at the expected places");
        dropControl(s, demo);
        return;
    }

    const gweni::Point on=centreOf(row);
    const gweni::Point off(s.size.x-1, s.size.y-1);    // SelectableDemo, no row

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> idle=settledRegion(s, row, shotDir, "selectable-idle", ok);

    moveMouse(s, on.x, on.y);
    renderFrames(s, 1);
    const gweni::controls::ChangeStats hoverChanges=gweni::controls::getChangeStats();
    const size_t hoverSlots=s.renderer->getFrameStats().primitiveSlotsWritten;
    const std::vector<uint8_t> hover=settledRegion(s, row, shotDir, "selectable-hover", ok);

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> unhovered=settledRegion(s, row, shotDir, "selectable-unhover", ok);

    clickMouse(s, on.x, on.y);
    moveMouse(s, off.x, off.y);
    const bool selectedByClick=row->isSelected();
    const std::vector<uint8_t> selected=settledRegion(s, row, shotDir, "selectable-selected", ok);

    clickMouse(s, on.x, on.y);
    moveMouse(s, off.x, off.y);
    const bool deselectedByClick=!row->isSelected();
    const std::vector<uint8_t> deselected=settledRegion(s, row, shotDir, "selectable-deselected", ok);

    // the group: A selected, the mouse settled on B, then the click on B alone
    const gweni::Point firstAt=centreOf(first);
    const gweni::Point secondAt=centreOf(second);
    clickMouse(s, firstAt.x, firstAt.y);
    moveMouse(s, secondAt.x, secondAt.y);
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-selectable-group-before.png"))
        ok=false;
    const std::vector<uint8_t> groupBefore=framePixels(s);
    const bool firstSelected=first->isSelected() && first->getGroup()->getSelected() == first;

    clickMouse(s, secondAt.x, secondAt.y);
    gweni::controls::resetChangeStats();
    s.canvas->updateCanvas();
    size_t pending=0, pendingFirst=0, pendingSecond=0;
    countPending(s.canvas, pending);
    countPending(first, pendingFirst);
    countPending(second, pendingSecond);
    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();
    const gweni::controls::ChangeStats groupChanges=gweni::controls::getChangeStats();
    const gweni::renderer::SlotRenderer::FrameStats groupGpu=s.renderer->getFrameStats();
    if(s.stats)
        printFrameStats(s.frame, s.label, groupChanges, groupGpu, s.canvas->getLayoutStats(),
            s.canvas->getUpdateStats());
    checkOnePass(s, std::to_string(s.frame));
    ++s.frame;

    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-selectable-group-after.png"))
        ok=false;
    const std::vector<uint8_t> groupAfter=framePixels(s);
    const SplitDifferences groupDiff=differencesSplit(s, groupBefore, groupAfter, first, second);
    const bool handedOver=!first->isSelected() && second->isSelected()
        && second->getGroup()->getSelected() == second;
    const size_t glyphs=static_cast<size_t>(first->textLength()+second->textLength());

    std::cout<<"interaction selectable_group controls_changed="<<groupChanges.controls
        <<" background="<<groupChanges.background<<" text="<<groupChanges.text
        <<" in_rows="<<(pendingFirst+pendingSecond)
        <<" prim_slots_written="<<groupGpu.primitiveSlotsWritten
        <<" text_slots_written="<<groupGpu.textSlotsWritten<<"\n";

    forgetMouse(s);
    dropControl(s, demo);

    checks.check("selectable_hover", hover != idle && unhovered == idle,
        "hovering the Selectable left its pixels as they were, or moving off did not bring them back");
    checks.check("selectable_hover_bounded",
        hoverChanges.background >= 1 && hoverChanges.background <= g_hoverBound
        && hoverSlots >= 1 && hoverSlots <= g_hoverBound && hoverChanges.controls <= g_hoverBound,
        "the hover frame had controls_changed="+std::to_string(hoverChanges.controls)
        +" background="+std::to_string(hoverChanges.background)
        +" prim_slots_written="+std::to_string(hoverSlots));
    checks.check("selectable_select", selectedByClick && selected != idle,
        "a click did not select the Selectable, or left its pixels as they were idle");
    checks.check("selectable_deselect", deselectedByClick && deselected == idle,
        "a second click did not deselect the Selectable back to its first look");
    checks.check("selectable_group_select", firstSelected && handedOver,
        "clicking B after A did not move the group's selection from A to B");
    checks.check("selectable_group_pixels",
        groupDiff.outside == 0 && groupDiff.inFirst > 0 && groupDiff.inSecond > 0,
        std::to_string(groupDiff.outside)+" pixels outside A and B changed, "
        +std::to_string(groupDiff.inFirst)+" in A, "+std::to_string(groupDiff.inSecond)+" in B");
    checks.check("selectable_group_rows_only",
        pending > 0 && pending == pendingFirst+pendingSecond
        && groupChanges.background == 2 && groupGpu.primitiveSlotsWritten <= 2
        && groupGpu.textSlotsWritten <= glyphs,
        "the selection frame had "+std::to_string(pending-pendingFirst-pendingSecond)
        +" of "+std::to_string(pending)+" changed controls outside A and B, background="
        +std::to_string(groupChanges.background)+" (want 2), prim_slots_written="
        +std::to_string(groupGpu.primitiveSlotsWritten)+" (want at most 2), text_slots_written="
        +std::to_string(groupGpu.textSlotsWritten)+" (want at most "+std::to_string(glyphs)+")");
}

// CheckBoxDemo: its first CheckBox, its CheckBoxWithLabel and its disabled
// third CheckBox, in that order. Hovering the first box changes its pixels and
// leaving gives them back; a click checks it (the tick changes them) and a
// second click gives the unchecked look back. Hovering the labelled row's label
// lights its box, as a click there toggles it. The disabled box looks unlike
// the enabled one, the same hovered and clicked, and stays unchecked. Each hover frame stays inside
// g_hoverBound.
static void checkCheckBox(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "CheckBoxDemo");
    renderFrames(s, s.frames);

    auto *box=gweni::gweni_cast<gweni::controls::CheckBox>(findControl(demo, "CheckBox"));
    auto *row=gweni::gweni_cast<gweni::controls::CheckBoxWithLabel>(findControl(demo, "CheckBoxWithLabel"));
    auto *disabled=gweni::gweni_cast<gweni::controls::CheckBox>(findControl(demo, "CheckBox", 2));
    if(!box || !row || !disabled || box->isDisabled() || !disabled->isDisabled()
        || box->isChecked() || row->checkBox()->isChecked() || disabled->isChecked())
    {
        checks.check("checkbox_found", false,
            "CheckBoxDemo has no unchecked enabled CheckBox, CheckBoxWithLabel and disabled CheckBox in that order");
        dropControl(s, demo);
        return;
    }
    gweni::controls::CheckBox *labelled=row->checkBox();

    const gweni::Point on=centreOf(box);
    const gweni::Point label=centreOf(row->label());
    const gweni::Point onDisabled=centreOf(disabled);
    const gweni::Point off(s.size.x-1, s.size.y-1);    // CheckBoxDemo, no box

    // the hover frame alone, so its counters are the transition's
    const auto hoverFrame=[&](const gweni::Point &at, gweni::controls::ChangeStats &changes, size_t &slots)
    {
        moveMouse(s, at.x, at.y);
        renderFrames(s, 1);
        changes=gweni::controls::getChangeStats();
        slots=s.renderer->getFrameStats().primitiveSlotsWritten;
    };
    const auto bounded=[&](const gweni::controls::ChangeStats &changes, size_t slots)
    {
        return changes.background >= 1 && changes.background <= g_hoverBound
            && slots >= 1 && slots <= g_hoverBound && changes.controls <= g_hoverBound;
    };
    const auto counters=[](const gweni::controls::ChangeStats &changes, size_t slots)
    {
        return "controls_changed="+std::to_string(changes.controls)
            +" background="+std::to_string(changes.background)
            +" prim_slots_written="+std::to_string(slots);
    };

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> idle=settledRegion(s, box, shotDir, "checkbox-idle", ok);
    const std::vector<uint8_t> labelledIdle=regionPixels(s, labelled);
    const std::vector<uint8_t> disabledIdle=regionPixels(s, disabled);

    gweni::controls::ChangeStats hoverChanges;
    size_t hoverSlots=0;
    hoverFrame(on, hoverChanges, hoverSlots);
    const std::vector<uint8_t> hover=settledRegion(s, box, shotDir, "checkbox-hover", ok);

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> unhovered=settledRegion(s, box, shotDir, "checkbox-unhover", ok);

    clickMouse(s, on.x, on.y);
    moveMouse(s, off.x, off.y);
    const bool checkedByClick=box->isChecked();
    const std::vector<uint8_t> checked=settledRegion(s, box, shotDir, "checkbox-checked", ok);

    clickMouse(s, on.x, on.y);
    moveMouse(s, off.x, off.y);
    const bool uncheckedByClick=!box->isChecked();
    const std::vector<uint8_t> unchecked=settledRegion(s, box, shotDir, "checkbox-unchecked", ok);

    // the label lights its box, and a click on it toggles the box
    gweni::controls::ChangeStats labelChanges;
    size_t labelSlots=0;
    hoverFrame(label, labelChanges, labelSlots);
    const std::vector<uint8_t> labelHover=settledRegion(s, labelled, shotDir, "checkbox-label-hover", ok);

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> labelUnhovered=settledRegion(s, labelled, shotDir, "checkbox-label-unhover", ok);

    clickMouse(s, label.x, label.y);
    moveMouse(s, off.x, off.y);
    const bool labelChecked=labelled->isChecked();
    const std::vector<uint8_t> labelCheckedPixels=settledRegion(s, labelled, shotDir, "checkbox-label-checked", ok);

    clickMouse(s, label.x, label.y);
    moveMouse(s, off.x, off.y);
    const bool labelUnchecked=!labelled->isChecked();
    const std::vector<uint8_t> labelUncheckedPixels=settledRegion(s, labelled, shotDir, "checkbox-label-unchecked", ok);

    // the disabled box: no hover, no toggle
    moveMouse(s, onDisabled.x, onDisabled.y);
    const std::vector<uint8_t> disabledHover=settledRegion(s, disabled, shotDir, "checkbox-disabled-hover", ok);

    clickMouse(s, onDisabled.x, onDisabled.y);
    moveMouse(s, off.x, off.y);
    const bool disabledStayed=!disabled->isChecked();
    const std::vector<uint8_t> disabledClicked=settledRegion(s, disabled, shotDir, "checkbox-disabled-clicked", ok);

    forgetMouse(s);
    dropControl(s, demo);

    checks.check("checkbox_hover", hover != idle && unhovered == idle,
        "hovering the CheckBox left its pixels as they were, or moving off did not bring them back");
    checks.check("checkbox_hover_bounded", bounded(hoverChanges, hoverSlots),
        "the CheckBox hover frame had "+counters(hoverChanges, hoverSlots));
    checks.check("checkbox_check", checkedByClick && checked != idle,
        "a click did not check the CheckBox, or left its pixels as they were unchecked");
    checks.check("checkbox_uncheck", uncheckedByClick && unchecked == idle,
        "a second click did not uncheck the CheckBox back to its first look");
    checks.check("checkbox_label_hover", labelHover != labelledIdle && labelUnhovered == labelledIdle,
        "hovering the CheckBoxWithLabel's label did not light its box, or moving off did not bring it back");
    checks.check("checkbox_label_hover_bounded", bounded(labelChanges, labelSlots),
        "the label hover frame had "+counters(labelChanges, labelSlots));
    checks.check("checkbox_label_toggle",
        labelChecked && labelCheckedPixels != labelledIdle && labelUnchecked && labelUncheckedPixels == labelledIdle,
        "two clicks on the label did not check and then uncheck its box back to its first look");
    // both boxes are 15x15, unchecked and at rest, so their pixels compare
    checks.check("checkbox_disabled_look", disabledIdle != idle,
        "the disabled CheckBox looks like the enabled one at rest");
    checks.check("checkbox_disabled",
        disabledStayed && disabledHover == disabledIdle && disabledClicked == disabledIdle,
        "hovering or clicking the disabled CheckBox changed its pixels, or the click checked it");
}

static size_t livePrimitiveIds()
{
    return gweni::primitiveIdCount()-gweni::releasedPrimitiveIdCount();
}

// WindowDemo: the close button of the window "Normal Window" opens shows
// something at rest, a hover and a press change its pixels and moving off gives
// the rest look back; the hover frame writes only the button's two slots; a
// press let go off the button is no click, and one let go on it closes the
// window and gives the window's primitive ids back.
static void checkWindowClose(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "WindowDemo");
    renderFrames(s, s.frames);

    const size_t idsBefore=livePrimitiveIds();
    gweni::controls::Base *open=findControl(demo, "Button");
    if(!open)
    {
        checks.check("window_close_found", false, "WindowDemo has no visible Button");
        dropControl(s, demo);
        return;
    }

    const gweni::Point openAt=centreOf(open);
    clickMouse(s, openAt.x, openAt.y);
    renderFrames(s, s.frames);

    gweni::controls::Base *window=findControl(s.canvas, "WindowControl");
    gweni::controls::Base *close=window?findControl(window, "WindowCloseButton"):nullptr;
    if(!close)
    {
        checks.check("window_close_found", false, "\"Normal Window\" opened no window with a close button");
        forgetMouse(s);
        dropControl(s, demo);
        return;
    }

    // a canvas corner the window does not cover, so moving there leaves it alone
    const gweni::Point at=window->localPosToCanvas();
    const gweni::Point corners[3]={gweni::Point(s.size.x-1, s.size.y-1), gweni::Point(s.size.x-1, 0),
        gweni::Point(0, s.size.y-1)};
    gweni::Point off=corners[0];
    for(const gweni::Point &corner : corners)
    {
        if(corner.x < at.x || corner.x >= at.x+window->getWidth()
            || corner.y < at.y || corner.y >= at.y+window->getHeight())
        {
            off=corner;
            break;
        }
    }
    const gweni::Point on=centreOf(close);

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> idle=settledRegion(s, close, shotDir, "window-close-idle", ok);
    // what the title bar shows there without the button's two slots
    s.renderer->hidePrimitive(close->getSkinControl()->getPrimitives()[0]);
    s.renderer->hidePrimitive(close->getSkinControl()->getPrimitives()[1]);
    renderFrames(s, 1);
    const std::vector<uint8_t> bare=regionPixels(s, close);
    close->addStateChange(gweni::controls::StateChange_Background);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> restored=regionPixels(s, close);

    moveMouse(s, on.x, on.y);
    renderFrames(s, 1);
    const size_t hoverSlots=s.renderer->getFrameStats().primitiveSlotsWritten;
    const std::vector<uint8_t> hover=settledRegion(s, close, shotDir, "window-close-hover", ok);

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> unhovered=settledRegion(s, close, shotDir, "window-close-unhover", ok);

    // Pressed and let go off the button: not a click, so the window stays open
    // and the button is back at rest.
    moveMouse(s, on.x, on.y);
    mouseDown(s, on.x, on.y, true);
    const std::vector<uint8_t> press=settledRegion(s, close, shotDir, "window-close-press", ok);
    moveMouse(s, off.x, off.y);
    renderFrames(s, 1);
    mouseUp(s, off.x, off.y);
    renderFrames(s, s.frames);
    const bool stayedOpen=(findControl(s.canvas, "WindowControl") == window);
    if(!stayedOpen)
    {
        // the window and its close button are gone, so the rest cannot run
        checks.check("window_close_cancel", false,
            "a press on the close button let go off it closed the window");
        forgetMouse(s);
        dropControl(s, demo);
        return;
    }
    const std::vector<uint8_t> cancelled=settledRegion(s, close, shotDir, "window-close-cancelled", ok);

    // Pressed and let go on it: the release closes the window.
    moveMouse(s, on.x, on.y);
    mouseDown(s, on.x, on.y, true);
    renderFrames(s, s.frames);
    mouseUp(s, on.x, on.y);
    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const bool closed=(findControl(s.canvas, "WindowControl") == nullptr);
    const size_t idsAfter=livePrimitiveIds();

    forgetMouse(s);
    dropControl(s, demo);

    checks.check("window_close_rest", idle != bare && restored == idle,
        "the close button at rest draws nothing over its title bar");
    checks.check("window_close_hover", hover != idle && unhovered == idle,
        "hovering the close button left its pixels as they were, or moving off did not bring"
        " its rest look back");
    checks.check("window_close_press", press != idle && press != hover,
        "pressing the close button did not change its pixels from rest and hover");
    checks.check("window_close_cancel", cancelled == idle,
        "the close button let go off it did not get its rest look back");
    checks.check("window_close_hover_bounded", hoverSlots >= 1 && hoverSlots <= 2,
        "the hover frame wrote "+std::to_string(hoverSlots)+" primitive slots; want 1..2");
    checks.check("window_close_closes", closed && idsAfter == idsBefore,
        std::string(closed ? "" : "a click on the close button left the window open; ")
        +"live primitive ids "+std::to_string(idsBefore)+" before the window, "
        +std::to_string(idsAfter)+" after it closed");
}

// SliderDemo: dragging the first slider's thumb, a unit at a time along the
// track and then in larger steps along it and off it, changes the thumb alone
// on every drag frame (the mouse is held, so the controls it crosses get no
// hover), each frame moves the thumb by exactly the mouse's step (its position
// is rounded from the value read off it, #466), and the release off the thumb
// takes its hover away.
static void checkSliderDrag(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "SliderDemo");
    renderFrames(s, s.frames);

    gweni::controls::Base *thumb=findControl(demo, "SliderBar");
    gweni::controls::Base *slider=thumb?thumb->getParent():nullptr;
    if(!slider)
    {
        checks.check("slider_drag_found", false, "SliderDemo has no visible SliderBar");
        dropControl(s, demo);
        return;
    }

    const gweni::Point grab=centreOf(thumb);
    moveMouse(s, grab.x, grab.y);
    renderFrames(s, s.frames);
    mouseDown(s, grab.x, grab.y, true);
    renderFrames(s, s.frames);

    // towards the track's far end, in steps that stay inside it: 32 of one
    // unit along the track (from the thumb's rest at 34 they pass 56, which a
    // truncated position left at 55, #466), then two of a tenth of the slider
    // along it and two more of those below the track, off the slider
    const gweni::Point sliderAt=slider->localPosToCanvas();
    const int direction=(grab.x < sliderAt.x+slider->getWidth()/2) ? 1 : -1;
    const int coarse=std::max(1, slider->getWidth()/10);
    const int below=sliderAt.y+slider->getHeight()+5;
    const int unitSteps=32;

    std::string frames;
    std::string missed;
    bool onlyThumb=true;
    int mouseX=grab.x;
    for(int i=1; i <= unitSteps+4; ++i)
    {
        const int step=direction*(i <= unitSteps ? 1 : coarse);
        const bool alongTrack=(i <= unitSteps+2);
        const int expectedX=thumb->getX()+step;

        mouseX+=step;
        moveMouse(s, mouseX, alongTrack ? grab.y : below);
        renderFrames(s, 1);
        const gweni::controls::ChangeStats changes=gweni::controls::getChangeStats();

        if(changes.controls != 1)
        {
            onlyThumb=false;
            frames+=" step "+std::to_string(i)+": controls_changed="+std::to_string(changes.controls)
                +" background="+std::to_string(changes.background)+";";
        }
        if(thumb->getX() != expectedX)
            missed+=" step "+std::to_string(i)+(alongTrack ? " (along the track)" : " (below it)")
                +": mouse moved "+std::to_string(step)+", thumb x "
                +std::to_string(thumb->getX())+", expected "+std::to_string(expectedX)+";";
    }
    const bool followed=missed.empty();
    const bool hoveredWhileHeld=thumb->isHovered();
    if(!shoot(s, shotDir+"/interaction-slider-drag.png"))
        ok=false;

    mouseUp(s, s.mouse.x, s.mouse.y);
    renderFrames(s, s.frames);
    const bool unhovered=!thumb->isHovered() && gweni::MouseFocus == nullptr;

    forgetMouse(s);
    dropControl(s, demo);

    checks.check("slider_drag_one_control", onlyThumb,
        "a drag frame changed other controls than the thumb:"+frames+" want controls_changed=1 each");
    checks.check("slider_drag_follows", followed && hoveredWhileHeld,
        std::string(followed ? "" : "the thumb did not move by the mouse's step:"+missed+" ")
        +(hoveredWhileHeld ? "" : "the held thumb was not hovered"));
    checks.check("slider_drag_release", unhovered,
        "the thumb let go off itself kept the hover or the mouse");
}

// SliderTextDemo: the first slider shows its value as "%.0f" over 0..100, so a
// one-unit drag step changes the value by about half a unit of it. Dragging
// its thumb a unit at a time, a frame whose string stays writes no glyph slot
// and at most the thumb's one primitive slot, and a frame whose string
// changes writes no more glyph slots than the new string has glyphs. Both
// kinds must happen.
static void checkSliderText(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "SliderTextDemo");
    renderFrames(s, s.frames);

    gweni::controls::Slider *slider=gweni::gweni_cast<gweni::controls::Slider>(findControl(demo, "HorizontalSlider"));
    if(!slider || !slider->getValueTextControl())
    {
        checks.check("slider_text_found", false, "SliderTextDemo has no visible HorizontalSlider with a value text");
        dropControl(s, demo);
        return;
    }

    gweni::controls::Base *thumb=slider->getSliderBar();
    const gweni::Point grab=centreOf(thumb);
    moveMouse(s, grab.x, grab.y);
    renderFrames(s, s.frames);
    mouseDown(s, grab.x, grab.y, true);
    renderFrames(s, s.frames);

    std::string frames;
    int kept=0, changed=0;
    bool keptWroteNothing=true;
    bool changedWithinRun=true;

    for(int i=1; i <= 12; ++i)
    {
        const gweni::String shown=slider->getValueText();
        moveMouse(s, grab.x+i, grab.y);
        renderFrames(s, 1);

        const gweni::renderer::SlotRenderer::FrameStats &gpu=s.renderer->getFrameStats();
        const gweni::String now=slider->getValueText();

        frames+=" \""+now+"\":prim="+std::to_string(gpu.primitiveSlotsWritten)
            +",text="+std::to_string(gpu.textSlotsWritten);
        if(now == shown)
        {
            ++kept;
            if(gpu.textSlotsWritten != 0 || gpu.primitiveSlotsWritten > 1)
                keptWroteNothing=false;
        }
        else
        {
            ++changed;
            if(gpu.textSlotsWritten == 0 || gpu.textSlotsWritten > now.size())
                changedWithinRun=false;
        }
    }

    if(!shoot(s, shotDir+"/interaction-slider-text.png"))
        ok=false;

    mouseUp(s, s.mouse.x, s.mouse.y);
    renderFrames(s, s.frames);
    forgetMouse(s);
    dropControl(s, demo);

    std::cout<<"slider_text_drag"<<frames<<"\n";
    checks.check("slider_text_kept", kept > 0 && keptWroteNothing,
        "a drag frame that kept the shown value wrote glyph slots or more than the thumb's slot:"+frames);
    checks.check("slider_text_changed", changed > 0 && changedWithinRun,
        "a drag frame that changed the shown value wrote no glyph slot or more than the new string's:"+frames);
}

// The ids a control and everything under it hold: primitive ids, and a Text's
// text id (its skin Control keeps it with them).
static size_t ownedIds(gweni::controls::Base *control)
{
    size_t count=control->getSkinControl()?control->getSkinControl()->getPrimitives().size():0;

    for(auto &&child : control->getActualChildren())
        count+=ownedIds(child);
    return count;
}

// TabCloseDemo: hovering "README.md", a closable tab that is not selected,
// shows its close button. The hover frame writes at least one primitive slot
// and no more than the tab's and its button's, the hover changes no pixel
// outside the tab, and moving off gives the frame before back. A click on the
// button closes the tab: it leaves the strip, the selected tab stays selected,
// and the ids of the tab and its page are given back.
static void checkTabClose(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "TabCloseDemo");
    renderFrames(s, s.frames);

    gweni::controls::TabControl *control=gweni::gweni_cast<gweni::controls::TabControl>(findControl(demo, "TabControl"));
    gweni::controls::TabButton *tab=(control && control->tabCount() > 2)?control->getTab(2):nullptr;
    if(!tab || !tab->getCloseButton() || tab == control->getCurrentButton())
    {
        checks.check("tab_close_found", false, "TabCloseDemo has no closable unselected third tab");
        dropControl(s, demo);
        return;
    }

    gweni::controls::TabButton *selected=control->getCurrentButton();
    gweni::controls::Base *close=tab->getCloseButton();
    const size_t tabSlots=tab->getSkinControl()->getPrimitives().size()
        +close->getSkinControl()->getPrimitives().size();
    const gweni::Point origin=tab->localPosToCanvas();
    const gweni::Point body(origin.x+4, origin.y+tab->getHeight()/2);
    const gweni::Point off(s.size.x-1, s.size.y-1);
    const int tabs=control->tabCount();

    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const bool hiddenAtRest=!close->visible();
    const std::vector<uint8_t> rest=framePixels(s);

    moveMouse(s, body.x, body.y);
    renderFrames(s, 1);
    const size_t hoverSlots=s.renderer->getFrameStats().primitiveSlotsWritten;
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tab-close-hover.png"))
        ok=false;
    const bool shownOnHover=close->visible();
    const std::vector<uint8_t> hover=framePixels(s);
    const SplitDifferences hoverDiff=differencesSplit(s, rest, hover, tab, tab);

    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> unhovered=framePixels(s);

    auto liveIds=[]()
    {
        return livePrimitiveIds()+gweni::textIdCount()-gweni::releasedTextIdCount();
    };
    const size_t idsBefore=liveIds();
    const size_t owned=ownedIds(tab)+ownedIds(tab->getPage());
    const gweni::Point on=centreOf(close);
    moveMouse(s, body.x, body.y);
    renderFrames(s, s.frames);
    clickMouse(s, on.x, on.y);
    renderFrames(s, s.frames);
    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tab-closed.png"))
        ok=false;

    bool gone=control->tabCount() == tabs-1;
    for(int i=0; i < control->tabCount(); ++i)
    {
        if(control->getTab(i) == tab)
            gone=false;
    }
    const bool selectionKept=control->getCurrentButton() == selected;
    const size_t idsAfter=liveIds();

    forgetMouse(s);
    dropControl(s, demo);

    checks.check("tab_close_hover", hiddenAtRest && shownOnHover && hover != rest && hoverDiff.outside == 0
        && unhovered == rest,
        "hovering the unselected closable tab did not show its close button inside the tab alone"
        " ("+std::to_string(hoverDiff.outside)+" pixels changed outside it), or moving off did not"
        " give the frame before back");
    checks.check("tab_close_hover_bounded", hoverSlots >= 1 && hoverSlots <= tabSlots,
        "the hover frame wrote "+std::to_string(hoverSlots)+" primitive slots; want 1.."
        +std::to_string(tabSlots)+", the tab's and its close button's");
    checks.check("tab_close_closes", gone && selectionKept && idsAfter+owned == idsBefore,
        std::string(gone ? "" : "a click on the close button left the tab in the strip; ")
        +(selectionKept ? "" : "the selected tab changed; ")
        +"live primitive and text ids "+std::to_string(idsBefore)+" before the close, "
        +std::to_string(idsAfter)+" after, the tab and its page held "+std::to_string(owned));
}

static size_t differencesOutside(const Session &s, const std::vector<uint8_t> &a,
    const std::vector<uint8_t> &b, gweni::controls::Base *control);
static void countPending(gweni::controls::Base *control, size_t &count);
static void checkSteady(Session &s, InteractionChecks &checks, const std::string &name);

// The controls with a change pending that are under none of \p roots.
static void countPendingOutside(gweni::controls::Base *control,
    const std::vector<gweni::controls::Base *> &roots, size_t &count)
{
    bool under=false;
    for(gweni::controls::Base *at=control; at; at=at->getParent())
    {
        if(std::find(roots.begin(), roots.end(), at) != roots.end())
            under=true;
    }
    if(control->getStateChange() != gweni::controls::StateChange_Nothing && !under)
        ++count;
    for(auto &&child : control->getActualChildren())
        countPendingOutside(child, roots, count);
}

// TabOverflowDemo: its first TabControl's twelve tabs overflow it. A press on
// the right scroll button scrolls them: the frame leaves nothing pending
// outside the strip's and the three buttons' subtrees, writes no more primitive
// slots than they own and no more glyph slots than the tabs' captions hold,
// and changes no pixel outside the TabControl. A press on the left one gives
// the first look back. Settled frames write nothing, with the tab list closed
// and open. The list button opens one row per tab, and a click on the tenth
// row selects that tab, closes the list and shows the whole tab.
static void checkTabOverflow(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "TabOverflowDemo");
    renderFrames(s, s.frames);

    auto *control=gweni::gweni_cast<gweni::controls::TabControl>(findControl(demo, "TabControl"));
    if(!control || !control->isOverflowing() || !control->getTabListButton()->visible()
       || control->tabCount() < 10)
    {
        checks.check("tab_overflow_found", false,
            "TabOverflowDemo has no overflowing TabControl of ten tabs or more with a tab list");
        dropControl(s, demo);
        return;
    }

    gweni::controls::TabStrip *strip=control->getTabStrip();
    const std::vector<gweni::controls::Base *> buttons=
        {control->getScrollButton(0), control->getScrollButton(1), control->getTabListButton()};
    std::vector<gweni::controls::Base *> scrolled=buttons;
    scrolled.push_back(strip);
    size_t slots=ownedIds(strip);
    size_t glyphs=0;
    for(gweni::controls::Base *button : buttons)
        slots+=ownedIds(button);
    for(int i=0; i < control->tabCount(); ++i)
        glyphs+=control->getTab(i)->getText().size();

    const gweni::Point off(s.size.x-1, s.size.y-1);
    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> rest=framePixels(s);

    const gweni::Point right=centreOf(control->getScrollButton(1));
    moveMouse(s, right.x, right.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> hovered=framePixels(s);

    clickMouse(s, right.x, right.y);
    gweni::controls::resetChangeStats();
    s.canvas->updateCanvas();
    size_t elsewhere=0;
    countPendingOutside(s.canvas, scrolled, elsewhere);
    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();
    const gweni::renderer::SlotRenderer::FrameStats press=s.renderer->getFrameStats();
    if(s.stats)
        printFrameStats(s.frame, s.label, gweni::controls::getChangeStats(), press,
            s.canvas->getLayoutStats(), s.canvas->getUpdateStats());
    checkOnePass(s, std::to_string(s.frame));
    ++s.frame;
    const int offset=control->getScrollOffset();
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tab-overflow-scrolled.png"))
        ok=false;
    const std::vector<uint8_t> after=framePixels(s);
    const size_t outside=differencesOutside(s, hovered, after, control);
    checkSteady(s, checks, "tab_overflow_steady");

    const gweni::Point left=centreOf(control->getScrollButton(0));
    moveMouse(s, left.x, left.y);
    renderFrames(s, s.frames);
    clickMouse(s, left.x, left.y);
    renderFrames(s, s.frames);
    const int back=control->getScrollOffset();
    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> unscrolled=framePixels(s);

    checks.check("tab_overflow_scroll", offset > 0 && back == 0 && elsewhere == 0 && outside == 0
        && unscrolled == rest,
        "a press on the right scroll button left the offset at "+std::to_string(offset)
        +", "+std::to_string(elsewhere)+" controls pending outside the strip and the buttons and "
        +std::to_string(outside)+" pixels changed outside the TabControl; the left one left it at "
        +std::to_string(back)+(unscrolled == rest ? "" : ", and the frame differs from the first"));
    checks.check("tab_overflow_scroll_bounded", press.primitiveSlotsWritten <= slots
        && press.textSlotsWritten <= glyphs,
        "the press frame wrote "+std::to_string(press.primitiveSlotsWritten)+" primitive and "
        +std::to_string(press.textSlotsWritten)+" glyph slots; want at most "+std::to_string(slots)
        +" (the strip's and the buttons') and "+std::to_string(glyphs)+" (the tabs' captions)");

    // the tab list
    const gweni::Point list=centreOf(control->getTabListButton());
    moveMouse(s, list.x, list.y);
    renderFrames(s, s.frames);
    clickMouse(s, list.x, list.y);
    renderFrames(s, s.frames);
    gweni::controls::Popup *popup=control->getTabListPopup();
    std::vector<gweni::controls::Base *> rows;
    if(popup)
    {
        for(auto &&child : popup->getChildren())
        {
            if(gweni::gweni_cast<gweni::controls::Selectable>(child))
                rows.push_back(child);
        }
    }
    const bool opened=popup && popup->isOpen() && rows.size() == (size_t)control->tabCount();
    if(!shoot(s, shotDir+"/interaction-tab-overflow-list.png"))
        ok=false;
    checkSteady(s, checks, "tab_overflow_list_steady");

    bool chosen=false;
    if(opened)
    {
        gweni::controls::TabButton *tenth=control->getTab(9);
        const gweni::Point row=centreOf(rows[9]);
        moveMouse(s, row.x, row.y);
        renderFrames(s, s.frames);
        clickMouse(s, row.x, row.y);
        renderFrames(s, s.frames);
        moveMouse(s, off.x, off.y);
        renderFrames(s, s.frames);
        const int tabRight=tenth->localPosToCanvas().x-control->localPosToCanvas().x+tenth->getWidth();
        chosen=!popup->isOpen() && control->getCurrentButton() == tenth
            && tabRight <= control->getTabsRight();
    }

    forgetMouse(s);
    dropControl(s, demo);

    checks.check("tab_overflow_list", opened && chosen,
        std::string(opened ? "" : "the tab-list button opened no popup of one row per tab; ")
        +(chosen || !opened ? "" : "choosing its tenth row did not select that tab, close the list"
        " and show the whole tab"));
}

// TabControlDemo: the tabs of its last TabControl ("You", "Can", ...), drawn
// as a notebook's (#540). The first TabControl's preferred height follows its
// shown page (its "Controls" page holds the radio buttons, the others nothing),
// so selecting there lays the demo out again and moves the last one; the last
// one's pages are all empty and nothing comes after it, so a selection there
// changes nothing else. Hovering "Can", an unselected tab, writes its one
// primitive slot and glyphs of its own alone, changes no pixel outside it, and
// moving off gives the frame before back. A click on it selects it: no frame
// from the click until settled writes a primitive or glyph slot outside the two
// tabs' and the two pages' subtrees (the old page's controls are hidden, the new
// one's shown), the page frame's (TabControlInner's) slot is never written, and
// no pixel outside the TabControl changes. Selecting the first tab again gives
// the frame before back.
static void checkTabControlSelect(Session &s, const std::string &shotDir, InteractionChecks &checks,
    bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "TabControlDemo");
    renderFrames(s, s.frames);

    gweni::controls::TabControl *control=nullptr;
    for(gweni::controls::Base *child : demo->getChildren())
    {
        if(auto *tabs=gweni::gweni_cast<gweni::controls::TabControl>(child))
            control=tabs;
    }
    if(!control || control->tabCount() < 2 || control->getCurrentButton() != control->getTab(0)
       || !control->getTab(0)->getPage() || !control->getTab(1)->getPage()
       || static_cast<const gweni::controls::TabControl *>(control)->getInnerPanel()->getSkinControl()
            ->getPrimitives().size() != 1
       || control->getTab(0)->getSkinControl()->getPrimitives().size() != 1
       || control->getTab(1)->getSkinControl()->getPrimitives().size() != 1)
    {
        checks.check("tab_select_found", false,
            "TabControlDemo has no TabControl of two tabs or more with its first selected, its"
            " page frame and tabs of one primitive slot each");
        dropControl(s, demo);
        return;
    }

    gweni::controls::TabButton *first=control->getTab(0);
    gweni::controls::TabButton *second=control->getTab(1);
    const std::vector<size_t> &frameSlots=
        static_cast<const gweni::controls::TabControl *>(control)->getInnerPanel()
            ->getSkinControl()->getPrimitives();
    std::vector<size_t> secondPrimitives, secondTexts;
    gweni::tools::appendSubtreeIds(second, secondPrimitives, secondTexts);
    std::vector<size_t> allowedPrimitives, allowedTexts;
    for(gweni::controls::Base *c : {static_cast<gweni::controls::Base *>(first),
        static_cast<gweni::controls::Base *>(second), first->getPage(), second->getPage()})
        gweni::tools::appendSubtreeIds(c, allowedPrimitives, allowedTexts);
    std::sort(allowedPrimitives.begin(), allowedPrimitives.end());
    std::sort(allowedTexts.begin(), allowedTexts.end());

    const std::vector<size_t> secondSlot=second->getSkinControl()->getPrimitives();
    const gweni::Point on=centreOf(second);
    const gweni::Point off(s.size.x-1, s.size.y-1);

    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> rest=framePixels(s);

    // the hover: the tab's one slot, and only its own glyphs
    moveMouse(s, on.x, on.y);
    renderFrames(s, 1);
    const std::vector<size_t> hoverPrimitives=s.renderer->writtenPrimitiveIds();
    size_t hoverForeignTexts=0;
    for(const gweni::renderer::WrittenText &text : s.renderer->writtenTextIds())
    {
        if(std::find(secondTexts.begin(), secondTexts.end(), text.textId) == secondTexts.end())
            hoverForeignTexts+=text.glyphSlots;
    }
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tab-select-hover.png"))
        ok=false;
    const std::vector<uint8_t> hover=framePixels(s);
    const SplitDifferences hoverDiff=differencesSplit(s, rest, hover, second, second);

    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> unhovered=framePixels(s);

    // the selection, from a settled hover
    moveMouse(s, on.x, on.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> beforeClick=framePixels(s);
    size_t foreignPrimitives=0;
    size_t foreignGlyphs=0;
    size_t written=0;
    bool frameWritten=false;
    std::vector<size_t> tabSlotsWritten;
    const size_t firstSlot=first->getSkinControl()->getPrimitives().front();
    clickMouse(s, on.x, on.y);
    for(int i=0; i < s.frames; ++i)
    {
        renderFrames(s, 1);
        for(size_t id : s.renderer->writtenPrimitiveIds())
        {
            ++written;
            if(id == firstSlot || id == secondSlot.front())
                tabSlotsWritten.push_back(id);
            if(!std::binary_search(allowedPrimitives.begin(), allowedPrimitives.end(), id))
                ++foreignPrimitives;
            if(std::find(frameSlots.begin(), frameSlots.end(), id) != frameSlots.end())
                frameWritten=true;
        }
        for(const gweni::renderer::WrittenText &text : s.renderer->writtenTextIds())
        {
            if(!std::binary_search(allowedTexts.begin(), allowedTexts.end(), text.textId))
                foreignGlyphs+=text.glyphSlots;
        }
    }
    if(!shoot(s, shotDir+"/interaction-tab-selected.png"))
        ok=false;
    const bool selected=control->getCurrentButton() == second && second->isActive() && !first->isActive();
    const bool bothTabs=std::find(tabSlotsWritten.begin(), tabSlotsWritten.end(), firstSlot)
        != tabSlotsWritten.end() && std::find(tabSlotsWritten.begin(), tabSlotsWritten.end(),
        secondSlot.front()) != tabSlotsWritten.end();
    const size_t outside=differencesOutside(s, beforeClick, framePixels(s), control);

    // and back
    const gweni::Point firstOn=centreOf(first);
    clickMouse(s, firstOn.x, firstOn.y);
    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const bool back=framePixels(s) == rest && control->getCurrentButton() == first;

    forgetMouse(s);
    dropControl(s, demo);

    checks.check("tab_select_hover", hoverPrimitives == secondSlot && hoverForeignTexts == 0
        && hover != rest && hoverDiff.outside == 0 && unhovered == rest,
        "hovering the unselected tab wrote "+std::to_string(hoverPrimitives.size())
        +" primitive slots (want its one) and "+std::to_string(hoverForeignTexts)
        +" glyph slots of other text ids, changed "+std::to_string(hoverDiff.outside)
        +" pixels outside it, or moving off did not give the frame before back");
    checks.check("tab_select_local", selected && bothTabs && written > 0 && foreignPrimitives == 0
        && foreignGlyphs == 0 && !frameWritten && outside == 0,
        std::string(selected ? "" : "a click on the second tab did not select it; ")
        +(bothTabs ? "" : "the selection did not rewrite both tabs' slots; ")
        +std::to_string(foreignPrimitives)+" primitive slots and "+std::to_string(foreignGlyphs)
        +" glyph slots written outside the two tabs and their pages, the page frame's slot "
        +(frameWritten ? "written" : "not written")+", "+std::to_string(outside)
        +" pixels changed outside the TabControl");
    checks.check("tab_select_back", back,
        "selecting the first tab again and moving off did not give the frame before back");
}

static void renderInspectedFrame(Session &s, gweni::controls::Base *root,
    size_t &pending, size_t &pendingUnderRoot);
static void checkSteady(Session &s, InteractionChecks &checks, const std::string &name);

// PropertiesDemo: the top Properties is its rows' height (its preferred one)
// and the PropertyTree runs from one layout spacing below it to the demo's
// bottom, so the spare height is all inside the tree. The first PropertyRow's
// look follows its subtree. Hovering its value's TextBox changes the row's
// pixels (the column fill, the lines and the label's colour) and none outside
// it, in a frame whose changes are all in the row; moving onto its label
// changes nothing and marks nothing on the row; moving off (onto the tree's
// empty area) gives the rest look back; a click on the value (its editor
// focused) shows the Editing look, unlike the hover's, with the mouse off, and
// the focus dropped gives the rest look back.
static void checkPropertyRow(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "PropertiesDemo");
    renderFrames(s, s.frames);

    // the layout's spare height is the tree's alone (#516): the Properties is
    // its rows' height and the tree runs from one spacing below it to the
    // demo's bottom
    gweni::controls::Base *props=findControl(demo, "Properties");
    gweni::controls::Base *tree=findControl(demo, "PropertyTree");
    gweni::controls::LayoutItem *layout=demo->getLayout();
    if(!props || !tree || !layout || props->getParent() != demo || tree->getParent() != demo)
    {
        checks.check("property_layout_found", false,
            "PropertiesDemo has no layout with a Properties and a PropertyTree as its children");
    }
    else
    {
        const gweni::Rect &p=props->getBounds();
        const gweni::Rect &t=tree->getBounds();
        const std::string bounds="Properties ("+std::to_string(p.x)+", "+std::to_string(p.y)+", "
            +std::to_string(p.w)+", "+std::to_string(p.h)+") pref height "
            +std::to_string(props->getPreferredSize().height)+", PropertyTree ("+std::to_string(t.x)+", "
            +std::to_string(t.y)+", "+std::to_string(t.w)+", "+std::to_string(t.h)+"), demo height "
            +std::to_string(demo->getHeight())+", spacing "+std::to_string(layout->getSpacing());
        checks.check("property_layout_rows_height", p.h == props->getPreferredSize().height,
            "the top Properties is not its rows' height: "+bounds);
        checks.check("property_layout_tree_top", t.y == p.y+p.h+layout->getSpacing(),
            "the PropertyTree does not start one spacing below the Properties: "+bounds);
        checks.check("property_layout_tree_bottom", t.y+t.h == demo->getHeight(),
            "the PropertyTree does not reach the demo's bottom: "+bounds);
    }

    auto *row=gweni::gweni_cast<gweni::controls::PropertyRow>(findControl(demo, "PropertyRow"));
    gweni::controls::Base *value=row?findControl(row, "TextBox"):nullptr;
    if(!row || !value)
    {
        checks.check("property_row_found", false, "PropertiesDemo has no PropertyRow with a TextBox value");
        dropControl(s, demo);
        return;
    }

    const gweni::Point onValue=centreOf(value);
    const gweni::Point onLabel=centreOf(row->getLabel());
    const gweni::Point off(s.size.x-1, s.size.y-1);    // the tree's empty area, no row

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> idle=settledRegion(s, row, shotDir, "property-row-idle", ok);
    const std::vector<uint8_t> idleFrame=framePixels(s);

    // the hover frame alone: everything it changes is in the row, but for the
    // control the mouse leaves (the demo page), which repaints on its leave
    size_t leftOutside=gweni::HoveredControl ? 1 : 0;
    for(gweni::controls::Base *at=gweni::HoveredControl; at; at=at->getParent())
    {
        if(at == row)
            leftOutside=0;
    }
    moveMouse(s, onValue.x, onValue.y);
    size_t pending=0;
    size_t pendingInRow=0;
    renderInspectedFrame(s, row, pending, pendingInRow);
    const bool rowMarked=pendingInRow > 0;
    const std::vector<uint8_t> hover=settledRegion(s, row, shotDir, "property-row-hover", ok);
    const size_t hoverOutside=differencesOutside(s, idleFrame, framePixels(s), row);

    // to the label of the same row: the row keeps its look and is not marked
    moveMouse(s, onLabel.x, onLabel.y);
    gweni::controls::resetChangeStats();
    s.canvas->updateCanvas();
    const bool labelMoveMarked=row->getStateChange() != gweni::controls::StateChange_Nothing
        || row->getLabel()->getStateChange() != gweni::controls::StateChange_Nothing;
    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();
    ++s.frame;
    const std::vector<uint8_t> labelHover=settledRegion(s, row, shotDir, "property-row-label-hover", ok);

    moveMouse(s, off.x, off.y);
    const std::vector<uint8_t> unhovered=settledRegion(s, row, shotDir, "property-row-unhover", ok);
    checkSteady(s, checks, "property_row_steady");

    // editing: a click focuses the value's TextBox
    clickMouse(s, onValue.x, onValue.y);
    moveMouse(s, off.x, off.y);
    const bool editing=row->isEditing();
    const std::vector<uint8_t> edited=settledRegion(s, row, shotDir, "property-row-editing", ok);

    value->unfocus();
    const std::vector<uint8_t> unfocused=settledRegion(s, row, shotDir, "property-row-unfocused", ok);

    forgetMouse(s);
    dropControl(s, demo);

    checks.check("property_row_hover", rowMarked && hover != idle && hoverOutside == 0 && unhovered == idle,
        "hovering the PropertyRow's value left the row's pixels as they were, changed "
        +std::to_string(hoverOutside)+" pixels outside it, or moving off did not bring them back");
    checks.check("property_row_hover_local", pendingInRow > 0 && pending <= pendingInRow+leftOutside,
        "the hover frame had "+std::to_string(pending)+" controls with a change pending, "
        +std::to_string(pendingInRow)+" of them in the row and "+std::to_string(leftOutside)
        +" the control the mouse left");
    checks.check("property_row_label", !labelMoveMarked && labelHover == hover,
        "moving from the value to the label of the same row marked the row or its label, or changed its look");
    checks.check("property_row_editing", editing && edited != idle && edited != hover && unfocused == idle,
        "a click on the value did not give the row its Editing look, unlike its hover, or dropping the"
        " focus did not give the rest look back");
}

static bool inSubtree(gweni::controls::Base *control, gweni::controls::Base *root)
{
    for(; control; control=control->getParent())
    {
        if(control == root)
            return true;
    }
    return false;
}

// The controls under \p control with a change pending that are neither \p a
// nor under \p b.
static void countPendingBesides(gweni::controls::Base *control, gweni::controls::Base *a,
    gweni::controls::Base *b, size_t &count)
{
    if(control->getStateChange() != gweni::controls::StateChange_Nothing && control != a
        && !inSubtree(control, b))
        ++count;
    for(auto &&child : control->getActualChildren())
        countPendingBesides(child, a, b, count);
}

// ColorEditDemo: the R, G, B, A ColorEdit's R field clicked into, End, then
// Backspace and a digit typed, each leaving a whole value the field applies as
// it is typed: each edit frame has nothing pending outside the swatch and that
// field, writes no more primitive slots than they own and no more glyph slots
// than the field's new text has, and changes the swatch's colour. A settled
// frame writes nothing, with the picker closed and, after a click on the
// swatch, open below it; Escape closes it.
static void checkColorEdit(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "ColorEditDemo");
    renderFrames(s, s.frames);

    gweni::controls::ColorEdit *edit=nullptr;
    for(gweni::controls::Base *child : demo->getChildren())
    {
        auto *candidate=gweni::gweni_cast<gweni::controls::ColorEdit>(child);
        if(candidate && candidate->getMode() == gweni::controls::ColorEditMode::Rgba)
            edit=candidate;
    }
    if(!edit)
    {
        checks.check("color_edit_found", false, "ColorEditDemo has no R, G, B, A ColorEdit");
        dropControl(s, demo);
        return;
    }

    gweni::controls::ColorButton *swatch=edit->getSwatch();
    gweni::controls::TextBox *red=edit->getField(0);
    const size_t ownSlots=ownedIds(swatch)+ownedIds(red);
    const gweni::Point onRed=centreOf(red);

    clickMouse(s, onRed.x, onRed.y);
    renderFrames(s, s.frames);
    sendKey(s, "end");
    renderFrames(s, s.frames);

    std::string frames;
    bool alone=true, bounded=true, applied=true;
    for(const char *key : {"backspace", "5"})
    {
        const gweni::Color before=swatch->getColor();
        size_t outside=0;

        sendKey(s, key);
        countPendingBesides(s.canvas, swatch, red, outside);
        renderFrames(s, 1);

        const gweni::renderer::SlotRenderer::FrameStats &gpu=s.renderer->getFrameStats();
        frames+=std::string(" ")+key+":\""+red->getText()+"\",outside="+std::to_string(outside)
            +",prim="+std::to_string(gpu.primitiveSlotsWritten)+",text="+std::to_string(gpu.textSlotsWritten);
        if(outside != 0)
            alone=false;
        if(gpu.primitiveSlotsWritten > ownSlots || gpu.textSlotsWritten > red->getText().size())
            bounded=false;
        if(swatch->getColor() == before || !(swatch->getColor() == edit->getColor()))
            applied=false;
    }
    renderFrames(s, s.frames);
    checkSteady(s, checks, "color_edit_steady_closed");

    const gweni::Point onSwatch=centreOf(swatch);
    clickMouse(s, onSwatch.x, onSwatch.y);
    renderFrames(s, s.frames);
    const bool opened=edit->isPickerOpen()
        && edit->getPopup()->localPosToCanvas().y >= swatch->localPosToCanvas().y+swatch->getHeight();
    if(!shoot(s, shotDir+"/interaction-color-edit-open.png"))
        ok=false;
    checkSteady(s, checks, "color_edit_steady_open");
    sendKey(s, "escape");
    renderFrames(s, s.frames);
    const bool closed=!edit->isPickerOpen();

    forgetMouse(s);
    dropControl(s, demo);

    std::cout<<"color_edit_typing"<<frames<<"\n";
    checks.check("color_edit_field_alone", alone && applied,
        "an edit frame had changes pending outside the swatch and the field, or did not apply the"
        " typed value to the swatch:"+frames);
    checks.check("color_edit_field_bounded", bounded,
        "an edit frame wrote more primitive slots than the swatch's and the field's ("
        +std::to_string(ownSlots)+") or more glyph slots than the field's text:"+frames);
    checks.check("color_edit_picker", opened && closed,
        std::string(opened ? "" : "a click on the swatch did not open its picker below it; ")
        +(closed ? "" : "Escape did not close the picker"));
}

// DataTableDemo: hovering a row that is not selected leaves nothing pending
// outside it but the control the mouse left, writes no more primitive slots
// than the row's own (its fill and rules) and no more glyph slots than its
// cells' text, changes no pixel outside it, and moving off gives the frame
// before back; a settled frame writes nothing. A click on the Number header
// sorts the rows by it, the demo putting them in that order, all laid out in
// one update-phase pass, the rows then shown in ascending numbers.
static void checkDataTable(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "DataTableDemo");
    renderFrames(s, s.frames);

    auto *table=gweni::gweni_cast<gweni::controls::DataTable>(findControl(demo, "DataTable"));
    gweni::controls::DataTableRow *row=(table && table->rowCount() > 2)?table->getRow(1):nullptr;
    if(!row || row->isSelected() || table->getColumnCount() < 3)
    {
        checks.check("data_table_found", false,
            "DataTableDemo has no table of three columns or more with an unselected second row");
        dropControl(s, demo);
        return;
    }

    const size_t rowSlots=row->getSkinControl()->getPrimitives().size();
    size_t glyphs=0;
    for(int c=0; c < row->getCellCount(); ++c)
        glyphs+=static_cast<size_t>(row->getCell(c)->textLength());
    const gweni::Point on=centreOf(row);
    const gweni::Point off(s.size.x-1, s.size.y-1);

    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> rest=framePixels(s);

    gweni::controls::Base *left=gweni::HoveredControl;
    size_t outside=0;
    moveMouse(s, on.x, on.y);
    countPendingBesides(s.canvas, left, row, outside);
    renderFrames(s, 1);
    const gweni::renderer::SlotRenderer::FrameStats hoverGpu=s.renderer->getFrameStats();
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-data-table-hover.png"))
        ok=false;
    const std::vector<uint8_t> hover=framePixels(s);
    const SplitDifferences hoverDiff=differencesSplit(s, rest, hover, row, row);

    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> unhovered=framePixels(s);
    checkSteady(s, checks, "data_table_steady");

    // the Number header, clear of its grip at the right edge
    gweni::controls::internal::DataTableHeaderCell *number=table->getHeader()->getCells().at(2);
    const gweni::Point origin=number->localPosToCanvas();
    clickMouse(s, origin.x+number->getWidth()/3, origin.y+number->getHeight()/2);
    gweni::controls::resetChangeStats();
    s.canvas->updateCanvas();
    const gweni::controls::LayoutStats sortLayout=s.canvas->getLayoutStats();
    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();
    if(s.stats)
        printFrameStats(s.frame, s.label, gweni::controls::getChangeStats(), s.renderer->getFrameStats(),
            s.canvas->getLayoutStats(), s.canvas->getUpdateStats());
    checkOnePass(s, std::to_string(s.frame));
    ++s.frame;
    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-data-table-sorted.png"))
        ok=false;

    bool ordered=table->getSortColumn() == 2
        && table->getSortDirection() == gweni::controls::SortDirection::Ascending;
    for(int i=1; i < table->rowCount(); ++i)
    {
        gweni::controls::DataTableRow *above=table->getRow(i-1);
        gweni::controls::DataTableRow *below=table->getRow(i);

        if(std::atoi(above->getCellText(2).c_str()) > std::atoi(below->getCellText(2).c_str())
            || above->localPosToCanvas().y >= below->localPosToCanvas().y)
            ordered=false;
    }
    checkSteady(s, checks, "data_table_steady_sorted");

    forgetMouse(s);
    dropControl(s, demo);

    std::cout<<"interaction data_table_hover outside="<<outside<<" prim_slots_written="
        <<hoverGpu.primitiveSlotsWritten<<" text_slots_written="<<hoverGpu.textSlotsWritten
        <<" sort_layout_passes="<<sortLayout.passes<<"\n";
    checks.check("data_table_hover", hover != rest && hoverDiff.outside == 0 && unhovered == rest && outside == 0,
        "hovering the second row left its pixels as they were, changed "+std::to_string(hoverDiff.outside)
        +" pixels outside it or left "+std::to_string(outside)+" controls outside it with a change"
        " pending, or moving off did not give the frame before back");
    checks.check("data_table_hover_bounded", hoverGpu.primitiveSlotsWritten >= 1
        && hoverGpu.primitiveSlotsWritten <= rowSlots && hoverGpu.textSlotsWritten <= glyphs,
        "the hover frame wrote "+std::to_string(hoverGpu.primitiveSlotsWritten)+" primitive slots and "
        +std::to_string(hoverGpu.textSlotsWritten)+" glyph slots; want 1.."+std::to_string(rowSlots)
        +" and at most "+std::to_string(glyphs)+", the row's");
    checks.check("data_table_sort", ordered && sortLayout.passes == 1 && !sortLayout.capped,
        "a click on the Number header did not sort the rows by number in one layout pass (it took "
        +std::to_string(sortLayout.passes)+")");
}

// DragNumberDemo: the integer field pressed and dragged, a horizontal step at a
// time (each changes its shown value), a vertical one (keeps it) and, with Alt
// held, a tenth-unit one (changes the value's fraction only, the shown whole
// number kept): every drag frame has nothing pending outside the field and its
// Text, a frame that keeps the shown string writes no slot at all and one that
// changes it at most the field's one primitive slot and the new string's
// glyphs. The capture keeps the field hovered, so nothing the mouse crosses
// repaints (#188 is about a Dragger re-marking controls on every move; this
// field is no Dragger). The release and a settled frame write nothing; a
// double-click opens its edit box, and "7" and Return set the value to 7.
static void checkDragNumber(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *control=addControl(s, "DragNumberDemo");
    auto *demo=gweni::gweni_cast<gweni::DragNumberDemo>(control);
    renderFrames(s, s.frames);

    if(!demo || !demo->integerField())
    {
        checks.check("drag_number_found", false, "DragNumberDemo has no integer field");
        dropControl(s, control);
        return;
    }

    gweni::controls::DragNumber *field=demo->integerField();
    const gweni::Point grab=centreOf(field);
    moveMouse(s, grab.x, grab.y);
    renderFrames(s, s.frames);
    mouseDown(s, grab.x, grab.y, true);
    renderFrames(s, s.frames);

    std::string frames;
    int kept=0, changed=0;
    bool alone=true, keptWroteNothing=true, changedBounded=true;
    gweni::Point at=grab;

    auto dragTo=[&](int x, int y)
    {
        const gweni::String shown=field->getText();
        const double before=field->getNumber();
        size_t outside=0;

        moveMouse(s, x, y);
        countPendingBesides(s.canvas, nullptr, field, outside);
        renderFrames(s, 1);

        const gweni::renderer::SlotRenderer::FrameStats &gpu=s.renderer->getFrameStats();
        const gweni::String now=field->getText();

        frames+=" "+std::to_string(field->getNumber())+":\""+now+"\",outside="+std::to_string(outside)
            +",prim="+std::to_string(gpu.primitiveSlotsWritten)+",text="+std::to_string(gpu.textSlotsWritten);
        if(outside != 0)
            alone=false;
        if(now == shown)
        {
            ++kept;
            if(gpu.primitiveSlotsWritten != 0 || gpu.textSlotsWritten != 0)
                keptWroteNothing=false;
        }
        else
        {
            ++changed;
            if(field->getNumber() == before || gpu.primitiveSlotsWritten > 1 || gpu.textSlotsWritten == 0
                || gpu.textSlotsWritten > now.size())
                changedBounded=false;
        }
        at=gweni::Point(x, y);
    };

    for(int i=0; i < 4; ++i)
    {
        dragTo(at.x+1, at.y);
        dragTo(at.x, at.y+3);
    }
    // Alt: a tenth of a unit per unit of motion, so the whole number shown
    // stays for four steps and changes on the fifth
    glfwKey(s, GLFW_KEY_LEFT_ALT, GLFW_PRESS, 0);
    for(int i=0; i < 5; ++i)
        dragTo(at.x+1, at.y);
    glfwKey(s, GLFW_KEY_LEFT_ALT, GLFW_RELEASE, GLFW_MOD_ALT);
    // far off the field, to the right
    dragTo(at.x+200, at.y+40);

    if(!shoot(s, shotDir+"/interaction-drag-number.png"))
        ok=false;

    mouseUp(s, s.mouse.x, s.mouse.y);
    renderFrames(s, s.frames);
    checkSteady(s, checks, "drag_number_steady");

    doubleClickMouse(s, grab.x, grab.y);
    renderFrames(s, s.frames);
    const bool editing=field->isEditing();
    sendKey(s, "7");
    sendKey(s, "return");
    renderFrames(s, s.frames);
    const bool typed=!field->isEditing() && field->getNumber() == 7.0 && field->getText() == "7";
    checkSteady(s, checks, "drag_number_steady_after_edit");

    forgetMouse(s);
    dropControl(s, control);

    std::cout<<"drag_number_drag"<<frames<<"\n";
    checks.check("drag_number_alone", alone,
        "a drag frame had changes pending outside the field and its Text:"+frames);
    checks.check("drag_number_kept", kept > 0 && keptWroteNothing,
        "a drag frame that kept the shown value wrote slots:"+frames);
    checks.check("drag_number_changed", changed > 0 && changedBounded,
        "a drag frame that changed the shown value wrote more than the field's slot and the new"
        " string's glyphs, or none:"+frames);
    checks.check("drag_number_edit", editing && typed,
        std::string(editing ? "" : "a double-click did not open the edit box; ")
        +(typed ? "" : "typing 7 and Return did not set the value to 7"));
}

// Showcase -> Text input: its DragNumber rows are docked Top and Elastic, so
// the field is as high as its text. The first one ("128") double-clicked, "42"
// typed and Escape, then Return on the focused field and Return again: the box
// covers the field at its full size, and no other control moves with it open
// or closed (#521: the hidden Text counted for nothing, the field shrank to
// its padding under the box, clipping the typed text, and the sections below
// it moved). Each open and close frame lays out in one update-phase pass that
// enters no more than the field's subtree and its ancestors, and changes no
// pixel outside the field and the control that had the focus.
static void checkShowcaseDragNumber(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *control=addControl(s, "ShowcaseInputDemo");
    renderFrames(s, s.frames);

    gweni::controls::DragNumber *field=nullptr;
    std::function<void(gweni::controls::Base *)> find=[&](gweni::controls::Base *at)
    {
        if(!field)
            field=gweni::gweni_cast<gweni::controls::DragNumber>(at);
        for(gweni::controls::Base *child : at->getActualChildren())
            find(child);
    };
    if(control)
        find(control);
    if(!field)
    {
        checks.check("showcase_drag_number_found", false, "ShowcaseInputDemo has no DragNumber");
        if(control)
            dropControl(s, control);
        return;
    }

    using BoundsList=std::vector<std::pair<gweni::controls::Base *, gweni::Rect>>;
    std::function<void(gweni::controls::Base *, BoundsList &)> collect=
        [&](gweni::controls::Base *at, BoundsList &out)
    {
        if(field->getEditBox() && at == field->getEditBox())
            return;
        out.push_back({at, at->getBounds()});
        for(gweni::controls::Base *child : at->getActualChildren())
            collect(child, out);
    };
    // the entries a pass that stops at the field makes: its ancestors' and
    // its subtree's (its hidden Text measured too), each measured and
    // arranged in X and Y
    std::function<size_t(gweni::controls::Base *)> subtreeSize=[&](gweni::controls::Base *at)
    {
        size_t count=1;
        for(gweni::controls::Base *child : at->getActualChildren())
            count+=subtreeSize(child);
        return count;
    };

    const BoundsList rest=[&]() { BoundsList out; collect(s.canvas, out); return out; }();
    const gweni::Rect fieldRest=field->getBounds();
    const gweni::String shown=field->getText();
    std::string failures;

    // after a frame that opened or closed the box
    auto checkFrame=[&](const std::string &step, const std::vector<uint8_t> &before,
        gweni::controls::Base *focusedBefore)
    {
        const gweni::controls::LayoutStats &layout=s.canvas->getLayoutStats();
        size_t ancestors=0;
        for(gweni::controls::Base *at=field->getActualParent(); at; at=at->getActualParent())
            ++ancestors;
        const size_t bound=4*(ancestors+subtreeSize(field));

        BoundsList now;
        collect(s.canvas, now);
        std::string moved;
        if(now.size() != rest.size())
            moved=" the tree changed";
        for(size_t i=0; i < now.size() && i < rest.size(); ++i)
        {
            const gweni::Rect &a=rest[i].second;
            const gweni::Rect &b=now[i].second;
            if(now[i].first != rest[i].first || a.x != b.x || a.y != b.y || a.w != b.w || a.h != b.h)
            {
                moved+=std::string(" ")+now[i].first->getTypeName()+"#"+std::to_string(now[i].first->getControlId())
                    +" "+std::to_string(a.x)+","+std::to_string(a.y)+","+std::to_string(a.w)+"x"+std::to_string(a.h)
                    +"->"+std::to_string(b.x)+","+std::to_string(b.y)+","+std::to_string(b.w)+"x"+std::to_string(b.h);
            }
        }
        if(!moved.empty())
            failures+="; "+step+": moved"+moved;

        if(field->isEditing())
        {
            const gweni::Rect box=field->getEditBox()->getBounds();
            if(box.x != 0 || box.y != 0 || box.w != fieldRest.w || box.h != fieldRest.h)
                failures+="; "+step+": the box is "+std::to_string(box.x)+","+std::to_string(box.y)+","
                    +std::to_string(box.w)+"x"+std::to_string(box.h)+", not 0,0,"+std::to_string(fieldRest.w)
                    +"x"+std::to_string(fieldRest.h);
        }
        if(layout.passes != 1 || layout.renderLayout || layout.visited > bound)
            failures+="; "+step+": "+std::to_string(layout.passes)+" update-phase passes"
                +(layout.renderLayout ? " and a render-phase one" : "")+", "+std::to_string(layout.visited)
                +" entries (at most "+std::to_string(bound)+": the field's subtree and ancestors)";
        const SplitDifferences diff=differencesSplit(s, before, framePixels(s), field,
            focusedBefore ? focusedBefore : field);
        if(diff.outside != 0)
            failures+="; "+step+": "+std::to_string(diff.outside)+" pixels changed outside the field"
                " and the control that had the focus";
        std::cout<<"showcase_drag_number "<<step<<" passes="<<layout.passes<<" layout_visited="<<layout.visited
            <<" bound="<<bound<<" outside="<<diff.outside<<"\n";
    };

    const gweni::Point at=centreOf(field);
    std::vector<uint8_t> before=framePixels(s);
    gweni::controls::Base *focused=gweni::KeyboardFocus;
    doubleClickMouse(s, at.x, at.y);
    renderFrames(s, 1);
    const bool opened=field->isEditing();
    checkFrame("double-click", before, focused);
    for(const char *key : {"4", "2"})
        sendKey(s, key);
    renderFrames(s, s.frames);
    const bool typed=field->isEditing() && field->getEditBox()->getText() == "42";
    if(!shoot(s, shotDir+"/interaction-showcase-drag-number.png"))
        ok=false;

    before=framePixels(s);
    sendKey(s, "escape");
    renderFrames(s, 1);
    const bool cancelled=!field->isEditing() && field->getText() == shown;
    checkFrame("escape", before, nullptr);
    renderFrames(s, s.frames);

    before=framePixels(s);
    sendKey(s, "return");
    renderFrames(s, 1);
    const bool reopened=field->isEditing();
    checkFrame("return-open", before, nullptr);
    renderFrames(s, s.frames);

    before=framePixels(s);
    sendKey(s, "return");
    renderFrames(s, 1);
    const bool committed=!field->isEditing() && field->getText() == shown;
    checkFrame("return-commit", before, nullptr);
    renderFrames(s, s.frames);
    checkSteady(s, checks, "showcase_drag_number_steady");

    gweni::KeyboardFocus=nullptr;
    forgetMouse(s);
    dropControl(s, control);

    checks.check("showcase_drag_number_edit", opened && typed && cancelled && reopened && committed,
        std::string(opened ? "" : "a double-click did not open the edit box; ")
        +(typed ? "" : "typing 42 did not reach the box; ")
        +(cancelled ? "" : "Escape did not close it with the value kept; ")
        +(reopened ? "" : "Return on the focused field did not open it; ")
        +(committed ? "" : "Return did not commit the shown value"));
    checks.check("showcase_drag_number_layout", failures.empty(),
        "the edit box opening or closing changed the layout outside the field"+failures);
}

// VectorFieldDemo: the tagged float3's Y component pressed and dragged a unit
// at a time: every drag frame leaves nothing pending outside that component's
// field (and its Text), writes no more than its one primitive slot and the new
// string's glyphs, changes no pixel outside it and fires onChanged once, for
// component 1, when the value changes. The release and a settled frame write
// nothing, and Tab from the focused Y field focuses the Z field.
static void checkVectorField(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *control=addControl(s, "VectorFieldDemo");
    auto *demo=gweni::gweni_cast<gweni::VectorFieldDemo>(control);
    renderFrames(s, s.frames);

    gweni::controls::VectorField *vector=demo?demo->taggedField():nullptr;
    if(!vector || vector->getComponentCount() < 3)
    {
        checks.check("vector_field_found", false, "VectorFieldDemo has no tagged float3");
        dropControl(s, control);
        return;
    }

    struct Changes: public gweni::event::Handler
    {
        std::vector<size_t> components;
        void changed(gweni::event::Info info) { components.push_back(info.integer); }
    } changes;
    vector->onChanged.add(&changes, &Changes::changed);

    gweni::controls::DragNumber *field=vector->getField(1);
    const gweni::Point grab=centreOf(field);
    moveMouse(s, grab.x, grab.y);
    renderFrames(s, s.frames);
    mouseDown(s, grab.x, grab.y, true);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> pressed=framePixels(s);

    std::string frames;
    int changed=0;
    bool alone=true, bounded=true, events=true;
    gweni::Point at=grab;

    for(int i=0; i < 6; ++i)
    {
        const gweni::String shown=field->getText();
        const size_t eventsBefore=changes.components.size();
        size_t outside=0;

        at.x+=1;
        moveMouse(s, at.x, at.y);
        countPendingBesides(s.canvas, nullptr, field, outside);
        renderFrames(s, 1);

        const gweni::renderer::SlotRenderer::FrameStats &gpu=s.renderer->getFrameStats();
        const gweni::String now=field->getText();

        frames+=" "+std::to_string(field->getNumber())+":\""+now+"\",outside="+std::to_string(outside)
            +",prim="+std::to_string(gpu.primitiveSlotsWritten)+",text="+std::to_string(gpu.textSlotsWritten);
        if(outside != 0)
            alone=false;
        if(now != shown)
            ++changed;
        if(gpu.primitiveSlotsWritten > 1 || gpu.textSlotsWritten > now.size()
            || (now == shown && (gpu.primitiveSlotsWritten != 0 || gpu.textSlotsWritten != 0)))
            bounded=false;
        const size_t fired=changes.components.size()-eventsBefore;
        if(fired > 1 || (fired == 1 && changes.components.back() != 1))
            events=false;
    }

    const SplitDifferences dragDiff=differencesSplit(s, pressed, framePixels(s), field, field);
    if(!shoot(s, shotDir+"/interaction-vector-field.png"))
        ok=false;

    mouseUp(s, s.mouse.x, s.mouse.y);
    renderFrames(s, s.frames);
    checkSteady(s, checks, "vector_field_steady");

    // the press focused the Y field; Tab steps to Z
    const bool focused=gweni::KeyboardFocus == field;
    sendKey(s, "tab");
    renderFrames(s, s.frames);
    const bool tabbed=gweni::KeyboardFocus == vector->getField(2);
    gweni::KeyboardFocus=nullptr;

    vector->onChanged.removeHandler(&changes);
    forgetMouse(s);
    dropControl(s, control);

    std::cout<<"vector_field_drag"<<frames<<"\n";
    checks.check("vector_field_alone", alone && dragDiff.outside == 0,
        "a drag frame of the Y component had changes pending outside its field, or the drag changed "
        +std::to_string(dragDiff.outside)+" pixels outside it:"+frames);
    checks.check("vector_field_bounded", changed > 0 && bounded,
        "a drag frame wrote more than the Y field's slot and its new string's glyphs, one keeping the"
        " string wrote slots, or no frame changed it:"+frames);
    checks.check("vector_field_changed", events && changes.components.size() == size_t(changed),
        "onChanged fired "+std::to_string(changes.components.size())+" times for "+std::to_string(changed)
        +" changes, or for another component than 1");
    checks.check("vector_field_tab", focused && tabbed,
        std::string(focused ? "" : "the press did not focus the Y field; ")
        +(tabbed ? "" : "Tab did not move the focus to the Z field"));
}

static bool runInteractionScenario(Session &s, const std::string &shotDir, bool &passed)
{
    // Frozen like --stats, so a frame with no input is genuinely idle (no
    // caret blink); a scripted press still steps it.
    s.freezeClock=true;

    InteractionChecks checks;
    bool ok=true;
    checkButton(s, shotDir, checks, ok);
    checkToggle(s, shotDir, checks, ok);
    checkFocusHandOff(s, shotDir, checks, ok);
    checkTreeCollapse(s, shotDir, checks, ok);
    checkSelectable(s, shotDir, checks, ok);
    checkCheckBox(s, shotDir, checks, ok);
    checkWindowClose(s, shotDir, checks, ok);
    checkSliderDrag(s, shotDir, checks, ok);
    checkSliderText(s, shotDir, checks, ok);
    checkTabClose(s, shotDir, checks, ok);
    checkTabOverflow(s, shotDir, checks, ok);
    checkTabControlSelect(s, shotDir, checks, ok);
    checkPropertyRow(s, shotDir, checks, ok);
    checkColorEdit(s, shotDir, checks, ok);
    checkDataTable(s, shotDir, checks, ok);
    checkDragNumber(s, shotDir, checks, ok);
    checkShowcaseDragNumber(s, shotDir, checks, ok);
    checkVectorField(s, shotDir, checks, ok);

    std::cout<<"interaction";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second ? "yes" : "no");
    std::cout<<"\n";

    passed=checks.allPassed();
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario clip: children are cut at their clipping parent's edges
//
// Two scenes on the bare canvas, each compared against the frame with less in
// it, so a pixel drawn past an edge (or left behind from an earlier frame)
// shows as a difference. The comparisons are whole frames, or every pixel
// outside one control's bounds.
//-----------------------------------------------------------------------------

static std::vector<uint8_t> framePixels(const Session &s)
{
    std::vector<uint8_t> frame;
    s.renderer->readPixels(s.pixels.x, s.pixels.y, frame);
    return frame;
}

// The pixels of two frames that differ outside a control's bounds (snapped to
// device pixels as the renderer snaps them).
static size_t differencesOutside(const Session &s, const std::vector<uint8_t> &a,
    const std::vector<uint8_t> &b, gweni::controls::Base *control)
{
    const gweni::Point origin=control->localPosToCanvas();
    const gweni::Point topLeft=devicePixels(origin, s.scale);
    const gweni::Point bottomRight=devicePixels(origin+control->getSize(), s.scale);

    size_t count=0;
    for(int y=0; y<s.pixels.y; ++y)
    {
        for(int x=0; x<s.pixels.x; ++x)
        {
            if(x >= topLeft.x && x < bottomRight.x && y >= topLeft.y && y < bottomRight.y)
                continue;
            const size_t at=(static_cast<size_t>(y)*s.pixels.x+x)*4;
            if(std::memcmp(&a[at], &b[at], 4) != 0)
                ++count;
        }
    }
    return count;
}

static void countPending(gweni::controls::Base *control, size_t &count)
{
    if(control->getStateChange() != gweni::controls::StateChange_Nothing)
        ++count;
    for(auto &&child : control->getActualChildren())
        countPending(child, count);
}

// One frame whose update phase is inspected before it renders: how many
// controls have a change pending, in the whole tree and under `root`.
static void renderInspectedFrame(Session &s, gweni::controls::Base *root,
    size_t &pending, size_t &pendingUnderRoot)
{
    gweni::controls::resetChangeStats();
    s.canvas->updateCanvas();

    pending=0;
    pendingUnderRoot=0;
    countPending(s.canvas, pending);
    countPending(root, pendingUnderRoot);

    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();

    if(s.stats)
        printFrameStats(s.frame, s.label, gweni::controls::getChangeStats(),
            s.renderer->getFrameStats(), s.canvas->getLayoutStats(),
            s.canvas->getUpdateStats());
    checkOnePass(s, std::to_string(s.frame));
    ++s.frame;
}

// A frame with no input once a step has settled: nothing changes, nothing is
// uploaded (the clip boxes included).
static void checkSteady(Session &s, InteractionChecks &checks, const std::string &name)
{
    renderFrames(s, 1);
    const gweni::controls::ChangeStats changes=gweni::controls::getChangeStats();
    const gweni::renderer::SlotRenderer::FrameStats gpu=s.renderer->getFrameStats();

    checks.check(name, changes.controls == 0 && gpu.uploadBytes == 0
        && gpu.primitiveSlotsWritten == 0 && gpu.textSlotsWritten == 0,
        "a settled frame had controls_changed="+std::to_string(changes.controls)
        +" upload_bytes="+std::to_string(gpu.uploadBytes)
        +" prim_slots_written="+std::to_string(gpu.primitiveSlotsWritten)
        +" text_slots_written="+std::to_string(gpu.textSlotsWritten));
}

// Where the scene puts a control, kept by layout: a dock-None child is otherwise
// aligned to its parent's top-left, and a size flag other than Fixed lets its
// preferred size win.
static void place(gweni::controls::Base *control, int x, int y, int w, int h)
{
    control->setAlignment(gweni::Alignment::Absolute);
    control->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
    control->setBounds(x, y, w, h);
}

// The same at the control's preferred size.
static void place(gweni::controls::Base *control, int x, int y)
{
    control->setAlignment(gweni::Alignment::Absolute);
    control->setPos(x, y);
}

static std::vector<uint8_t> settledFrame(Session &s, const std::string &shotDir,
    const std::string &name, bool &ok)
{
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/clip-"+name+".png"))
        ok=false;
    return framePixels(s);
}

// (1) A ScrollControl with no scroll bars, holding content bigger than itself
// that runs past its right and bottom edges: a flat fill, a Button (rounded
// rect and text), a checked CheckBox (icon) and a Label. Then (3) the content
// is moved down below the ScrollControl, where it must leave nothing behind.
static void checkScrollContent(Session &s, const std::string &shotDir,
    const std::vector<uint8_t> &empty, InteractionChecks &checks, bool &ok)
{
    auto *scroll=s.canvas->newChild<gweni::controls::ScrollControl>();
    scroll->setScroll(false, false);
    place(scroll, 40, 40, 240, 160);

    std::vector<gweni::controls::Base *> content;

    auto *fill=scroll->newChild<gweni::TextDemoLightPanel>();
    place(fill, 150, 90, 200, 140);
    content.push_back(fill);

    auto *button=scroll->newChild<gweni::controls::Button>();
    button->setText("A Button wider than its ScrollControl");
    place(button, 10, 10, 300, 40);
    content.push_back(button);

    auto *check=scroll->newChild<gweni::controls::CheckBox>();
    check->setChecked(true);
    place(check, 228, 64);
    content.push_back(check);

    auto *label=scroll->newChild<gweni::controls::Label>();
    label->setText("A Label that runs on past the right edge");
    place(label, 120, 100);
    content.push_back(label);

    const std::vector<uint8_t> inside=settledFrame(s, shotDir, "scroll", ok);
    const size_t outside=differencesOutside(s, inside, empty, scroll);
    checks.check("scroll_cut", outside == 0,
        std::to_string(outside)+" pixels outside the ScrollControl differ from the empty canvas");
    checks.check("scroll_drawn", inside != empty, "the ScrollControl's content drew nothing");
    checkSteady(s, checks, "scroll_steady");

    for(gweni::controls::Base *child : content)
        child->setPos(child->getX(), child->getY()+200);

    const std::vector<uint8_t> out=settledFrame(s, shotDir, "scroll-out", ok);
    checks.check("scroll_out_gone", out == empty,
        "content moved out of the ScrollControl still draws (or left a stale copy)");
    checkSteady(s, checks, "scroll_out_steady");

    dropControl(s, scroll);
}

// Where the mouse holds a window's title bar, from the window's top-left.
static const gweni::Point g_titleGrip(40, 12);

static void dragTitleTo(Session &s, gweni::controls::Base *parent, const gweni::Point &local)
{
    const gweni::Point at=parent->localPosToCanvas()+local+g_titleGrip;
    moveMouse(s, at.x, at.y);
}

// (2) A Window in a parent smaller than the canvas, dragged by its title bar
// partly past the parent's right and bottom edges, then its left and top ones,
// then (3) right out of it, and back to where it started.
static void checkWindowDrag(Session &s, const std::string &shotDir,
    const std::vector<uint8_t> &empty, InteractionChecks &checks, bool &ok)
{
    auto *parent=s.canvas->newChild<gweni::TextDemoLightPanel>();
    place(parent, 120, 90, 400, 300);
    const std::vector<uint8_t> parentOnly=settledFrame(s, shotDir, "window-parent", ok);

    const gweni::Point start(100, 80);
    auto *window=parent->newChild<gweni::controls::WindowControl>();
    window->setTitle("Clipped Window");
    window->setClampMovement(false);
    place(window, start.x, start.y, 200, 140);
    // a Button fills the content area, so the window's subtree holds more than
    // its own surface
    auto *button=window->newChild<gweni::controls::Button>();
    button->setText("Inside the Window");
    button->setDock(gweni::DockPosition::Center);

    dragTitleTo(s, parent, start);
    const std::vector<uint8_t> inside=settledFrame(s, shotDir, "window-inside", ok);
    checks.check("window_drawn", inside != parentOnly, "the Window drew nothing");

    mouseDown(s, s.mouse.x, s.mouse.y, true);
    renderFrames(s, s.frames);

    // one drag step, inspected: only the window's subtree may have changed
    dragTitleTo(s, parent, start+gweni::Point(10, 10));
    size_t pending=0, pendingInWindow=0;
    renderInspectedFrame(s, window, pending, pendingInWindow);
    std::cout<<"clip drag controls_changed="<<pending<<" in_window="<<pendingInWindow<<"\n";
    checks.check("drag_only_window", pending > 0 && pending == pendingInWindow,
        std::to_string(pending-pendingInWindow)+" of the "+std::to_string(pending)
        +" controls changed by a drag step are outside the Window");

    const struct { const char *name; gweni::Point local; } edges[]=
    {
        {"window-bottomright", gweni::Point(290, 220)},
        {"window-topleft", gweni::Point(-70, -50)},
    };
    for(const auto &edge : edges)
    {
        dragTitleTo(s, parent, edge.local);
        const std::vector<uint8_t> frame=settledFrame(s, shotDir, edge.name, ok);
        const size_t outside=differencesOutside(s, frame, empty, parent);
        checks.check(std::string(edge.name)+"_cut", outside == 0,
            std::to_string(outside)+" pixels outside the parent differ from the empty canvas");
        checks.check(std::string(edge.name)+"_drawn", frame != parentOnly,
            "the Window drew nothing inside its parent");
    }

    dragTitleTo(s, parent, gweni::Point(420, 100));
    const std::vector<uint8_t> out=settledFrame(s, shotDir, "window-out", ok);
    checks.check("window_out_gone", out == parentOnly,
        "a Window dragged out of its parent still draws (or left a stale copy)");
    checkSteady(s, checks, "window_out_steady");

    dragTitleTo(s, parent, start);
    mouseUp(s, s.mouse.x, s.mouse.y);
    const std::vector<uint8_t> back=settledFrame(s, shotDir, "window-back", ok);
    checks.check("window_back", back == inside,
        "the Window dragged back to where it started does not look as it did");
    checkSteady(s, checks, "window_back_steady");

    forgetMouse(s);
    dropControl(s, parent);
}

// The pixels of `frame` inside `control`'s on-canvas device rect that differ
// from `reference` read `shift` device pixels further right and down, where the
// same control was drawn whole. A channel may be 1 off: the GPU's interpolation
// of a gradient is not exactly the same at every position (a Window moved
// between two positive positions already differs by that much).
static size_t differencesShifted(const Session &s, const std::vector<uint8_t> &frame,
    const std::vector<uint8_t> &reference, gweni::controls::Base *control,
    const gweni::Point &shift)
{
    const gweni::Point origin=control->localPosToCanvas();
    const gweni::Point topLeft=devicePixels(origin, s.scale);
    const gweni::Point bottomRight=devicePixels(origin+control->getSize(), s.scale);

    size_t count=0;
    for(int y=std::max(topLeft.y, 0); y<std::min(bottomRight.y, s.pixels.y); ++y)
    {
        for(int x=std::max(topLeft.x, 0); x<std::min(bottomRight.x, s.pixels.x); ++x)
        {
            const int rx=x+shift.x;
            const int ry=y+shift.y;
            if(rx < 0 || ry < 0 || rx >= s.pixels.x || ry >= s.pixels.y)
            {
                ++count;
                continue;
            }
            const size_t at=(static_cast<size_t>(y)*s.pixels.x+x)*4;
            const size_t refAt=(static_cast<size_t>(ry)*s.pixels.x+rx)*4;
            for(int c=0; c<4; ++c)
            {
                if(std::abs(int(frame[at+c])-int(reference[refAt+c])) > 1)
                {
                    ++count;
                    break;
                }
            }
        }
    }
    return count;
}

// (4) A Window on the canvas itself, dragged by its title bar past the canvas's
// own top-left corner, so its rects straddle device x=0 and y=0 (a primitive's
// corner at a negative device coordinate). Everything outside it must be the
// empty canvas, and its on-canvas part the matching part of the same Window
// drawn whole, shifted. The drag is a multiple of 8 logical units on each
// axis, so the shift is whole device pixels at scales 1.25, 1.5 and 2 too. The
// grip stays on the canvas.
static void checkWindowPastCanvasOrigin(Session &s, const std::string &shotDir,
    const std::vector<uint8_t> &empty, InteractionChecks &checks, bool &ok)
{
    const gweni::Point start(60, 56);
    const gweni::Point past(-28, -8);
    auto *window=s.canvas->newChild<gweni::controls::WindowControl>();
    window->setTitle("Clipped Window");
    window->setClampMovement(false);
    place(window, start.x, start.y, 200, 140);
    auto *button=window->newChild<gweni::controls::Button>();
    button->setText("Inside the Window");
    button->setDock(gweni::DockPosition::Center);

    // laid out before the mouse comes, and the reference taken with the title bar
    // held, as the shot past the corner is
    renderFrames(s, s.frames);
    dragTitleTo(s, s.canvas, start);
    mouseDown(s, s.mouse.x, s.mouse.y, true);
    const std::vector<uint8_t> whole=settledFrame(s, shotDir, "canvas-window", ok);
    checks.check("canvas_window_drawn", whole != empty, "the Window on the canvas drew nothing");

    dragTitleTo(s, s.canvas, past);
    const std::vector<uint8_t> frame=settledFrame(s, shotDir, "canvas-topleft", ok);
    const size_t outside=differencesOutside(s, frame, empty, window);
    checks.check("canvas_topleft_cut", outside == 0,
        std::to_string(outside)+" pixels outside the Window past the canvas's top-left corner"
        " differ from the empty canvas");
    const gweni::Point shift=devicePixels(start, s.scale)-devicePixels(past, s.scale);
    const size_t moved=differencesShifted(s, frame, whole, window, shift);
    checks.check("canvas_topleft_drawn", frame != empty && moved == 0,
        std::to_string(moved)+" pixels of the Window past the canvas's top-left corner"
        " differ from the same Window drawn whole");
    checkSteady(s, checks, "canvas_topleft_steady");

    dragTitleTo(s, s.canvas, start);
    const std::vector<uint8_t> back=settledFrame(s, shotDir, "canvas-back", ok);
    checks.check("canvas_back", back == whole,
        "the Window dragged back onto the canvas does not look as it did");
    mouseUp(s, s.mouse.x, s.mouse.y);

    forgetMouse(s);
    dropControl(s, window);
}

static bool runClipScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;
    s.label="clip";

    InteractionChecks checks;
    checks.scenario="clip";
    bool ok=true;

    const std::vector<uint8_t> empty=settledFrame(s, shotDir, "empty", ok);
    checkScrollContent(s, shotDir, empty, checks, ok);
    checkWindowDrag(s, shotDir, empty, checks, ok);
    checkWindowPastCanvasOrigin(s, shotDir, empty, checks, ok);

    std::cout<<"clip";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second ? "yes" : "no");
    std::cout<<"\n";

    passed=checks.allPassed();
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario tooltip: the canvas's retained tooltip, on ButtonDemo
//
// The clock steps here even under --stats: the hover delay is what is checked.
// Each check compares whole frames or the pixels inside one rect, and reads the
// canvas's tooltip state (Canvas::toolTipShown, toolTipControl, toolTipOwner).
//-----------------------------------------------------------------------------

static gweni::Rect canvasRectOf(gweni::controls::Base *control)
{
    const gweni::Point origin=control->localPosToCanvas();
    return gweni::Rect(origin.x, origin.y, control->getWidth(), control->getHeight());
}

// The pixels of two frames that differ inside a rect in logical units (snapped
// to device pixels as the renderer snaps it), clipped to the framebuffer.
static size_t differencesIn(const Session &s, const std::vector<uint8_t> &a,
    const std::vector<uint8_t> &b, const gweni::Rect &rect)
{
    const gweni::Point topLeft=devicePixels(gweni::Point(rect.x, rect.y), s.scale);
    const gweni::Point bottomRight=devicePixels(gweni::Point(rect.x+rect.w, rect.y+rect.h), s.scale);

    size_t count=0;
    for(int y=std::max(0, topLeft.y); y<std::min(s.pixels.y, bottomRight.y); ++y)
    {
        for(int x=std::max(0, topLeft.x); x<std::min(s.pixels.x, bottomRight.x); ++x)
        {
            const size_t at=(static_cast<size_t>(y)*s.pixels.x+x)*4;
            if(std::memcmp(&a[at], &b[at], 4) != 0)
                ++count;
        }
    }
    return count;
}

// The `skip`-th visible control under `root` with a tooltip, depth first.
static gweni::controls::Base *findWithToolTip(gweni::controls::Base *root, int skip)
{
    std::vector<gweni::controls::Base *> stack(1, root);
    while(!stack.empty())
    {
        gweni::controls::Base *at=stack.back();
        stack.pop_back();
        if(at->hidden())
            continue;
        if(at->hasToolTip() && skip-- == 0)
            return at;

        const auto &children=at->getActualChildren();
        for(auto it=children.rbegin(); it != children.rend(); ++it)
            stack.push_back(*it);
    }
    return nullptr;
}

static bool isUnder(gweni::controls::Base *control, gweni::controls::Base *root)
{
    for(gweni::controls::Base *at=control; at; at=at->getParent())
        if(at == root)
            return true;
    return false;
}

// The highest z index of a visible control outside `box`'s subtree.
static int highestZOutside(gweni::controls::Base *control, gweni::controls::Base *box)
{
    if(control == box || control->hidden())
        return 0;

    int highest=control->getZIndex();
    for(auto &&child : control->getActualChildren())
        highest=std::max(highest, highestZOutside(child, box));
    return highest;
}

// The controls with a change pending that are not under one of `allowed`;
// with `visibleOnly`, hidden subtrees (which render nothing) are left out.
static void pendingOutside(gweni::controls::Base *control,
    const std::vector<gweni::controls::Base *> &allowed, std::vector<std::string> &strays,
    bool visibleOnly=false)
{
    if(visibleOnly && control->hidden())
        return;

    if(control->getStateChange() != gweni::controls::StateChange_Nothing)
    {
        bool inside=false;
        for(gweni::controls::Base *root : allowed)
            inside=inside || isUnder(control, root);
        if(!inside)
            strays.push_back(std::string(control->getTypeName())+"#"+std::to_string(control->getControlId()));
    }
    for(auto &&child : control->getActualChildren())
        pendingOutside(child, allowed, strays, visibleOnly);
}

// One frame, its update phase inspected before it renders: the controls with a
// change pending outside `allowed` go to `strays`.
static void frameCheckingStrays(Session &s, const std::vector<gweni::controls::Base *> &allowed,
    std::vector<std::string> &strays, bool visibleOnly=false)
{
    gweni::controls::resetChangeStats();
    s.canvas->updateCanvas();
    pendingOutside(s.canvas, allowed, strays, visibleOnly);

    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();

    if(s.stats)
        printFrameStats(s.frame, s.label, gweni::controls::getChangeStats(),
            s.renderer->getFrameStats(), s.canvas->getLayoutStats(),
            s.canvas->getUpdateStats());
    checkOnePass(s, std::to_string(s.frame));
    ++s.frame;
    g_clockSeconds+=g_frameStep;
}

static std::string rectText(const gweni::Rect &r)
{
    return "("+std::to_string(r.x)+", "+std::to_string(r.y)+", "+std::to_string(r.w)
        +", "+std::to_string(r.h)+")";
}

static bool insideRect(const gweni::Rect &inner, const gweni::Rect &outer)
{
    return inner.x >= outer.x && inner.y >= outer.y
        && inner.x+inner.w <= outer.x+outer.w && inner.y+inner.h <= outer.y+outer.h;
}

static std::vector<uint8_t> toolTipShot(Session &s, const std::string &shotDir,
    const std::string &name, bool &ok)
{
    if(!shoot(s, shotDir+"/tooltip-"+name+".png"))
        ok=false;
    return framePixels(s);
}

// The frames from a move onto a control until its tooltip shows, at most `limit`.
static int framesUntilShown(Session &s, int limit)
{
    for(int frame=1; frame <= limit; ++frame)
    {
        renderFrames(s, 1);
        if(s.canvas->toolTipShown())
            return frame;
    }
    return -1;
}

static void checkToolTip(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Canvas *canvas=s.canvas;
    gweni::controls::Base *demo=addControl(s, "ButtonDemo");
    renderFrames(s, s.frames);

    gweni::controls::Base *first=findWithToolTip(demo, 0);
    gweni::controls::Base *second=findWithToolTip(demo, 1);
    if(!first || !second)
    {
        checks.check("buttons_found", false, "ButtonDemo has fewer than two controls with a tooltip");
        dropControl(s, demo);
        return;
    }
    // "Toggle Me" comes first in the demo; the one named for it is checked first
    gweni::controls::Base *withToolTip=(first->getY() > second->getY())?first:second;
    gweni::controls::Base *other=(withToolTip == first)?second:first;

    const gweni::Point off(s.size.x-1, s.size.y-1);    // ButtonDemo, no button
    const gweni::Rect canvasArea(0, 0, s.size.x, s.size.y);
    const gweni::Rect button=canvasRectOf(withToolTip);
    const int delayFrames=(int)std::ceil(canvas->getToolTipDelay()/g_frameStep);

    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> idle=toolTipShot(s, shotDir, "idle", ok);

    // Resting on the button: nothing but its hover shows until the delay.
    moveMouse(s, button.x+button.w/2, button.y+button.h/2);
    renderFrames(s, std::max(1, delayFrames-2));
    const bool early=canvas->toolTipShown();
    const std::vector<uint8_t> waiting=toolTipShot(s, shotDir, "waiting", ok);
    const size_t waitingOutside=differencesOutside(s, idle, waiting, withToolTip);
    const int shownAfter=framesUntilShown(s, 8);
    renderFrames(s, 1);
    const std::vector<uint8_t> shown=toolTipShot(s, shotDir, "shown", ok);

    gweni::controls::ToolTip *box=canvas->toolTipControl();
    checks.check("waits", !early && waitingOutside == 0,
        "the tooltip showed (or something outside the button changed: "+std::to_string(waitingOutside)
        +" px) "+std::to_string(std::max(1, delayFrames-2))+" frames into a "
        +std::to_string(canvas->getToolTipDelay())+" s delay");
    checks.check("shown", box && shownAfter > 0 && canvas->toolTipOwner() == withToolTip,
        "no tooltip for the button within 8 frames of its delay");
    if(!box || !canvas->toolTipShown())
    {
        dropControl(s, demo);
        return;
    }

    const gweni::Rect tip=canvasRectOf(box);
    checks.check("below", tip.x == button.x && tip.y == button.y+button.h+gweni::controls::ToolTip::Gap
        && insideRect(tip, canvasArea),
        "the box is at "+rectText(tip)+" for a button at "+rectText(button));
    checks.check("drawn", differencesIn(s, idle, shown, tip) > 0,
        "the box's rect "+rectText(tip)+" looks as it did before hovering");
    checks.check("on_top", box->getZIndex() > highestZOutside(canvas, box)
        && canvas->getActualChildren().back() == box,
        "the box's z "+std::to_string(box->getZIndex())+" is not above the rest of the tree ("
        +std::to_string(highestZOutside(canvas, box))+"), or it is not the canvas's last child");

    // Shown and settled, a frame and a move inside the button write nothing.
    renderFrames(s, 1);
    const gweni::renderer::SlotRenderer::FrameStats settled=s.renderer->getFrameStats();
    const size_t settledControls=gweni::controls::getChangeStats().controls;
    moveMouse(s, button.x+3, button.y+3);
    renderFrames(s, 1);
    const gweni::renderer::SlotRenderer::FrameStats moved=s.renderer->getFrameStats();
    const size_t movedControls=gweni::controls::getChangeStats().controls;
    checks.check("settled_zero", settledControls == 0 && settled.primitiveSlotsWritten == 0
        && settled.textSlotsWritten == 0 && movedControls == 0 && moved.primitiveSlotsWritten == 0
        && moved.textSlotsWritten == 0 && canvas->toolTipShown(),
        "with the tooltip shown, a settled frame had controls_changed="+std::to_string(settledControls)
        +" prim_slots_written="+std::to_string(settled.primitiveSlotsWritten)
        +" text_slots_written="+std::to_string(settled.textSlotsWritten)
        +" and a move inside the button controls_changed="+std::to_string(movedControls)
        +" prim_slots_written="+std::to_string(moved.primitiveSlotsWritten)
        +" text_slots_written="+std::to_string(moved.textSlotsWritten));

    // Straight onto the other button: the same box, at once, and nothing else
    // but the two buttons changes.
    const gweni::Rect otherRect=canvasRectOf(other);
    moveMouse(s, otherRect.x+otherRect.w/2, otherRect.y+otherRect.h/2);
    std::vector<std::string> strays;
    frameCheckingStrays(s, {box, withToolTip, other}, strays);
    const gweni::renderer::SlotRenderer::FrameStats handOver=s.renderer->getFrameStats();
    const gweni::Rect moveTip=canvasRectOf(box);
    std::string strayList;
    for(const std::string &stray : strays)
        strayList+=" "+stray;
    checks.check("reused", canvas->toolTipShown() && canvas->toolTipOwner() == other
        && canvas->toolTipControl() == box
        && moveTip.x == otherRect.x && moveTip.y == otherRect.y+otherRect.h+gweni::controls::ToolTip::Gap,
        "moving to the other button did not show the same box below it at once (box at "
        +rectText(moveTip)+", button at "+rectText(otherRect)+")");
    checks.check("reuse_bounded", strays.empty() && handOver.primitiveSlotsWritten >= 1,
        "the hand-over frame changed controls outside the box and the two buttons:"+strayList
        +" (prim_slots_written="+std::to_string(handOver.primitiveSlotsWritten)+")");
    renderFrames(s, s.frames);
    toolTipShot(s, shotDir, "other", ok);

    // Off both: hidden, and the frame is the idle one again.
    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> left=toolTipShot(s, shotDir, "left", ok);
    checks.check("hides", !canvas->toolTipShown() && box->hidden() && left == idle,
        "after moving off, the tooltip is still shown or the frame differs from the idle one in "
        +std::to_string(differencesIn(s, idle, left, canvasArea))+" px");

    // A press hides it, and it stays hidden while the mouse stays.
    moveMouse(s, button.x+button.w/2, button.y+button.h/2);
    const int again=framesUntilShown(s, delayFrames+8);
    mouseDown(s, button.x+button.w/2, button.y+button.h/2, true);
    renderFrames(s, 1);
    const bool pressHid=!canvas->toolTipShown();
    mouseUp(s, button.x+button.w/2, button.y+button.h/2);
    renderFrames(s, delayFrames+4);
    const bool stayedHidden=!canvas->toolTipShown();
    checks.check("press_hides", again > 0 && pressHid && stayedHidden,
        "shown again="+std::string(again > 0 ? "yes" : "no")+", hidden by the press="
        +(pressHid ? "yes" : "no")+", still hidden after the delay="+(stayedHidden ? "yes" : "no"));

    // A fast click: the move onto the button and the press in one frame (a
    // --click), from off it and from the other button's shown tooltip.
    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    clickMouse(s, button.x+button.w/2, button.y+button.h/2);
    renderFrames(s, delayFrames+4);
    const bool fastFromOff=!canvas->toolTipShown();
    moveMouse(s, otherRect.x+otherRect.w/2, otherRect.y+otherRect.h/2);
    const bool otherShown=framesUntilShown(s, delayFrames+8) > 0;
    clickMouse(s, button.x+button.w/2, button.y+button.h/2);
    renderFrames(s, delayFrames+4);
    const bool fastFromOther=!canvas->toolTipShown();
    checks.check("fast_click_hides", fastFromOff && otherShown && fastFromOther,
        "a click with no frame between the move and the press: from off the button the tooltip "
        +std::string(fastFromOff ? "stayed hidden" : "showed")+"; from the other button's tooltip ("
        +(otherShown ? "shown" : "not shown")+") it "+(fastFromOther ? "stayed hidden" : "showed"));

    // With a Menu open over where the box goes, the box looks as it does alone.
    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    moveMouse(s, button.x+button.w/2, button.y+button.h/2);
    framesUntilShown(s, delayFrames+8);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> alone=toolTipShot(s, shotDir, "alone", ok);

    gweni::controls::Menu *menu=canvas->newChild<gweni::controls::Menu>();
    menu->addItem("Menu item one");
    menu->addItem("Menu item two");
    menu->addItem("Menu item three");
    menu->open(gweni::Alignment::Left);
    menu->setPos(button.x+30, button.y+button.h+1);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> overMenu=toolTipShot(s, shotDir, "over-menu", ok);
    const gweni::Rect menuRect=canvasRectOf(menu);
    const gweni::Rect tipRect=canvasRectOf(box);
    const bool overlaps=menu->visible() && menuRect.x < tipRect.x+tipRect.w && tipRect.x < menuRect.x+menuRect.w
        && menuRect.y < tipRect.y+tipRect.h && tipRect.y < menuRect.y+menuRect.h;
    const size_t covered=differencesIn(s, alone, overMenu, tipRect);
    const size_t menuDrawn=differencesIn(s, alone, overMenu, menuRect);
    checks.check("over_menu", canvas->toolTipShown() && overlaps && covered == 0 && menuDrawn > 0
        && box->getZIndex() > highestZOutside(canvas, box),
        "the Menu at "+rectText(menuRect)+(overlaps ? "" : " (not over the box)")+" changed "
        +std::to_string(covered)+" px of the box at "+rectText(tipRect)+" ("
        +std::to_string(menuDrawn)+" px of its own rect)");

    menu->delayedDelete();
    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    dropControl(s, demo);
}

// A button in the canvas's bottom-right corner: its tooltip goes above it,
// pulled inside the right edge.
static void checkToolTipCorner(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Canvas *canvas=s.canvas;
    forgetMouse(s);
    s.label="tooltip-corner";

    gweni::controls::Button *corner=canvas->newChild<gweni::controls::Button>();
    corner->setText("Corner");
    corner->setToolTipText("A tooltip that does not fit below or to the right");
    place(corner, s.size.x-50, s.size.y-16, 50, 16);
    renderFrames(s, s.frames);

    const gweni::Rect button=canvasRectOf(corner);
    moveMouse(s, button.x+button.w/2, button.y+button.h/2);
    framesUntilShown(s, (int)std::ceil(canvas->getToolTipDelay()/g_frameStep)+8);
    renderFrames(s, 1);
    toolTipShot(s, shotDir, "corner", ok);

    gweni::controls::ToolTip *box=canvas->toolTipControl();
    const gweni::Rect tip=box ? canvasRectOf(box) : gweni::Rect();
    checks.check("corner", canvas->toolTipShown() && canvas->toolTipOwner() == corner
        && insideRect(tip, gweni::Rect(0, 0, s.size.x, s.size.y))
        && tip.y+tip.h == button.y-gweni::controls::ToolTip::Gap && tip.x+tip.w == s.size.x,
        "the corner button at "+rectText(button)+" has its tooltip at "+rectText(tip)
        +"; want it above the button and against the canvas's right edge");

    forgetMouse(s);
    corner->delayedDelete();
    renderFrames(s, s.frames);
}

static bool runToolTipScenario(Session &s, const std::string &shotDir, bool &passed)
{
    // The delay is what is checked, so the clock steps, under --stats too.
    s.freezeClock=false;
    s.statsClockSteps=true;

    InteractionChecks checks;
    checks.scenario="tooltip";
    bool ok=true;
    checkToolTip(s, shotDir, checks, ok);
    checkToolTipCorner(s, shotDir, checks, ok);

    std::cout<<"tooltip";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second ? "yes" : "no");
    std::cout<<"\n";

    passed=checks.allPassed();
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario context-menu: Base::setContextMenu, on ContextMenuDemo
//
// Right-clicks go through the GLFW translator as --click-right does. The clock
// steps (under --stats too), for the tooltip check's delay. Each check reads
// the canvas's context-menu state (Canvas::shownContextMenu,
// contextMenuOwner), the menus' canvas rects or whole frames.
//-----------------------------------------------------------------------------

static void rightClick(Session &s, const gweni::Point &at)
{
    clickMouse(s, at.x, at.y, GLFW_MOUSE_BUTTON_RIGHT);
}

static std::vector<uint8_t> contextMenuShot(Session &s, const std::string &shotDir,
    const std::string &name, bool &ok)
{
    if(!shoot(s, shotDir+"/context-menu-"+name+".png"))
        ok=false;
    return framePixels(s);
}

// One frame after the ones before it, with its counters: nothing may change.
static bool settledFrame(Session &s, std::string &why)
{
    renderFrames(s, 1);
    const gweni::renderer::SlotRenderer::FrameStats stats=s.renderer->getFrameStats();
    const size_t controls=gweni::controls::getChangeStats().controls;
    why="controls_changed="+std::to_string(controls)+" prim_slots_written="
        +std::to_string(stats.primitiveSlotsWritten)+" text_slots_written="+std::to_string(stats.textSlotsWritten);
    return controls == 0 && stats.primitiveSlotsWritten == 0 && stats.textSlotsWritten == 0;
}

static gweni::controls::MenuItem *menuItemNamed(gweni::controls::Menu *menu, const gweni::String &text)
{
    // its items are in its inner panel, which getChildren() lists
    for(gweni::controls::Base *child : menu->getChildren())
    {
        auto *item=gweni::gweni_cast<gweni::controls::MenuItem>(child);
        if(item && item->getText() == text)
            return item;
    }
    return nullptr;
}

static bool runContextMenuScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=false;
    s.statsClockSteps=true;

    InteractionChecks checks;
    checks.scenario="context-menu";
    bool ok=true;

    gweni::controls::Canvas *canvas=s.canvas;
    gweni::controls::Base *demo=addControl(s, "ContextMenuDemo");
    renderFrames(s, s.frames);

    auto *page=gweni::gweni_cast<gweni::ContextMenuDemo>(demo);
    gweni::controls::Base *document=findControl(demo, "GroupBox");
    gweni::controls::Base *corner=findControl(demo, "GroupBox", 1);
    gweni::controls::Base *help=findControl(demo, "Button");

    if(!page || !document || !corner || !help || !document->getContextMenu() || !corner->getContextMenu())
    {
        checks.check("demo_found", false, "ContextMenuDemo has no two boxes with context menus and a button");
        dropControl(s, demo);
        passed=false;
        return ok;
    }

    gweni::controls::Menu *documentMenu=document->getContextMenu();
    gweni::controls::Menu *cornerMenu=corner->getContextMenu();
    const gweni::Rect canvasArea(0, 0, s.size.x, s.size.y);
    const gweni::Rect documentRect=canvasRectOf(document);
    const gweni::Rect cornerRect=canvasRectOf(corner);
    const gweni::Rect helpRect=canvasRectOf(help);
    // the middle of the page, where nothing is
    const gweni::Point blank(s.size.x/2, s.size.y/2);
    const gweni::Point at(documentRect.x+documentRect.w-30, documentRect.y+50);
    const gweni::Point far(cornerRect.x+cornerRect.w-10, cornerRect.y+cornerRect.h-10);
    std::string why;

    moveMouse(s, blank.x, blank.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> idle=contextMenuShot(s, shotDir, "idle", ok);

    // A right-click in the document box: its menu with its top-left at the
    // cursor, and nothing visible outside the menu and the box changes.
    moveMouse(s, at.x, at.y);
    renderFrames(s, s.frames);
    rightClick(s, at);
    std::vector<std::string> strays;
    frameCheckingStrays(s, {documentMenu, document}, strays, true);
    const gweni::Rect opened=canvasRectOf(documentMenu);
    std::string strayList;
    for(const std::string &stray : strays)
        strayList+=" "+stray;
    checks.check("opens_at_cursor", canvas->shownContextMenu() == documentMenu
        && canvas->contextMenuOwner() == document && documentMenu->visible()
        && opened.x == at.x && opened.y == at.y,
        "a right-click at ("+std::to_string(at.x)+", "+std::to_string(at.y)+") left the menu at "
        +rectText(opened)+(documentMenu->visible() ? "" : ", hidden"));
    checks.check("open_bounded", strays.empty(),
        "the frame that opened it changed visible controls outside the menu and its box:"+strayList);
    const bool openSettled=settledFrame(s, why);
    checks.check("open_settled", openSettled, "a settled frame with the menu open had "+why);
    contextMenuShot(s, shotDir, "document", ok);

    // A left click outside closes it, back to the idle frame.
    clickMouse(s, blank.x, blank.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> clickedOff=contextMenuShot(s, shotDir, "clicked-off", ok);
    const bool closedSettled=settledFrame(s, why);
    checks.check("click_outside_closes", !canvas->shownContextMenu() && documentMenu->hidden()
        && clickedOff == idle && closedSettled,
        "after a click outside, shown="+std::string(canvas->shownContextMenu() ? "yes" : "no")
        +", the frame differs from the idle one in "
        +std::to_string(differencesIn(s, idle, clickedOff, canvasArea))+" px, settled: "+why);

    // Escape closes it, whatever has the focus.
    rightClick(s, at);
    renderFrames(s, s.frames);
    const bool reopened=canvas->shownContextMenu() == documentMenu;
    sendKey(s, "escape");
    moveMouse(s, blank.x, blank.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> escaped=contextMenuShot(s, shotDir, "escaped", ok);
    checks.check("escape_closes", reopened && !canvas->shownContextMenu() && documentMenu->hidden()
        && escaped == idle,
        "reopened="+std::string(reopened ? "yes" : "no")+", after Escape shown="
        +(canvas->shownContextMenu() ? "yes" : "no")+", "
        +std::to_string(differencesIn(s, idle, escaped, canvasArea))+" px differ from the idle frame");

    // Near the canvas's bottom-right corner: flipped to the cursor's left and
    // above it, inside the canvas.
    rightClick(s, far);
    renderFrames(s, s.frames);
    const gweni::Rect flipped=canvasRectOf(cornerMenu);
    checks.check("flips_in_corner", canvas->shownContextMenu() == cornerMenu && insideRect(flipped, canvasArea)
        && flipped.x+flipped.w == far.x && flipped.y+flipped.h == far.y,
        "a right-click at ("+std::to_string(far.x)+", "+std::to_string(far.y)+") put the corner menu at "
        +rectText(flipped));
    contextMenuShot(s, shotDir, "corner", ok);

    // A right-click on the other box while that one is open: it closes, this
    // one opens.
    rightClick(s, at);
    renderFrames(s, s.frames);
    checks.check("reopens_elsewhere", cornerMenu->hidden() && canvas->shownContextMenu() == documentMenu
        && documentMenu->visible(),
        "with the corner menu open, a right-click in the document box left the corner menu "
        +std::string(cornerMenu->hidden() ? "closed" : "open")+" and the document menu "
        +(documentMenu->visible() ? "open" : "closed"));

    // A disabled item does nothing; an enabled one is chosen and closes it.
    gweni::controls::MenuItem *deleteItem=menuItemNamed(documentMenu, "Delete");
    gweni::controls::MenuItem *copyItem=menuItemNamed(documentMenu, "Copy");
    const gweni::String before=page->m_chosen->getText();
    if(deleteItem)
    {
        const gweni::Point on=centreOf(deleteItem);
        clickMouse(s, on.x, on.y);
        renderFrames(s, s.frames);
    }
    const bool stayedOpen=deleteItem && documentMenu->visible() && page->m_chosen->getText() == before;
    if(copyItem)
    {
        const gweni::Point on=centreOf(copyItem);
        clickMouse(s, on.x, on.y);
        renderFrames(s, s.frames);
    }
    checks.check("item_chosen", stayedOpen && copyItem && documentMenu->hidden() && !canvas->shownContextMenu()
        && page->m_chosen->getText() == "Chosen: Copy",
        "the disabled item "+std::string(stayedOpen ? "did nothing" : "closed the menu or was chosen")
        +"; choosing Copy left the menu "+(documentMenu->hidden() ? "closed" : "open")
        +" and the label at \""+page->m_chosen->getText()+"\"");

    // The Help button's tooltip shows over a context menu open under it.
    const gweni::Point under(documentRect.x+documentRect.w-5, helpRect.y+helpRect.h+2);
    moveMouse(s, under.x, under.y);
    renderFrames(s, s.frames);
    rightClick(s, under);
    renderFrames(s, s.frames);
    moveMouse(s, helpRect.x+helpRect.w/2, helpRect.y+helpRect.h/2);
    framesUntilShown(s, (int)std::ceil(canvas->getToolTipDelay()/g_frameStep)+8);
    renderFrames(s, s.frames);
    contextMenuShot(s, shotDir, "tooltip", ok);
    gweni::controls::ToolTip *box=canvas->toolTipControl();
    const gweni::Rect tipRect=box ? canvasRectOf(box) : gweni::Rect();
    const gweni::Rect menuRect=canvasRectOf(documentMenu);
    const bool overlaps=menuRect.x < tipRect.x+tipRect.w && tipRect.x < menuRect.x+menuRect.w
        && menuRect.y < tipRect.y+tipRect.h && tipRect.y < menuRect.y+menuRect.h;
    checks.check("tooltip_above", box && canvas->toolTipShown() && documentMenu->visible() && overlaps
        && box->getZIndex() > highestZOutside(canvas, box) && canvas->getActualChildren().back() == box,
        "the tooltip at "+rectText(tipRect)+" (shown="+(canvas->toolTipShown() ? "yes" : "no")
        +") and the open menu at "+rectText(menuRect)+(overlaps ? "" : " do not overlap")
        +", or the box is not above the menu");
    moveMouse(s, blank.x, blank.y);
    clickMouse(s, blank.x, blank.y);
    renderFrames(s, s.frames);

    // Hiding the box closes its menu.
    rightClick(s, at);
    renderFrames(s, s.frames);
    const bool openBeforeHide=documentMenu->visible();
    document->setHidden(true);
    renderFrames(s, s.frames);
    checks.check("hidden_owner_closes", openBeforeHide && documentMenu->hidden() && !canvas->shownContextMenu(),
        "hiding the document box left its menu "+std::string(documentMenu->hidden() ? "closed" : "open"));
    document->setHidden(false);
    moveMouse(s, blank.x, blank.y);
    renderFrames(s, s.frames);

    // Deleting the corner box closes its menu and deletes it with the box.
    rightClick(s, far);
    renderFrames(s, s.frames);
    const bool openBeforeDelete=canvas->shownContextMenu() == cornerMenu;
    corner->delayedDelete();
    renderFrames(s, s.frames);
    const auto &children=canvas->getActualChildren();
    const bool menuGone=std::find(children.begin(), children.end(),
        static_cast<gweni::controls::Base *>(cornerMenu)) == children.end();
    checks.check("deleted_owner_closes", openBeforeDelete && !canvas->shownContextMenu() && menuGone,
        "deleting the corner box with its menu open left shown="
        +std::string(canvas->shownContextMenu() ? "yes" : "no")+", its menu "
        +(menuGone ? "deleted" : "still on the canvas"));

    forgetMouse(s);
    dropControl(s, demo);

    std::cout<<"context-menu";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second ? "yes" : "no");
    std::cout<<"\n";

    passed=checks.allPassed();
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario popup: controls::Popup, on PopupDemo
//
// Clicks go through the GLFW translator. The clock steps (under --stats too),
// for the tooltip check's delay. Each check reads the canvas's popup state
// (Canvas::shownPopup, popupAnchor), the popups' canvas rects or whole frames.
//-----------------------------------------------------------------------------

static std::vector<uint8_t> popupShot(Session &s, const std::string &shotDir,
    const std::string &name, bool &ok)
{
    if(!shoot(s, shotDir+"/popup-"+name+".png"))
        ok=false;
    return framePixels(s);
}

// The pixels of two frames that differ outside every one of `rects` (logical
// units, snapped to device pixels as the renderer snaps them).
static size_t differencesOutsideRects(const Session &s, const std::vector<uint8_t> &a,
    const std::vector<uint8_t> &b, const std::vector<gweni::Rect> &rects)
{
    std::vector<std::pair<gweni::Point, gweni::Point>> boxes;
    for(const gweni::Rect &rect : rects)
        boxes.push_back({devicePixels(gweni::Point(rect.x, rect.y), s.scale),
            devicePixels(gweni::Point(rect.x+rect.w, rect.y+rect.h), s.scale)});

    size_t count=0;
    for(int y=0; y<s.pixels.y; ++y)
    {
        for(int x=0; x<s.pixels.x; ++x)
        {
            bool inside=false;
            for(const auto &box : boxes)
                inside=inside || (x >= box.first.x && x < box.second.x && y >= box.first.y && y < box.second.y);
            if(inside)
                continue;
            const size_t at=(static_cast<size_t>(y)*s.pixels.x+x)*4;
            if(std::memcmp(&a[at], &b[at], 4) != 0)
                ++count;
        }
    }
    return count;
}

static bool overlap(const gweni::Rect &a, const gweni::Rect &b)
{
    return a.x < b.x+b.w && b.x < a.x+a.w && a.y < b.y+b.h && b.y < a.y+a.h;
}

static bool runPopupScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=false;
    s.statsClockSteps=true;

    InteractionChecks checks;
    checks.scenario="popup";
    bool ok=true;

    gweni::controls::Canvas *canvas=s.canvas;
    gweni::controls::Base *demo=addControl(s, "PopupDemo");
    renderFrames(s, s.frames);

    auto *page=gweni::gweni_cast<gweni::PopupDemo>(demo);
    gweni::controls::Base *help=findWithToolTip(demo, 0);

    if(!page || !page->m_options || !page->m_corner || !page->m_optionsPopup || !page->m_cornerPopup || !help)
    {
        checks.check("demo_found", false, "PopupDemo has no two buttons with popups and a button with a tooltip");
        dropControl(s, demo);
        passed=false;
        return ok;
    }

    gweni::controls::Button *options=page->m_options;
    gweni::controls::Button *corner=page->m_corner;
    gweni::controls::Popup *popup=page->m_optionsPopup;
    gweni::controls::Popup *cornerPopup=page->m_cornerPopup;
    const gweni::Rect canvasArea(0, 0, s.size.x, s.size.y);
    const gweni::Rect optionsRect=canvasRectOf(options);
    const gweni::Rect cornerRect=canvasRectOf(corner);
    const gweni::Rect helpRect=canvasRectOf(help);
    const gweni::Point onOptions=centreOf(options);
    const gweni::Point onCorner=centreOf(corner);
    // the middle of the page, where nothing is
    const gweni::Point blank(s.size.x/2, s.size.y/2);
    std::string why;

    moveMouse(s, blank.x, blank.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> idle=popupShot(s, shotDir, "idle", ok);

    // Clicking Options opens its popup below it; the frame that opened it
    // changes nothing outside the popup and the button. The mouse is on the
    // button first, so the page's hover leaving it is not in that frame.
    moveMouse(s, onOptions.x, onOptions.y);
    renderFrames(s, s.frames);
    clickMouse(s, onOptions.x, onOptions.y);
    std::vector<std::string> strays;
    frameCheckingStrays(s, {popup, options}, strays, true);
    renderFrames(s, s.frames);
    const gweni::Rect opened=canvasRectOf(popup);
    std::string strayList;
    for(const std::string &stray : strays)
        strayList+=" "+stray;
    checks.check("opens_below", popup->isOpen() && canvas->shownPopup() == popup
        && canvas->popupAnchor() == options && !canvas->shownContextMenu()
        && opened.x == optionsRect.x && opened.y == optionsRect.y+optionsRect.h+gweni::controls::Popup::Gap,
        "a click on Options left the popup at "+rectText(opened)+(popup->isOpen() ? "" : ", closed")
        +", the button at "+rectText(optionsRect));
    const std::vector<uint8_t> open=popupShot(s, shotDir, "open", ok);
    const size_t outside=differencesOutsideRects(s, idle, open, {opened, optionsRect});
    checks.check("open_bounded", strays.empty() && outside == 0,
        "opening it changed "+std::to_string(outside)+" px outside the popup and the button, and"
        " controls outside them:"+strayList);
    const bool openSettled=settledFrame(s, why);
    checks.check("open_settled", openSettled, "a settled frame with the popup open had "+why);

    // A click outside closes it, back to the idle frame.
    clickMouse(s, blank.x, blank.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> clickedOff=popupShot(s, shotDir, "clicked-off", ok);
    const bool closedSettled=settledFrame(s, why);
    checks.check("click_outside_closes", !popup->isOpen() && popup->hidden() && !canvas->shownPopup()
        && clickedOff == idle && closedSettled,
        "after a click outside, open="+std::string(popup->isOpen() ? "yes" : "no")
        +", the frame differs from the idle one in "
        +std::to_string(differencesIn(s, idle, clickedOff, canvasArea))+" px, settled: "+why);

    // A click on its button closes it only because the button toggles it.
    clickMouse(s, onOptions.x, onOptions.y);
    renderFrames(s, s.frames);
    const bool toggledOpen=popup->isOpen();
    clickMouse(s, onOptions.x, onOptions.y);
    renderFrames(s, s.frames);
    checks.check("anchor_toggles", toggledOpen && !popup->isOpen() && popup->hidden(),
        "a click on Options "+std::string(toggledOpen ? "opened" : "did not open")
        +" it and a second one left it "+(popup->isOpen() ? "open" : "closed"));

    // Escape closes it, back to the frame before it opened (the button has
    // the keyboard focus in both, from the clicks).
    moveMouse(s, blank.x, blank.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> beforeEscape=popupShot(s, shotDir, "before-escape", ok);
    clickMouse(s, onOptions.x, onOptions.y);
    renderFrames(s, s.frames);
    const bool reopened=popup->isOpen();
    sendKey(s, "escape");
    moveMouse(s, blank.x, blank.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> escaped=popupShot(s, shotDir, "escaped", ok);
    checks.check("escape_closes", reopened && !popup->isOpen() && popup->hidden() && escaped == beforeEscape,
        "reopened="+std::string(reopened ? "yes" : "no")+", after Escape open="
        +(popup->isOpen() ? "yes" : "no")+", "
        +std::to_string(differencesIn(s, beforeEscape, escaped, canvasArea))
        +" px differ from the frame before it opened");

    // The Corner button's popup, with no room below it or on its right:
    // above it, right edges aligned, inside the canvas.
    clickMouse(s, onCorner.x, onCorner.y);
    renderFrames(s, s.frames);
    const gweni::Rect flipped=canvasRectOf(cornerPopup);
    checks.check("flips_in_corner", cornerPopup->isOpen() && insideRect(flipped, canvasArea)
        && flipped.x+flipped.w == cornerRect.x+cornerRect.w
        && flipped.y+flipped.h == cornerRect.y-gweni::controls::Popup::Gap,
        "the Corner popup is at "+rectText(flipped)+", the button at "+rectText(cornerRect)
        +(cornerPopup->isOpen() ? "" : ", closed"));
    popupShot(s, shotDir, "corner", ok);

    // One at a time: opening Options closes the Corner one.
    clickMouse(s, onOptions.x, onOptions.y);
    renderFrames(s, s.frames);
    checks.check("one_at_a_time", popup->isOpen() && !cornerPopup->isOpen() && cornerPopup->hidden()
        && canvas->shownPopup() == popup,
        "with the Corner popup open, a click on Options left Options "
        +std::string(popup->isOpen() ? "open" : "closed")+" and Corner "
        +(cornerPopup->isOpen() ? "open" : "closed"));

    // A ComboBox in it opens its list over it; Escape closes the list
    // first, and choosing an item from it keeps the popup open.
    auto *zoom=gweni::gweni_cast<gweni::controls::ComboBox>(findControl(popup, "ComboBox"));
    auto openList=[&]() -> gweni::controls::Base *
    {
        const gweni::Point onZoom=centreOf(zoom);
        clickMouse(s, onZoom.x, onZoom.y);
        renderFrames(s, s.frames);
        for(gweni::controls::Base *child : canvas->getActualChildren())
        {
            if(gweni::gweni_cast<gweni::controls::Menu>(child) && child->visible())
                return child;
        }
        return nullptr;
    };
    bool listOver=false, escapedList=false, chosen=false;
    if(zoom)
    {
        if(gweni::controls::Base *list=openList())
        {
            listOver=list->getZIndex() > highestZOutside(popup, nullptr)
                && overlap(canvasRectOf(list), canvasRectOf(popup));
            popupShot(s, shotDir, "combo", ok);
            sendKey(s, "escape");
            renderFrames(s, s.frames);
            escapedList=list->hidden() && popup->isOpen();
        }
        if(gweni::controls::Base *list=openList())
        {
            const gweni::Point onItem=centreOf(list->getChildren().back());
            clickMouse(s, onItem.x, onItem.y);
            renderFrames(s, s.frames);
            chosen=list->hidden() && popup->isOpen() && zoom->getSelectedItem()
                && zoom->getSelectedItem()->getText() == "Zoom 200%";
        }
    }
    checks.check("combo_list_over", zoom && listOver && escapedList && chosen,
        "the popup's ComboBox list is "+std::string(listOver ? "" : "not ")+"shown over the popup, Escape "
        +(escapedList ? "closed the list only" : "did not close the list alone")+", choosing an item "
        +(chosen ? "kept the popup open" : "did not keep the popup open with the item selected"));
    if(!popup->isOpen())
    {
        clickMouse(s, onOptions.x, onOptions.y);
        renderFrames(s, s.frames);
    }

    // The Help button's tooltip shows over the open popup.
    moveMouse(s, helpRect.x+helpRect.w/2, helpRect.y+helpRect.h/2);
    framesUntilShown(s, (int)std::ceil(canvas->getToolTipDelay()/g_frameStep)+8);
    renderFrames(s, s.frames);
    popupShot(s, shotDir, "tooltip", ok);
    gweni::controls::ToolTip *box=canvas->toolTipControl();
    const gweni::Rect tipRect=box ? canvasRectOf(box) : gweni::Rect();
    const gweni::Rect popupRect=canvasRectOf(popup);
    const bool overlaps=overlap(tipRect, popupRect);
    checks.check("tooltip_above", box && canvas->toolTipShown() && popup->isOpen() && overlaps
        && box->getZIndex() > highestZOutside(canvas, box) && canvas->getActualChildren().back() == box,
        "the tooltip at "+rectText(tipRect)+" (shown="+(canvas->toolTipShown() ? "yes" : "no")
        +") and the open popup at "+rectText(popupRect)+(overlaps ? "" : " do not overlap")
        +", or the box is not above the popup");
    moveMouse(s, blank.x, blank.y);
    renderFrames(s, s.frames);

    // Hiding its button closes it.
    const bool openBeforeHide=popup->isOpen();
    options->setHidden(true);
    renderFrames(s, s.frames);
    checks.check("hidden_anchor_closes", openBeforeHide && !popup->isOpen() && popup->hidden()
        && !canvas->shownPopup(),
        "hiding Options left its popup "+std::string(popup->isOpen() ? "open" : "closed"));
    options->setHidden(false);
    renderFrames(s, s.frames);

    // The popups are the page's (Base::createPopup): deleting it deletes them.
    forgetMouse(s);
    dropControl(s, demo);
    size_t popupsLeft=0;
    for(gweni::controls::Base *child : canvas->getActualChildren())
    {
        if(gweni::gweni_cast<gweni::controls::Popup>(child))
            ++popupsLeft;
    }
    checks.check("deleted_with_demo", popupsLeft == 0 && !canvas->shownPopup(),
        "deleting the page left "+std::to_string(popupsLeft)+" popups on the canvas");

    // A canvas deleted with the page after its popups among its children (a
    // popup opened and closed, then the page brought to the front) deletes
    // them first; the page touches neither (#490: it did, a heap-use-after-free
    // in the ASAN tree).
    gweni::controls::Base *second=addControl(s, "PopupDemo");
    renderFrames(s, s.frames);
    auto *secondPage=gweni::gweni_cast<gweni::PopupDemo>(second);
    bool popupsFirst=false;
    if(secondPage && secondPage->m_options && secondPage->m_optionsPopup)
    {
        const gweni::Point onSecond=centreOf(secondPage->m_options);
        clickMouse(s, onSecond.x, onSecond.y);
        renderFrames(s, s.frames);
        sendKey(s, "escape");
        renderFrames(s, s.frames);
        second->bringToFront();
        renderFrames(s, s.frames);

        const auto &children=canvas->getActualChildren();
        const auto at=[&](gweni::controls::Base *control)
        {
            return std::distance(children.begin(), std::find(children.begin(), children.end(), control));
        };
        popupsFirst=at(secondPage->m_optionsPopup) < at(second) && at(secondPage->m_cornerPopup) < at(second);
    }
    forgetMouse(s);
    replaceCanvas(s);
    checks.check("teardown_popups_first", popupsFirst,
        "the second page's popups were not before it among the canvas's children");

    std::cout<<"popup";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second ? "yes" : "no");
    std::cout<<"\n";

    passed=checks.allPassed();
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario virtual-list: VirtualListDemo at 1 000 and 100 000 items
//
// The list holds a row for each one on screen plus one, whatever its count, so
// what a frame does is the same at both counts: the controls and ids it holds,
// a settled frame (nothing), a one-row scroll from the top and a wheel notch
// (the rows on screen moved and those that came into view bound), each within
// the list's subtree and its shown rows' slots.
//-----------------------------------------------------------------------------

namespace
{

struct VirtualListStep
{
    size_t primSlots=0;
    size_t textSlots=0;
    size_t visited=0;
    size_t zVisited=0;
    size_t changed=0;
    int passes=0;
    int requests=0;
    // what the rows on screen hold after the step: their controls' primitive
    // slots and their strings' glyphs
    size_t rowPrims=0;
    size_t rowGlyphs=0;
    // whether the step moved the vertical bar's thumb, which then rewrites its
    // one primitive slot
    bool thumbMoved=false;

    bool operator==(const VirtualListStep &o) const
    {
        return primSlots == o.primSlots && textSlots == o.textSlots && visited == o.visited
            && zVisited == o.zVisited && changed == o.changed && passes == o.passes
            && requests == o.requests && rowPrims == o.rowPrims && rowGlyphs == o.rowGlyphs;
    }
};

struct VirtualListRun
{
    size_t items=0;
    size_t rows=0;          // the pool
    size_t subtree=0;       // controls under the list, the list included
    size_t primIds=0;       // live primitive ids the demo holds
    size_t idleSlots=0;     // slots the settled frames wrote
    size_t idleChanged=0;   // and controls they changed
    size_t idsLeft=0;       // live primitive ids left once the demo is gone
    VirtualListStep row, wheel;
};

size_t subtreeControls(gweni::controls::Base *control)
{
    size_t count=1;

    for(gweni::controls::Base *child : control->getActualChildren())
        count+=subtreeControls(child);
    return count;
}

void addRowCost(gweni::controls::Base *control, VirtualListStep &step)
{
    if(const gweni::skin::Control *skin=control->getSkinControl())
        step.rowPrims+=skin->getPrimitives().size();
    if(auto *text=gweni::gweni_cast<gweni::controls::internal::Text>(control))
        step.rowGlyphs+=text->getText().size();

    for(gweni::controls::Base *child : control->getActualChildren())
        addRowCost(child, step);
}

}//namespace

static VirtualListStep virtualListFrame(Session &s, gweni::controls::VirtualList *list)
{
    renderFrames(s, 1);

    VirtualListStep step;
    const gweni::renderer::SlotRenderer::FrameStats &gpu=s.renderer->getFrameStats();
    const gweni::controls::ChangeStats &changes=gweni::controls::getChangeStats();

    step.primSlots=gpu.primitiveSlotsWritten;
    step.textSlots=gpu.textSlotsWritten;
    step.visited=changes.visited;
    step.changed=changes.controls;
    step.zVisited=s.canvas->getUpdateStats().zVisited;
    step.passes=s.canvas->getLayoutStats().passes;
    step.requests=s.canvas->getLayoutStats().requests;

    for(gweni::controls::Base *row : list->getRows())
    {
        if(!row->hidden())
            addRowCost(row, step);
    }
    return step;
}

static void printVirtualListStep(const char *name, const VirtualListStep &step)
{
    std::cout<<" "<<name<<"_prim_slots="<<step.primSlots<<" "<<name<<"_text_slots="<<step.textSlots
             <<" "<<name<<"_visited="<<step.visited<<" "<<name<<"_z_visited="<<step.zVisited
             <<" "<<name<<"_changed="<<step.changed<<" "<<name<<"_passes="<<step.passes
             <<" "<<name<<"_row_prims="<<step.rowPrims<<" "<<name<<"_row_glyphs="<<step.rowGlyphs
             <<" "<<name<<"_thumb_moved="<<(step.thumbMoved?1:0);
}

// The frames after a step: nothing to do, so nothing written or changed.
static void settleVirtualList(Session &s, gweni::controls::VirtualList *list, VirtualListRun &run)
{
    for(int i=0; i<2; ++i)
    {
        const VirtualListStep idle=virtualListFrame(s, list);

        run.idleSlots+=idle.primSlots+idle.textSlots;
        run.idleChanged+=idle.changed;
    }
}

static bool runVirtualList(Session &s, const std::string &shotDir, size_t items, VirtualListRun &run)
{
    const size_t idsBefore=livePrimitiveIds();
    gweni::controls::Base *control=addControl(s, "VirtualListDemo");
    auto *demo=gweni::gweni_cast<gweni::VirtualListDemo>(control);

    if(!demo || !demo->m_list)
    {
        std::cerr<<"ControlTest: virtual-list: VirtualListDemo has no list\n";
        return false;
    }

    gweni::controls::VirtualList *list=demo->m_list;

    s.label="VirtualListDemo-"+std::to_string(items);
    list->setItemCount(items);
    renderFrames(s, s.frames);

    run.items=items;
    run.rows=list->getRows().size();
    run.subtree=subtreeControls(list);
    run.primIds=livePrimitiveIds()-idsBefore;
    settleVirtualList(s, list, run);

    list->scrollByRows(1);
    run.row=virtualListFrame(s, list);
    settleVirtualList(s, list, run);

    // a wheel notch over the list, through the input path: three rows
    const gweni::Point origin=list->localPosToCanvas();

    moveMouse(s, origin.x+list->getWidth()/2, origin.y+list->getHeight()/2);
    renderFrames(s, 1);
    settleVirtualList(s, list, run);
    gweni::controls::Base *bar=findControl(list, "VerticalScrollBar");
    gweni::controls::Base *thumb=bar?findControl(bar, "ScrollBarBar"):nullptr;
    const gweni::Rect thumbBefore=thumb?thumb->getBounds():gweni::Rect();

    scrollWheel(s, -60);
    run.wheel=virtualListFrame(s, list);
    run.wheel.thumbMoved=thumb && !(thumb->getBounds() == thumbBefore);
    settleVirtualList(s, list, run);

    // the last item in view, for the picture
    forgetMouse(s);
    list->scrollToItem(items-1);
    renderFrames(s, s.frames);

    const bool shot=shoot(s, shotDir+"/virtual-list-"+std::to_string(items)+"-end.png");

    dropControl(s, control);
    renderFrames(s, 1);
    run.idsLeft=livePrimitiveIds()-idsBefore;
    return shot;
}

static bool runVirtualListScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    VirtualListRun small, large;
    bool ok=runVirtualList(s, shotDir, 1000, small);

    ok=runVirtualList(s, shotDir, 100000, large) && ok;

    auto withinRows=[](const VirtualListRun &run, const VirtualListStep &step)
    {
        // the list's subtree and the canvas and demo above it
        return step.passes == 1 && step.zVisited == 0 && step.requests == 0
            && step.visited <= run.subtree+2 && step.changed <= run.subtree
            && step.primSlots+step.textSlots > 0
            && step.primSlots <= step.rowPrims && step.textSlots <= step.rowGlyphs;
    };

    // The wheel's three rows (60 units) move the bar's thumb one pixel at
    // 1 000 items and not at all at 100 000 (in the demo's list, which fills
    // the canvas's height: a 436-unit viewport at 640x480), so that step's
    // controls differ by the thumb, and its primitive slots by the thumb's one
    // slot; the rest of its slots do not. thumbMoved takes either case, so a
    // height where the thumb moves at both counts or neither passes too.
    auto wheelPrimSlots=[](const VirtualListRun &run)
    {
        return run.wheel.primSlots-(run.wheel.thumbMoved?1:0);
    };
    const bool sameCost=small.rows == large.rows && small.subtree == large.subtree
        && small.primIds == large.primIds && small.row == large.row
        && wheelPrimSlots(small) == wheelPrimSlots(large)
        && small.wheel.textSlots == large.wheel.textSlots
        && small.wheel.rowPrims == large.wheel.rowPrims
        && small.wheel.rowGlyphs == large.wheel.rowGlyphs;
    const bool settled=small.idleSlots == 0 && small.idleChanged == 0
        && large.idleSlots == 0 && large.idleChanged == 0;
    const bool rowsOnly=withinRows(small, small.row) && withinRows(small, small.wheel)
        && withinRows(large, large.row) && withinRows(large, large.wheel);
    const bool idsBack=small.idsLeft == 0 && large.idsLeft == 0;

    for(const VirtualListRun *run : {&small, &large})
    {
        std::cout<<"virtual-list items="<<run->items<<" rows="<<run->rows<<" subtree="<<run->subtree
                 <<" prim_ids="<<run->primIds<<" idle_slots="<<run->idleSlots
                 <<" idle_changed="<<run->idleChanged;
        printVirtualListStep("row", run->row);
        printVirtualListStep("wheel", run->wheel);
        std::cout<<" ids_left="<<run->idsLeft<<"\n";
    }

    passed=sameCost && settled && rowsOnly && idsBack;

    std::cout<<"virtual-list same_cost="<<(sameCost?"yes":"no")<<" settled="<<(settled?"yes":"no")
             <<" rows_only="<<(rowsOnly?"yes":"no")<<" ids_back="<<(idsBack?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: virtual-list: "
                 <<(!sameCost ? "a frame at 100 000 items did not cost what it did at 1 000"
                     : !settled ? "a settled frame wrote slots or changed controls"
                     : !rowsOnly ? "a scroll wrote more than its shown rows' slots, entered more than"
                         " the list, renumbered z or took more than one layout pass"
                     : "the demo did not give its primitive ids back")<<"\n";
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario data-table-virtual: DataTableVirtualDemo at 1 000 and 100 000
// records
//
// The table's body is a VirtualList of pooled rows, so as for the virtual
// list, what a frame does is the same at both counts: the controls and ids it
// holds, a settled frame (nothing), a one-row scroll from the top and a wheel
// notch, each within the table's subtree and its shown rows' slots. A click on
// a header (the app sorting its records and calling rebind()) and a drag of a
// column's grip are each laid out in one update-phase pass.
//-----------------------------------------------------------------------------

namespace
{

struct DataTableVirtualRun
{
    size_t items=0;
    size_t rows=0;          // the pool
    size_t subtree=0;       // controls under the table, the table included
    size_t primIds=0;       // live primitive ids the demo holds
    size_t idleSlots=0;     // slots the settled frames wrote
    size_t idleChanged=0;   // and controls they changed
    size_t idsLeft=0;       // live primitive ids left once the demo is gone
    VirtualListStep row, wheel, sort, drag;
    bool sorted=false;      // the click sorted by the column, rows shown in its order
    bool resized=false;     // the drag gave the column its width
};

}//namespace

static void settleDataTableVirtual(Session &s, gweni::controls::VirtualList *body, DataTableVirtualRun &run)
{
    for(int i=0; i<2; ++i)
    {
        const VirtualListStep idle=virtualListFrame(s, body);

        run.idleSlots+=idle.primSlots+idle.textSlots;
        run.idleChanged+=idle.changed;
    }
}

static bool runDataTableVirtual(Session &s, const std::string &shotDir, size_t items, DataTableVirtualRun &run)
{
    const size_t idsBefore=livePrimitiveIds();
    gweni::controls::Base *control=addControl(s, "DataTableVirtualDemo");
    auto *demo=gweni::gweni_cast<gweni::DataTableVirtualDemo>(control);
    gweni::controls::DataTable *table=demo?demo->m_table:nullptr;
    gweni::controls::VirtualList *body=table?table->getVirtualBody():nullptr;

    if(!body || table->getColumnCount() < 3)
    {
        std::cerr<<"ControlTest: data-table-virtual: DataTableVirtualDemo has no source-mode table"
            " of three columns or more\n";
        return false;
    }

    s.label="DataTableVirtualDemo-"+std::to_string(items);
    demo->setRecordCount(items);
    renderFrames(s, s.frames);

    run.items=table->getItemCount();
    run.rows=body->getRows().size();
    run.subtree=subtreeControls(table);
    run.primIds=livePrimitiveIds()-idsBefore;
    settleDataTableVirtual(s, body, run);

    body->scrollByRows(1);
    run.row=virtualListFrame(s, body);
    settleDataTableVirtual(s, body, run);

    // a wheel notch over the body, through the input path: three rows
    const gweni::Point origin=body->localPosToCanvas();

    moveMouse(s, origin.x+body->getWidth()/2, origin.y+body->getHeight()/2);
    renderFrames(s, 1);
    settleDataTableVirtual(s, body, run);
    gweni::controls::Base *bar=findControl(body, "VerticalScrollBar");
    gweni::controls::Base *thumb=bar?findControl(bar, "ScrollBarBar"):nullptr;
    const gweni::Rect thumbBefore=thumb?thumb->getBounds():gweni::Rect();

    scrollWheel(s, -60);
    run.wheel=virtualListFrame(s, body);
    run.wheel.thumbMoved=thumb && !(thumb->getBounds() == thumbBefore);
    settleDataTableVirtual(s, body, run);

    // a click on the third header, clear of its grip: the demo sorts its
    // records by it and rebinds the rows on screen
    gweni::controls::internal::DataTableHeaderCell *header=table->getHeader()->getCells().at(2);
    const gweni::Point at=header->localPosToCanvas();

    clickMouse(s, at.x+header->getWidth()/3, at.y+header->getHeight()/2);
    run.sort=virtualListFrame(s, body);
    settleDataTableVirtual(s, body, run);

    run.sorted=table->getSortColumn() == 2
        && table->getSortDirection() == gweni::controls::SortDirection::Ascending;
    {
        // the rows on screen, top to bottom, in the column's order
        std::vector<std::pair<int, std::string>> shown;

        for(gweni::controls::Base *row : body->getRows())
        {
            auto *tableRow=gweni::gweni_cast<gweni::controls::DataTableRow>(row);

            if(tableRow && !row->hidden())
                shown.push_back({row->getY(), tableRow->getCellText(2)});
        }
        std::sort(shown.begin(), shown.end());
        for(size_t i=1; i < shown.size(); ++i)
        {
            if(std::atof(shown[i-1].second.c_str()) > std::atof(shown[i].second.c_str()))
                run.sorted=false;
        }
        if(shown.empty())
            run.sorted=false;
    }

    // the second column's grip dragged 30 units right
    gweni::controls::internal::DataTableColumnGrip *grip=table->getHeader()->getCells().at(1)->getGrip();
    const gweni::Point gripAt=centreOf(grip);
    const int width=table->getColumnWidth(1);

    mouseDown(s, gripAt.x, gripAt.y, true);
    moveMouse(s, gripAt.x+30, gripAt.y);
    mouseUp(s, gripAt.x+30, gripAt.y);
    run.drag=virtualListFrame(s, body);
    settleDataTableVirtual(s, body, run);
    run.resized=table->getColumnWidth(1) == width+30;

    forgetMouse(s);
    renderFrames(s, s.frames);

    const bool shot=shoot(s, shotDir+"/data-table-virtual-"+std::to_string(items)+".png");

    dropControl(s, control);
    renderFrames(s, 1);
    run.idsLeft=livePrimitiveIds()-idsBefore;
    return shot;
}

static bool runDataTableVirtualScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    DataTableVirtualRun small, large;
    bool ok=runDataTableVirtual(s, shotDir, 1000, small);

    ok=runDataTableVirtual(s, shotDir, 100000, large) && ok;

    auto withinRows=[](const DataTableVirtualRun &run, const VirtualListStep &step)
    {
        // the table's subtree and the canvas and demo above it
        return step.passes == 1 && step.zVisited == 0 && step.requests == 0
            && step.visited <= run.subtree+2 && step.changed <= run.subtree
            && step.primSlots+step.textSlots > 0
            && step.primSlots <= step.rowPrims && step.textSlots <= step.rowGlyphs;
    };
    // as for the virtual list: the wheel's three rows may move the bar's
    // thumb a pixel at one count and not the other, rewriting its one slot
    auto wheelPrimSlots=[](const DataTableVirtualRun &run)
    {
        return run.wheel.primSlots-(run.wheel.thumbMoved?1:0);
    };
    const bool sameCost=small.rows == large.rows && small.subtree == large.subtree
        && small.primIds == large.primIds && small.row == large.row
        && wheelPrimSlots(small) == wheelPrimSlots(large)
        && small.wheel.textSlots == large.wheel.textSlots
        && small.wheel.rowPrims == large.wheel.rowPrims
        && small.wheel.rowGlyphs == large.wheel.rowGlyphs;
    const bool settled=small.idleSlots == 0 && small.idleChanged == 0
        && large.idleSlots == 0 && large.idleChanged == 0;
    const bool rowsOnly=withinRows(small, small.row) && withinRows(small, small.wheel)
        && withinRows(large, large.row) && withinRows(large, large.wheel);
    // the sort and the drag: one pass each, no z renumber, no layout request
    auto onePass=[](const VirtualListStep &step)
    {
        return step.passes == 1 && step.zVisited == 0 && step.requests == 0;
    };
    const bool sortOnePass=small.sorted && large.sorted && onePass(small.sort) && onePass(large.sort);
    const bool dragOnePass=small.resized && large.resized && onePass(small.drag) && onePass(large.drag);
    const bool idsBack=small.idsLeft == 0 && large.idsLeft == 0;

    for(const DataTableVirtualRun *run : {&small, &large})
    {
        std::cout<<"data-table-virtual items="<<run->items<<" rows="<<run->rows<<" subtree="<<run->subtree
                 <<" prim_ids="<<run->primIds<<" idle_slots="<<run->idleSlots
                 <<" idle_changed="<<run->idleChanged;
        printVirtualListStep("row", run->row);
        printVirtualListStep("wheel", run->wheel);
        printVirtualListStep("sort", run->sort);
        printVirtualListStep("drag", run->drag);
        std::cout<<" ids_left="<<run->idsLeft<<"\n";
    }

    passed=sameCost && settled && rowsOnly && sortOnePass && dragOnePass && idsBack;

    std::cout<<"data-table-virtual same_cost="<<(sameCost?"yes":"no")<<" settled="<<(settled?"yes":"no")
             <<" rows_only="<<(rowsOnly?"yes":"no")<<" sort_one_pass="<<(sortOnePass?"yes":"no")
             <<" drag_one_pass="<<(dragOnePass?"yes":"no")<<" ids_back="<<(idsBack?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: data-table-virtual: "
                 <<(!sameCost ? "a frame at 100 000 records did not cost what it did at 1 000"
                     : !settled ? "a settled frame wrote slots or changed controls"
                     : !rowsOnly ? "a scroll wrote more than its shown rows' slots, entered more than"
                         " the table, renumbered z or took more than one layout pass"
                     : !sortOnePass ? "a header click did not sort the rows on screen in one layout pass"
                     : !dragOnePass ? "a grip drag did not resize its column in one layout pass"
                     : "the demo did not give its primitive ids back")<<"\n";
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario typing: the keyboard, through the GLFW translator
//
// Every keystroke is a synthesised GLFW event fed to gweni::input::GLFW3, the
// same object the examples' key and char callbacks feed, so this covers the
// translator as well as TextBox's editing. Each step is followed by a frame and
// checks the keyboard-focused control's text, so a step that went wrong is
// named on its own.
//-----------------------------------------------------------------------------

enum class TypingAction
{
    Click,          // click the centre of the first visible control of type arg
    Key,            // --key arg
    Type,           // --type arg
    HoldControl,    // press Left Ctrl and do not let it go
    LoseFocus,      // the window loses focus (glfwSetWindowFocusCallback)
};

struct TypingStep
{
    TypingAction action;
    const char *arg;
    const char *expect;     // the focused control's text afterwards
};

static const TypingStep g_typingSteps[]=
{
    {TypingAction::Click, "TextBox", ""},
    {TypingAction::Key, "ctrl+a", ""},              // a Ctrl+letter inserts nothing
    {TypingAction::Type, "hello", "hello"},
    {TypingAction::Key, "backspace", "hell"},
    {TypingAction::Key, "left", "hell"},
    {TypingAction::Key, "left", "hell"},            // caret between "he" and "ll"
    {TypingAction::Key, "backspace", "hll"},        // caret after "h"
    {TypingAction::Key, "delete", "hl"},
    {TypingAction::Key, "home", "hl"},
    {TypingAction::Type, "a", "ahl"},
    {TypingAction::Key, "end", "ahl"},
    {TypingAction::Type, "b", "ahlb"},
    {TypingAction::Key, "right", "ahlb"},           // already at the end
    {TypingAction::Key, "shift+left", "ahlb"},      // selects the "b"
    {TypingAction::Type, "c", "ahlc"},              // typing replaces the selection
    {TypingAction::Key, "shift+left", "ahlc"},
    {TypingAction::Key, "shift+left", "ahlc"},      // selects "lc"
    {TypingAction::Key, "delete", "ah"},
    {TypingAction::Key, "ctrl+a", "ah"},
    {TypingAction::Type, "x", "x"},
    {TypingAction::Key, "ctrl+a", "x"},
    {TypingAction::Key, "ctrl+c", "x"},
    {TypingAction::Key, "end", "x"},
    {TypingAction::Key, "ctrl+v", "xx"},
    {TypingAction::Key, "ctrl+a", "xx"},
    {TypingAction::Key, "ctrl+x", ""},
    {TypingAction::Key, "ctrl+v", "xx"},            // the cut text is on the clipboard
    {TypingAction::Key, "ctrl+b", "xx"},            // no accelerator: still not typed
    {TypingAction::Key, "escape", "xx"},
    // Ctrl held as the window loses focus, its release never seen: typing has
    // to work once the window has it back.
    {TypingAction::HoldControl, nullptr, "xx"},
    {TypingAction::LoseFocus, nullptr, nullptr},
    {TypingAction::Type, "y", "xxy"},
    // Tab moves the focus to the next tabable control in child order.
    {TypingAction::Key, "tab", "Normal Everyday Label"},
    // A numeric box takes the digits and drops the rest.
    {TypingAction::Click, "TextBoxNumeric", "2004"},
    {TypingAction::Key, "ctrl+a", "2004"},
    {TypingAction::Type, "a1b2", "12"},
    // Return and keypad Enter break the line in a multiline box.
    {TypingAction::Click, "TextBoxMultiline", nullptr},
    {TypingAction::Key, "ctrl+a", nullptr},
    {TypingAction::Type, "ab", "ab"},
    {TypingAction::Key, "return", "ab\n"},
    {TypingAction::Type, "cd", "ab\ncd"},
    {TypingAction::Key, "kp-enter", "ab\ncd\n"},
    {TypingAction::Type, "e", "ab\ncd\ne"},
};

static std::string typingStepName(size_t index, const TypingStep &step)
{
    static const char *const actions[]={"click", "key", "type", "hold-control", "lose-focus"};
    std::string name="step "+std::to_string(index)+" "+actions[static_cast<int>(step.action)];
    if(step.arg)
        name+=std::string(" ")+step.arg;
    return name;
}

static bool runTypingStep(Session &s, gweni::controls::Base *demo, const TypingStep &step)
{
    switch(step.action)
    {
    case TypingAction::Click:
    {
        gweni::controls::Base *target=findControl(demo, step.arg);
        if(!target)
        {
            std::cerr<<"ControlTest: typing: TextBoxDemo has no visible "<<step.arg<<"\n";
            return false;
        }
        const gweni::Point at=centreOf(target);
        clickMouse(s, at.x, at.y);
        return true;
    }
    case TypingAction::Key: return sendKey(s, step.arg);
    case TypingAction::Type: typeText(s, step.arg); return true;
    case TypingAction::HoldControl: glfwKey(s, GLFW_KEY_LEFT_CONTROL, GLFW_PRESS, 0); return true;
    case TypingAction::LoseFocus: glfwInput(s).processFocus(false); trace(s, "focus lost"); return true;
    }
    return false;
}

static bool runTypingScenario(Session &s, const std::string &shotDir, bool &passed)
{
    // Frozen like --stats: no caret blink, and no key repeat, which would need
    // the clock to pass c_KeyRepeatDelay with a key held.
    s.freezeClock=true;

    gweni::controls::Base *demo=addControl(s, "TextBoxDemo");
    renderFrames(s, s.frames);

    InteractionChecks checks;
    checks.scenario="typing";
    bool ok=true;

    // The first box starts empty. Its first character, and the Backspace that
    // empties it again, change that box alone, as any other keystroke does: an
    // empty Text is as high as a typed one, so the box keeps its height and
    // the layout moves no other row (#268).
    if(gweni::controls::Base *empty=findControl(demo, "TextBox"))
    {
        clickMouse(s, centreOf(empty).x, centreOf(empty).y);
        renderFrames(s, s.frames);

        std::string start;
        checks.check("empty_box_focused", focusedText(start) && start.empty(),
            "clicking the first TextBox did not focus an empty box");

        for(const char *key : {"a", "backspace"})
        {
            const bool typing=(std::strcmp(key, "a") == 0);
            if(typing)
                typeText(s, key);
            else
                sendKey(s, key);

            std::vector<std::string> strays;
            frameCheckingStrays(s, {empty}, strays, true);
            const gweni::controls::ChangeStats changes=gweni::controls::getChangeStats();
            const gweni::renderer::SlotRenderer::FrameStats gpu=s.renderer->getFrameStats();
            const std::string name=typing ? "empty_box_first_char" : "empty_box_last_char";

            std::cout<<"typing "<<(typing ? "first-char" : "last-char")
                     <<" controls_changed="<<changes.controls
                     <<" geometry="<<changes.geometry
                     <<" text="<<changes.text
                     <<" prim_slots_written="<<gpu.primitiveSlotsWritten
                     <<" text_slots_written="<<gpu.textSlotsWritten<<"\n";

            std::string strayList;
            for(const std::string &stray : strays)
                strayList+=" "+stray;
            checks.check(name, strays.empty() && changes.text == 1 && gpu.textSlotsWritten <= 1,
                std::string(typing ? "typing the first character" : "deleting the last character")
                +" changed controls outside the box:"+(strays.empty() ? std::string(" none") : strayList)
                +", text="+std::to_string(changes.text)
                +" text_slots_written="+std::to_string(gpu.textSlotsWritten)
                +"; want that box's one text run rewritten");
        }
        renderFrames(s, s.frames);
    }

    size_t index=0;
    for(const TypingStep &step : g_typingSteps)
    {
        const std::string name=typingStepName(index++, step);
        if(!runTypingStep(s, demo, step))
        {
            checks.check(name, false, "the step could not be sent");
            continue;
        }
        renderFrames(s, 1);

        if(!step.expect)
            continue;

        std::string actual;
        const bool hasText=focusedText(actual);
        checks.check(name, hasText && actual == step.expect,
            "expected "+quotedText(step.expect)+", the keyboard focus "
            +(hasText ? std::string("has ")+quotedText(actual)
                : std::string("(")+(gweni::KeyboardFocus ? gweni::KeyboardFocus->getTypeName() : "none")
                  +") has no text"));

        // The picture: "hello" typed, the caret after it.
        if(step.action == TypingAction::Type && std::strcmp(step.arg, "hello") == 0)
        {
            renderFrames(s, s.frames);
            if(!shoot(s, shotDir+"/typing-hello.png"))
                ok=false;
        }
    }

    // One keystroke into a settled box: it rewrites that box's text run and
    // nothing else, and the frame after it is idle again.
    gweni::controls::Base *box=findControl(demo, "TextBox");
    if(box)
    {
        clickMouse(s, centreOf(box).x, centreOf(box).y);
        renderFrames(s, s.frames);

        typeText(s, "z");
        renderFrames(s, 1);
        const gweni::controls::ChangeStats keyChanges=gweni::controls::getChangeStats();
        const gweni::renderer::SlotRenderer::FrameStats keyGpu=s.renderer->getFrameStats();

        renderFrames(s, 1);
        const gweni::controls::ChangeStats idleChanges=gweni::controls::getChangeStats();
        const gweni::renderer::SlotRenderer::FrameStats idleGpu=s.renderer->getFrameStats();

        std::cout<<"typing keystroke controls_changed="<<keyChanges.controls
                 <<" text="<<keyChanges.text
                 <<" prim_slots_written="<<keyGpu.primitiveSlotsWritten
                 <<" text_slots_written="<<keyGpu.textSlotsWritten
                 <<" upload_bytes="<<keyGpu.uploadBytes<<"\n";

        checks.check("keystroke_one_text_run", keyChanges.text == 1 && keyGpu.textSlotsWritten >= 1,
            "typing one character changed text="+std::to_string(keyChanges.text)
            +" controls, text_slots_written="+std::to_string(keyGpu.textSlotsWritten)
            +"; want one control's text run rewritten");
        checks.check("keystroke_then_idle",
            idleChanges.controls == 0 && idleGpu.uploadBytes == 0,
            "the frame after the keystroke had controls_changed="+std::to_string(idleChanges.controls)
            +" upload_bytes="+std::to_string(idleGpu.uploadBytes));
    }

    dropControl(s, demo);

    size_t failed=0;
    for(const auto &result : checks.results)
        if(!result.second)
            ++failed;
    std::cout<<"typing checks="<<checks.results.size()<<" failed="<<failed<<"\n";

    passed=checks.allPassed();
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario hide-show: a demo hidden and shown again, and one built hidden
//
// The demo is built into a host page: a plain Base docked to fill the canvas,
// as PageControl::addPage makes the shell's pages. Showing the host must give
// back exactly what was there before it was hidden, and a demo built into a
// host that is hidden already (every shell page from index 1 on) must come
// out as one built shown: the parts a control hid itself (a ComboBox's list, a
// TabControl's other pages, a collapsed node, an unneeded scroll bar) stay
// hidden, since hiding or showing a control never writes its descendants' own
// flags (docs/architecture.md -> "Hidden and visible").
//-----------------------------------------------------------------------------

// A fresh host page on the canvas, from addControl()'s starting point: no
// hover, clock and frame counter at zero.
static gweni::controls::Base *addHost(Session &s, const std::string &name, bool hidden)
{
    forgetMouse(s);
    g_clockSeconds=0.0f;
    s.frame=0;
    restartFrameCounts();
    s.label=name;

    gweni::controls::Base *host=s.canvas->newChild<gweni::controls::Base>("HideShowHost");
    host->setDock(gweni::DockPosition::Center);
    if(hidden)
        host->hide();
    return host;
}

static gweni::controls::Base *addHostedControl(gweni::controls::Base *host, const std::string &name)
{
    gweni::controls::Base *control=createControl(host, name);
    if(control)
        control->setDock(gweni::DockPosition::Center);
    return control;
}

static size_t liveTextIds()
{
    return gweni::textIdCount()-gweni::releasedTextIdCount();
}

// What a control under the host showed before a hide or show.
struct VisibleBefore
{
    gweni::controls::Base *control;
    bool visible;
    bool hidden;
};

static void collectVisible(gweni::controls::Base *control, std::vector<VisibleBefore> &controls)
{
    controls.push_back({control, control->visible(), control->hidden()});
    for(auto &&child : control->getActualChildren())
        collectVisible(child, controls);
}

// Whether the frame's layout changed the own hidden flag of `control` or of an
// ancestor below `host` (a TabControl hiding its unneeded scroll buttons when
// it is first laid out, once shown): that marks StateChange_Visibility on
// what it reaches, as it should, even where visible() ends the frame as it was.
static bool hiddenFlagMoved(gweni::controls::Base *control, gweni::controls::Base *host,
    const std::map<gweni::controls::Base *, bool> &hiddenBefore)
{
    for(gweni::controls::Base *at=control; at && at != host; at=at->getParent())
    {
        const auto was=hiddenBefore.find(at);
        if(was != hiddenBefore.end() && was->second != at->hidden())
            return true;
    }
    return false;
}

// The host hidden or shown and one frame rendered, its update phase inspected
// before it renders: nothing outside the host's subtree has a change pending,
// and exactly the controls whose visible() changed are marked
// StateChange_Visibility (a show does not enter a subtree that hid itself).
static void toggleHost(Session &s, gweni::controls::Base *host, bool show,
    InteractionChecks &checks, const std::string &name)
{
    std::vector<VisibleBefore> before;
    collectVisible(host, before);
    std::map<gweni::controls::Base *, bool> hiddenBefore;
    for(const VisibleBefore &entry : before)
        hiddenBefore[entry.control]=entry.hidden;

    host->setHidden(!show);

    gweni::controls::resetChangeStats();
    s.canvas->updateCanvas();

    std::vector<std::string> strays;
    pendingOutside(s.canvas, {host}, strays);

    size_t changed=0;
    std::string wrong;
    for(const VisibleBefore &entry : before)
    {
        gweni::controls::Base *control=entry.control;
        const bool flipped=control->visible() != entry.visible;
        const bool marked=(control->getStateChange() & gweni::controls::StateChange_Visibility) != 0;
        if(flipped)
            ++changed;
        if(flipped == marked || (marked && hiddenFlagMoved(control, host, hiddenBefore)))
            continue;
        wrong+=std::string(" ")+control->getTypeName()+"#"+std::to_string(control->getControlId())
            +(flipped ? " (changed, not marked)" : " (marked, not changed)");
    }

    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();
    if(s.stats)
        printFrameStats(s.frame, s.label, gweni::controls::getChangeStats(),
            s.renderer->getFrameStats(), s.canvas->getLayoutStats(),
            s.canvas->getUpdateStats());
    checkOnePass(s, std::to_string(s.frame));
    ++s.frame;

    std::string strayList;
    for(const std::string &stray : strays)
        strayList+=" "+stray;
    checks.check(name+"_local", strays.empty(),
        "controls outside the host had a change pending:"+strayList);
    checks.check(name+"_marks", wrong.empty() && changed > 0,
        std::to_string(changed)+" controls changed visible(); wrongly marked:"+wrong);
}

// The renderer mirror keyed by who holds each slot: a control's listing path
// and the place of the id among its skin Control's ids. A demo built hidden
// can take its ids in another order than one built shown (a wrapped Label's
// lines are made by its first layout, which waits for the show), so the
// built-hidden state is compared with the reference by owner, not by id.
static void collectMirrorByOwner(gweni::controls::Base *control, const std::string &path,
    const gweni::renderer::SlotRenderer::MirrorSnapshot &mirror,
    std::map<std::string, std::vector<uint8_t>> &slots)
{
    if(const gweni::skin::Control *skinControl=control->getSkinControl())
    {
        const bool text=dynamic_cast<const gweni::skin::textured::Text *>(skinControl) != nullptr;
        const auto &ids=text ? mirror.texts : mirror.primitives;
        size_t index=0;
        for(size_t id : skinControl->getPrimitives())
        {
            const auto found=ids.find(id);
            const std::string key=path+(text ? " text[" : " primitive[")+std::to_string(index++)+"]";
            std::vector<uint8_t> bytes=(found != ids.end()) ? found->second : std::vector<uint8_t>();
            // A hidden slot (style 0 in all four vertexes) draws nothing, and
            // the rest of it is whatever was last written there, by this
            // owner or an earlier one of a reused id: only "hidden" counts.
            // A slot is a quad: four vertexes, each starting with its style.
            if(!text && !bytes.empty() && bytes.size()%4 == 0)
            {
                const size_t vertexBytes=bytes.size()/4;
                bool hidden=true;
                for(size_t vertex=0; vertex<4; ++vertex)
                {
                    uint16_t style;
                    std::memcpy(&style, &bytes[vertex*vertexBytes], sizeof(style));
                    hidden=hidden && style == 0;
                }
                if(hidden)
                    bytes.assign(1, 0);
            }
            slots[key]=bytes;
        }
    }

    size_t index=0;
    for(auto &&child : control->getActualChildren())
        collectMirrorByOwner(child, childPath(path, child, index++), mirror, slots);
}

static bool mirrorsMatchByOwner(Session &s, const ResizeState &before,
    const std::map<std::string, std::vector<uint8_t>> &beforeSlots, const ResizeState &after)
{
    std::map<std::string, std::vector<uint8_t>> afterSlots;
    collectMirrorByOwner(s.canvas, s.canvas->getTypeName(), after.mirror, afterSlots);
    if(beforeSlots == afterSlots)
        return true;

    static const size_t maxReported=10;
    size_t differing=0;
    std::cerr<<"ControlTest: renderer mirror of "<<after.name<<" differs from "<<before.name
             <<", slot by owner:\n";
    for(const auto &entry : beforeSlots)
    {
        const auto now=afterSlots.find(entry.first);
        const char *how=(now == afterSlots.end()) ? "held only in the first"
            : (now->second != entry.second) ? "bytes differ" : nullptr;
        if(!how)
            continue;
        if(differing++ < maxReported)
            std::cerr<<"  "<<entry.first<<" "<<how<<"\n";
    }
    for(const auto &entry : afterSlots)
        if(beforeSlots.find(entry.first) == beforeSlots.end() && differing++ < maxReported)
            std::cerr<<"  "<<entry.first<<" held only in the second\n";
    std::cerr<<"  "<<differing<<" slots differ\n";
    return false;
}

// The listing path of `target` under `control` (at `path`), or empty.
static std::string pathOf(gweni::controls::Base *control, const std::string &path,
    gweni::controls::Base *target)
{
    if(control == target)
        return path;

    size_t index=0;
    for(auto &&child : control->getActualChildren())
    {
        const std::string found=pathOf(child, childPath(path, child, index++), target);
        if(!found.empty())
            return found;
    }
    return std::string();
}

static gweni::controls::Base *controlAtPath(gweni::controls::Base *control, const std::string &path,
    const std::string &want)
{
    if(path == want)
        return control;
    if(want.compare(0, path.size(), path) != 0)
        return nullptr;

    size_t index=0;
    for(auto &&child : control->getActualChildren())
        if(gweni::controls::Base *found=controlAtPath(child, childPath(path, child, index++), want))
            return found;
    return nullptr;
}

// A hidden control loses the keyboard focus (input::dropLostKeyboardFocus)
// and showing it does not give it back: an app does that, as the shell's
// showcaseShown() does. So once shown, the control that held the focus in
// the reference gets it again, before the state is compared.
static void giveFocusBack(Session &s, const std::string &focusPath)
{
    if(focusPath.empty())
        return;
    if(gweni::controls::Base *control=controlAtPath(s.canvas, s.canvas->getTypeName(), focusPath))
    {
        control->focus();
        renderFrames(s, s.frames);
    }
}

static bool shootState(Session &s, const std::string &path, ResizeState &state)
{
    if(!shoot(s, path))
        return false;
    captureResizeState(s, path, state);
    return true;
}

// Every ComboBox under `root`, the property grid's included.
static void collectComboBoxes(gweni::controls::Base *control,
    std::vector<gweni::controls::ComboBox *> &boxes)
{
    if(gweni::controls::ComboBox *box=dynamic_cast<gweni::controls::ComboBox *>(control))
        boxes.push_back(box);
    for(auto &&child : control->getActualChildren())
        collectComboBoxes(child, boxes);
}

// The shell (Demo) with one of its pages selected, as OpenGLCore --demo does:
// the page was built hidden and is shown now. At least `minimum` ComboBoxes are
// drawn, inside the canvas, and none of them has its list open.
static void checkShellPage(Session &s, gweni::Demo *demo, const std::string &page, size_t minimum,
    const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    if(!demo->showDemo(page))
    {
        checks.check("shell_"+page, false, "the shell has no page "+page);
        return;
    }
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/hide-show-shell-"+page+".png"))
        ok=false;

    std::vector<gweni::controls::ComboBox *> boxes;
    collectComboBoxes(demo, boxes);

    const gweni::Rect canvasRect(0, 0, s.size.x, s.size.y);
    size_t shown=0;
    std::string problems;
    for(gweni::controls::ComboBox *box : boxes)
    {
        if(!box->visible())
            continue;
        ++shown;

        const gweni::Rect rect=canvasRectOf(box);
        if(rect.w <= 0 || rect.h <= 0 || !insideRect(rect, canvasRect))
            problems+=" ComboBox#"+std::to_string(box->getControlId())+" at "+rectText(rect)
                +" is not inside the canvas;";

        for(auto &&child : box->getActualChildren())
            if(dynamic_cast<gweni::controls::Menu *>(child) && child->visible())
                problems+=" ComboBox#"+std::to_string(box->getControlId())+" draws its list;";
    }
    if(shown < minimum)
        problems+=" "+std::to_string(shown)+" ComboBoxes drawn, want at least "+std::to_string(minimum)+";";
    checks.check("shell_"+page, problems.empty(), page+":"+problems);
}

// A: the demo built shown and settled, the reference. B: its host hidden for
// a frame (and settled hidden), then shown again. C: on a fresh canvas, the
// demo built into a host that is hidden already, then shown. B and C have to
// be A byte for byte (listing, pixels, and the renderer mirror: B's id by id,
// C's slot by owner), the hidden frames the empty canvas's, and each a few
// more checks, named on stderr.
static bool runHideShowScenario(Session &s, const std::string &controlName,
    const std::string &shotDir, bool &passed)
{
    const std::string emptyShot=shotDir+"/hide-show-00-empty.png";
    const std::string shownShot=shotDir+"/hide-show-01-shown.png";
    const std::string hiddenShot=shotDir+"/hide-show-02-hidden.png";
    const std::string againShot=shotDir+"/hide-show-03-shown-again.png";
    const std::string builtHiddenShot=shotDir+"/hide-show-04-built-hidden.png";
    const std::string builtShownShot=shotDir+"/hide-show-05-built-hidden-shown.png";

    ResizeState emptyState, shownState, hiddenState, againState, builtHiddenState, builtShownState;
    InteractionChecks checks;
    checks.scenario="hide-show control="+controlName;
    bool ok=true;

    // the shots are compared with each other: an animation stands still
    s.freezeClock=true;

    // A, on a new canvas as C is, so both hand out their ids alike
    replaceCanvas(s);
    gweni::controls::Base *host=addHost(s, controlName, false);
    renderFrames(s, s.frames);
    if(!shootState(s, emptyShot, emptyState)) return false;

    gweni::controls::Base *control=addHostedControl(host, controlName);
    if(!control) return false;
    renderFrames(s, s.frames);
    if(!shootState(s, shownShot, shownState)) return false;
    const size_t primitiveIds=livePrimitiveIds();
    const size_t textIds=liveTextIds();
    std::map<std::string, std::vector<uint8_t>> shownSlots;
    collectMirrorByOwner(s.canvas, s.canvas->getTypeName(), shownState.mirror, shownSlots);
    const std::string focusPath=(gweni::KeyboardFocus && isUnder(gweni::KeyboardFocus, host))
        ? pathOf(s.canvas, s.canvas->getTypeName(), gweni::KeyboardFocus) : std::string();

    // B
    toggleHost(s, host, false, checks, "hide");
    renderFrames(s, s.frames);
    if(!shootState(s, hiddenShot, hiddenState)) return false;
    checkSteady(s, checks, "hidden_settles");

    toggleHost(s, host, true, checks, "show");
    renderFrames(s, s.frames);
    giveFocusBack(s, focusPath);
    checkSteady(s, checks, "shown_settles");
    if(!shootState(s, againShot, againState)) return false;
    checks.check("ids_kept", livePrimitiveIds() == primitiveIds && liveTextIds() == textIds,
        "live primitive ids "+std::to_string(primitiveIds)+" -> "+std::to_string(livePrimitiveIds())
        +", text ids "+std::to_string(textIds)+" -> "+std::to_string(liveTextIds()));
    dropControl(s, host);

    // C
    replaceCanvas(s);
    host=addHost(s, controlName, true);
    renderFrames(s, s.frames);
    control=addHostedControl(host, controlName);
    if(!control) return false;
    renderFrames(s, s.frames);
    if(!shootState(s, builtHiddenShot, builtHiddenState)) return false;
    checkSteady(s, checks, "built_hidden_settles");

    toggleHost(s, host, true, checks, "built_hidden_show");
    renderFrames(s, s.frames);
    giveFocusBack(s, focusPath);
    checkSteady(s, checks, "built_hidden_shown_settles");
    if(!shootState(s, builtShownShot, builtShownState)) return false;
    bool builtHidden=mirrorsMatchByOwner(s, shownState, shownSlots, builtShownState);
    dropControl(s, host);

    // Every comparison runs, so a failing run names all that differs.
    bool roundTrip=listingsMatch(shownState, againState);
    roundTrip=mirrorsMatch(shownState, againState) && roundTrip;
    roundTrip=pixelsMatch(shownState, againState) && roundTrip;
    checks.check("round_trip", roundTrip, "the demo did not come back from a hide and show (above)");

    builtHidden=listingsMatch(shownState, builtShownState) && builtHidden;
    builtHidden=pixelsMatch(shownState, builtShownState) && builtHidden;
    checks.check("built_hidden", builtHidden, "the demo built hidden and shown differs from one built shown (above)");

    bool hiddenEmpty=pixelsMatch(emptyState, hiddenState);
    hiddenEmpty=pixelsMatch(emptyState, builtHiddenState) && hiddenEmpty;
    checks.check("hidden_empty", hiddenEmpty, "a hidden demo drew something (above)");

    // The shell: its pages are built hidden and shown when selected.
    if(controlName == g_dockedControlName)
    {
        replaceCanvas(s);
        gweni::controls::Base *shell=addControl(s, controlName);
        renderFrames(s, s.frames);
        if(gweni::Demo *demo=dynamic_cast<gweni::Demo *>(shell))
        {
            checkShellPage(s, demo, "PropertiesDemo", 1, shotDir, checks, ok);
            checkShellPage(s, demo, "ComboBoxDemo", 3, shotDir, checks, ok);
        }
        dropControl(s, shell);
    }

    std::cout<<"hide-show control="<<controlName;
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second ? "yes" : "no");
    std::cout<<"\n";

    passed=checks.allPassed();
    return ok;
}

//-----------------------------------------------------------------------------
// --check-teardown: teardown identity
//
// In a retained renderer a control's primitives are a persistent slot in the
// GPU buffer, so a destroyed control whose slots are never cleared keeps
// drawing forever. The sharpest statement that this has not happened is that
// the canvas, once every control is gone, renders byte for byte like the empty
// canvas it started as. That does not care which control leaked or what its
// leftover vertices drew, so it catches the whole class.
//-----------------------------------------------------------------------------

// The same starting point addControl() gives a control: no hover, clock and
// frame counter back at zero, so the two empty renders are comparable.
static void renderEmptyCanvas(Session &s, std::vector<uint8_t> &pixels)
{
    forgetMouse(s);
    g_clockSeconds=0.0f;
    s.frame=0;
    restartFrameCounts();
    s.label="(empty)";

    renderFrames(s, s.frames);
    s.renderer->readPixels(s.pixels.x, s.pixels.y, pixels);
}

static bool checkTeardown(Session &s, const std::vector<std::string> &controls,
    const std::string &shotDir)
{
    std::string set;
    for(const std::string &name : controls)
        set+=(set.empty() ? "" : ",")+name;

    std::vector<uint8_t> reference;
    renderEmptyCanvas(s, reference);

    for(const std::string &name : controls)
    {
        gweni::controls::Base *control=addControl(s, name);
        renderFrames(s, s.frames);
        dropControl(s, control);
    }

    std::vector<uint8_t> after;
    renderEmptyCanvas(s, after);

    size_t differing=0;
    for(size_t i=0; i<reference.size(); ++i)
        if(reference[i] != after[i])
            ++differing;

    if(differing == 0)
    {
        std::cout<<"teardown controls="<<set<<" identical=yes\n";
        return true;
    }

    const std::string referencePath=shotDir+"/teardown-reference.png";
    const std::string afterPath=shotDir+"/teardown-after.png";

    std::cout<<"teardown controls="<<set<<" identical=no"
             <<" differing_bytes="<<differing<<" of "<<reference.size()<<"\n";
    std::cerr<<"ControlTest: the canvas does not render empty after every control"
               " was destroyed: "<<differing<<" of "<<reference.size()
             <<" bytes differ, so something is still drawing. Compare "
             <<referencePath<<" with "<<afterPath<<"\n";

    bool wrote=writeRgbaPng(reference, s.pixels.x, s.pixels.y, referencePath);
    wrote=writeRgbaPng(after, s.pixels.x, s.pixels.y, afterPath) && wrote;
    if(wrote)
        std::cout<<"wrote "<<referencePath<<"\nwrote "<<afterPath<<"\n";
    return false;
}

#if GWENI_EXAMPLE_WEBGL
//-----------------------------------------------------------------------------
// The interactive window in the browser: what a script driving the page reads
// (tests/web/interactive.mjs), through Module.ccall(name, 'string'). Each
// returns a string that stays valid until the next call of the same function.
//-----------------------------------------------------------------------------

static std::string g_windowLabel;   // the control the window shows

//! The window's --dump-bounds listing as it is now: the canvas at its logical
//! size, as a capture of the same size and scale lists it.
extern "C" EMSCRIPTEN_KEEPALIVE const char *gweni_web_bounds_listing()
{
    static std::string listing;

    listing.clear();
    if(g_canvas)
    {
        Session s;
        s.canvas=g_canvas;
        s.label=g_windowLabel;
        s.size=g_canvas->getSize();
        listing=boundsListing(s);
    }
    return listing.c_str();
}

//! The keyboard-focused control's text (a Label, which every TextBox is), or
//! an empty string if it has none.
extern "C" EMSCRIPTEN_KEEPALIVE const char *gweni_web_focused_text()
{
    static std::string text;

    text.clear();
    focusedText(text);
    return text.c_str();
}
#endif//GWENI_EXAMPLE_WEBGL

//-----------------------------------------------------------------------------
// --window-test: the interactive window, scripted (window_vulkan,
// tests/windowVulkan.cmake). It drives the loop runWindowLoop runs one
// iteration at a time (beginWindowFrame, a frame, presentWindowFrame, an event
// poll), on the stepped clock and with no input callbacks, so what it shows
// depends on the window's sizes alone and not on where the X pointer is.
//-----------------------------------------------------------------------------

#ifdef GWENI_HAS_VULKAN
// frames drawn at a size before its image is read back, as a --shot settles
static constexpr int kWindowSettleFrames=kSettleFrames;
// how long the window system has to do a resize, an iconify or a restore
static constexpr double kWindowDeadlineSeconds=10.0;

static bool runWindowTest(gweni::capture::Device &device, gweni::controls::Canvas *canvas,
    gweni::renderer::SlotRenderer *renderer, const gweni::Color &background,
    const gweni::Point &size, const std::string &dir)
{
    typedef std::chrono::steady_clock Clock;

    GLFWwindow *window=device.context().window;
    const gweni::capture::Device::WindowStats *stats=device.windowStats();

    if(!stats)
    {
        std::cerr<<"ControlTest: --window-test needs --renderer vulkan (a swapchain to count)\n";
        return false;
    }

    // One iteration of the window's loop; \a readback reads back what it presents.
    auto iteration=[&](std::vector<uint8_t> *readback, gweni::Point *readSize)
    {
        bool shown=false;

        if(device.beginWindowFrame())
        {
            canvas->updateCanvas();
            renderer->clearTarget(background);
            canvas->renderCanvas();
            g_clockSeconds+=g_frameStep;
            shown=device.presentWindowFrame(readback, readSize);
            glfwPollEvents();
        }
        else
        {
            // runWindowLoop waits for the next event; here with a timeout, so
            // the script goes on to its next step
            glfwWaitEventsTimeout(0.05);
        }
        return shown;
    };

    auto timedOut=[&](Clock::time_point start)
    {
        return std::chrono::duration<double>(Clock::now()-start).count() > kWindowDeadlineSeconds;
    };

    // Draws until the swapchain is \a wanted (the size callback has fitted the
    // canvas, the target follows it), settles, and writes what the next present
    // shows as \a name.
    auto shootAt=[&](const gweni::Point &wanted, const std::string &name)
    {
        const Clock::time_point start=Clock::now();
        int settled=0;

        while(settled < kWindowSettleFrames)
        {
            if(timedOut(start))
            {
                std::cerr<<"ControlTest: --window-test: no "<<wanted.x<<"x"<<wanted.y
                         <<" swapchain within "<<kWindowDeadlineSeconds<<" s (it is "
                         <<stats->extent.x<<"x"<<stats->extent.y<<")\n";
                return false;
            }
            if(iteration(nullptr, nullptr) && stats->extent.x == wanted.x && stats->extent.y == wanted.y)
                ++settled;
        }

        std::vector<uint8_t> pixels;
        gweni::Point shown;

        while(!iteration(&pixels, &shown))
        {
            if(timedOut(start))
            {
                std::cerr<<"ControlTest: --window-test: nothing presented at "<<wanted.x<<"x"<<wanted.y<<"\n";
                return false;
            }
        }

        const std::string path=(fs::path(dir)/name).string();

        if(shown.x != wanted.x || shown.y != wanted.y)
        {
            std::cerr<<"ControlTest: --window-test: "<<name<<" presented "<<shown.x<<"x"<<shown.y
                     <<", not "<<wanted.x<<"x"<<wanted.y<<"\n";
            return false;
        }
        if(!writeRgbaPng(pixels, shown.x, shown.y, path))
            return false;
        std::cout<<"window-test: presented "<<shown.x<<"x"<<shown.y<<" wrote "<<path<<"\n";
        return true;
    };

    gweni::platform::setTimeSource(&steppedClock);
    gweni::capture::installFixedProfileClock();
    g_clockSeconds=0.0f;

    if(!ensureDirectory(dir))
        return false;
    std::cout<<"window-test: content_scale="<<gweni::capture::contentScale(window)<<"\n";

    // 1. the size it opened at; 2. two others and back, each a recreate
    const gweni::Point sizes[]=
    {
        size,
        gweni::Point(size.x*5/4, size.y*5/4),
        gweni::Point(size.x*4/5, size.y*4/5),
        size
    };
    long resizes=0;

    for(size_t i=0; i<sizeof(sizes)/sizeof(sizes[0]); ++i)
    {
        const gweni::Point &wanted=sizes[i];

        if(i > 0)
        {
            glfwSetWindowSize(window, wanted.x, wanted.y);
            ++resizes;
        }
        if(!shootAt(wanted, "window_"+std::to_string(i)+"_"+std::to_string(wanted.x)+"x"
            +std::to_string(wanted.y)+".png"))
            return false;
    }

    // 3. iconified: nothing drawn or presented until it is restored
    const gweni::Point last=sizes[sizeof(sizes)/sizeof(sizes[0])-1];
    Clock::time_point start=Clock::now();

    glfwIconifyWindow(window);
    while(glfwGetWindowAttrib(window, GLFW_ICONIFIED) != GLFW_TRUE)
    {
        if(timedOut(start))
        {
            std::cerr<<"ControlTest: --window-test: the window was not iconified within "
                     <<kWindowDeadlineSeconds<<" s (is a window manager running?)\n";
            return false;
        }
        iteration(nullptr, nullptr);
    }

    const long framesIconified=stats->frames;
    const long presentsIconified=stats->presents;
    const long skippedIconified=stats->skipped;

    start=Clock::now();
    while(std::chrono::duration<double>(Clock::now()-start).count() < 0.5)
        iteration(nullptr, nullptr);

    const long framesWhileIconified=stats->frames-framesIconified;
    const long presentsWhileIconified=stats->presents-presentsIconified;
    const long skippedWhileIconified=stats->skipped-skippedIconified;

    std::cout<<"window-test: iconified frames="<<framesWhileIconified
             <<" presents="<<presentsWhileIconified<<" skipped="<<skippedWhileIconified<<"\n";

    glfwRestoreWindow(window);
    start=Clock::now();
    while(glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE)
    {
        if(timedOut(start))
        {
            std::cerr<<"ControlTest: --window-test: the window was not restored within "
                     <<kWindowDeadlineSeconds<<" s\n";
            return false;
        }
        iteration(nullptr, nullptr);
    }
    if(!shootAt(last, "window_restored_"+std::to_string(last.x)+"x"+std::to_string(last.y)+".png"))
        return false;

    // 4. what the swapchain did
    std::cout<<"window-test: resizes="<<resizes
             <<" swapchains="<<stats->swapchains
             <<" recreates="<<(stats->swapchains-1)
             <<" resize_recreates="<<stats->resizeRecreates
             <<" out_of_date="<<stats->outOfDate
             <<" suboptimal="<<stats->suboptimal
             <<" frames="<<stats->frames
             <<" skipped="<<stats->skipped
             <<" presents="<<stats->presents
             <<" dropped="<<stats->dropped<<"\n";

    bool ok=true;

    if(stats->resizeRecreates < resizes)
    {
        std::cerr<<"ControlTest: --window-test: "<<resizes<<" resizes but "<<stats->resizeRecreates
                 <<" swapchain recreates for a new size\n";
        ok=false;
    }
    if(presentsWhileIconified != 0 || framesWhileIconified != 0)
    {
        std::cerr<<"ControlTest: --window-test: "<<presentsWhileIconified<<" present(s) and "
                 <<framesWhileIconified<<" frame(s) while the window was iconified\n";
        ok=false;
    }
    if(skippedWhileIconified == 0)
    {
        std::cerr<<"ControlTest: --window-test: the loop did not wait while the window was iconified\n";
        ok=false;
    }
    return ok;
}
#endif//GWENI_HAS_VULKAN

int main(int argc, char *argv[])
{
    gweni::capture::setToolName("ControlTest");
    gweni::capture::installGlfwErrorHandler();

    std::string which="ButtonDemo";
    std::string shotPath;
    std::string shotDir="/tmp";
    std::string resourceDir;
    std::string rendererName;
    std::string windowTestDir;
    gweni::Point screenSize(640, 480);
    float scale=1.0f;
    bool scaleGiven=false;
    int frames=kSettleFrames;
    bool shotAll=false;
    bool forceHeadless=false;
    bool stats=false;
    bool statsClockSteps=false;
    bool requireOnePass=false;
    bool traceInput=false;
    bool chosenControl=false;
    bool dumpBounds=false;
    bool checkTeardownMode=false;
    std::string dumpPath;
    Scenario scenario=Scenario::None;
    std::vector<Step> steps;
    bool scripted=false;    // any scripted input switch turns --frames/--shot into steps

    for(int i=1; i<argc; ++i)
    {
        const std::string arg=argv[i];
        const bool hasValue=(i+1)<argc;

        const bool isMouseStep=(arg == "--move" || arg == "--press" || arg == "--release"
            || arg == "--click" || arg == "--double-click" || arg == "--scroll"
            || arg == "--press-right" || arg == "--release-right" || arg == "--click-right");

        if(arg == "--control" && hasValue){ which=argv[++i]; chosenControl=true; }
        else if(arg == "--shot" && hasValue)
        {
            shotPath=argv[++i];
            steps.push_back({StepKind::Shot, 0, 0, shotPath});
        }
        else if(arg == "--shot-dir" && hasValue) shotDir=argv[++i];
        else if(arg == "--resources" && hasValue) resourceDir=argv[++i];
        else if(arg == "--renderer" && hasValue) rendererName=argv[++i];
        else if(arg == "--window-test" && hasValue) windowTestDir=argv[++i];
        else if(arg == "--frames" && hasValue)
        {
            frames=std::atoi(argv[++i]);
            steps.push_back({StepKind::Frames, frames, 0, {}});
        }
        else if(arg == "--shot-all") shotAll=true;
        else if(arg == "--headless") forceHeadless=true;
        else if(arg == "--stats-overlay") g_statsOverlay=gweni::capture::statsOverlayArgument(argc, argv, i);
        else if(arg == "--stats") stats=true;
        else if(arg == "--stats-clock" && hasValue)
        {
            const std::string mode=argv[++i];
            if(mode == "step") statsClockSteps=true;
            else if(mode == "frozen") statsClockSteps=false;
            else
            {
                std::cerr<<"ControlTest: --stats-clock wants frozen or step, not "<<mode<<"\n";
                return EXIT_FAILURE;
            }
        }
        else if(arg == "--require-one-pass") requireOnePass=true;
        else if(arg == "--verify-layout") gweni::controls::setLayoutVerify(true);
        else if(arg == "--check-teardown") checkTeardownMode=true;
        else if(arg == "--trace") traceInput=true;
        else if(arg == "--list"){ printControlNames(std::cout); return EXIT_SUCCESS; }
        else if(arg == "--help" || arg == "-h"){ printUsage(std::cout); return EXIT_SUCCESS; }
        else if(arg == "--dump-bounds")
        {
            dumpBounds=true;
            // The path is optional, so only a following word that is not itself
            // a switch is one.
            if(hasValue && std::strncmp(argv[i+1], "--", 2) != 0)
                dumpPath=argv[++i];
        }
        else if(arg == "--key" && hasValue)
        {
            scripted=true;
            steps.push_back({StepKind::Key, 0, 0, argv[++i]});
        }
        else if(arg == "--type" && hasValue)
        {
            scripted=true;
            steps.push_back({StepKind::Type, 0, 0, argv[++i]});
        }
        else if(arg == "--expect-text" && hasValue)
        {
            scripted=true;
            steps.push_back({StepKind::ExpectText, 0, 0, argv[++i]});
        }
        else if(isMouseStep && hasValue)
        {
            int x=0, y=0;
            if(!parsePoint(argv[++i], x, y))
            {
                std::cerr<<"ControlTest: "<<arg<<" wants X,Y, e.g. 100,20\n";
                return EXIT_FAILURE;
            }
            StepKind kind=StepKind::Move;
            if(arg == "--press") kind=StepKind::Down;
            else if(arg == "--release") kind=StepKind::Up;
            else if(arg == "--click") kind=StepKind::Click;
            else if(arg == "--double-click") kind=StepKind::DoubleClick;
            else if(arg == "--press-right") kind=StepKind::DownRight;
            else if(arg == "--release-right") kind=StepKind::UpRight;
            else if(arg == "--click-right") kind=StepKind::ClickRight;
            else if(arg == "--scroll") kind=StepKind::Scroll;

            scripted=true;
            steps.push_back({kind, x, y, {}});
        }
        else if(arg == "--scenario" && hasValue)
        {
            const std::string name=argv[++i];
            if(!parseScenario(name, scenario))
            {
                std::cerr<<"ControlTest: unknown scenario: "<<name
                         <<"; try per-control, resize, docked, scroll, interaction, typing, clip,"
                           " text-size, scale, tooltip, context-menu, popup, virtual-list,"
                           " hide-show, demo-labels or plot-histogram\n";
                return EXIT_FAILURE;
            }
        }
        else if(arg == "--scale" && hasValue)
        {
            if(!gweni::capture::parseScale(argv[++i], scale))
            {
                std::cerr<<"ControlTest: --scale wants a number in (0, 16], e.g. 1.5\n";
                return EXIT_FAILURE;
            }
            scaleGiven=true;
        }
        else if(arg == "--size" && hasValue)
        {
            if(!parseSize(argv[++i], screenSize))
            {
                std::cerr<<"ControlTest: --size wants WxH, e.g. 640x480\n";
                return EXIT_FAILURE;
            }
        }
        else
        {
            std::cerr<<"ControlTest: unknown argument: "<<arg<<"\n";
            printUsage(std::cerr);
            return EXIT_FAILURE;
        }
    }

    // The scenarios and a run with no --shot settle at least one frame; a
    // single --shot after --frames 0 renders none, and shoot() refuses it.
    const int framesBeforeShot=std::max(frames, 0);
    if(frames<1) frames=1;

    // The flag beats the environment.
    if(!gweni::capture::manualScale(scaleGiven, scale))
        return EXIT_FAILURE;

    gweni::capture::Backend backend;
    if(!gweni::capture::chooseBackend(rendererName, backend))
        return EXIT_FAILURE;

    // The scale scenario sets both of its scales itself.
    if(scenario == Scenario::Scale)
        scale=1.0f;
    g_manualScale=scale;

    if(scripted && scenario != Scenario::None)
    {
        std::cerr<<"ControlTest: --scenario runs its own script;"
                   " drop the scripted input switches\n";
        return EXIT_FAILURE;
    }

    if(checkTeardownMode && (scenario != Scenario::None || scripted || shotAll
        || !shotPath.empty()))
    {
        std::cerr<<"ControlTest: --check-teardown runs its own build/render/destroy"
                   " cycle; drop --scenario, --shot, --shot-all and the scripted"
                   " input switches\n";
        return EXIT_FAILURE;
    }

    // per-control is --shot-all under a name, so it shares the machinery.
    if(scenario == Scenario::PerControl)
        shotAll=true;

    // The resize and scale checks are about the dock layout re-settling, so
    // they shoot the whole demo unless a control was named.
    if((scenario == Scenario::Resize || scenario == Scenario::Scale) && !chosenControl)
        which=g_dockedControlName;

    if(!isKnownControl(which))
    {
        std::cout<<"Unknown control: "<<which<<"\n";
        printControlNames(std::cout);
        return EXIT_FAILURE;
    }

    if(shotAll && !shotPath.empty())
    {
        std::cerr<<"ControlTest: --shot and --shot-all are exclusive;"
                   " --shot-all names its files after the controls\n";
        return EXIT_FAILURE;
    }

    // A run that shoots more than once names its listings after its shots, so
    // there is nowhere for one explicit path to go.
    const bool dumpPerShot=shotAll || scenario != Scenario::None;
    if(dumpBounds && dumpPerShot && !dumpPath.empty())
    {
        std::cerr<<"ControlTest: --dump-bounds takes no path here;"
                   " a scenario names its listings after its shots\n";
        return EXIT_FAILURE;
    }

    // --stats on its own still renders offscreen; it just keeps no PNG.
    const bool writingPng=shotAll || !shotPath.empty() || scenario != Scenario::None;
    const bool capturing=writingPng || stats || requireOnePass || scripted || dumpBounds
        || checkTeardownMode || scenario != Scenario::None;

    // The two fixed-size scenarios say what they render at, so start there
    // rather than resizing away from --size on the first frame.
    if(scenario == Scenario::Resize || scenario == Scenario::Scale) screenSize=gweni::Point(640, 480);
    else if(scenario == Scenario::Docked) screenSize=gweni::Point(1024, 768);

    // The canvas is screenSize logical units; the framebuffer, the window and
    // the view are that in device pixels.
    const gweni::Point screenPixels=devicePixels(screenSize, scale);

    if(capturing && !ensureDirectory(shotDir))
        return EXIT_FAILURE;
    if(!shotPath.empty() && !ensureDirectory(fs::path(shotPathIn(shotDir, shotPath)).parent_path()))
        return EXIT_FAILURE;

    if(forceHeadless && !capturing)
    {
        std::cerr<<"ControlTest: --headless has no window to show;"
                   " pass --shot, --shot-all, --stats, --require-one-pass or --check-teardown\n";
        return EXIT_FAILURE;
    }

    if(!windowTestDir.empty() && (capturing || scale != 1.0f))
    {
        std::cerr<<"ControlTest: --window-test is the window, scripted at --scale 1;"
                   " drop the capture switches and --scale\n";
        return EXIT_FAILURE;
    }

    // Declared before the renderer, which keeps a reference to it. A missing
    // directory is reported here and the run carries on: what loads from it
    // degrades below.
    bool resourcesFound=true;
    const std::unique_ptr<gweni::ResourcePaths> paths=gweni::capture::resourcePaths(resourceDir, resourcesFound);

    gweni::capture::DeviceOptions deviceOptions;
    deviceOptions.size=screenPixels;
    deviceOptions.title="Gweni ControlTest - "+which;
    deviceOptions.capturing=capturing;
    deviceOptions.headless=forceHeadless;

    std::unique_ptr<gweni::capture::Device> device=gweni::capture::openDevice(backend, deviceOptions);
    if(!device)
        return EXIT_FAILURE;

    const gweni::capture::Context &ctx=device->context();

    gweni::renderer::SlotRenderer *renderer=
        device->newRenderer(*paths, gweni::Rect(gweni::Point(0,0), screenPixels));
    renderer->init();

    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(renderer);
    // A skin config or default font that does not load is reported by the
    // library and drawn degraded (every surface magenta; the built-in face, or no
    // text if that fails too). The window keeps running; a capture exits 2.
    bool degraded=!resourcesFound;
    if(!skin->init("DefaultSkin.json"))
        degraded=true;
    if(gweni::capture::loadDefaultFont(*renderer, skin->getDefaultFont()) != gweni::Font::Status::Loaded)
        degraded=true;

    // The canvas fill comes from the theme, so a "theme" block in
    // resources/DefaultSkin.json moves it too. Canvas::renderCanvas does not
    // currently paint its own background (the draw call there is commented
    // out), so the colour each frame clears the target to is what actually
    // shows.
    const gweni::Color background=gweni::skin::textured::tokyonight::Canvas.top;

    gweni::controls::Canvas *canvas=createCanvas(skin, screenSize, scale);

    g_renderer=renderer;
    g_canvas=canvas;

    int result=EXIT_SUCCESS;

    if(capturing)
    {
        // Everything time-dependent in the library now reads this instead of the
        // platform clock; --stats leaves the per-frame step out so time only
        // moves when a scripted press moves it.
        gweni::platform::setTimeSource(&steppedClock);
        // and the library's frame times count clock reads instead
        gweni::capture::installFixedProfileClock();

        Session session;
        session.renderer=renderer;
        session.canvas=canvas;
        session.device=device.get();
        session.background=background;
        session.size=screenSize;
        session.scale=scale;
        session.pixels=screenPixels;
        session.stats=stats;
        session.statsClockSteps=statsClockSteps;
        session.requireOnePass=requireOnePass;
        session.trace=traceInput;
        session.writePng=writingPng;
        session.frames=frames;
        session.dumpBounds=dumpBounds;
        session.dumpPerShot=dumpPerShot;
        session.dumpPath=dumpPath;

        bool ok=true;
        if(checkTeardownMode)
        {
            // The whole set by default, so one run covers every control; a named
            // --control narrows it, which is how a failure gets bisected.
            std::vector<std::string> set;
            if(chosenControl)
                set.push_back(which);
            else
                for(const char *name : g_controlNames)
                    set.push_back(name);

            ok=checkTeardown(session, set, shotDir);
        }
        else switch(scenario)
        {
        case Scenario::PerControl:
            ok=runPerControlScenario(session, shotDir);
            break;

        case Scenario::Resize:
        {
            bool settled=false;
            ok=runResizeScenario(session, which, shotDir, settled);
            if(ok && !settled)
                ok=false;   // the shrink did not re-settle: a failing assertion
            break;
        }

        case Scenario::Docked:
            ok=runDockedScenario(session, shotDir);
            break;

        case Scenario::Scale:
        {
            bool settled=false;
            ok=runScaleScenario(session, which, shotDir, settled);
            if(ok && !settled)
                ok=false;   // the round trip did not come back: a failing assertion
            break;
        }

        case Scenario::Scroll:
            ok=runScrollScenario(session, shotDir);
            break;

        case Scenario::Interaction:
        {
            bool passed=false;
            ok=runInteractionScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // something did not repaint: a failing assertion
            break;
        }

        case Scenario::Typing:
        {
            bool passed=false;
            ok=runTypingScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a step left the wrong text: a failing assertion
            break;
        }

        case Scenario::Clip:
        {
            bool passed=false;
            ok=runClipScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // something drew past its parent: a failing assertion
            break;
        }

        case Scenario::PlotHistogram:
        {
            bool passed=false;
            ok=runPlotHistogramScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a change wrote more than its bars' slots: a failing assertion
            break;
        }

        case Scenario::PlotLines:
        {
            bool passed=false;
            ok=runPlotLinesScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a change wrote more than its segments' slots: a failing assertion
            break;
        }

        case Scenario::Inspector:
        {
            bool passed=false;
            ok=runInspectorScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // the inspector did not settle or showed the wrong counts
            break;
        }

        case Scenario::StatsOverlay:
        {
            bool passed=false;
            ok=runStatsOverlayScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // the overlay wrote outside its refreshes or showed the wrong counts
            break;
        }

        case Scenario::Theme:
        {
            bool passed=false;
            ok=runThemeScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a live theme change did not draw what a fresh build does
            break;
        }

        case Scenario::TextSize:
        {
            bool passed=false;
            ok=runTextSizeScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // the size change cost more than its run: a failing assertion
            break;
        }

        case Scenario::ToolTip:
        {
            bool passed=false;
            ok=runToolTipScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // the tooltip showed, moved or cost wrongly: a failing assertion
            break;
        }
        case Scenario::ContextMenu:
        {
            bool passed=false;
            ok=runContextMenuScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a context menu opened, sat or closed wrongly: a failing assertion
            break;
        }

        case Scenario::Popup:
        {
            bool passed=false;
            ok=runPopupScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a popup opened, sat or closed wrongly: a failing assertion
            break;
        }

        case Scenario::VirtualList:
        {
            bool passed=false;
            ok=runVirtualListScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a frame cost more at 100 000 items, or more than its rows: a failing assertion
            break;
        }

        case Scenario::DataTableVirtual:
        {
            bool passed=false;
            ok=runDataTableVirtualScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a frame cost more at 100 000 records, or a sort or drag took two passes
            break;
        }

        case Scenario::HideShow:
        {
            bool passed=false;
            ok=runHideShowScenario(session, which, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a hide and show did not give the demo back: a failing assertion
            break;
        }

        case Scenario::DemoLabels:
        {
            bool passed=false;
            ok=runDemoLabelsScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a demo-list label is clipped: a failing assertion
            break;
        }

        case Scenario::None:
            if(shotAll)
            {
                ok=runPerControlScenario(session, shotDir);
                break;
            }

            {
                gweni::controls::Base *control=addControl(session, which);
                if(scripted)
                {
                    ok=runSteps(session, steps, shotDir);
                }
                else
                {
                    renderFrames(session, writingPng ? framesBeforeShot : frames);
                    if(writingPng)
                        ok=shoot(session, shotPathIn(shotDir, shotPath));
                }
                if(ok && dumpBounds && !dumpBoundsTo(session, dumpPath))
                    ok=false;
                dropControl(session, control);
            }
            break;
        }

        if(session.expectFailures > 0)
        {
            std::cerr<<"ControlTest: "<<session.expectFailures
                     <<" --expect-text check(s) failed\n";
            ok=false;
        }

        if(session.multiPassFrames > 0)
        {
            std::cerr<<"ControlTest: "<<session.multiPassFrames
                     <<" frame(s) took more than one layout pass\n";
            ok=false;
        }

        if(!ok)
            result=EXIT_FAILURE;

        // The resize and scale scenarios build a canvas of their own for their
        // fresh reference.
        canvas=session.canvas;
    }
    else
    {
        gweni::controls::Base *control=createControl(canvas, which);
        control->setDock(gweni::DockPosition::Center);
#if GWENI_EXAMPLE_WEBGL
        g_windowLabel=which;
#endif

        // --window-test takes no input: the X pointer is wherever the server
        // left it, maybe over a button
        if(windowTestDir.empty())
        {
            g_glfwInput.initialize(canvas);
            glfwSetKeyCallback(ctx.window, key_callback);
            glfwSetCharCallback(ctx.window, char_callback);
            glfwSetWindowFocusCallback(ctx.window, window_focus_callback);
            glfwSetCursorPosCallback(ctx.window, cursor_pos_callback);
            glfwSetCursorEnterCallback(ctx.window, cursor_enter_callback);
            glfwSetMouseButtonCallback(ctx.window, mouse_button_callback);
            glfwSetScrollCallback(ctx.window, scroll_callback);
        }
        glfwSetFramebufferSizeCallback(ctx.window, framebuffer_size_callback);
        glfwSetWindowContentScaleCallback(ctx.window, content_scale_callback);

        // The window's own content scale, and its real framebuffer size.
        fitCanvas(ctx.window);

        if(!windowTestDir.empty())
        {
#ifdef GWENI_HAS_VULKAN
            if(!runWindowTest(*device, canvas, renderer, background, screenPixels, windowTestDir))
                result=EXIT_FAILURE;
#else
            std::cerr<<"ControlTest: --window-test needs --renderer vulkan, which this build has not\n";
            result=EXIT_FAILURE;
#endif
        }
        else
        {
            // One frame per displayed frame until the window closes (in the
            // browser, per animation frame, never returning); the frame holds
            // only the canvas and the renderer, heap objects, and the colour, a
            // copy, as the browser's loop needs.
            device->runWindowLoop([canvas, renderer, background]()
            {
                canvas->updateCanvas();
                renderer->clearTarget(background);
                canvas->renderCanvas();
            });
        }

        delete control;
    }

    delete canvas;
    delete skin;
    delete renderer;
    device.reset();

    // setSkin() printed each of these when it first met it; the run fails on
    // them whatever the mode, so a control that draws nothing is not silent.
    const std::vector<std::string> unrendered=gweni::controls::typesWithoutSkinRenderer();
    if(!unrendered.empty())
    {
        std::cerr<<"ControlTest: "<<unrendered.size()<<" control type(s) with no skin renderer:";
        for(const std::string &type : unrendered)
            std::cerr<<" "<<type;
        std::cerr<<"\n";
        result=EXIT_FAILURE;
    }

    // Base printed each of these as it happened: a control added, removed,
    // reordered or deleted during a render walk.
    const std::vector<std::string> mutations=gweni::controls::renderWalkMutations();
    if(!mutations.empty())
    {
        std::cerr<<"ControlTest: "<<mutations.size()<<" tree change(s) during the render walk:";
        for(const std::string &mutation : mutations)
            std::cerr<<" ["<<mutation<<"]";
        std::cerr<<"\n";
        result=EXIT_FAILURE;
    }

    // Canvas printed each one as it found it.
    if(gweni::controls::layoutVerifyFailures())
    {
        std::cerr<<"ControlTest: "<<gweni::controls::layoutVerifyFailures()
                 <<" layout verify difference(s) against a full pass\n";
        result=EXIT_FAILURE;
    }
    return capturing?gweni::capture::exitStatus(result, degraded):result;
}
