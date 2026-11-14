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
 *    --depth-view [on|off]  open the depth view (gweni::tools::DepthView) on
 *                       the canvas once its first frame has numbered the z
 *                       labels, in the window or a script's frames (on if
 *                       neither word follows; a later one overrides it)
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
 *                       window_vulkan (tests/windowVulkan.cmake) runs it. It
 *                       drives the loop's steps itself and draws one frame
 *                       each, whatever --loop says
 *    --idle-test        the window's idle loop (input::GlfwLoop), measured, with
 *                       no input taken: once --control has settled it idles
 *                       2 s, which must draw no frame, swap or present nothing
 *                       and take under 0.1 s of the process's CPU (getrusage,
 *                       every thread), then a post from another thread must
 *                       wake it and draw exactly one frame, writing glyphs,
 *                       and a canvas timer closes the window. One idle-test:
 *                       line per measure on stdout; window_idle (and _vulkan,
 *                       tests/windowIdle.cmake) runs it under Xvfb
 *    --loop <idle|continuous>  the window draws only when the canvas has work
 *                       (idle, the default; input::GlfwLoop) or every frame
 *    --stats            print one key=value line per frame; freezes the clock.
 *                       prim_slots_written= and text_slots_written= count
 *                       the distinct primitive and glyph slots the frame's
 *                       flush uploads (vertexes or clip box): every draw,
 *                       hide, show or release that changed one since the
 *                       last flush, each slot once;
 *                       buffer_regrows= the vertex buffers and clip-box
 *                       textures the frame re-allocated and sent whole,
 *                       because the mirror outgrew them (geometrically, so
 *                       rarely); image_regrows= the image pages made
 *                       again (twice the layers) since the last frame, an
 *                       image having found no layer free;
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
 *    --verify-idle      every frame an idle window loop would have skipped
 *                       (the canvas's nextWorkTime() past the clock before its
 *                       update) is drawn and fails the run (exit non-zero) if
 *                       it wrote a primitive or glyph slot, uploaded anything
 *                       or changed a pixel from the frame before; each is
 *                       named on stderr as
 *                         ControlTest: idle verify: control=<c> frame=<n> ...
 *                       In the window it is the loop's own verify mode, as
 *                       GWENI_IDLE_VERIFY=1 is (which turns both on).
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
 *                       context-menu | popup | dialog | toast |
 *                       virtual-list |
 *                       virtual-list-variable |
 *                       data-table-virtual | hide-show | demo-labels |
 *                       plot-histogram | plot-lines | chart | timeline |
 *                       inspector |
 *                       stats-overlay | depth-view | zoom-panel |
 *                       node-graph | code-editor | log-view |
 *                       chip-badge | navigation-rail | flow | board |
 *                       switch | spinner | image-grid | segmented |
 *                       range-slider (see below)
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
 *    --scroll DX,DY     mouse wheel, in wheel units (60 a notch): DY through
 *                       Canvas::inputMouseWheel, then DX, if not 0, through
 *                       inputMouseWheelH (the horizontal wheel)
 *    --key <name>       a key: return, kp-enter, tab, space, escape, up, down,
 *                       left, right, home, end, pageup, pagedown, backspace,
 *                       delete, shift, control, alt, command, f9, f10, f11
 *                       and f12 (which,
 *                       as in the window, open and close the depth view, the
 *                       stats overlay, the theme editor, saving to and
 *                       loading from gweni-theme.json in the cwd, and the
 *                       inspector, and are not passed on),
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
 *                       this (a TextBox's getText(), a CodeEditor's caret
 *                       line). A mismatch prints both on
 *                       stderr and the run exits non-zero at the end; the
 *                       script carries on, so every bad step is reported.
 *    --clipboard-test   run the script in a shown GL window with the system
 *                       clipboard (input::GLFW3::useSystemClipboard) instead
 *                       of offscreen with the in-process one, so its copies
 *                       reach other X clients and its pastes come from them.
 *                       It needs a display; clipboard_x11 runs it.
 *    --wait-clipboard-change <s>
 *                       with --clipboard-test: poll the window's events (which
 *                       serves the selection to other clients) until the
 *                       clipboard's text is no longer what it was when the
 *                       step began, failing the run after <s> seconds
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
 *                   node below it up; hovering a label changes its pixels
 *                   alone in a frame writing 1..2 primitive slots and moving
 *                   off gives the frame before back, a click selects it with
 *                   no pixel changed outside it, a click on another label
 *                   moves the selection, changing no pixel and writing no
 *                   primitive slot outside the two labels and no more glyph
 *                   slots a frame than their strings, and a settled frame
 *                   writes nothing (a line reports the counts).
 *                   SelectableDemo: hovering a free row
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
 *                   where they were before it opened; "Modal Window" opens a
 *                   window on the canvas's modal stack, under which a move
 *                   onto and a click on "Normal Window" hover, open and write
 *                   nothing, and Escape closes it with the frame before it
 *                   opened and the live ids back. SliderDemo: each frame
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
 *    chart        ChartTimeBarsDemo, ChartLinesDemo and ChartAreasDemo, the clock
 *                 frozen, their
 *                 x and value ranges fixed as laid out: a stacked point changed
 *                 has to write at most its stack's bars at its x, an appended
 *                 bar its own slot, a line point at most the four segments
 *                 around it, a step line point the five around its two
 *                 vertices, an appended line point its segment and its
 *                 joint's, a moved marker its slot and its caption's glyphs,
 *                 the mouse onto the plot the crosshair's slot alone, the
 *                 canvas's ToolTip box once its delay passed, a move within
 *                 one x nothing; a legend click toggles its series once each
 *                 way (onSeriesToggled), a click on a bar, a stacked segment
 *                 above it and next to a line point fires onPointClicked with
 *                 that point; an area point (ChartLinesDemo's swap) at most
 *                 the two trapezoids beside it and its line's segments around
 *                 it, a stacked area point (ChartAreasDemo) at most the two
 *                 trapezoids beside it and those of each band stacked on it
 *                 at its x, the top band's its own two, the lines' segments
 *                 around them, a hover over the bands the crosshair's slot, a
 *                 click inside a band its point, and a legend click hides a
 *                 band (the one above dropping onto the one below) and shows
 *                 it again; every settled frame nothing; each demo gives
 *                 every primitive id back (chart-bars.png, chart-lines.png,
 *                 chart-areas.png).
 *                 One line
 *                   chart stack_bars=4 stack_prim_slots=... ids_back=yes
 *                 Exits non-zero if any check is no.
 *    timeline     TimelineDemo (1000 runs over 96 lanes, its day's x range
 *                 fixed), the clock frozen: a bar shortened has to write its own
 *                 slot alone, a bar added its own (a spare slot of its
 *                 lane's list), the mouse onto a bar the hover look's slot
 *                 alone, the canvas's ToolTip box once its delay passed, a
 *                 move within the bar nothing, onto the next bar no more than
 *                 the two bars', the look's and the box's slots; a click on
 *                 a bar fires onBarClicked with its lane, index and id; a bar
 *                 added over another shows in front of it (its own colour at
 *                 its middle pixel), is the one the hit test finds and takes
 *                 the click; a wheel notch scrolls WheelLanes lanes writing
 *                 no more than the stripes, bars and captions of the lanes in
 *                 view before or after it, the hover look and the scroll bar,
 *                 and says whether it wrote bar slots (bars_moved=); every
 *                 settled frame nothing; the demo gives every primitive id
 *                 back (timeline.png, timeline-scrolled.png). One line
 *                   timeline lanes=96 bars=1000 change_prim_slots=1 ... ids_back=yes
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
 *    depth-view   ButtonDemo, the clock frozen, the mouse resting on "Toggle
 *                 Me": F9 (scripted, through the window's key handler) opens
 *                 the depth view (gweni::tools::DepthView), a fixed camera
 *                 (perspective, turned and dollied out) is set, then the mouse
 *                 and keys move it (a left-drag, the wheel, Left, P and +), a
 *                 click lands on the toggle and F9 closes it. Every one of
 *                 those frames must enter no control, change, write and upload
 *                 nothing, lay out nothing, renumber no z and hit-test nothing
 *                 (its input included); the click presses nothing; the toggle
 *                 is still hovered, the focus, tree serial and live ids as they
 *                 were; and the frame after the close is the one before the
 *                 open, byte for byte. Shots depth-view-before.png,
 *                 -perspective.png, -ortho.png and -after.png. Then the demo
 *                 shell's Depth button (Demo built on the canvas): an off
 *                 toggle docked Right in the status bar left of Stats, tooltip
 *                 "Depth view (F9, Esc leaves)", on exactly while the view is
 *                 open after F9, Escape, a click on it, a click on it with the
 *                 view open (which has the mouse, so nothing changes), F9, and
 *                 a shell built with the view open, then Escape; the view told
 *                 (DepthView::onOpenChangedCaller) once per open and close.
 *                 One line
 *                   depth-view open=visited:0,... camera_drawn=yes shell_button=yes
 *                 Exits non-zero if any check is no. controltest_depth_view
 *                 (tests/depthView.cmake) runs it twice and requires the same
 *                 PNGs.
 *    zoom-panel   ButtonDemo in a ZoomPanel filling a scale-1 canvas at zoom
 *                 1, 1.5, 1.37 and 2 against ButtonDemo on a canvas at
 *                 setScale(zoom) (zoom-<z>-canvas.png, -panel.png): the same
 *                 pixels, every 3rd device px hitting the same control, the
 *                 mouse at each Button's centre hovering the same one, and the
 *                 same pixels once "Toggle Me" is clicked. Then ZoomPanelDemo
 *                 at scale 1: a wheel zoom, a setZoom, an SDF-range zoom (2 to
 *                 2.5, uploading no atlas) and right, middle and Space+left
 *                 pans each lay nothing out, call no invalidate(), write slots
 *                 of the panel alone and leave a frame writing nothing; the
 *                 wheel keeps the point under the cursor and stops at the
 *                 range's ends, and over the ScrollControl scrolls it; the
 *                 tooltip box, the ComboBox's list and a right click's context
 *                 menu open as canvas children at the controls as drawn
 *                 (zoom-tooltip.png, -combo-list.png, -context-menu.png); the
 *                 demo deleted gives its ids back. One line per zoom and per
 *                 frame kind, e.g.
 *                   zoom-panel zoom=1.37 pixels_match=yes grid_misses=0 ...
 *                   zoom-panel right_pan layout_passes=0 invalidations=0 ...
 *                 Exits non-zero if any check fails, naming it on stderr.
 *    node-graph   NodeGraphDemo on a 900x560 canvas at scale 1, the clock
 *                 frozen, through the GLFW translator: a title-bar drag of a
 *                 two-node selection (every frame writing the two nodes' and
 *                 their links' slots alone, changing no more controls than
 *                 they hold, one layout pass entering no more than them and
 *                 their ancestors, onNodesMoved once on the release) and the
 *                 app moving them back at the same cost; a pin's and a link's
 *                 hover (2 units off the curve hovers it, 6 does not), each
 *                 writing the hovered item and the left one, and at zoom 4
 *                 every canvas unit across five of the link's segments hovered
 *                 exactly when its centre is within 4 of the curve
 *                 (hover_link_zoom4 tested=... wrong=0); a pin drag's
 *                 preview at the mouse, a vetoed and a made link, a drop on
 *                 empty space (onLinkDroppedOnEmpty at the mouse's graph point,
 *                 the demo's Add node menu) and Escape; click, Shift and Ctrl
 *                 selection and an empty-space clear; a marquee whose every
 *                 frame writes its slots and the (de)selected items' alone,
 *                 Escape restoring; the graph's, a node's and a link's context
 *                 menus at the right press's graph point and a right drag that
 *                 pans; a pan and a zoom with no layout pass; a vetoed and a
 *                 made Delete giving every id back; the demo deleted giving its
 *                 ids back (node-graph.png). One line per measured frame, e.g.
 *                   node-graph drag_3 prim_slots=... strays=0
 *                 Exits non-zero if any check fails, naming it on stderr.
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
 *                 box's pixels (all but its corners outside the rounded
 *                 arcs, where the Menu shows) are the ones it has with no
 *                 Menu. One line
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
 *    dialog       dialogs::Dialog and the standard dialogs on DialogDemo and
 *                 LargeDialogDemo, the clock frozen and the mouse off the
 *                 canvas: each of the message, the two confirms and the
 *                 prompt opened by its button's action writes only the
 *                 dialog's ids in its open frame and nothing (rendering no
 *                 control) in a settled one, and Enter, Escape or "abc" and
 *                 Enter close it with the result label naming the result and
 *                 no other pixel changed; the large dialog is 1200 x 0.92 of
 *                 the canvas clamped to it and centred, a change behind it
 *                 writes only that Label's ids and changes no pixel inside
 *                 it, a confirm from its Force release button stacks over it,
 *                 Escape gives it back and Enter (Yes) logs into it, and
 *                 Escape and Enter each close it with the frame before it
 *                 opened and the live ids back. Shots dialog-*.png. One line
 *                   dialog message_open_alone=yes|no ... large_return_closes=yes|no
 *                 reports the checks. Exits non-zero if any check is no.
 *    toast        Canvas::notify's toasts on ToastDemo, the clock stepping:
 *                 Info then Positive stack in the top-right corner, the newer
 *                 above, Toast::Gap in from the corner; a settled frame writes
 *                 and renders nothing and runs no animation; the mouse onto a
 *                 toast pauses the stack, which outlasts both 4 s timeouts
 *                 while it rests there, its entry, held and exit frames
 *                 writing nothing; off it, they go some seconds later, not at
 *                 once, giving back the frame before them and the live ids;
 *                 a multi-line toast is taller than a one-line one, and a
 *                 sticky one outlasts it and goes with its close button, the
 *                 frame and the ids back again. Shots toast-*.png. One line
 *                   toast stack=yes|no ... multi_line_sticky=yes|no
 *                 reports the checks. Exits non-zero if any check is no.
 *    code-editor  CodeEditorDemo's 5000-line document (line numbers on,
 *                 highlighted as YAML), the clock frozen, scrolled to line
 *                 2500, a press at the end of a short line ("    model: ...")
 *                 giving the editor the focus; then each input one frame: "x"
 *                 typed there, a burst typed two lines down and undone
 *                 (Ctrl+Z), a block scalar opened at the last agent entry on
 *                 screen ("  - name: ..." made "  - |", the entry's lines
 *                 below it, past the screen, in the block) and undone, Return
 *                 there, and a one-line scroll (scrollByRows), each followed
 *                 by settled frames (code-editor.png at the end). One line
 *                   code-editor settled_focused=yes|no ... ids_back=yes|no
 *                 reports the checks: every settled frame writes and changes
 *                 nothing; the typed character, which keeps the state the
 *                 next line starts in, tokenizes its line alone and writes
 *                 slots of its row, the gutter and the caret alone, no more
 *                 glyphs than its line, in one pass, and changes no pixel
 *                 outside the line, the gutter and the caret; the frame after
 *                 the undo is the frame before the burst, byte for byte; the
 *                 opened block gives every row on screen below the entry new
 *                 colours, writes the slots of the rows from it down, the
 *                 gutter and the caret alone, no more glyphs than they hold,
 *                 in one pass, changes no pixel outside the editor and
 *                 tokenizes no line past the screen's last, and its undo
 *                 gives the frame before it back; Return and the scroll
 *                 write slots inside the editor alone, no more glyphs than
 *                 its rows and numbers on screen hold, in one pass, and
 *                 change no pixel outside it. Then wrapped, through the demo's
 *                 "Wrap lines" toggle: the caret's line stays on screen; "y"
 *                 typed at the end of a one-row "    host:" line writes its
 *                 row's, the gutter's and the caret's slots alone and changes
 *                 no pixel outside them; that line filled to its last cell
 *                 and one more letter gives it a second row and moves the row
 *                 below a line down, in one pass, writing inside the editor
 *                 alone and changing no pixel outside it; the canvas at 3/4
 *                 and back keeps the caret's line on screen; a wrapped line
 *                 selected draws one rect of the CodeSelection fill on each
 *                 of its visual rows, and after a resize on each of its new
 *                 ones; the toggle off keeps the caret's line on screen; the
 *                 demo gives every primitive id back. Last CodeEditorFormDemo:
 *                 its comment field (plain, auto-height 2 to 6 rows) filled to
 *                 two rows and one letter more grows a row in one pass,
 *                 entering at most its subtree and ancestors and each field
 *                 below once, moving the last field a line down, writing
 *                 inside the form and changing no pixel above the field; the
 *                 form gives its ids back. Every settled frame writes and
 *                 changes nothing. Exits non-zero if any check is no.
 *    log-view     LogViewDemo's log filled with 1 000 and then 100 000 events,
 *                 the clock frozen, the demo's stream stopped and the idle
 *                 re-measure off; at each count: 3 appends while following,
 *                 a wheel notch up and 5 appends, a press on the jump button,
 *                 then from the middle (scrollToRow) a replace 5 rows above
 *                 the viewport, the canvas 120 units narrower and back, a
 *                 press on a collapsed block's title and a second one, a click
 *                 and a Shift+click selecting 3 plain rows and Ctrl+C, the
 *                 view scrolled to the top and back to the same row, on those
 *                 rows a drag inside the first, on to the middle of the third
 *                 and Ctrl+C, the view scrolled to the top and back again, a
 *                 double-click on a word of the second and a Shift+click on
 *                 the third, and the
 *                 cap set to the row count with 10 appends (log-view-<N>.png
 *                 at the end). One line
 *                   log-view settled_1000=yes|no ... same_pool=yes|no
 *                 reports the checks: every settled frame writes and changes
 *                 nothing; following, the appends write the rows' and the
 *                 bar's slots alone and the last row ends at the viewport's
 *                 bottom; the wheel detaches (the jump button shown), and the
 *                 appends then write no row slot, change no pixel outside the
 *                 bar and the button and are counted on it; its press follows
 *                 again; the replace changes no pixel in the viewport and
 *                 moves the offset; the resize keeps the row at the top; the
 *                 expand changes no pixel above the block or outside the log,
 *                 and the collapse gives the frame back; Ctrl+C copies the 3
 *                 rows' text joined by '\n'; at the top every row is marked
 *                 selected as its row is, and back the selection shows again
 *                 in the viewport byte for byte; a drag frame inside a row
 *                 writes that row's highlight slots alone (no glyph slot, no
 *                 pixel changed outside it), the drag to the third row copies
 *                 exactly the characters from the press to the mouse joined
 *                 by '\n', scrolled off and back it shows byte for byte (at
 *                 the top no row showing it), the double-click selects its
 *                 word and the Shift+click extends from the word's start to
 *                 the character clicked; the cap's drops move nothing outside
 *                 the bar and the button; the pool, the controls and the ids
 *                 the log holds are the same at both counts, and the demo
 *                 gives every primitive id back. Exits non-zero if any check
 *                 is no.
 *    chip-badge   ChipBadgeDemo, the clock frozen (chip-badge.png): the
 *                 Inbox entry's badge counted one more (the same width),
 *                 then 9 -> 10 (wider); the "Corners" button, a badge at
 *                 each corner, moved 16, 8 units and back; the mouse onto
 *                 the "project: gweni" chip and off; a click on the first
 *                 removable chip's x. One line
 *                   chip-badge count_same_width=yes|no ... ids_back=yes|no
 *                 reports the checks: a count of the same width writes the
 *                 badge's ids alone, lays nothing out and changes no pixel
 *                 outside it; a wider one the same, in one pass entering only
 *                 the way down to the badge; the move enters only the way down
 *                 to the anchor (neither its Text nor its badges) and its and
 *                 its badges' pixels move with it; the hover writes the chip's
 *                 own slot alone, changes no pixel outside it, and moving off
 *                 gives the frame back; the removal (the demo's onRemove
 *                 handler deleting the chip) lays out in one pass entering no
 *                 more than the row and the way down to it and gives back the
 *                 chip's ids; settled frames write nothing; the demo gives
 *                 every id back. Exits non-zero if any check is no.
 *    navigation-rail NavigationRailDemo at 640x480, the clock frozen except
 *                 for the tooltip delay (navigation-rail*.png): the app's
 *                 selection callback, menu and count buttons, then the rail's
 *                 hover, click and Material keyboard activation; footer keys
 *                 skip the disabled Archive row. The app callback is detached
 *                 for the rail's cost checks. Hover changes only its row;
 *                 selection writes the two rows and their label glyphs, with
 *                 no layout. Badge counts of the same and a wider size touch
 *                 only the badge, expanded and collapsed. Collapse takes one
 *                 pass through the rail and its parent's path, hiding labels
 *                 and showing a delayed canvas tooltip. Expansion restores
 *                 the pre-collapse frame byte for byte. Resizing below 560
 *                 collapses from the app's layout in one pass; resizing back
 *                 restores that same frame. Every settled frame writes
 *                 nothing. One line
 *                   navigation-rail hover=yes|no ... idle_work=N
 *                 reports the checks. Exits non-zero if any check is no.
 *    flow         FlowDemo, the clock frozen, the canvas 480 high (flow-*.png):
 *                 canvas widths worked out from the canvas less the Flow's
 *                 width at 640 (the bar and the insets), so the Flow is 1023,
 *                 1024, 1535 and 1536 wide, then 640, 2560 and 640 again; at
 *                 2560 card 0's body grown by a few words that keep its row's
 *                 height; at 640 scrolled 200 down, card 1's body grown, the
 *                 cards replaced in one frame (delayedDelete and a new set),
 *                 and the cards removed in one frame and added in the next.
 *                 One line
 *                   flow columns=yes|no ... ids_back=yes|no inset=N
 *                   columns=<flow width>:<columns> ...
 *                 reports the checks: each size shows its breakpoint's
 *                 columns (the cards' distinct x in the --dump-bounds
 *                 listing; 1 below 1024, 2 below 1536, else 3), the Flow that
 *                 wide and its full rows ending at its edge, in one pass; the
 *                 first frame is one pass; the last 640 is the first one's
 *                 listing, renderer mirror (slot by owner) and pixels; the
 *                 card's edit moves no card, changes no pixel outside it and
 *                 enters no more than the way down to it and its subtree; the
 *                 growth and the rebuild keep the offset (clamped only to a
 *                 shorter content's end); the empty frame clamps it to 0,
 *                 where it stays; settled frames write nothing; the demo gives
 *                 every id back. Exits non-zero if any check is no.
 *    board        BoardDemo at 640x480, the clock frozen (board*.png): a
 *                 vertical wheel over the overflowing column's body, then at
 *                 its bottom and its top and over the fitting column's body;
 *                 a vertical wheel over the board's inset, the gap after the
 *                 first column and its header; a horizontal wheel and a
 *                 Shift+wheel over a body; then, scrolled, a card added to
 *                 the overflowing and to the fitting column and a card's text
 *                 grown, then cards added to the fitting column until it
 *                 overflows. One line
 *                   board board_overflows=yes|no body_wheel=yes|no ...
 *                   ids_back=yes|no offsets=<board>,<body>,...
 *                 reports the checks: each wheel's frame has nothing pending
 *                 outside the ScrollControl it is meant to scroll and writes
 *                 only that subtree's shown slots, in at most one pass; the
 *                 body wheel moves that body alone, at either end and in the
 *                 fitting column it is consumed with nothing moved, and the
 *                 inset, gap, header, horizontal and Shift wheels move the
 *                 board alone; the edits keep every offset in one pass and
 *                 the fitting column's cards their widths, also once it
 *                 overflows; every body shows its bar, disabled exactly while
 *                 its cards fit, the fitting one's enabled once it overflows; settled
 *                 frames write nothing; the demo gives every id back. Exits
 *                 non-zero if any check is no.
 *    switch       SwitchDemo, the clock stepping (switch.png, switch-on.png),
 *                 the "Label on the right" switch focused throughout and the
 *                 mouse off it between inputs: the mouse onto its track, onto
 *                 its label and off; a click on the track and a second one; a
 *                 click on the label twice; Space twice; the mouse onto and a
 *                 click on each disabled switch; the "Allow apply" switch
 *                 clicked twice; a click and the demo hidden two frames into
 *                 the slide. One line
 *                   switch settled_start=yes|no hover=yes|no ... ids_back=yes|no
 *                 reports the checks: the hover writes the track's slot alone
 *                 (the label's the same look, writing nothing more) and
 *                 changes no pixel outside the switch, and moving off gives
 *                 the frame back; each toggle's input frame writes the
 *                 switch's ids alone and each frame of its slide (3 or more
 *                 writing the thumb) the thumb's slot alone, with no layout
 *                 pass and no pixel changed outside the switch, until a frame
 *                 runs no animation and writes nothing; the second click,
 *                 the label and Space give the same on and off looks; a
 *                 disabled switch writes nothing, keeps its state and its
 *                 pixels and takes no focus, hovered or clicked; the Apply
 *                 button follows the "Allow apply" switch; the frame after
 *                 the hide runs no animation, the thumb at its end; settled
 *                 frames write nothing; the demo gives every id back. Exits
 *                 non-zero if any check is no.
 *    spinner      SpinnerDemo, the clock stepping and the mouse off the
 *                 canvas between clicks (spinner.png, spinner-stopped.png):
 *                 20 frames of every spinner turning; a click on Stop and 10
 *                 frames; a click on Start and 10; a click on "Hide spinner",
 *                 10 frames, and a second click; the demo hidden and shown;
 *                 every spinner stopped. One line
 *                   spinner spinning=yes|no stop=yes|no ... ids_back=yes|no
 *                 reports the checks: a spinning frame writes arc slots alone,
 *                 each of a spinner whose arc's bits changed, lays nothing
 *                 out and changes no pixel outside the spinners, with
 *                 anims_run the turning spinners' count; after Stop the card's
 *                 spinner is a still ring, writes nothing and anims_run is
 *                 one fewer, and Start turns it from the clock's arc again;
 *                 the frame after the hide runs one animation fewer and the
 *                 show resumes it from the clock's arc; with the demo hidden
 *                 the next frame runs none and settled frames write nothing,
 *                 and shown every spinner turns again; every spinner stopped,
 *                 settled frames write nothing and run no animation; the demo
 *                 gives every id back. Exits non-zero if any check is no.
 *    field-validation
 *                 FieldValidationDemo, the clock frozen and the mouse off
 *                 the canvas between inputs (field-validation.png,
 *                 field-validation-error.png, field-validation-required.png):
 *                 the key field's error set and cleared by the app
 *                 (FieldValidationDemo::setKeyError, clearKeyError), then by
 *                 its two buttons; "a", "b", "cdefg" and "h" typed into the
 *                 Change-validated password; a click into the empty host
 *                 field and Tab. One line
 *                   field-validation app_set=yes|no ... ids_back=yes|no
 *                 reports the checks: the set and the clear each lay out the
 *                 field alone (one pass, entering no more than the way down
 *                 to it and its subtree), write its frame's slot and the
 *                 message's glyphs alone and change no pixel outside it, at
 *                 the same height; the clear gives the frame before the set
 *                 back; each button click does the same, its button's own
 *                 slots and pixels allowed too, and with the mouse gone the
 *                 frame before them is back; "a" shows the error in the
 *                 field's ids alone, "b" writes no message glyph, "h" (the
 *                 eighth) clears it; Tab shows "required"; settled frames
 *                 write nothing; the demo gives every id back. Exits non-zero
 *                 if any check is no.
 *    image-grid   ImageGridDemo, the clock frozen (image-grid*.png): a click
 *                 on Replace swaps the first cell's image and a second gives
 *                 it back, a click on Free clears the second cell's and a
 *                 second loads it again. One line
 *                   image-grid images_loaded=yes|no ... ids_back=yes|no
 *                 reports the checks: each click's frame writes no slot
 *                 outside the button and the cell and changes no pixel
 *                 outside them, the renderer's live image count stays (one
 *                 less while freed), the second clicks give the cells' first
 *                 pixels back, settled frames write nothing, and the demo
 *                 gives its ids and images back. Exits non-zero if any check
 *                 is no.
 *    segmented    SegmentedButtonsDemo, the clock frozen (segmented.png): the
 *                 view row's joints measured on the settled frame, then with
 *                 the demo's selection label not updating: the mouse onto
 *                 Month and off, a click on it (Week selected before), Right
 *                 and Left, then the Format row's Underline focused, toggled
 *                 by Space twice and by two clicks; last the label updating
 *                 again and a click on Day. One line
 *                   segmented joints=yes|no ... ids_back=yes|no border_px=N
 *                     outer=N 0|1=N ...
 *                 reports the checks: every inner joint of the view row,
 *                 scanned just under the top border from one segment's
 *                 middle to the next one's, is one run of the border colour
 *                 as wide as the outer border (max(1, snap(1)) device px);
 *                 focus_joints: each segment after the first focused in turn
 *                 leaves its left and right joints one run of SegmentFocus's
 *                 border that wide, none of Segment's beside it (at 1.5 a
 *                 1-unit joint is 1 or 2 px by its position), and unfocused
 *                 gives the settled frame back;
 *                 the hover writes the segment's one slot, no glyph and no
 *                 pixel outside it, marks no control outside it, and moving
 *                 off gives the frame back; the click and each key write the
 *                 two segments' slots and glyphs alone, mark no control
 *                 outside them, lay nothing out and change no pixel outside
 *                 them, and
 *                 Left gives the frame before Right back; each toggle writes
 *                 the one segment, lays nothing out, and the second gives the
 *                 first frame back; the label names the view chosen; settled
 *                 frames write nothing; the demo gives every id back. Exits
 *                 non-zero if any check is no.
 *    range-slider RangeSliderDemo, the clock frozen (range-slider.png,
 *                 range-slider-gap.png): the mouse onto the price's low thumb
 *                 and off; a drag of each price thumb toward the other, of the
 *                 vertical one's thumbs and of the joined one's high thumb,
 *                 each 12 one-unit steps, 2 of a tenth of the slider along
 *                 the track and 2 off it, then one past the other thumb, and
 *                 the release there; the price's low thumb focused and Right,
 *                 Left, Up, Down, PageUp, PageDown, End and Home, then Tab and
 *                 Home. One line
 *                   range-slider settled_start=yes|no hover=yes|no
 *                     drag_<name>=yes|no gap_<name>=yes|no ... ids_back=yes|no
 *                     glyph_kept_frames=N
 *                 reports the checks: the hover writes the thumb's slot alone
 *                 and moving off gives the frame back; every drag and key
 *                 frame writes the thumb's slot, the fill's and its text's
 *                 glyphs alone (none when its string stays), lays nothing
 *                 out, changes at most those three controls and no pixel
 *                 outside the slider, and moves the thumb by exactly the step
 *                 unless it stopped at a limit; each drag ends at the gap and
 *                 its release leaves the thumb unhovered; the keys give the
 *                 values a step, ten steps and the limits give; Tab focuses
 *                 the high thumb, in front; settled frames write nothing; the
 *                 demo gives every id back. Exits non-zero if any check is no.
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
 *    virtual-list-variable
 *                 VirtualListVariableDemo (a VirtualList of wrapped rows,
 *                 each its measurer's height) with 1 000 and then 100 000
 *                 items, the clock frozen and the idle re-measure off: a
 *                 settled frame, a one-row scroll and a wheel notch, each
 *                 followed by settled frames; then from the middle of the
 *                 list an item above the viewport grown (itemChanged) and
 *                 one inserted above it, the list made 60 units narrower,
 *                 and the idle re-measure given a budget (64 at 1 000, 8192
 *                 at 100 000) and run to its end
 *                 (virtual-list-variable-1000.png, ...-100000.png at the
 *                 end of the list). One line per count
 *                   virtual-list-variable items=N rows=N subtree=N
 *                          prim_ids=N idle_slots=0 ... row_measures=N
 *                          ... grow_changed=0 grow_moved=1 ...
 *                          remeasure_frames=N remeasure_ended=1
 *                          content_height=N measured_sum=N ids_left=0
 *                 gives the counters, and one line
 *                   virtual-list-variable same_cost=yes|no settled=yes|no
 *                          rows_only=yes|no edits_above_still=yes|no
 *                          resize_keeps_top=yes|no remeasure_ends=yes|no
 *                          ids_back=yes|no
 *                 the checks: the pool, the subtree, the ids and the
 *                 one-row scroll's and the wheel's counters (their measures
 *                 included, less the thumb's slot where it moved) are the
 *                 same at both counts; settled frames write and change
 *                 nothing; each scroll writes no more than its shown rows'
 *                 slots, enters no more than the list's subtree and the two
 *                 controls above it and lays out in one pass with no
 *                 request (its z renumber, for a row whose new item has
 *                 another line count, inside the list); the growth and the
 *                 insert above the viewport move the offset and change no
 *                 pixel inside the viewport; the narrower list measures
 *                 again (a new generation) with the same item's row at the
 *                 same top; the re-measure ends, changes no pixel inside the
 *                 viewport meanwhile, runs no animation after, and leaves
 *                 the content height the sum of every item's measure; the
 *                 demo gives every primitive id back. Exits non-zero if any
 *                 check is no.
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
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/input/glfwLoop.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/windowControl.h>
#include <gweni/controls/checkBox.h>
#include <gweni/controls/label.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/button.h>
#include <gweni/controls/rangeSlider.h>
#include <gweni/controls/segmentedButtons.h>
#include <gweni/controls/selectable.h>
#include <gweni/controls/toolTip.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/toast.h>
#include <gweni/controls/slider.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/scrollBarButton.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/codeEditor.h>
#include <gweni/controls/virtualList.h>
#include <gweni/controls/dataTable.h>
#include <gweni/controls/properties.h>
#include <gweni/controls/zoomPanel.h>
#include <gweni/tools/depthView.h>
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
#include <set>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#if !defined(__EMSCRIPTEN__)
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <sys/resource.h>
#endif

#include <demo/button.h>
#include <demo/demo.h>
#include <demo/checkBox.h>
#include <demo/collapsibleList.h>
#include <demo/colorPicker.h>
#include <demo/comboBox.h>
#include <demo/contextMenu.h>
#include <demo/popup.h>
#include <demo/chipBadge.h>
#include <demo/spinner.h>
#include <demo/switch.h>
#include <demo/fieldValidation.h>
#include <gweni/controls/spinner.h>
#include <gweni/controls/switch.h>
#include <demo/navigationRail.h>
#include <gweni/controls/navigationRail.h>
#include <demo/segmentedButtons.h>
#include <gweni/controls/badge.h>
#include <gweni/controls/chip.h>
#include <demo/dialog.h>
#include <demo/toast.h>
#include <demo/crossSplitter.h>
#include <demo/dataTable.h>
#include <demo/codeEditor.h>
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
#include <demo/chartTimeBars.h>
#include <demo/timeline.h>
#include <demo/chartLines.h>
#include <demo/chartAreas.h>
#include <gweni/controls/chart.h>
#include <gweni/controls/timeline.h>
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
#include <demo/rangeSlider.h>
#include <demo/sliderText.h>
#include <demo/dragNumber.h>
#include <gweni/controls/dragNumber.h>
#include <demo/vectorField.h>
#include <demo/zoomPanel.h>
#include <demo/flow.h>
#include <demo/board.h>
#include <gweni/controls/board.h>
#include <gweni/controls/boardColumn.h>
#include <demo/imageGrid.h>
#include <gweni/controls/imagePanel.h>
#include <gweni/controls/layout/flow.h>
#include <demo/nodeGraph.h>
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
#include <demo/virtualListVariable.h>
#include <demo/logView.h>
#include <demo/window.h>
#include <demo/windowButtons.h>

// A capture's clock: one fixed step per rendered frame, so every run sees the
// same times. 1/60 s is the step, i.e. a nominal 60fps. The clock counts whole
// steps and reads as steps*(1.0/60) in double, so the time at a frame does not
// depend on how many steps were added before it.
static const double g_frameStep=1.0/60.0;

// Every scripted press also steps the clock, by more than inputHandler's 0.5s
// double-click window (source/inputHandler.cpp), so that two --clicks in a row
// are two clicks however fast the process runs. --double-click is the only way
// to ask for a double click: it issues its second press with no step in
// between, i.e. 0s apart, which is inside any window. 60 steps, 1 s.
static const int64_t g_pressSteps=60;

static int64_t g_clockSteps=0;

static double steppedClock()
{
    return double(g_clockSteps)*g_frameStep;
}

static gweni::input::GLFW3 g_glfwInput;
static gweni::renderer::Base *g_renderer=nullptr;
static gweni::controls::Canvas *g_canvas=nullptr;
static float g_manualScale=1.0f;        // --scale or GWENI_SCALE

// --stats-overlay: every canvas createCanvas() makes opens the stats overlay.
static bool g_statsOverlay=false;
// --depth-view: renderFrames and the window open the depth view on the canvas
// after its first update phase (capture::openDepthViewOnce).
static bool g_depthView=false;

// The app's own keys, before the canvas sees them: F12 opens and closes the
// inspector, F11 the theme editor, F10 the stats overlay, F9 the depth view
// (the library binds no key). Coming first, they work while the depth view
// takes the canvas's input.
static bool appKey(int key, int action)
{
    if(((key != GLFW_KEY_F12) && (key != GLFW_KEY_F11) && (key != GLFW_KEY_F10)
        && (key != GLFW_KEY_F9)) || !g_canvas)
        return false;
    if(action != GLFW_PRESS)
        return true;
    if(key == GLFW_KEY_F9)
        gweni::tools::DepthView::toggle(g_canvas);
    else if(key == GLFW_KEY_F12)
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
    X(GroupBoxDemo) X(ImagePanelDemo) X(StatusBarDemo) X(SeparatorDemo) X(PlotHistogramDemo) X(PlotLinesDemo) X(ChartTimeBarsDemo) X(ChartLinesDemo) X(ChartAreasDemo) X(TimelineDemo) X(ComboBoxDemo) \
    X(TextBoxDemo) X(ListBoxDemo) X(SelectableDemo) X(NavigationRailDemo) X(VirtualListDemo) X(VirtualListVariableDemo) X(DataTableDemo) X(DataTableVirtualDemo) X(CodeEditorDemo) X(CodeEditorFormDemo) X(LogViewDemo) X(ContextMenuDemo) X(PopupDemo) X(ChipBadgeDemo) X(SegmentedButtonsDemo) X(DialogDemo) X(LargeDialogDemo) X(ToastDemo) X(CrossSplitterDemo) X(RadioButtonDemo) \
    X(CheckBoxDemo) X(SwitchDemo) X(SpinnerDemo) X(FieldValidationDemo) X(NumericDemo) X(SliderDemo) X(SliderTextDemo) X(RangeSliderDemo) X(DragNumberDemo) X(VectorFieldDemo) X(MenuStripDemo) \
    X(WindowDemo) X(WindowButtonsDemo) X(TreeControlDemo) X(PropertiesDemo) X(TabControlDemo) X(TabCloseDemo) X(TabOverflowDemo) \
    X(ScrollControlDemo) X(PageControlDemo) X(CollapsibleListDemo) X(ColorPickerDemo) X(ColorEditDemo) \
    X(UnicodeTextDemo) X(TextSizesDemo) X(TextDemo) X(LayoutDemo) X(InspectorDemo) X(ThemeEditorDemo) \
    X(StatsOverlayDemo) X(ZoomPanelDemo) X(NodeGraphDemo) X(FlowDemo) X(BoardDemo) X(ImageGridDemo)

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
         "  --depth-view [on|off]  open the depth view after the first frame (in\n"
         "                     the window or a script; on if neither follows);\n"
         "                     F9 toggles it in the window, Escape closes it\n"
       <<gweni::capture::RendererHelp
       <<gweni::capture::LoopHelp<<
         "  --idle-test        the window's idle loop, measured: idle 2 s (no frame, no\n"
         "                     swap or present, near-zero CPU), then one frame for a post\n"
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
         "  --verify-idle      fail a frame an idle loop would skip that draws anything\n"
         "                     (also GWENI_IDLE_VERIFY=1)\n"
         "  --check-teardown   check the canvas renders empty again once every\n"
         "                     control has been destroyed; --control narrows it\n"
         "  --trace            log each scripted input on stderr\n"
         "  --scenario <name>  per-control | resize | docked | scroll | interaction\n"
         "                     | typing | clip | text-size | scale | tooltip\n"
         "                     | context-menu | popup | dialog | toast\n"
         "                     | virtual-list\n"
         "                     | virtual-list-variable\n"
         "                     | data-table-virtual | hide-show | demo-labels\n"
         "                     | plot-histogram | plot-lines | chart | timeline\n"
         "                     | inspector\n"
         "                     | stats-overlay | depth-view | zoom-panel\n"
         "                     | node-graph | code-editor | log-view | chip-badge\n"
         "                     | navigation-rail | flow | board | switch | spinner\n"
         "                     | image-grid | segmented | range-slider;\n"
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
         "                     virtual-list-variable that for wrapped rows of\n"
         "                     their own heights (and edits above the viewport,\n"
         "                     a resize and the idle re-measure moving nothing\n"
         "                     on screen),\n"
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
         "  --clipboard-test   the script in a shown GL window with the system\n"
         "                     clipboard (needs a display)\n"
         "  --wait-clipboard-change <s>  poll the window until the clipboard's\n"
         "                     text changes, failing after <s> seconds\n"
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
        " atlas_upload_calls=%zu atlas_upload_bytes=%zu buffer_regrows=%zu image_regrows=%zu"
        " layout_passes=%d redirtied_by=%s render_layout=%d layout_requests=%d requested_by=%s"
        " layout_visited=%zu layout_children_scanned=%zu measure_calls=%zu invalidations=%zu"
        " visited=%zu render_carried=%zu z_visited=%zu anims_run=%zu prim_ids=%zu excluded_changed=%zu\n",
        frame, control.c_str(),
        changes.controls, changes.created, changes.visibility,
        changes.geometry, changes.background, changes.text,
        gpu.primitiveSlotsWritten, gpu.textSlotsWritten,
        gpu.uploadCalls, gpu.uploadBytes, gpu.mirrorBytes, gpu.drawCalls,
        gpu.atlasUploadCalls, gpu.atlasUploadBytes, gpu.bufferRegrows, gpu.imagePageRegrows,
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
    // --verify-idle: whether an idle loop would have skipped the frame being
    // drawn (the canvas's nextWorkTime() was past the clock before its
    // update), and the frame before it as drawn, on the canvas it was drawn on
    bool idleSkip=false;
    std::vector<uint8_t> idlePixels;
    gweni::controls::Canvas *idleCanvas=nullptr;
    gweni::Point idlePixelsSize;
};

// --verify-idle (or GWENI_IDLE_VERIFY=1): every frame an idle window loop
// would have skipped is drawn and checked, and the frames that wrote, uploaded
// or showed anything are counted here and fail the run.
static bool g_verifyIdle=false;
static int g_idleVerifyFailures=0;

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

static std::vector<uint8_t> framePixels(const Session &s);

// --verify-idle, before a frame's update: whether an idle loop
// (gweni::platform::IdleLoop) would draw it at all.
static void verifyIdleBefore(Session &s)
{
    if(!g_verifyIdle)
        return;
    s.idleSkip=s.canvas->nextWorkTime() > steppedClock();
}

// --verify-idle, after a frame's render: one an idle loop would have skipped
// must write and upload nothing (the loop's own verify mode,
// input::GlfwLoop::frameWrites) and leave every pixel as the frame before it,
// which a change that writes no slot (a view transform) would not.
static void verifyIdleAfter(Session &s)
{
    if(!g_verifyIdle)
        return;

    const std::vector<uint8_t> pixels=framePixels(s);

    if(s.idleSkip)
    {
        std::string why=gweni::input::GlfwLoop::frameWrites(s.renderer->frameStats());
        const gweni::renderer::SlotRenderer::FrameStats &stats=s.renderer->getFrameStats();

        if(stats.atlasUploadCalls != 0)
            why+=std::string(why.empty() ? "" : ", ")+"uploaded "+std::to_string(stats.atlasUploadCalls)
                +" atlas regions";
        if((s.idleCanvas == s.canvas) && (s.idlePixelsSize.x == s.pixels.x) && (s.idlePixelsSize.y == s.pixels.y) && (pixels.size() == s.idlePixels.size()) && (pixels != s.idlePixels))
        {
            size_t changed=0;

            for(size_t i=0; i < pixels.size(); i+=4)
            {
                if(std::memcmp(&pixels[i], &s.idlePixels[i], 4) != 0)
                    ++changed;
            }
            why+=std::string(why.empty() ? "" : ", ")+"changed "+std::to_string(changed)+" pixels";
        }
        if(!why.empty())
        {
            ++g_idleVerifyFailures;
            std::cerr<<"ControlTest: idle verify: control="<<s.label<<" frame="<<s.frame
                     <<" drawn with no work reported (nextWorkTime past the clock): "<<why<<"\n";
        }
    }
    s.idlePixels=pixels;
    s.idlePixelsSize=s.pixels;
    s.idleCanvas=s.canvas;
}

static void renderFrames(Session &s, int count)
{
    for(int i=0; i<count; ++i)
    {
        gweni::controls::resetChangeStats();

        verifyIdleBefore(s);
        s.canvas->updateCanvas();
        if(g_depthView)
            gweni::capture::openDepthViewOnce(*s.canvas);
        s.renderer->clearTarget(s.background);
        s.canvas->renderCanvas();
        verifyIdleAfter(s);

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
            ++g_clockSteps;
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
    // the depth view's camera is built from the view (as in fitCanvasToWindow)
    gweni::tools::DepthView::viewChanged(s.canvas);
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
    gweni::tools::DepthView::viewChanged(s.canvas);
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
             <<" clock="<<steppedClock()
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
        g_clockSteps+=g_pressSteps;
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
    {"kp-enter", GLFW_KEY_KP_ENTER},     {"f9", GLFW_KEY_F9},
    {"f10", GLFW_KEY_F10},               {"f11", GLFW_KEY_F11},
    {"f12", GLFW_KEY_F12},               {"pageup", GLFW_KEY_PAGE_UP},
    {"pagedown", GLFW_KEY_PAGE_DOWN},
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

// The keyboard-focused control's text, if it has one: a Label's (which every
// TextBox is), or the line a CodeEditor's caret is on.
static bool focusedText(std::string &text)
{
    if(const auto *editor=dynamic_cast<const gweni::controls::CodeEditor *>(gweni::KeyboardFocus))
    {
        text=editor->getDocument().line(editor->getCaret().line);
        return true;
    }

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

// --wait-clipboard-change: the window's events polled until another client has
// put new text on the clipboard. While gweni owns the selection GLFW reads its
// own copy, so the change is the other client taking it; the polling is what
// answers that client's requests for gweni's copy before it does.
static bool waitClipboardChange(Session &s, int seconds)
{
    GLFWwindow *window=s.device->context().window;
    if(!window)
    {
        std::cerr<<"ControlTest: --wait-clipboard-change needs the --clipboard-test window\n";
        return false;
    }

    const std::string before=gweni::platform::getClipboardText();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);

    trace(s, "wait-clipboard-change from "+quotedText(before));
    while(std::chrono::steady_clock::now() < deadline)
    {
        glfwWaitEventsTimeout(0.05);

        const std::string now=gweni::platform::getClipboardText();
        if(now != before)
        {
            std::cout<<"clipboard-test: the clipboard changed to "<<quotedText(now)<<"\n";
            return true;
        }
    }
    std::cerr<<"ControlTest: --wait-clipboard-change: the clipboard still holds "<<quotedText(before)
             <<" after "<<seconds<<" s\n";
    return false;
}

static bool scrollWheel(Session &s, int delta)
{
    const bool used=s.canvas->inputMouseWheel(delta);
    trace(s, std::string("wheel ")+std::to_string(delta)+(used?" consumed":" ignored"));
    return used;
}

static bool scrollWheelH(Session &s, int delta)
{
    const bool used=s.canvas->inputMouseWheelH(delta);
    trace(s, std::string("wheel-h ")+std::to_string(delta)+(used?" consumed":" ignored"));
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
    g_clockSteps=0;
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
    ExpectText, WaitClipboard, Frames, Shot };

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
        if(step.kind != StepKind::ExpectText && step.kind != StepKind::WaitClipboard
            && step.kind != StepKind::Frames && step.kind != StepKind::Shot)
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
        case StepKind::Scroll:
            if(step.y)
                (void)scrollWheel(s, step.y);
            if(step.x)
                (void)scrollWheelH(s, step.x);
            break;
        case StepKind::Key: if(!sendKey(s, step.text)) return false; break;
        case StepKind::Type: typeText(s, step.text); break;
        case StepKind::ExpectText: (void)expectText(s, step.text); break;
        case StepKind::WaitClipboard: if(!waitClipboardChange(s, step.x)) return false; break;
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
    ToolTip, ContextMenu, Popup, Dialog, Toast, VirtualList, VirtualListVariable, DataTableVirtual, HideShow, DemoLabels, PlotHistogram,
    PlotLines, Chart, Timeline, Inspector, Theme, StatsOverlay, DepthView, ZoomPanel, NodeGraph, CodeEditor, LogView,
    ChipBadge, NavigationRail, Flow, Board, Switch, Spinner, FieldValidation, ImageGrid, Segmented,
    RangeSlider };

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
    else if(name == "dialog") scenario=Scenario::Dialog;
    else if(name == "toast") scenario=Scenario::Toast;
    else if(name == "virtual-list") scenario=Scenario::VirtualList;
    else if(name == "virtual-list-variable") scenario=Scenario::VirtualListVariable;
    else if(name == "data-table-virtual") scenario=Scenario::DataTableVirtual;
    else if(name == "hide-show") scenario=Scenario::HideShow;
    else if(name == "demo-labels") scenario=Scenario::DemoLabels;
    else if(name == "plot-histogram") scenario=Scenario::PlotHistogram;
    else if(name == "plot-lines") scenario=Scenario::PlotLines;
    else if(name == "chart") scenario=Scenario::Chart;
    else if(name == "timeline") scenario=Scenario::Timeline;
    else if(name == "inspector") scenario=Scenario::Inspector;
    else if(name == "theme") scenario=Scenario::Theme;
    else if(name == "stats-overlay") scenario=Scenario::StatsOverlay;
    else if(name == "depth-view") scenario=Scenario::DepthView;
    else if(name == "zoom-panel") scenario=Scenario::ZoomPanel;
    else if(name == "node-graph") scenario=Scenario::NodeGraph;
    else if(name == "code-editor") scenario=Scenario::CodeEditor;
    else if(name == "log-view") scenario=Scenario::LogView;
    else if(name == "chip-badge") scenario=Scenario::ChipBadge;
    else if(name == "navigation-rail") scenario=Scenario::NavigationRail;
    else if(name == "flow") scenario=Scenario::Flow;
    else if(name == "board") scenario=Scenario::Board;
    else if(name == "switch") scenario=Scenario::Switch;
    else if(name == "spinner") scenario=Scenario::Spinner;
    else if(name == "field-validation") scenario=Scenario::FieldValidation;
    else if(name == "image-grid") scenario=Scenario::ImageGrid;
    else if(name == "segmented") scenario=Scenario::Segmented;
    else if(name == "range-slider") scenario=Scenario::RangeSlider;
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
        std::vector<size_t> primitiveIds, textIds;

        skinControl->appendIds(primitiveIds, textIds);
        for(size_t id : primitiveIds)
            owners.primitives[id]=path;
        for(size_t id : textIds)
            owners.texts[id]=path;
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

// A primitive slot hidden in all four vertexes (style 0) draws nothing, and the
// rest of it is whatever was last written there: only "hidden" counts, as in
// collectMirrorByOwner(). So a slot its owner drew in one state and hid in
// another (a node graph's link segment cut away by the view) compares equal to
// one never drawn.
static void blankHiddenSlots(gweni::renderer::SlotRenderer::MirrorSnapshot &mirror)
{
    for(auto &entry : mirror.primitives)
    {
        std::vector<uint8_t> &bytes=entry.second;

        if(bytes.empty() || (bytes.size()%4 != 0))
            continue;

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
}

static void captureResizeState(Session &s, const std::string &path, ResizeState &state)
{
    state.name=fs::path(path).filename().string();
    state.listing=boundsListing(s);
    s.renderer->readPixels(s.pixels.x, s.pixels.y, state.pixels);
    state.mirror=s.renderer->snapshotMirror();
    rankMirrorDepths(state.mirror);
    blankHiddenSlots(state.mirror);
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
// --scenario depth-view: gweni::tools::DepthView over ButtonDemo
//
// The view only swaps the matrix the renderer projects through and takes the
// canvas's input, so the frame opening it, every frame that only moves its
// camera and the frame closing it do no work in the UI: they enter no control,
// write and upload nothing, lay out nothing and renumber no z; a click while it
// is open presses nothing, and the hovered control, the focus, the tree and
// the live ids are as they were. Closed, the canvas draws the frame before it
// opened again, byte for byte (docs/architecture.md -> "The depth view").
//
// Then the demo shell's Depth button follows the view: on exactly while it is
// open, however it was opened or closed.
//-----------------------------------------------------------------------------

// What one frame did, of the work the view must not cause
struct DepthViewFrame
{
    std::string name;
    size_t visited=0, changed=0, primSlots=0, textSlots=0, uploadBytes=0, atlasBytes=0;
    size_t layoutPasses=0, layoutVisited=0, zVisited=0, hitTests=0;

    bool idle() const
    {
        return visited == 0 && changed == 0 && primSlots == 0 && textSlots == 0 && uploadBytes == 0
            && atlasBytes == 0 && layoutPasses == 0 && layoutVisited == 0 && zVisited == 0
            && hitTests == 0;
    }
};

static gweni::controls::Base *findControl(gweni::controls::Base *control, const char *type, int skip);
static gweni::Point centreOf(gweni::controls::Base *control);
static std::vector<uint8_t> framePixels(const Session &s);
static size_t liveTextIds();

static bool runDepthViewScenario(Session &s, const std::string &shotDir, bool &passed)
{
    using gweni::tools::DepthView;

    s.freezeClock=true;

    gweni::controls::Base *demo=addControl(s, "ButtonDemo");

    if(!demo)
        return false;
    s.label="depth-view";
    renderFrames(s, std::max(s.frames, 3));

    // the toggle, which a click would turn on
    gweni::controls::Button *toggle=nullptr;

    for(int i=0; !toggle; ++i)
    {
        gweni::controls::Base *found=findControl(demo, "Button", i);

        if(!found)
            break;
        auto *button=static_cast<gweni::controls::Button *>(found);

        if(button->getText() == "Toggle Me")
            toggle=button;
    }
    if(!toggle)
    {
        std::cerr<<"ControlTest: depth-view: ButtonDemo has no \"Toggle Me\" button\n";
        return false;
    }

    // rest on it, settled
    const gweni::Point on=centreOf(toggle);

    moveMouse(s, on.x, on.y);
    renderFrames(s, 3);

    const std::vector<uint8_t> before=framePixels(s);
    const bool shotBefore=shoot(s, shotDir+"/depth-view-before.png");
    gweni::controls::Base *hovered=gweni::HoveredControl;
    gweni::controls::Base *focused=gweni::KeyboardFocus;
    const unsigned serial=s.canvas->treeSerial();
    const size_t primIds=livePrimitiveIds();
    const size_t textIds=liveTextIds();
    const bool toggleState=toggle->getToggleState();

    const gweni::renderer::SlotRenderer::FrameStats &gpu=s.renderer->getFrameStats();
    std::vector<DepthViewFrame> frames;
    // the hit tests since the last frame: its input's and its own
    size_t hitTests=gweni::input::hitTestVisits();
    auto frame=[&](const std::string &name)
    {
        renderFrames(s, 1);

        DepthViewFrame f;
        const gweni::controls::ChangeStats changes=gweni::controls::getChangeStats();
        const gweni::controls::LayoutStats &layout=s.canvas->getLayoutStats();

        f.name=name;
        f.visited=changes.visited;
        f.changed=changes.controls;
        f.primSlots=gpu.primitiveSlotsWritten;
        f.textSlots=gpu.textSlotsWritten;
        f.uploadBytes=gpu.uploadBytes;
        f.atlasBytes=gpu.atlasUploadBytes;
        f.layoutPasses=(size_t)layout.passes;
        f.layoutVisited=layout.visited;
        f.zVisited=s.canvas->getUpdateStats().zVisited;
        f.hitTests=gweni::input::hitTestVisits()-hitTests;
        hitTests=gweni::input::hitTestVisits();
        frames.push_back(f);
    };

    // F9, as the window's key callback delivers it
    glfwKey(s, GLFW_KEY_F9, GLFW_PRESS, 0);
    glfwKey(s, GLFW_KEY_F9, GLFW_RELEASE, 0);

    DepthView *view=DepthView::find(s.canvas);

    if(!view)
    {
        std::cerr<<"ControlTest: depth-view: F9 opened no depth view\n";
        return false;
    }
    frame("open");

    // a fixed camera: perspective, turned and dollied out
    DepthView::Camera camera=view->resetCamera();

    camera.yaw=-35.0f;
    camera.pitch=20.0f;
    camera.distance=1.6f;
    view->setCamera(camera);
    frame("camera");

    const std::vector<uint8_t> turned=framePixels(s);
    const bool shotTurned=shoot(s, shotDir+"/depth-view-perspective.png");

    // moved by the mouse and the keys: a left-drag (an orbit), the wheel, an
    // arrow, then P (orthographic) and + (wider spacing)
    mouseDown(s, on.x, on.y, false);
    moveMouse(s, on.x+30, on.y+12);
    mouseUp(s, on.x+30, on.y+12);
    frame("orbit");
    const DepthView::Camera orbited=view->camera();
    const std::vector<uint8_t> orbit=framePixels(s);

    scrollWheel(s, -60);
    frame("dolly");
    glfwKey(s, GLFW_KEY_LEFT, GLFW_PRESS, 0);
    glfwKey(s, GLFW_KEY_LEFT, GLFW_RELEASE, 0);
    frame("move");
    glfwChar(s, 'p');
    glfwChar(s, '+');
    frame("ortho");

    const std::vector<uint8_t> ortho=framePixels(s);
    const bool shotOrtho=shoot(s, shotDir+"/depth-view-ortho.png");
    const DepthView::Camera moved=view->camera();

    // a click on the toggle, while the view has the input: nothing is pressed
    mouseDown(s, on.x, on.y, false);
    mouseUp(s, on.x, on.y);
    frame("click");

    const bool clickIgnored=toggle->getToggleState() == toggleState && view->camera() == moved;

    // F9 again closes it
    glfwKey(s, GLFW_KEY_F9, GLFW_PRESS, 0);
    glfwKey(s, GLFW_KEY_F9, GLFW_RELEASE, 0);
    const bool closed=DepthView::find(s.canvas) == nullptr && !s.renderer->hasViewTransform();
    frame("close");

    const std::vector<uint8_t> after=framePixels(s);
    const bool shotAfter=shoot(s, shotDir+"/depth-view-after.png");
    const bool kept=gweni::HoveredControl == hovered && hovered == toggle
        && gweni::KeyboardFocus == focused && s.canvas->treeSerial() == serial
        && livePrimitiveIds() == primIds && liveTextIds() == textIds
        && toggle->getToggleState() == toggleState;
    const bool same=after == before;
    const bool drawn=turned != before && orbit != turned && ortho != orbit
        && orbited != camera && moved.panX != orbited.panX && !moved.perspective
        && moved.zSpacing > orbited.zSpacing && moved.distance > orbited.distance;

    forgetMouse(s);
    dropControl(s, demo);
    renderFrames(s, 1);

    // The demo shell's Depth button: in the status bar, docked Right left of
    // Stats, on exactly while the view is open, whether F9, a click on it or a
    // view opened before the shell was built (as --depth-view can) opened it,
    // and off once F9 or Escape closed it. While the view is open it has the
    // mouse, so a click on the button does nothing.
    std::string shellFailure;
    {
        gweni::controls::Base *shellControl=addControl(s, "Demo");
        gweni::Demo *shell=gweni::gweni_cast<gweni::Demo>(shellControl);

        if(!shell || !shell->depthButton() || !shell->statsButton())
        {
            std::cerr<<"ControlTest: depth-view: the shell has no Depth or Stats button\n";
            return false;
        }
        renderFrames(s, 3);

        gweni::controls::Button *depthButton=shell->depthButton();
        gweni::controls::Button *statsButton=shell->statsButton();
        const gweni::Point depthAt=depthButton->localPosToCanvas();
        const gweni::Point statsAt=statsButton->localPosToCanvas();

        if(!(depthButton->getText() == "Depth" && depthButton->isToggle()
            && depthButton->getToolTipText() == "Depth view (F9, Esc leaves)"
            && depthButton->getParent() == shell->statusBar()
            && depthButton->getDock() == gweni::DockPosition::Right
            && statsButton->getDock() == gweni::DockPosition::Right
            && depthAt.x+depthButton->getWidth() <= statsAt.x && !depthButton->getToggleState()))
            shellFailure="the Depth button is not an off toggle docked Right left of Stats";

        int told=0;
        struct Counter: public gweni::event::Handler
        {
            int *told;
            void onOpenChanged(gweni::event::Info) { ++*told; }
        } counter;

        counter.told=&told;
        DepthView::onOpenChangedCaller.add(&counter, &Counter::onOpenChanged);

        // after `what`: the view is open as `open` says, the button with it,
        // and the view was told `calls` times
        auto step=[&](const char *what, bool open, int calls)
        {
            renderFrames(s, 1);

            const bool found=DepthView::find(s.canvas) != nullptr;

            if(shellFailure.empty() && (found != open || depthButton->getToggleState() != open
                || told != calls))
                shellFailure=std::string(what)+": the view "+(found?"open":"closed")+", the button "
                    +(depthButton->getToggleState()?"on":"off")+", told "+std::to_string(told)
                    +" time(s) in all";
            told=0;
        };
        const gweni::Point depthCentre=centreOf(depthButton);

        glfwKey(s, GLFW_KEY_F9, GLFW_PRESS, 0);
        glfwKey(s, GLFW_KEY_F9, GLFW_RELEASE, 0);
        step("F9", true, 1);
        glfwKey(s, GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        glfwKey(s, GLFW_KEY_ESCAPE, GLFW_RELEASE, 0);
        step("Escape", false, 1);
        clickMouse(s, depthCentre.x, depthCentre.y);
        step("a click on the button", true, 1);
        clickMouse(s, depthCentre.x, depthCentre.y);
        step("a click on the button with the view open", true, 0);
        glfwKey(s, GLFW_KEY_F9, GLFW_PRESS, 0);
        glfwKey(s, GLFW_KEY_F9, GLFW_RELEASE, 0);
        step("F9 again", false, 1);

        // the view open when a shell is built
        forgetMouse(s);
        dropControl(s, shellControl);
        DepthView::open(s.canvas);
        told=0;
        shellControl=addControl(s, "Demo");
        shell=gweni::gweni_cast<gweni::Demo>(shellControl);
        depthButton=shell->depthButton();
        step("a shell built with the view open", true, 0);
        glfwKey(s, GLFW_KEY_ESCAPE, GLFW_PRESS, 0);
        glfwKey(s, GLFW_KEY_ESCAPE, GLFW_RELEASE, 0);
        step("Escape in that shell", false, 1);

        DepthView::onOpenChangedCaller.removeHandler(&counter);
        forgetMouse(s);
        dropControl(s, shellControl);
        renderFrames(s, 1);
    }
    const bool shellFollows=shellFailure.empty();

    bool idle=true;

    for(const DepthViewFrame &f : frames)
        idle=idle && f.idle();
    passed=idle && clickIgnored && closed && kept && same && drawn && shellFollows;

    std::cout<<"depth-view";
    for(const DepthViewFrame &f : frames)
        std::cout<<" "<<f.name<<"=visited:"<<f.visited<<",changed:"<<f.changed
                 <<",prim:"<<f.primSlots<<",text:"<<f.textSlots<<",upload:"<<f.uploadBytes+f.atlasBytes
                 <<",layout:"<<f.layoutPasses<<"/"<<f.layoutVisited<<",z:"<<f.zVisited
                 <<",hit:"<<f.hitTests;
    std::cout<<" idle="<<(idle?"yes":"no")<<" click_ignored="<<(clickIgnored?"yes":"no")
             <<" closed="<<(closed?"yes":"no")<<" kept="<<(kept?"yes":"no")
             <<" same_after="<<(same?"yes":"no")<<" camera_drawn="<<(drawn?"yes":"no")
             <<" shell_button="<<(shellFollows?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: depth-view: "
                 <<(!idle ? "a frame that opened, moved or closed the view did work in the UI"
                     : !clickIgnored ? "a click while the view was open reached the canvas"
                     : !closed ? "F9 did not close it, or the renderer kept its view transform"
                     : !kept ? "the hover, the focus, the tree or the live ids changed"
                     : !same ? "closed, it did not draw the frame before it opened"
                     : !drawn ? "a camera move did not change what was drawn as it should"
                     : "the shell's Depth button: "+shellFailure)<<"\n";
    return shotBefore && shotTurned && shotOrtho && shotAfter;
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

            // the overlay's ids and the box's, a tool's now
            std::vector<size_t> tipPrimitives;
            std::vector<size_t> tipTexts;

            overlay->ownIds(tipPrimitives, tipTexts);
            gweni::tools::appendSubtreeIds(box, tipPrimitives, tipTexts);
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
    // frame k, nor one a tool held after frame k-1 that is live no more (a
    // tool's slot k cleared; an id the app took again is the app's). The
    // Inspector open in frame k+1 must show exactly frame k's, and each
    // overlay refresh the most of its window's frames, so a frame only a tool
    // wrote in reads 0 in the other.
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
        // the canvas's ToolTip box while it is a tool's (the "interval"
        // tooltip above left it so)
        gweni::controls::ToolTip *box=s.canvas->toolTipControl();

        if(box && box->isExcludedFromStats())
            gweni::tools::appendSubtreeIds(box, primitives, texts);
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

        std::vector<size_t> primitives=nowPrimitives;
        std::vector<size_t> texts=nowTexts;

        for(size_t id : heldPrimitives)
        {
            if(!gweni::isPrimitiveIdLive(id))
                primitives.push_back(id);
        }
        for(size_t id : heldTexts)
        {
            if(!gweni::isTextIdLive(id))
                texts.push_back(id);
        }
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
// byte (docs/architecture.md -> "The theme editor"). Then TimelineDemo, whose
// renderer draws its bars, lanes and grid again only on a frame that can move
// them, with its surfaces changed as an editor row changes one
// (applyToCanvas() with no relayout): it must draw what one built under them
// draws, and the first TimelineDemo again once the defaults are given back.
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

    // TimelineDemo's bars, lane stripes and grid lines, which its renderer
    // draws again only on a frame that can move them, edited as the editor's
    // rows edit a colour: applyToCanvas() with no relayout
    TexturedSkin::ThemeValues timelineChanged=defaults;

    timelineChanged.surfaces.at("TimelineBar").top=gweni::Color(0xc0, 0x40, 0x40);
    timelineChanged.surfaces.at("TimelineBar").bottom=gweni::Color(0xa0, 0x30, 0x30);
    timelineChanged.surfaces.at("TimelineLane").top=gweni::Color(0x20, 0x40, 0x28);
    timelineChanged.surfaces.at("TimelineLane").bottom=gweni::Color(0x20, 0x40, 0x28);
    timelineChanged.surfaces.at("TimelineGrid").top=gweni::Color(0x60, 0x60, 0x20);

    std::vector<unsigned char> timelineBefore, timelineLive, timelineFresh, timelineBack;

    control=addControl(s, "TimelineDemo");
    if(!control)
        return false;
    renderFrames(s, settleFrames);
    s.renderer->readPixels(s.pixels.x, s.pixels.y, timelineBefore);
    skin->applyTheme(timelineChanged);
    ThemeEditor::applyToCanvas(s.canvas, false);
    renderFrames(s, settleFrames);
    s.renderer->readPixels(s.pixels.x, s.pixels.y, timelineLive);
    shot=shoot(s, shotDir+"/theme-05-timeline-edited.png") && shot;
    dropControl(s, control);
    replaceCanvas(s);
    control=addControl(s, "TimelineDemo");
    if(!control)
        return false;
    renderFrames(s, settleFrames);
    s.renderer->readPixels(s.pixels.x, s.pixels.y, timelineFresh);
    skin->applyTheme(defaults);
    ThemeEditor::applyToCanvas(s.canvas, false);
    renderFrames(s, settleFrames);
    s.renderer->readPixels(s.pixels.x, s.pixels.y, timelineBack);
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
    const bool timelineShows=timelineLive != timelineBefore;
    const bool timelineMatches=same(timelineLive, timelineFresh,
        "TimelineDemo's theme changed live differs from one built under it");
    const bool timelineBackMatches=same(timelineBack, timelineBefore,
        "TimelineDemo's defaults given back live differ from the first TimelineDemo");
    const bool timelineOk=timelineShows && timelineMatches && timelineBackMatches;

    passed=editShows && liveMatchesFresh && revertMatches && loadMatches && settles
        && saved && loadedFile && valuesBack && defaultsBack && timelineOk;

    std::cout<<"theme edit_prim_slots="<<editPrim<<" edit_text_slots="<<editText
             <<" settled_slots="<<settledSlots<<" edit_shows="<<(editShows?"yes":"no")
             <<" live_matches_fresh="<<(liveMatchesFresh?"yes":"no")
             <<" revert_matches="<<(revertMatches?"yes":"no")
             <<" saved="<<(saved?"yes":"no")<<" loaded="<<(loadedFile?"yes":"no")
             <<" values_back="<<(valuesBack?"yes":"no")<<" defaults_back="<<(defaultsBack?"yes":"no")
             <<" load_matches="<<(loadMatches?"yes":"no")
             <<" timeline_shows="<<(timelineShows?"yes":"no")
             <<" timeline_matches_fresh="<<(timelineMatches?"yes":"no")
             <<" timeline_back="<<(timelineBackMatches?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: theme: "
                 <<(!editShows ? "the changed theme drew nothing new"
                     : !timelineShows ? "TimelineDemo's changed theme drew nothing new"
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
// --scenario chart: ChartTimeBarsDemo, ChartLinesDemo and ChartAreasDemo
// changed, hovered and clicked
//
// Each bar, segment, marker and the crosshair is a primitive slot of the
// chart's renderer, redrawn only when what it draws moves. With the x and
// value ranges fixed (the scenario fixes them as the demos laid them out):
// a stacked point changed writes its bar and the bars stacked on it at its x,
// a line point the four segments around it, a step line point the five
// around its two vertices, an appended point its own slot (a line's the
// segment before it too, its joint); a moved marker its line's slot and its
// caption's glyphs; a hover the crosshair's slot (and the canvas's ToolTip
// box once its delay passed), a move within one x nothing. A legend click
// toggles its series once each way; a click on a bar, on a stacked segment
// above it and next to a line point fires onPointClicked with that series
// and point. An area's fill is a trapezoid slot per segment: a changed point
// writes the two beside it, in a stack those of each band on it at its x too,
// and its line's segments around it (and those of the bands on it); a hover
// over the bands the crosshair's slot, a click inside a band picks its
// point, a legend click hides a band and the one above drops onto what is
// left. Every settled frame writes nothing and each demo gives its primitive
// ids back.
//-----------------------------------------------------------------------------

namespace
{

struct ChartEvents: public gweni::event::Handler
{
    void toggled(gweni::event::Info info) { toggles.push_back(info.integer); }
    void clicked(gweni::event::Info info) { clicks.push_back({info.integer, size_t(info.point.x)}); }

    std::vector<size_t> toggles;
    std::vector<std::pair<size_t, size_t>> clicks;
};

//! What a frame wrote: its primitive ids, its text ids' glyph slots and the
//! controls it changed.
struct ChartStep
{
    std::vector<size_t> prims;
    std::vector<std::pair<size_t, size_t>> texts;
    size_t changed=0;

    size_t glyphs() const
    {
        size_t count=0;

        for(const auto &text : texts)
            count+=text.second;
        return count;
    }
};

}//namespace

static ChartStep chartFrame(Session &s)
{
    renderFrames(s, 1);

    ChartStep step;

    step.prims=s.renderer->writtenPrimitiveIds();
    for(const gweni::renderer::WrittenText &text : s.renderer->writtenTextIds())
        step.texts.push_back({text.textId, text.glyphSlots});
    step.changed=gweni::controls::getChangeStats().controls;
    return step;
}

//! Whether every slot the frame wrote is one of \p prims and every glyph run
//! one of \p texts.
static bool chartWithin(const ChartStep &step, const std::vector<size_t> &prims, const std::vector<size_t> &texts)
{
    for(size_t id : step.prims)
    {
        if(std::find(prims.begin(), prims.end(), id) == prims.end())
            return false;
    }
    for(const auto &text : step.texts)
    {
        if(std::find(texts.begin(), texts.end(), text.first) == texts.end())
            return false;
    }
    return true;
}

static void printChartStep(const char *name, const ChartStep &step)
{
    std::cout<<" "<<name<<"_prim_slots="<<step.prims.size()<<" "<<name<<"_text_slots="<<step.glyphs()
             <<" "<<name<<"_changed="<<step.changed;
}

static bool runChartScenario(Session &s, const std::string &shotDir, bool &passed)
{
    using gweni::controls::Chart;
    using gweni::controls::ChartAxisSide;

    s.freezeClock=true;

    ChartEvents events;
    size_t idleSlots=0;
    size_t idleChanged=0;
    auto settle=[&]()
    {
        const ChartStep step=chartFrame(s);

        idleSlots+=step.prims.size()+step.glyphs();
        idleChanged+=step.changed;
    };
    auto slotsOf=[](const Chart *chart, size_t series, size_t from, size_t to)
    {
        const auto *renderer=static_cast<const gweni::skin::textured::Chart *>(chart->getSkinControl());
        const std::vector<size_t> &ids=renderer->seriesPrimitives(series);
        std::vector<size_t> slots;

        for(size_t i=from; i < to && i < ids.size(); ++i)
            slots.push_back(ids[i]);
        return slots;
    };
    auto lineSlotsOf=[](const Chart *chart, size_t series, size_t from, size_t to)
    {
        const auto *renderer=static_cast<const gweni::skin::textured::Chart *>(chart->getSkinControl());
        const std::vector<size_t> ids=renderer->areaLinePrimitives(series);
        std::vector<size_t> slots;

        for(size_t i=from; i < to && i < ids.size(); ++i)
            slots.push_back(ids[i]);
        return slots;
    };
    // how many of a step's slots are in \p ids
    auto countIn=[](const ChartStep &step, const std::vector<size_t> &ids)
    {
        size_t count=0;

        for(size_t id : step.prims)
            count+=std::find(ids.begin(), ids.end(), id) != ids.end();
        return count;
    };
    auto clickAt=[&](Chart *chart, gweni::Point local)
    {
        const gweni::Point at=chart->localPosToCanvas(local);

        clickMouse(s, at.x, at.y);
        renderFrames(s, 1);
    };
    auto indexAt=[](const Chart *chart, size_t series, double x) -> size_t
    {
        const std::vector<gweni::controls::ChartPoint> &points=chart->getSeriesPoints(series);

        for(size_t i=0; i < points.size(); ++i)
        {
            if(points[i].x == x)
                return i;
        }
        return points.size();
    };

    // ---- the stacked time bars ----
    const size_t barsIdsBefore=livePrimitiveIds();
    gweni::controls::Base *control=addControl(s, "ChartTimeBarsDemo");
    auto *barsDemo=gweni::gweni_cast<gweni::ChartTimeBarsDemo>(control);

    if(!barsDemo || !barsDemo->chart())
    {
        std::cerr<<"ControlTest: chart: ChartTimeBarsDemo has no chart\n";
        return false;
    }

    Chart *bars=barsDemo->chart();

    bars->onSeriesToggled.add(&events, &ChartEvents::toggled);
    bars->onPointClicked.add(&events, &ChartEvents::clicked);
    s.label="chart";
    renderFrames(s, s.frames);

    // the ranges fixed as laid out, with room for two more days
    const double day=86400.0;
    const double firstDay=gweni::ChartTimeBarsDemo::firstDay();

    bars->setXRange(bars->getXMin(), bars->getXMax()+2*day);
    bars->setYRange(ChartAxisSide::Left, bars->getYMin(ChartAxisSide::Left), bars->getYMax(ChartAxisSide::Left));
    renderFrames(s, 2);
    settle();

    // the bottom of day 3's stack raised: its bar and the three on it
    const double day3=firstDay+3*day;
    std::vector<size_t> stackSlots;

    for(size_t series=0; series < bars->getSeriesCount(); ++series)
    {
        const size_t index=indexAt(bars, series, day3);

        if(index < bars->getSeriesPoints(series).size())
        {
            const std::vector<size_t> slot=slotsOf(bars, series, index, index+1);

            stackSlots.insert(stackSlots.end(), slot.begin(), slot.end());
        }
    }
    bars->setPointValue(0, indexAt(bars, 0, day3), bars->getSeriesPoints(0)[indexAt(bars, 0, day3)].y+2.0);

    const ChartStep stack=chartFrame(s);
    settle();

    // a day appended to the bottom series: its own bar's slot
    bars->appendPoint(0, firstDay+gweni::ChartTimeBarsDemo::Days*day, 20.0);

    const ChartStep appendBar=chartFrame(s);
    const std::vector<size_t> newBar=slotsOf(bars, 0, bars->getSeriesPoints(0).size()-1,
        bars->getSeriesPoints(0).size());
    settle();

    // the legend: a click hides "haiku", a second shows it again
    gweni::controls::internal::ChartLegendRow *row=bars->getLegendRow(2);
    bool hiddenByClick=false;

    if(row)
    {
        const gweni::Point at=row->localPosToCanvas(gweni::Point(row->getWidth()/2, row->getHeight()/2));

        clickMouse(s, at.x, at.y);
        renderFrames(s, 1);
        hiddenByClick=!bars->isSeriesVisible(2);
        clickMouse(s, at.x, at.y);
        renderFrames(s, 1);
    }

    const bool legendToggles=row && hiddenByClick && bars->isSeriesVisible(2)
        && events.toggles == std::vector<size_t>{2, 2};

    // a click on day 5's bottom bar and on the segment of "haiku" above it
    const double day5=firstDay+5*day;
    const size_t bottomIndex=indexAt(bars, 0, day5);
    const size_t upperIndex=indexAt(bars, 2, day5);
    gweni::Rect bottom;
    gweni::Rect upper;
    const bool barRects=bars->getBarRect(0, bottomIndex, bottom) && bars->getBarRect(2, upperIndex, upper);

    if(barRects)
    {
        clickAt(bars, gweni::Point(bottom.x+bottom.w/2, bottom.y+bottom.h-2));
        clickAt(bars, gweni::Point(upper.x+upper.w/2, upper.y+1));
    }

    const bool barClicks=barRects && events.clicks.size() == 2
        && events.clicks[0] == std::make_pair(size_t(0), bottomIndex)
        && events.clicks[1] == std::make_pair(size_t(2), upperIndex);

    bool shot=shoot(s, shotDir+"/chart-bars.png");

    forgetMouse(s);
    renderFrames(s, 1);
    dropControl(s, control);
    renderFrames(s, 1);

    const size_t barsIdsLeft=livePrimitiveIds()-barsIdsBefore;

    // ---- the two-axis lines ----
    const size_t linesIdsBefore=livePrimitiveIds();

    control=addControl(s, "ChartLinesDemo");

    auto *linesDemo=gweni::gweni_cast<gweni::ChartLinesDemo>(control);

    if(!linesDemo || !linesDemo->chart())
    {
        std::cerr<<"ControlTest: chart: ChartLinesDemo has no chart\n";
        return false;
    }

    Chart *lines=linesDemo->chart();

    lines->onPointClicked.add(&events, &ChartEvents::clicked);
    s.label="chart";
    renderFrames(s, s.frames);

    // the left axis fixed as laid out, the x range an hour longer
    const double start=gweni::ChartLinesDemo::start();

    lines->setYRange(ChartAxisSide::Left, lines->getYMin(ChartAxisSide::Left), lines->getYMax(ChartAxisSide::Left));
    lines->setXRange(start, start+day+3600.0);
    renderFrames(s, 2);
    settle();

    // memory's sample 50 moved: segments 48..51
    lines->setPointValue(0, 50, lines->getSeriesPoints(0)[50].y+1.5);

    const ChartStep linePoint=chartFrame(s);
    const std::vector<size_t> lineSlots=slotsOf(lines, 0, 48, 52);
    settle();

    // the GPU's sample 12 (12:00) moved: its two vertices, 24 and 25, are in
    // segments 23..25, and 22 and 26 hold them as neighbours
    lines->setPointValue(1, 12, lines->getSeriesPoints(1)[12].y+10.0);

    const ChartStep stepPoint=chartFrame(s);
    const std::vector<size_t> stepSlots=slotsOf(lines, 1, 22, 27);
    settle();

    // swap's sample 50 moved: the two trapezoids beside it (49, 50) and its
    // line's segments 48..51
    lines->setPointValue(2, 50, lines->getSeriesPoints(2)[50].y+0.5);

    const ChartStep areaPoint=chartFrame(s);
    const std::vector<size_t> areaFill=slotsOf(lines, 2, 0, lines->getSeriesPoints(2).size());
    std::vector<size_t> areaSlots=slotsOf(lines, 2, 49, 51);
    const std::vector<size_t> areaLine=lineSlotsOf(lines, 2, 48, 52);

    areaSlots.insert(areaSlots.end(), areaLine.begin(), areaLine.end());
    settle();

    // a memory sample appended past the last: its segment and the one before
    const size_t memoryCount=lines->getSeriesPoints(0).size();

    lines->appendPoint(0, start+day+900.0, 20.0);

    const ChartStep appendLine=chartFrame(s);
    const std::vector<size_t> appendSlots=slotsOf(lines, 0, memoryCount-2, memoryCount);
    settle();

    // the "restart" marker moved an hour: its slot and its caption's glyphs
    const auto *linesRenderer=static_cast<const gweni::skin::textured::Chart *>(lines->getSkinControl());
    std::vector<size_t> captionPrims;
    std::vector<size_t> captionTexts;

    if(lines->getMarkerLabel(0))
        gweni::tools::appendSubtreeIds(lines->getMarkerLabel(0), captionPrims, captionTexts);
    lines->setMarkerX(0, lines->getMarkerX(0)+3600.0);

    const ChartStep marker=chartFrame(s);
    std::vector<size_t> markerAllowed=captionPrims;

    if(!linesRenderer->markerPrimitives().empty())
        markerAllowed.push_back(linesRenderer->markerPrimitives()[0]);
    settle();

    // the mouse onto memory's sample 30: the crosshair's slot
    const std::vector<size_t> crosshair=linesRenderer->crosshairPrimitives();
    const gweni::PointF sample30=lines->getPointPosition(0, 30);
    const gweni::Rect plot=lines->getPlotBounds();
    gweni::Point at=lines->localPosToCanvas(gweni::Point(int(sample30.x), plot.y+plot.h/2));

    moveMouse(s, at.x, at.y);

    const ChartStep hover=chartFrame(s);
    // read now: the chart goes with the demo before the checks
    const bool hovering=lines->isHovering();
    const bool hoveredSample30=lines->getHover().x == lines->getSeriesPoints(0)[30].x;
    const size_t captionLength=lines->getMarkerCaption(0).size();

    // resting past the canvas's tooltip delay: the box shows, at the crosshair
    g_clockSteps+=int64_t(std::ceil(s.canvas->getToolTipDelay()/g_frameStep))+1;

    const ChartStep tipShown=chartFrame(s);
    std::vector<size_t> boxPrims;
    std::vector<size_t> boxTexts;

    if(gweni::controls::ToolTip *box=s.canvas->toolTipControl())
        gweni::tools::appendSubtreeIds(box, boxPrims, boxTexts);

    std::vector<size_t> hoverAllowed=boxPrims;

    hoverAllowed.insert(hoverAllowed.end(), crosshair.begin(), crosshair.end());

    const bool toolTip=s.canvas->toolTipShown() && !lines->getToolTipText().empty();
    settle();

    // within the same x, further down: nothing
    at=lines->localPosToCanvas(gweni::Point(int(sample30.x), plot.y+plot.h-4));
    moveMouse(s, at.x, at.y);

    const ChartStep sameX=chartFrame(s);
    settle();

    const bool shotLines=shoot(s, shotDir+"/chart-lines.png");

    // a click just off memory's sample 70
    const gweni::PointF sample70=lines->getPointPosition(0, 70);

    clickAt(lines, gweni::Point(int(sample70.x)+2, int(sample70.y)+2));

    const bool pointClick=events.clicks.size() == 3 && events.clicks[2] == std::make_pair(size_t(0), size_t(70));

    forgetMouse(s);
    renderFrames(s, 1);
    settle();
    dropControl(s, control);
    renderFrames(s, 1);

    const size_t linesIdsLeft=livePrimitiveIds()-linesIdsBefore;

    // ---- the stacked areas ----
    const size_t areasIdsBefore=livePrimitiveIds();

    control=addControl(s, "ChartAreasDemo");

    auto *areasDemo=gweni::gweni_cast<gweni::ChartAreasDemo>(control);

    if(!areasDemo || !areasDemo->chart())
    {
        std::cerr<<"ControlTest: chart: ChartAreasDemo has no chart\n";
        return false;
    }

    Chart *areas=areasDemo->chart();
    ChartEvents areaEvents;

    areas->onSeriesToggled.add(&areaEvents, &ChartEvents::toggled);
    areas->onPointClicked.add(&areaEvents, &ChartEvents::clicked);
    s.label="chart";
    renderFrames(s, s.frames);
    areas->setXRange(areas->getXMin(), areas->getXMax());
    areas->setYRange(ChartAxisSide::Left, areas->getYMin(ChartAxisSide::Left), areas->getYMax(ChartAxisSide::Left));
    renderFrames(s, 2);
    settle();

    // the bottom band's month 6 raised: its two trapezoids and the two of
    // each band stacked on it there (5, 6), and the lines' segments 4..7
    const size_t bands=areas->getSeriesCount();
    std::vector<size_t> bandFills;
    std::vector<size_t> bandAllowed;

    for(size_t band=0; band < bands; ++band)
    {
        const std::vector<size_t> all=slotsOf(areas, band, 0, gweni::ChartAreasDemo::Months);
        const std::vector<size_t> fill=slotsOf(areas, band, 5, 7);
        const std::vector<size_t> line=lineSlotsOf(areas, band, 4, 8);

        bandFills.insert(bandFills.end(), all.begin(), all.end());
        bandAllowed.insert(bandAllowed.end(), fill.begin(), fill.end());
        bandAllowed.insert(bandAllowed.end(), line.begin(), line.end());
    }
    areas->setPointValue(0, 6, areas->getSeriesPoints(0)[6].y+20.0);

    const ChartStep areaStack=chartFrame(s);
    settle();

    // the top band's month 3: its own two trapezoids (2, 3) and its line's
    // segments 1..4
    const std::vector<size_t> topFill=slotsOf(areas, bands-1, 0, gweni::ChartAreasDemo::Months);
    std::vector<size_t> topAllowed=slotsOf(areas, bands-1, 2, 4);
    const std::vector<size_t> topLine=lineSlotsOf(areas, bands-1, 1, 5);

    topAllowed.insert(topAllowed.end(), topLine.begin(), topLine.end());
    areas->setPointValue(bands-1, 3, areas->getSeriesPoints(bands-1)[3].y+10.0);

    const ChartStep areaTop=chartFrame(s);
    settle();

    // the mouse into the bottom band at month 8: the crosshair's slot alone
    const auto *areasRenderer=static_cast<const gweni::skin::textured::Chart *>(areas->getSkinControl());
    const std::vector<size_t> areasCrosshair=areasRenderer->crosshairPrimitives();
    const double month8=gweni::ChartAreasDemo::month(8);
    const auto bandMiddle=[&](size_t band, size_t at)
    {
        return gweni::Point(int(areas->xPosition(gweni::ChartAreasDemo::month(int(at)))),
            int(areas->yPosition(ChartAxisSide::Left,
                0.5*(areas->getAreaStackBase(band, at)+areas->getAreaStackTop(band, at)))));
    };

    at=areas->localPosToCanvas(bandMiddle(0, 8));
    moveMouse(s, at.x, at.y);

    const ChartStep areaHover=chartFrame(s);
    const bool areaHovered=areas->isHovering() && areas->getHover().x == month8;
    settle();

    // a click inside the middle band at month 9
    clickAt(areas, bandMiddle(1, 9));

    const bool bandClick=areaEvents.clicks.size() == 1 && areaEvents.clicks[0] == std::make_pair(size_t(1), size_t(9));

    // the legend: a click hides the middle band (the top one drops onto the
    // bottom one), a second shows it again
    gweni::controls::internal::ChartLegendRow *areaRow=areas->getLegendRow(1);
    bool droppedOnto=false;

    if(areaRow)
    {
        const gweni::Point rowAt=areaRow->localPosToCanvas(gweni::Point(areaRow->getWidth()/2,
            areaRow->getHeight()/2));

        clickMouse(s, rowAt.x, rowAt.y);
        renderFrames(s, 1);
        droppedOnto=!areas->isSeriesVisible(1)
            && areas->getAreaStackBase(bands-1, 6) == areas->getAreaStackTop(0, 6);
        clickMouse(s, rowAt.x, rowAt.y);
        renderFrames(s, 1);
    }

    const bool areaLegend=areaRow && droppedOnto && areas->isSeriesVisible(1)
        && areaEvents.toggles == std::vector<size_t>{1, 1};

    forgetMouse(s);
    renderFrames(s, 1);
    settle();

    const bool shotAreas=shoot(s, shotDir+"/chart-areas.png");

    dropControl(s, control);
    renderFrames(s, 1);

    const size_t areasIdsLeft=livePrimitiveIds()-areasIdsBefore;

    shot=shot && shotLines && shotAreas;

    const bool stackOk=!stack.prims.empty() && stack.prims.size() <= stackSlots.size() && stack.texts.empty()
        && chartWithin(stack, stackSlots, {}) && stack.changed == 1;
    const bool appendBarOk=appendBar.prims == newBar && appendBar.texts.empty();
    const bool linePointOk=!linePoint.prims.empty() && linePoint.prims.size() <= 4 && linePoint.texts.empty()
        && chartWithin(linePoint, lineSlots, {}) && linePoint.changed == 1;
    const bool stepPointOk=!stepPoint.prims.empty() && stepPoint.prims.size() <= 5 && stepPoint.texts.empty()
        && chartWithin(stepPoint, stepSlots, {}) && stepPoint.changed == 1;
    // a changed area point: at most the two trapezoids beside it (plus, in
    // a stack, those of the bands on it at its x) and the lines around it
    const size_t areaFills=countIn(areaPoint, areaFill);
    const bool areaPointOk=areaFills >= 1 && areaFills <= 2 && areaPoint.texts.empty()
        && chartWithin(areaPoint, areaSlots, {}) && areaPoint.changed == 1;
    const size_t stackFills=countIn(areaStack, bandFills);
    const bool areaStackOk=stackFills >= 2 && stackFills <= 2*bands && areaStack.texts.empty()
        && chartWithin(areaStack, bandAllowed, {}) && areaStack.changed == 1;
    const size_t topFills=countIn(areaTop, topFill);
    const bool areaTopOk=topFills >= 1 && topFills <= 2 && areaTop.texts.empty()
        && chartWithin(areaTop, topAllowed, {}) && areaTop.changed == 1;
    const bool areaHoverOk=areaHovered && areaHover.prims.size() == 1 && areaHover.texts.empty()
        && chartWithin(areaHover, areasCrosshair, {});
    const bool appendLineOk=!appendLine.prims.empty() && appendLine.prims.size() <= 2 && appendLine.texts.empty()
        && chartWithin(appendLine, appendSlots, {});
    const bool markerOk=!marker.prims.empty() && marker.prims.size() <= 1 && chartWithin(marker, markerAllowed, captionTexts)
        && marker.glyphs() <= captionLength;
    const bool hoverOk=hover.prims.size() == 1 && hover.texts.empty() && chartWithin(hover, crosshair, {})
        && hovering && hoveredSample30;
    const bool tipOk=toolTip && chartWithin(tipShown, hoverAllowed, boxTexts);
    const bool sameXOk=sameX.prims.empty() && sameX.texts.empty() && sameX.changed == 0;
    const bool settled=idleSlots == 0 && idleChanged == 0;
    // the canvas's ToolTip box, made for the hover, stays on the canvas
    const bool idsBack=barsIdsLeft == 0 && linesIdsLeft == boxPrims.size() && areasIdsLeft == 0;

    passed=stackOk && appendBarOk && legendToggles && barClicks && linePointOk && stepPointOk && appendLineOk
        && markerOk && hoverOk && tipOk && sameXOk && pointClick && areaPointOk && areaStackOk && areaTopOk
        && areaHoverOk && bandClick && areaLegend && settled && idsBack;

    std::cout<<"chart stack_bars="<<stackSlots.size();
    printChartStep("stack", stack);
    printChartStep("append_bar", appendBar);
    printChartStep("line_point", linePoint);
    printChartStep("step_point", stepPoint);
    printChartStep("append_line", appendLine);
    printChartStep("marker", marker);
    printChartStep("hover", hover);
    printChartStep("tooltip", tipShown);
    printChartStep("same_x", sameX);
    printChartStep("area_point", areaPoint);
    std::cout<<" area_point_fills="<<areaFills<<" area_bands="<<bands;
    printChartStep("area_stack", areaStack);
    std::cout<<" area_stack_fills="<<stackFills;
    printChartStep("area_top", areaTop);
    std::cout<<" area_top_fills="<<topFills;
    printChartStep("area_hover", areaHover);
    std::cout<<" toggles="<<events.toggles.size()<<" clicks="<<events.clicks.size()
             <<" idle_slots="<<idleSlots<<" idle_changed="<<idleChanged
             <<" ids_left="<<(barsIdsLeft+linesIdsLeft+areasIdsLeft-boxPrims.size())
             <<" stack="<<(stackOk?"yes":"no")<<" append_bar="<<(appendBarOk?"yes":"no")
             <<" legend="<<(legendToggles?"yes":"no")<<" bar_clicks="<<(barClicks?"yes":"no")
             <<" line_point="<<(linePointOk?"yes":"no")<<" step_point="<<(stepPointOk?"yes":"no")
             <<" append_line="<<(appendLineOk?"yes":"no")<<" marker="<<(markerOk?"yes":"no")
             <<" hover="<<(hoverOk?"yes":"no")<<" tooltip="<<(tipOk?"yes":"no")
             <<" same_x="<<(sameXOk?"yes":"no")<<" point_click="<<(pointClick?"yes":"no")
             <<" area_point="<<(areaPointOk?"yes":"no")<<" area_stack="<<(areaStackOk?"yes":"no")
             <<" area_top="<<(areaTopOk?"yes":"no")<<" area_hover="<<(areaHoverOk?"yes":"no")
             <<" band_click="<<(bandClick?"yes":"no")<<" area_legend="<<(areaLegend?"yes":"no")
             <<" settled="<<(settled?"yes":"no")<<" ids_back="<<(idsBack?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: chart: "
                 <<(!stackOk ? "a stacked point wrote more than its stack's bars"
                     : !appendBarOk ? "an appended bar wrote more than its slot"
                     : !legendToggles ? "a legend click did not toggle its series once each way"
                     : !barClicks ? "a click on a bar or a stacked segment fired the wrong point"
                     : !linePointOk ? "a line point wrote more than the four segments around it"
                     : !stepPointOk ? "a step line point wrote more than the five segments around it"
                     : !appendLineOk ? "an appended line point wrote more than its segment and its joint"
                     : !markerOk ? "a moved marker wrote more than its slot and its caption"
                     : !hoverOk ? "a hover wrote more than the crosshair's slot"
                     : !tipOk ? "the tooltip did not show, or wrote outside the crosshair and its box"
                     : !sameXOk ? "a move within one x wrote or changed something"
                     : !pointClick ? "a click next to a point fired the wrong point"
                     : !areaPointOk ? "an area point wrote more than the two trapezoids beside it and its line"
                     : !areaStackOk ? "a stacked area point wrote more than its trapezoids and those stacked on it"
                     : !areaTopOk ? "the top band's point wrote more than its own two trapezoids and its line"
                     : !areaHoverOk ? "a hover over the areas wrote more than the crosshair's slot"
                     : !bandClick ? "a click inside a band fired the wrong point"
                     : !areaLegend ? "a legend click did not toggle the band once each way or restack the one above"
                     : !settled ? "a settled frame wrote slots or changed controls"
                     : "a demo did not give its primitive ids back")<<"\n";
    return shot;
}

//-----------------------------------------------------------------------------
// --scenario timeline: TimelineDemo's bars changed, added, hovered, clicked,
// overlapped and scrolled
//
// Each bar and the hover look is a primitive slot of the timeline's renderer,
// and the bars are drawn again only when something they are drawn from moves
// (Timeline::getBarsSerial()), then only the slots that differ. With the
// demo's x range fixed: a changed bar writes its own slot, an added one its
// own (a spare slot of its lane's list); a hover the hover look's slot (and
// the canvas's ToolTip box once its delay passed), at most the old and the
// new bar's slots with it, a move within a bar nothing; a click on a bar fires
// onBarClicked with its lane, index and id; a bar added over another one
// shows in front of it and is the one the hit test and a click pick. A wheel
// notch scrolls the lanes: the frame writes no more than the stripes, bars
// and captions of the lanes in view before or after it and the scroll bar,
// and says whether it wrote bar slots at all (the bars move) or none (they
// would only be clipped). Every settled frame writes nothing and the demo
// gives its primitive ids back.
//-----------------------------------------------------------------------------

namespace
{

struct TimelineEvents: public gweni::event::Handler
{
    void clicked(gweni::event::Info info)
    {
        clicks.push_back({size_t(info.point.x), size_t(info.point.y), uint64_t(info.integer)});
    }

    struct Click
    {
        size_t lane;
        size_t index;
        uint64_t id;
        bool operator==(const Click &other) const
        {
            return lane == other.lane && index == other.index && id == other.id;
        }
    };

    std::vector<Click> clicks;
};

}//namespace

static bool runTimelineScenario(Session &s, const std::string &shotDir, bool &passed)
{
    using gweni::controls::Timeline;
    using gweni::controls::TimelineBar;

    s.freezeClock=true;

    TimelineEvents events;
    size_t idleSlots=0;
    size_t idleChanged=0;
    auto settle=[&]()
    {
        const ChartStep step=chartFrame(s);

        idleSlots+=step.prims.size()+step.glyphs();
        idleChanged+=step.changed;
    };

    const size_t idsBefore=livePrimitiveIds();
    gweni::controls::Base *control=addControl(s, "TimelineDemo");
    auto *demo=gweni::gweni_cast<gweni::TimelineDemo>(control);

    if(!demo || !demo->timeline())
    {
        std::cerr<<"ControlTest: timeline: TimelineDemo has no timeline\n";
        return false;
    }

    Timeline *timeline=demo->timeline();

    timeline->onBarClicked.add(&events, &TimelineEvents::clicked);
    s.label="timeline";
    renderFrames(s, s.frames);
    settle();

    const auto *drawn=static_cast<const gweni::skin::textured::Timeline *>(timeline->getSkinControl());
    auto barSlot=[&](size_t lane, size_t index) -> size_t
    {
        const std::vector<size_t> ids=drawn->barPrimitives(lane);

        return index < ids.size() ? ids[index] : size_t(-1);
    };
    auto canvasAt=[&](gweni::Point local) { return timeline->localPosToCanvas(local); };
    auto middleOf=[&](size_t lane, size_t index, gweni::Point &at)
    {
        gweni::Rect rect;

        if(!timeline->getBarRect(lane, index, rect))
            return false;
        at=canvasAt(gweni::Point(rect.x+rect.w/2, rect.y+rect.h/2));
        return true;
    };
    const double day=gweni::TimelineDemo::day();
    const size_t lanes=timeline->getLaneCount();
    const size_t bars=timeline->getTotalBarCount();

    // bar 3 of lane 2 half an hour shorter: its slot alone
    TimelineBar moved=timeline->getBar(2, 3);

    moved.end-=1800.0;
    timeline->setBar(2, 3, moved);

    const ChartStep change=chartFrame(s);
    const bool changeOk=change.prims == std::vector<size_t>{barSlot(2, 3)} && change.texts.empty()
        && change.changed == 1;
    settle();

    // a run added at the end of lane 4's day: its own slot
    const size_t lane4=timeline->getBarCount(4);
    const double lastEnd=timeline->getBar(4, lane4-1).end;

    timeline->addBar(4, TimelineBar(lastEnd+60.0, std::min(day+86400.0, lastEnd+600.0), 9999));

    const ChartStep add=chartFrame(s);
    const bool addOk=add.prims == std::vector<size_t>{barSlot(4, lane4)} && add.texts.empty()
        && timeline->getBarCount(4) == lane4+1;
    settle();

    // the mouse onto bar 2 of lane 5: the hover look's slot
    const std::vector<size_t> hoverSlot=drawn->hoverPrimitives();
    gweni::Point at;
    const bool bar52=middleOf(5, 2, at);

    moveMouse(s, at.x, at.y);

    const ChartStep hover=chartFrame(s);
    const bool hovering=timeline->isHovering() && timeline->getHoverLane() == 5 && timeline->getHoverIndex() == 2;

    // resting past the canvas's tooltip delay: the box shows below the bar
    g_clockSteps+=int64_t(std::ceil(s.canvas->getToolTipDelay()/g_frameStep))+1;

    const ChartStep tipShown=chartFrame(s);
    std::vector<size_t> boxPrims;
    std::vector<size_t> boxTexts;

    if(gweni::controls::ToolTip *box=s.canvas->toolTipControl())
        gweni::tools::appendSubtreeIds(box, boxPrims, boxTexts);

    std::vector<size_t> hoverAllowed=boxPrims;

    hoverAllowed.insert(hoverAllowed.end(), hoverSlot.begin(), hoverSlot.end());
    hoverAllowed.push_back(barSlot(5, 2));

    const bool toolTip=s.canvas->toolTipShown() && !timeline->getToolTipText().empty();
    settle();

    // within the bar: nothing
    moveMouse(s, at.x+1, at.y+1);

    const ChartStep within=chartFrame(s);
    settle();

    // onto the next bar: the hover's slot, the two bars' and the box's at most
    gweni::Point next;
    const bool bar53=middleOf(5, 3, next);

    moveMouse(s, next.x, next.y);

    const ChartStep hoverNext=chartFrame(s);
    std::vector<size_t> nextAllowed=hoverAllowed;

    nextAllowed.push_back(barSlot(5, 3));

    const bool hoverNextOk=bar53 && timeline->getHoverIndex() == 3 && !hoverNext.prims.empty()
        && chartWithin(hoverNext, nextAllowed, boxTexts);
    settle();

    // a click on bar 1 of lane 6
    gweni::Point clickAt;
    const bool bar61=middleOf(6, 1, clickAt);

    clickMouse(s, clickAt.x, clickAt.y);
    renderFrames(s, 1);

    const bool clickOk=bar61 && events.clicks.size() == 1
        && events.clicks[0] == TimelineEvents::Click{6, 1, gweni::TimelineDemo::runId(6, 1)};
    settle();

    // a run added over bar 1 of lane 7, in a colour of its own: it shows in
    // front of it, is the bar under the mouse and takes the click
    const TimelineBar under=timeline->getBar(7, 1);
    const double middle=(under.start+under.end)/2.0;
    const gweni::Color overColor(250, 40, 210);
    const size_t overIndex=timeline->addBar(7,
        TimelineBar(middle-300.0, middle+300.0, 7777).withColor(overColor));

    renderFrames(s, 1);
    settle();

    gweni::Rect overRect;
    bool overlapShows=false;
    bool overlapHit=false;

    if(timeline->getBarRect(7, overIndex, overRect))
    {
        const gweni::Point local(overRect.x+overRect.w/2, overRect.y+overRect.h/2);
        const gweni::Point device=devicePixels(canvasAt(local), s.scale);
        const std::vector<uint8_t> frame=framePixels(s);
        const size_t pixel=(size_t(device.y)*s.pixels.x+device.x)*4;
        size_t lane=0;
        size_t index=0;

        overlapShows=frame[pixel] == overColor.r && frame[pixel+1] == overColor.g
            && frame[pixel+2] == overColor.b;
        overlapHit=timeline->barAt(local, lane, index) && lane == 7 && index == overIndex
            && timeline->getOverlapLevel(7, overIndex) == 1;

        const gweni::Point over=canvasAt(local);

        clickMouse(s, over.x, over.y);
        renderFrames(s, 1);
    }

    const bool overlapClick=events.clicks.size() == 2 && events.clicks[1] == TimelineEvents::Click{7, overIndex, 7777};
    const bool overlapOk=overlapShows && overlapHit && overlapClick;
    settle();

    const bool shot=shoot(s, shotDir+"/timeline.png");

    // a wheel notch over the lanes: the lanes in view before or after it, the
    // scroll bar
    auto lanesInView=[&]()
    {
        std::vector<size_t> shown;

        for(size_t lane=0; lane < lanes; ++lane)
        {
            gweni::Rect rect;

            if(timeline->getLaneRect(lane, rect))
                shown.push_back(lane);
        }
        return shown;
    };
    const std::vector<size_t> shownBefore=lanesInView();
    const int offsetBefore=timeline->getScrollOffset();
    const gweni::Rect area=timeline->getBarArea();
    const gweni::Point wheelAt=canvasAt(gweni::Point(area.x+area.w/2, area.y+area.h/2));

    moveMouse(s, wheelAt.x, wheelAt.y);
    renderFrames(s, 1);
    settle();
    scrollWheel(s, -60);

    const ChartStep scroll=chartFrame(s);
    const std::vector<size_t> shownAfter=lanesInView();
    std::vector<size_t> scrollPrims;
    std::vector<size_t> scrollTexts;
    std::vector<size_t> scrollBars;
    std::vector<size_t> touched=shownBefore;

    touched.insert(touched.end(), shownAfter.begin(), shownAfter.end());
    for(size_t lane : touched)
    {
        const std::vector<size_t> ids=drawn->barPrimitives(lane);

        scrollPrims.push_back(drawn->lanePrimitives()[lane]);
        scrollPrims.insert(scrollPrims.end(), ids.begin(), ids.end());
        scrollBars.insert(scrollBars.end(), ids.begin(), ids.end());
    }
    // the caption pool: the most lanes the column shows any of
    gweni::tools::appendSubtreeIds(timeline->getCaptionColumn(), scrollPrims, scrollTexts);
    // the hover look follows the bar under the mouse
    scrollPrims.insert(scrollPrims.end(), hoverSlot.begin(), hoverSlot.end());
    gweni::tools::appendSubtreeIds(timeline->getBody(), scrollPrims, scrollTexts);
    scrollPrims.insert(scrollPrims.end(), boxPrims.begin(), boxPrims.end());
    scrollTexts.insert(scrollTexts.end(), boxTexts.begin(), boxTexts.end());

    size_t barSlotsWritten=0;

    for(size_t id : scroll.prims)
    {
        if(std::find(scrollBars.begin(), scrollBars.end(), id) != scrollBars.end())
            ++barSlotsWritten;
    }

    const int scrolled=timeline->getScrollOffset()-offsetBefore;
    const bool scrollOk=scrolled == Timeline::WheelLanes*timeline->getLaneHeight()
        && chartWithin(scroll, scrollPrims, scrollTexts);
    const bool barsMoved=barSlotsWritten > 0;
    settle();

    const bool shotScrolled=shoot(s, shotDir+"/timeline-scrolled.png");

    forgetMouse(s);
    renderFrames(s, 1);
    settle();
    dropControl(s, control);
    renderFrames(s, 1);

    const size_t idsLeft=livePrimitiveIds()-idsBefore;

    const bool hoverOk=bar52 && hovering && hover.prims == hoverSlot && hover.texts.empty();
    const bool tipOk=toolTip && chartWithin(tipShown, hoverAllowed, boxTexts);
    const bool withinOk=within.prims.empty() && within.texts.empty() && within.changed == 0;
    const bool settled=idleSlots == 0 && idleChanged == 0;
    // the canvas's ToolTip box, made for the hover, stays on the canvas
    const bool idsBack=idsLeft == boxPrims.size();

    passed=changeOk && addOk && hoverOk && tipOk && withinOk && hoverNextOk && clickOk && overlapOk && scrollOk
        && settled && idsBack;

    std::cout<<"timeline lanes="<<lanes<<" bars="<<bars;
    printChartStep("change", change);
    printChartStep("add", add);
    printChartStep("hover", hover);
    printChartStep("tooltip", tipShown);
    printChartStep("within", within);
    printChartStep("hover_next", hoverNext);
    printChartStep("scroll", scroll);
    std::cout<<" scroll_lanes="<<shownBefore.size()<<"+"<<shownAfter.size()
             <<" scroll_bar_slots="<<barSlotsWritten<<" bars_moved="<<(barsMoved?"yes":"no")
             <<" clicks="<<events.clicks.size()
             <<" idle_slots="<<idleSlots<<" idle_changed="<<idleChanged
             <<" ids_left="<<(idsLeft-boxPrims.size())
             <<" change="<<(changeOk?"yes":"no")<<" add="<<(addOk?"yes":"no")
             <<" hover="<<(hoverOk?"yes":"no")<<" tooltip="<<(tipOk?"yes":"no")
             <<" within="<<(withinOk?"yes":"no")<<" hover_next="<<(hoverNextOk?"yes":"no")
             <<" click="<<(clickOk?"yes":"no")<<" overlap="<<(overlapOk?"yes":"no")
             <<" scroll="<<(scrollOk?"yes":"no")
             <<" settled="<<(settled?"yes":"no")<<" ids_back="<<(idsBack?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: timeline: "
                 <<(!changeOk ? "a changed bar wrote more than its slot"
                     : !addOk ? "an added bar wrote more than its slot"
                     : !hoverOk ? "a hover wrote more than the hover look's slot"
                     : !tipOk ? "the tooltip did not show, or wrote outside the hover and its box"
                     : !withinOk ? "a move within a bar wrote or changed something"
                     : !hoverNextOk ? "a hover onto the next bar wrote more than the two bars, the look and the box"
                     : !clickOk ? "a click on a bar fired the wrong lane, index or id"
                     : !overlapOk ? "a bar added over another did not show, hit-test or click in front of it"
                     : !scrollOk ? "a lane scroll wrote more than the lanes in view and the bar"
                     : !settled ? "a settled frame wrote slots or changed controls"
                     : "the demo did not give its primitive ids back")<<"\n";
    return shot && shotScrolled;
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
    g_clockSteps=0;
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
    verifyIdleBefore(s);
    s.canvas->updateCanvas();
    size_t pending=0, pendingFirst=0, pendingSecond=0;
    countPending(s.canvas, pending);
    countPending(first, pendingFirst);
    countPending(second, pendingSecond);
    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();
    verifyIdleAfter(s);
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

// The primitive slots a frame wrote, and how many of them are not in allowed.
static size_t slotsOutside(const Session &s, const std::vector<size_t> &allowed, size_t &written)
{
    size_t outside=0;

    for(size_t id : s.renderer->writtenPrimitiveIds())
    {
        ++written;
        if(std::find(allowed.begin(), allowed.end(), id) == allowed.end())
            ++outside;
    }
    return outside;
}

// TreeControlDemo's labels draw as flat list rows: label A ("Node One") hovered
// changes its pixels alone, in a frame writing 1..2 primitive slots (A's and
// the one of the control the mouse left), and moving off gives the frame
// before back; a click on A selects it, changing no pixel outside it; a click
// on label B ("Node Two Inside") moves the selection, every frame until
// settled writing no primitive slot but A's and B's and no more glyphs than
// their two strings, no pixel changed outside them; a settled frame writes
// nothing.
static void checkTreeSelect(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "TreeControlDemo");
    renderFrames(s, s.frames);

    // the first tree's first label and its third, a child of "Node Two"
    auto *first=gweni::gweni_cast<gweni::controls::Button>(findControl(demo, "TreeNodeText"));
    auto *second=gweni::gweni_cast<gweni::controls::Button>(findControl(demo, "TreeNodeText", 2));
    if(!first || !second || first->getToggleState() || second->getToggleState()
        || !first->getSkinControl() || !second->getSkinControl()
        || first->getSkinControl()->getPrimitives().empty()
        || second->getSkinControl()->getPrimitives().empty())
    {
        checks.check("tree_select_found", false,
            "TreeControlDemo has no two unselected tree labels with a primitive slot each");
        dropControl(s, demo);
        return;
    }

    const size_t firstSlot=first->getSkinControl()->getPrimitives().front();
    const std::vector<size_t> labelSlots={firstSlot, second->getSkinControl()->getPrimitives().front()};
    const size_t glyphs=static_cast<size_t>(first->textLength()+second->textLength());
    const gweni::Point firstAt=centreOf(first);
    const gweni::Point secondAt=centreOf(second);
    const gweni::Point off(s.size.x-1, s.size.y-1);    // TreeControlDemo, no tree

    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> rest=framePixels(s);

    // the hover: A's slot, and the left control's at most
    moveMouse(s, firstAt.x, firstAt.y);
    renderFrames(s, 1);
    const std::vector<size_t> hoverSlots=s.renderer->writtenPrimitiveIds();
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tree-hover.png"))
        ok=false;
    const std::vector<uint8_t> hover=framePixels(s);
    const SplitDifferences hoverDiff=differencesSplit(s, rest, hover, first, first);

    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> unhovered=framePixels(s);

    // the selection of A, the mouse moved off again
    clickMouse(s, firstAt.x, firstAt.y);
    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tree-selected.png"))
        ok=false;
    const std::vector<uint8_t> selected=framePixels(s);
    const bool firstSelected=first->getToggleState() && !second->getToggleState();
    const SplitDifferences selectDiff=differencesSplit(s, rest, selected, first, first);

    // the selection moved to B, from a settled hover on it
    moveMouse(s, secondAt.x, secondAt.y);
    renderFrames(s, s.frames);
    size_t moveWritten=0, moveForeign=0, moveGlyphs=0;
    clickMouse(s, secondAt.x, secondAt.y);
    for(int i=0; i < s.frames; ++i)
    {
        renderFrames(s, 1);
        moveForeign+=slotsOutside(s, labelSlots, moveWritten);
        moveGlyphs=std::max(moveGlyphs, s.renderer->getFrameStats().textSlotsWritten);
    }
    moveMouse(s, off.x, off.y);
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tree-moved.png"))
        ok=false;
    const std::vector<uint8_t> moved=framePixels(s);
    const bool handedOver=!first->getToggleState() && second->getToggleState();
    const SplitDifferences moveDiff=differencesSplit(s, selected, moved, first, second);

    // nothing to do: nothing written
    renderFrames(s, 1);
    const gweni::renderer::SlotRenderer::FrameStats settled=s.renderer->getFrameStats();

    std::cout<<"interaction tree_select hover_slots="<<hoverSlots.size()
        <<" move_slots="<<moveWritten<<" move_foreign_slots="<<moveForeign
        <<" move_text_slots_max="<<moveGlyphs<<"\n";

    forgetMouse(s);
    dropControl(s, demo);

    const bool hoverOwnSlot=std::find(hoverSlots.begin(), hoverSlots.end(), firstSlot) != hoverSlots.end();
    checks.check("tree_label_hover",
        hoverDiff.outside == 0 && hoverDiff.inFirst > 0 && unhovered == rest,
        "hovering the label changed "+std::to_string(hoverDiff.inFirst)+" pixels in it and "
        +std::to_string(hoverDiff.outside)+" outside it, or moving off did not give the frame before back");
    checks.check("tree_label_hover_bounded",
        hoverSlots.size() >= 1 && hoverSlots.size() <= 2 && hoverOwnSlot,
        "the hover frame wrote "+std::to_string(hoverSlots.size())+" primitive slots (want 1..2), "
        +(hoverOwnSlot?"the label's among them":"not the label's"));
    checks.check("tree_label_select",
        firstSelected && selectDiff.outside == 0 && selectDiff.inFirst > 0,
        "a click did not select the label alone, or changed "+std::to_string(selectDiff.outside)
        +" pixels outside it ("+std::to_string(selectDiff.inFirst)+" in it)");
    checks.check("tree_label_select_move",
        handedOver && moveDiff.outside == 0 && moveDiff.inFirst > 0 && moveDiff.inSecond > 0
        && moveWritten >= 2 && moveForeign == 0 && moveGlyphs <= glyphs,
        "a click on the second label did not move the selection to it alone: "
        +std::to_string(moveDiff.outside)+" pixels outside the two labels changed, "
        +std::to_string(moveDiff.inFirst)+" in A, "+std::to_string(moveDiff.inSecond)+" in B, "
        +std::to_string(moveWritten)+" primitive slots written, "+std::to_string(moveForeign)
        +" not the labels', at most "+std::to_string(moveGlyphs)+" glyph slots a frame (want at most "
        +std::to_string(glyphs)+")");
    checks.check("tree_label_settled",
        settled.primitiveSlotsWritten == 0 && settled.textSlotsWritten == 0,
        "a settled frame wrote "+std::to_string(settled.primitiveSlotsWritten)+" primitive and "
        +std::to_string(settled.textSlotsWritten)+" glyph slots");
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
    // hidden behind the canvas's back, as no control changed: it is told, as
    // an app is to be, so an idle loop (and --verify-idle) draws the frame
    s.canvas->redraw();
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

// WindowDemo: "Modal Window" opens a window on the canvas's modal stack. With
// it up, a move onto and a click on "Normal Window" under it hover nothing,
// open nothing and write nothing; Escape closes it, and with the mouse back
// where it was the frame before it opened comes back with the live primitive
// ids.
static void checkWindowModal(Session &s, const std::string &shotDir, InteractionChecks &checks, bool &ok)
{
    gweni::controls::Base *demo=addControl(s, "WindowDemo");
    renderFrames(s, s.frames);

    gweni::controls::Base *normal=findControl(demo, "Button", 0);
    gweni::controls::Base *modal=findControl(demo, "Button", 1);
    if(!normal || !modal)
    {
        checks.check("window_modal_found", false, "WindowDemo has no two visible Buttons");
        dropControl(s, demo);
        return;
    }

    const gweni::Point rest(s.size.x-1, s.size.y-1);
    moveMouse(s, rest.x, rest.y);
    const std::vector<uint8_t> before=settledRegion(s, demo, shotDir, "window-modal-before", ok);
    const size_t idsBefore=livePrimitiveIds();

    const gweni::Point openAt=centreOf(modal);
    clickMouse(s, openAt.x, openAt.y);
    renderFrames(s, s.frames);
    gweni::controls::Base *window=findControl(s.canvas, "WindowControl");
    const bool isModal=window && (s.canvas->topModal() == window);
    if(!shoot(s, shotDir+"/interaction-window-modal-open.png"))
        ok=false;

    // under it: no hover, no press, nothing written
    const gweni::Point behind=centreOf(normal);
    moveMouse(s, behind.x, behind.y);
    renderFrames(s, 1);
    const size_t moveSlots=s.renderer->getFrameStats().primitiveSlotsWritten;
    const bool hoveredBehind=normal->isHovered();
    clickMouse(s, behind.x, behind.y);
    renderFrames(s, 1);
    const size_t clickSlots=s.renderer->getFrameStats().primitiveSlotsWritten;
    renderFrames(s, s.frames);
    const bool oneWindow=(findControl(s.canvas, "WindowControl", 1) == nullptr);

    sendKey(s, "escape");
    renderFrames(s, s.frames);
    const bool closed=(findControl(s.canvas, "WindowControl") == nullptr) && !s.canvas->topModal();
    moveMouse(s, rest.x, rest.y);
    const std::vector<uint8_t> after=settledRegion(s, demo, shotDir, "window-modal-after", ok);
    const size_t idsAfter=livePrimitiveIds();

    forgetMouse(s);
    dropControl(s, demo);

    checks.check("window_modal_open", isModal,
        "\"Modal Window\" opened no window, or it is not the canvas's top modal");
    checks.check("window_modal_blocks", !hoveredBehind && oneWindow && moveSlots == 0 && clickSlots == 0,
        std::string(hoveredBehind ? "\"Normal Window\" under the modal window was hovered; " : "")
        +(oneWindow ? "" : "a click on \"Normal Window\" under the modal window opened a window; ")
        +"the move wrote "+std::to_string(moveSlots)+" and the click "+std::to_string(clickSlots)
        +" primitive slots; want 0");
    checks.check("window_modal_escape", closed && after == before && idsAfter == idsBefore,
        std::string(closed ? "" : "Escape left the modal window open; ")
        +(after == before ? "" : "the frame after it closed is not the frame before it opened; ")
        +"live primitive ids "+std::to_string(idsBefore)+" before, "+std::to_string(idsAfter)+" after");
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
    verifyIdleBefore(s);
    s.canvas->updateCanvas();
    size_t elsewhere=0;
    countPendingOutside(s.canvas, scrolled, elsewhere);
    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();
    verifyIdleAfter(s);
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
    verifyIdleBefore(s);
    s.canvas->updateCanvas();
    const bool labelMoveMarked=row->getStateChange() != gweni::controls::StateChange_Nothing
        || row->getLabel()->getStateChange() != gweni::controls::StateChange_Nothing;
    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();
    verifyIdleAfter(s);
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
    verifyIdleBefore(s);
    s.canvas->updateCanvas();
    const gweni::controls::LayoutStats sortLayout=s.canvas->getLayoutStats();
    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();
    verifyIdleAfter(s);
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

//-----------------------------------------------------------------------------
// --scenario zoom-panel: controls::ZoomPanel against Canvas::setScale, and its
// own input
//
// First, for z = 1, 1.5, 1.37, 2: ButtonDemo inside a ZoomPanel filling a
// scale-1 canvas, its content 640x480 at zoom z (pan 0, so its origin is the
// canvas's whole px 0,0), must draw the very pixels ButtonDemo does on a
// 640x480 canvas at setScale(z), each built on a fresh canvas onto a target of
// round(640x480*z). Every 3rd device px must hit the same control in both
// (the scaled canvas given the input layer's floor(px/z)); the mouse moved
// through the input layer to each Button's centre must hover the same
// control; and once "Toggle Me" is clicked there, the two frames must again
// be the same pixels.
//
// Then ZoomPanelDemo on a 640x480 canvas at scale 1 (its hint Label is
// outside the panel): every frame of a wheel zoom, a setZoom and a right,
// middle and Space+left drag pan lays nothing out (layout_passes=0), calls
// Base::invalidate() nowhere and writes no slot that is not the panel
// subtree's, and the frame after each writes nothing; the wheel keeps the
// content point under the cursor there and stops at the range's ends; a zoom
// inside the SDF range (2 -> 2.5) uploads nothing to a font atlas; the wheel
// over the ScrollControl scrolls it; a pan moves the content by the mouse's
// motion and opens no menu. From inside the zoomed panel the tooltip box, the
// ComboBox's list and a right click's context menu (with 2 units of motion,
// under the pan threshold) open as canvas children at the control's rect as
// it is drawn (getCanvasBounds). Deleting the demo gives back every id but the
// canvas's ToolTip box's.
//-----------------------------------------------------------------------------

// The child indices from root down to control, or "-" for one not under root.
static std::string pathUnder(gweni::controls::Base *root, gweni::controls::Base *control)
{
    std::string path;

    for(gweni::controls::Base *at=control; at; at=at->getParent())
    {
        if(at == root)
            return path.empty() ? std::string("/") : path;

        gweni::controls::Base *parent=at->getParent();

        if(!parent)
            break;

        size_t index=0;

        for(gweni::controls::Base *child : parent->getChildren())
        {
            if(child == at)
                break;
            ++index;
        }
        path="/"+std::to_string(index)+path;
    }
    return "-";
}

// The primitive and text ids every control under control holds.
static void subtreeIds(gweni::controls::Base *control, std::set<size_t> &primitives, std::set<size_t> &texts)
{
    if(const gweni::skin::Control *skinControl=control->getSkinControl())
    {
        std::vector<size_t> primitiveIds, textIds;

        skinControl->appendIds(primitiveIds, textIds);
        primitives.insert(primitiveIds.begin(), primitiveIds.end());
        texts.insert(textIds.begin(), textIds.end());
    }
    for(gweni::controls::Base *child : control->getActualChildren())
        subtreeIds(child, primitives, texts);
}

// What ButtonDemo gives in one tree: the hit-test grid, the hovered control at
// each Button's centre, and the pixels once "Toggle Me" is clicked.
struct ZoomTargets
{
    std::vector<std::string> grid;
    std::vector<std::string> hovered;
    std::vector<uint8_t> clicked;
};

// Device px centres of ButtonDemo's Buttons on the scaled canvas, and the one
// of "Toggle Me" (last).
static std::vector<gweni::Point> buttonCentres(gweni::controls::Base *demo, float scale)
{
    std::vector<gweni::Point> centres;
    gweni::Point toggle(-1, -1);

    for(int i=0;; ++i)
    {
        gweni::controls::Base *found=findControl(demo, "Button", i);

        if(!found)
            break;

        const gweni::Rect r=found->getCanvasBounds();
        const gweni::Point centre(static_cast<int>(std::lround((r.x+r.w/2.0)*scale)),
            static_cast<int>(std::lround((r.y+r.h/2.0)*scale)));

        centres.push_back(centre);
        if(static_cast<gweni::controls::Button *>(found)->getText() == "Toggle Me")
            toggle=centre;
    }
    centres.push_back(toggle);
    return centres;
}

// The targets in one tree: \p toCanvas maps a device px to the canvas point
// the input layer gives for it.
template<typename ToCanvas>
static void zoomTargets(Session &s, gweni::controls::Base *demo, const std::vector<gweni::Point> &centres,
    ToCanvas toCanvas, ZoomTargets &targets)
{
    for(int y=0; y < s.pixels.y; y+=3)
    {
        for(int x=0; x < s.pixels.x; x+=3)
        {
            const gweni::Point at=toCanvas(gweni::Point(x, y));

            targets.grid.push_back(pathUnder(demo, s.canvas->getControlAt(at.x, at.y)));
        }
    }

    for(size_t i=0; i+1 < centres.size(); ++i)
    {
        const gweni::Point at=toCanvas(centres[i]);

        moveMouse(s, at.x, at.y);
        renderFrames(s, 1);
        targets.hovered.push_back(pathUnder(demo, gweni::HoveredControl));
    }

    const gweni::Point toggle=toCanvas(centres.back());

    clickMouse(s, toggle.x, toggle.y);
    renderFrames(s, s.frames);
    s.renderer->readPixels(s.pixels.x, s.pixels.y, targets.clicked);
    forgetMouse(s);
}

static bool runZoomPanelInteraction(Session &s, const std::string &shotDir, bool &passed);
static int framesUntilShown(Session &s, int limit);

static bool runZoomPanelScenario(Session &s, const std::string &shotDir, bool &passed)
{
    static const float zooms[]={1.0f, 1.5f, 1.37f, 2.0f};
    const std::string controlName="ButtonDemo";

    passed=true;
    // an animation stands still between the shots
    s.freezeClock=true;

    for(float zoom : zooms)
    {
        char tag[32];

        std::snprintf(tag, sizeof(tag), "%.2f", zoom);

        ResizeState scaled, zoomed;
        ZoomTargets scaledTargets, zoomedTargets;

        // ButtonDemo on a canvas at setScale(zoom)
        if(!rescaleSession(s, zoom) || !resizeSession(s, 640, 480))
            return false;
        replaceCanvas(s);

        gweni::controls::Base *control=addControl(s, controlName);

        renderFrames(s, s.frames);
        scaled.name=std::string("zoom-")+tag+"-canvas.png";
        if(!shoot(s, shotDir+"/"+scaled.name)) return false;
        s.renderer->readPixels(s.pixels.x, s.pixels.y, scaled.pixels);

        const std::vector<gweni::Point> centres=buttonCentres(control, zoom);

        zoomTargets(s, control, centres, [zoom](const gweni::Point &px) {
                return gweni::Point(static_cast<int>(std::floor(double(px.x)/zoom)),
                    static_cast<int>(std::floor(double(px.y)/zoom)));
            }, scaledTargets);
        dropControl(s, control);

        // the same in a ZoomPanel at zoom, on a canvas at scale 1 of that target
        const gweni::Point target=s.pixels;

        if(!rescaleSession(s, 1.0f) || !resizeSession(s, target.x, target.y))
            return false;
        replaceCanvas(s);

        gweni::controls::ZoomPanel *panel=s.canvas->newChild<gweni::controls::ZoomPanel>();

        panel->setDock(gweni::DockPosition::Center);
        panel->setContentSize(gweni::Point(640, 480));
        panel->setZoom(zoom);
        control=addControl(s, controlName);
        control->setParent(panel);

        renderFrames(s, s.frames);
        zoomed.name=std::string("zoom-")+tag+"-panel.png";
        if(!shoot(s, shotDir+"/"+zoomed.name)) return false;
        s.renderer->readPixels(s.pixels.x, s.pixels.y, zoomed.pixels);
        zoomTargets(s, control, centres, [](const gweni::Point &px) { return px; }, zoomedTargets);

        const bool same=pixelsMatch(scaled, zoomed);
        size_t gridMisses=0;

        for(size_t i=0; i < scaledTargets.grid.size(); ++i)
        {
            if((i >= zoomedTargets.grid.size()) || (scaledTargets.grid[i] != zoomedTargets.grid[i]))
                ++gridMisses;
        }

        const bool hoverSame=(scaledTargets.hovered == zoomedTargets.hovered) && (centres.size() > 1)
            && (centres.back().x >= 0);
        const bool clickSame=(scaledTargets.clicked == zoomedTargets.clicked) && !scaledTargets.clicked.empty();

        std::cout<<"zoom-panel zoom="<<tag<<" pixels_match="<<(same?"yes":"no")
                 <<" grid_points="<<scaledTargets.grid.size()<<" grid_misses="<<gridMisses
                 <<" hovers="<<scaledTargets.hovered.size()<<" hovers_match="<<(hoverSame?"yes":"no")
                 <<" click_pixels_match="<<(clickSame?"yes":"no")<<"\n";
        if(!same || gridMisses || !hoverSame || !clickSame)
            std::cerr<<"ControlTest: zoom-panel: at zoom "<<tag<<" the panel did not draw or take input as a scaled canvas\n";
        passed=same && (gridMisses == 0) && hoverSame && clickSame && passed;
        panel->delayedDelete();
        s.canvas->updateCanvas();
    }

    bool interacted=false;

    if(!runZoomPanelInteraction(s, shotDir, interacted))
        return false;
    passed=interacted && passed;
    return true;
}

// One frame after \p input, as the zoom-panel checks count it.
struct ZoomFrame
{
    int passes=0;
    size_t invalidations=0;
    size_t primSlots=0;
    size_t textSlots=0;
    size_t atlasUploads=0;
    size_t strays=0;        // slots written that are not the panel subtree's
};

static bool runZoomPanelInteraction(Session &s, const std::string &shotDir, bool &passed)
{
    passed=true;
    s.freezeClock=true;
    if(!rescaleSession(s, 1.0f) || !resizeSession(s, 640, 480))
        return false;
    replaceCanvas(s);

    const size_t idsBefore=livePrimitiveIds()+liveTextIds();
    gweni::controls::Base *control=addControl(s, "ZoomPanelDemo");
    gweni::ZoomPanelDemo *demo=gweni::gweni_cast<gweni::ZoomPanelDemo>(control);

    if(!demo || !demo->panel())
    {
        std::cerr<<"ControlTest: zoom-panel: ZoomPanelDemo has no panel\n";
        return false;
    }

    gweni::controls::ZoomPanel *panel=demo->panel();
    const gweni::controls::Base *content=panel->getInnerPanel();
    gweni::controls::Canvas *canvas=s.canvas;

    s.label="zoom-panel";
    renderFrames(s, s.frames);

    auto check=[&](bool good, const std::string &what) {
        if(good)
            return;
        std::cerr<<"ControlTest: zoom-panel: "<<what<<"\n";
        passed=false;
    };

    std::set<size_t> panelPrimitives, panelTexts;

    subtreeIds(panel, panelPrimitives, panelTexts);

    auto frame=[&](const std::function<void()> &input) {
        ZoomFrame counted;
        const size_t invalidations=gweni::controls::invalidateCalls();

        input();
        renderFrames(s, 1);
        counted.passes=canvas->getLayoutStats().passes;
        counted.invalidations=gweni::controls::invalidateCalls()-invalidations;

        const gweni::renderer::SlotRenderer::FrameStats &gpu=s.renderer->getFrameStats();

        counted.primSlots=gpu.primitiveSlotsWritten;
        counted.textSlots=gpu.textSlotsWritten;
        counted.atlasUploads=gpu.atlasUploadCalls;
        for(size_t id : s.renderer->writtenPrimitiveIds())
            counted.strays+=panelPrimitives.count(id) ? 0 : 1;
        for(const gweni::renderer::WrittenText &text : s.renderer->writtenTextIds())
            counted.strays+=panelTexts.count(text.textId) ? 0 : 1;
        return counted;
    };

    // a zoom or pan frame: no layout, no invalidate, slots of the panel alone,
    // something written; the frame after it nothing
    auto spaceFrame=[&](const std::string &name, const std::function<void()> &input) {
        const ZoomFrame counted=frame(input);
        const ZoomFrame after=frame([]() {});

        std::cout<<"zoom-panel "<<name<<" layout_passes="<<counted.passes
                 <<" invalidations="<<counted.invalidations<<" prim_slots="<<counted.primSlots
                 <<" text_slots="<<counted.textSlots<<" atlas_uploads="<<counted.atlasUploads
                 <<" strays="<<counted.strays<<" settled_slots="<<(after.primSlots+after.textSlots)<<"\n";
        check(counted.passes == 0, name+" ran a layout pass");
        check(counted.invalidations == 0, name+" called invalidate()");
        check(counted.strays == 0, name+" wrote slots outside the panel");
        check(counted.primSlots > 0, name+" wrote no primitive slot");
        check((after.primSlots == 0) && (after.textSlots == 0), "the frame after "+name+" wrote slots");
        return counted;
    };

    // the content point at (300, 220), below the ScrollControl and right of
    // the TextBox: nothing but the inner panel there
    const gweni::PointF emptyPoint=content->localPosToCanvasF(gweni::PointF(300.0f, 220.0f));
    const gweni::Point empty(static_cast<int>(std::lround(emptyPoint.x)), static_cast<int>(std::lround(emptyPoint.y)));

    moveMouse(s, empty.x, empty.y);
    renderFrames(s, 2);
    check(gweni::HoveredControl == content, "the empty content point hovers "+traceName(gweni::HoveredControl));

    // the wheel: about the cursor, within the range
    {
        const gweni::PointF under=content->canvasPosToLocalF(gweni::PointF(empty));
        const float before=panel->zoom();
        const ZoomFrame zoomed=spaceFrame("wheel_zoom", [&]() { scrollWheel(s, 60); });
        const gweni::PointF at=content->localPosToCanvasF(under);

        check(zoomed.textSlots > 0, "the wheel zoom re-laid no text");
        check(panel->zoom() > before, "the wheel did not zoom in");
        check((std::fabs(at.x-empty.x) < 0.01f) && (std::fabs(at.y-empty.y) < 0.01f),
            "the wheel moved the content point under the cursor to "+std::to_string(at.x)+","+std::to_string(at.y));

        for(int i=0; i < 60; ++i)
            scrollWheel(s, 60);
        renderFrames(s, 1);
        check(panel->zoom() == panel->maxZoom(), "60 notches in did not stop at the range's top");
        check(scrollWheel(s, 60) && (panel->zoom() == panel->maxZoom()), "a notch past the top zoomed or was not taken");
        for(int i=0; i < 120; ++i)
            scrollWheel(s, -60);
        renderFrames(s, 1);
        check(panel->zoom() == panel->minZoom(), "120 notches out did not stop at the range's bottom");

        const gweni::PointF still=content->localPosToCanvasF(under);

        check((std::fabs(still.x-empty.x) < 0.01f) && (std::fabs(still.y-empty.y) < 0.01f),
            "the range's ends moved the content point under the cursor");
        std::cout<<"zoom-panel wheel max="<<panel->maxZoom()<<" min="<<panel->minZoom()<<"\n";
    }

    // back where the demo had it, and a zoom by code
    panel->setZoom(1.25f);
    panel->setPan(gweni::PointF(8.5f, 6.25f));
    renderFrames(s, 2);
    spaceFrame("set_zoom", [&]() { panel->setZoom(1.5f); });

    // an SDF-range zoom: 11 px text at 2 is 22 px, at 2.5 27.5
    panel->setZoom(2.0f);
    renderFrames(s, 2);
    {
        const ZoomFrame sdf=spaceFrame("sdf_zoom", [&]() { panel->setZoom(2.5f); });

        check(sdf.atlasUploads == 0, "an SDF-range zoom uploaded to a font atlas");
        check(sdf.textSlots > 0, "an SDF-range zoom re-laid no text");
    }
    panel->setZoom(1.25f);
    renderFrames(s, 2);

    // pans: right, middle, and Space with the left button
    struct PanCase { const char *name; int button; bool space; };
    static const PanCase pans[]={
        {"right_pan", GLFW_MOUSE_BUTTON_RIGHT, false},
        {"middle_pan", GLFW_MOUSE_BUTTON_MIDDLE, false},
        {"space_pan", GLFW_MOUSE_BUTTON_LEFT, true},
    };

    for(const PanCase &pan : pans)
    {
        const gweni::PointF before=panel->pan();
        const gweni::PointF content0=content->canvasPosToLocalF(gweni::PointF(empty));

        if(pan.space)
            glfwKey(s, GLFW_KEY_SPACE, GLFW_PRESS, 0);
        mouseDown(s, empty.x, empty.y, true, pan.button);
        renderFrames(s, 1);
        spaceFrame(pan.name, [&]() { moveMouse(s, empty.x+40, empty.y+25); });

        const gweni::PointF moved=panel->pan();

        mouseUp(s, empty.x+40, empty.y+25, pan.button);
        if(pan.space)
            glfwKey(s, GLFW_KEY_SPACE, GLFW_RELEASE, 0);
        renderFrames(s, 2);

        // the content point under the press is under the mouse
        const gweni::PointF at=content->localPosToCanvasF(content0);

        check((moved.x == before.x+40.0f) && (moved.y == before.y+25.0f),
            std::string(pan.name)+" moved the pan by "+std::to_string(moved.x-before.x)+","+std::to_string(moved.y-before.y));
        check((std::fabs(at.x-(empty.x+40)) < 0.01f) && (std::fabs(at.y-(empty.y+25)) < 0.01f),
            std::string(pan.name)+" did not keep the pressed point under the mouse");
        check(!canvas->shownPopup(), std::string(pan.name)+" opened a menu");
        check(!canvas->inputCapture(), std::string(pan.name)+" kept the canvas's input");
        moveMouse(s, empty.x, empty.y);
        renderFrames(s, 2);
    }
    panel->setPan(gweni::PointF(8.5f, 6.25f));
    renderFrames(s, 2);

    // the wheel over the ScrollControl scrolls it
    {
        auto *scroll=static_cast<gweni::controls::ScrollControl *>(findControl(control, "ScrollControl"));
        const float zoom=panel->zoom();

        check(scroll != nullptr, "ZoomPanelDemo has no ScrollControl");
        if(scroll)
        {
            const gweni::Rect r=scroll->getCanvasBounds();

            moveMouse(s, r.x+r.w/3, r.y+r.h/2);
            renderFrames(s, 1);
            scrollWheel(s, -60);
            renderFrames(s, 2);
            check(scroll->getVerticalScrollOffset() > 0, "the wheel over the ScrollControl did not scroll it");
            check(panel->zoom() == zoom, "the wheel over the ScrollControl zoomed");
            std::cout<<"zoom-panel scroll offset="<<scroll->getVerticalScrollOffset()<<"\n";
        }
    }

    const gweni::Rect area(0, 0, canvas->getWidth(), canvas->getHeight());
    auto onCanvas=[&](gweni::controls::Base *opened) {
        // a canvas child, so not in the panel's space
        return opened && (opened->getParent() == canvas);
    };

    // the tooltip box, with the clock stepping for the delay
    s.freezeClock=false;
    {
        gweni::controls::Base *button=findControl(control, "Button");

        check(button && !button->getToolTipText().empty(), "ZoomPanelDemo's first Button has no tooltip");
        if(button)
        {
            const gweni::Rect r=button->getCanvasBounds();

            moveMouse(s, r.x+r.w/2, r.y+r.h/2);
            const int shownAfter=framesUntilShown(s, 240);
            gweni::controls::ToolTip *box=canvas->toolTipControl();

            check(shownAfter > 0 && box, "the Button in the panel showed no tooltip");
            if(shownAfter > 0 && box)
            {
                const gweni::Point at=gweni::controls::ToolTip::placement(r,
                    gweni::Point(box->getWidth(), box->getHeight()), area);

                check(onCanvas(box), "the tooltip box is not the canvas's child");
                check((box->getX() == at.x) && (box->getY() == at.y), "the tooltip box is not at the Button as drawn");
                std::cout<<"zoom-panel tooltip button="<<rectText(r)<<" box="<<rectText(box->getBounds())<<"\n";
                if(!shoot(s, shotDir+"/zoom-tooltip.png")) return false;
            }
            moveMouse(s, empty.x, empty.y);
            renderFrames(s, 2);
        }
    }

    // the ComboBox's list, below the box as drawn and as wide
    {
        gweni::controls::Base *combo=findControl(control, "ComboBox");

        check(combo != nullptr, "ZoomPanelDemo has no ComboBox");
        if(combo)
        {
            const gweni::Rect r=combo->getCanvasBounds();

            clickMouse(s, r.x+r.w/2, r.y+r.h/2);
            renderFrames(s, 2);

            gweni::controls::Base *list=nullptr;

            for(gweni::controls::Base *child : canvas->getChildren())
            {
                if(gweni::gweni_cast<gweni::controls::Menu>(child) && !child->hidden())
                    list=child;
            }
            check(onCanvas(list), "the ComboBox opened no list on the canvas");
            if(list)
            {
                check((list->getX() == r.x) && (list->getY() == r.y+r.h) && (list->getWidth() == r.w),
                    "the ComboBox's list is at "+rectText(list->getBounds())+", not below "+rectText(r));
                std::cout<<"zoom-panel combo box="<<rectText(r)<<" list="<<rectText(list->getBounds())<<"\n";
                if(!shoot(s, shotDir+"/zoom-combo-list.png")) return false;
            }
            sendKey(s, "escape");
            renderFrames(s, 2);
        }
    }

    // a right click with 2 units of motion opens the Button's context menu at
    // the press
    {
        gweni::controls::Base *button=findControl(control, "Button");

        if(button)
        {
            gweni::controls::Menu *menu=button->createContextMenu();

            menu->addItem("Zoom in");
            menu->addItem("Zoom out");

            const gweni::Rect r=button->getCanvasBounds();
            const gweni::Point at(r.x+r.w/2, r.y+r.h/2);
            const gweni::PointF pan=panel->pan();

            mouseDown(s, at.x, at.y, true, GLFW_MOUSE_BUTTON_RIGHT);
            moveMouse(s, at.x+2, at.y);
            mouseUp(s, at.x+2, at.y, GLFW_MOUSE_BUTTON_RIGHT);
            renderFrames(s, 2);
            check(canvas->shownContextMenu() == menu, "a right click on the Button opened no context menu");
            check((panel->pan().x == pan.x) && (panel->pan().y == pan.y), "a right click panned");
            if(canvas->shownContextMenu() == menu)
            {
                const gweni::Point place=gweni::controls::Menu::placement(at,
                    gweni::Point(menu->getWidth(), menu->getHeight()), area);

                check(onCanvas(menu), "the context menu is not the canvas's child");
                check((menu->getX() == place.x) && (menu->getY() == place.y),
                    "the context menu is at "+rectText(menu->getBounds())+", not at the press");
                std::cout<<"zoom-panel context_menu at="<<at.x<<","<<at.y<<" menu="<<rectText(menu->getBounds())<<"\n";
                if(!shoot(s, shotDir+"/zoom-context-menu.png")) return false;
            }
            sendKey(s, "escape");
            renderFrames(s, 2);
        }
    }
    s.freezeClock=true;
    forgetMouse(s);
    renderFrames(s, 1);

    // every id back, but the canvas's ToolTip box's, which stays
    dropControl(s, control);
    renderFrames(s, 2);

    size_t boxIds=0;

    if(gweni::controls::ToolTip *box=canvas->toolTipControl())
    {
        std::set<size_t> primitives, texts;

        subtreeIds(box, primitives, texts);
        boxIds=primitives.size()+texts.size();
    }

    const size_t idsAfter=livePrimitiveIds()+liveTextIds()-boxIds;

    std::cout<<"zoom-panel ids before="<<idsBefore<<" after="<<idsAfter<<"\n";
    check(idsAfter == idsBefore, "deleting ZoomPanelDemo did not give its ids back");
    return true;
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
    checkTreeSelect(s, shotDir, checks, ok);
    checkCheckBox(s, shotDir, checks, ok);
    checkWindowClose(s, shotDir, checks, ok);
    checkWindowModal(s, shotDir, checks, ok);
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
    verifyIdleBefore(s);
    s.canvas->updateCanvas();

    pending=0;
    pendingUnderRoot=0;
    countPending(s.canvas, pending);
    countPending(root, pendingUnderRoot);

    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();
    verifyIdleAfter(s);

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
    verifyIdleBefore(s);
    s.canvas->updateCanvas();
    pendingOutside(s.canvas, allowed, strays, visibleOnly);

    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();
    verifyIdleAfter(s);

    if(s.stats)
        printFrameStats(s.frame, s.label, gweni::controls::getChangeStats(),
            s.renderer->getFrameStats(), s.canvas->getLayoutStats(),
            s.canvas->getUpdateStats());
    checkOnePass(s, std::to_string(s.frame));
    ++s.frame;
    ++g_clockSteps;
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
    // the box's own pixels: what is under it shows in its corners, outside
    // its rounded arcs, which write nothing
    const int radius=static_cast<int>(std::ceil(gweni::skin::textured::tokyonight::Tooltip.radius));
    size_t covered=differencesIn(s, alone, overMenu, tipRect);

    for(const gweni::Point &corner : {gweni::Point(tipRect.x, tipRect.y),
        gweni::Point(tipRect.x+tipRect.w-radius, tipRect.y), gweni::Point(tipRect.x, tipRect.y+tipRect.h-radius),
        gweni::Point(tipRect.x+tipRect.w-radius, tipRect.y+tipRect.h-radius)})
        covered-=differencesIn(s, alone, overMenu, gweni::Rect(corner.x, corner.y, radius, radius));
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

//-----------------------------------------------------------------------------
// --scenario node-graph: NodeGraphDemo's graph through the mouse and keys, on
// a 900x560 canvas at scale 1 with the clock frozen. Each input is one frame,
// and each frame's counters are checked against the part of the graph the
// input may touch: the primitive and glyph ids written (by id), the controls
// changed, the layout passes and the controls the layout entered.
//-----------------------------------------------------------------------------

// The counters of one frame after an input.
struct GraphFrame
{
    std::set<size_t> primitives;
    std::set<size_t> texts;
    size_t controls=0;
    int passes=0;
    size_t visited=0;
};

// What a node graph's events told: the scenario's handlers count them and may
// veto.
struct GraphEvents: public gweni::event::Handler
{
    int moved=0;
    int linkRequests=0;
    int linksMade=0;
    int emptyDrops=0;
    int selections=0;
    int nodeRemoveRequests=0;
    int nodesRemoved=0;
    bool vetoLink=false;
    bool vetoRemove=false;
    gweni::controls::NodeGraph *graph=nullptr;

    void onMoved(gweni::event::Info) { ++moved; }
    void onLinkRequested(gweni::event::Info)
    {
        ++linkRequests;
        if(vetoLink)
            graph->vetoLink();
    }
    void onLinkCreated(gweni::event::Info) { ++linksMade; }
    void onDroppedOnEmpty(gweni::event::Info) { ++emptyDrops; }
    void onSelection(gweni::event::Info) { ++selections; }
    void onNodeRemoveRequested(gweni::event::Info)
    {
        ++nodeRemoveRequests;
        if(vetoRemove)
            graph->vetoRemove();
    }
    void onNodeRemoved(gweni::event::Info) { ++nodesRemoved; }
};

static bool runNodeGraphScenario(Session &s, const std::string &shotDir, bool &passed)
{
    using namespace gweni::controls;

    passed=true;
    s.freezeClock=true;
    if(!rescaleSession(s, 1.0f) || !resizeSession(s, 900, 560))
        return false;
    replaceCanvas(s);

    const size_t idsBefore=livePrimitiveIds()+liveTextIds();
    Base *control=addControl(s, "NodeGraphDemo");
    gweni::NodeGraphDemo *demo=gweni::gweni_cast<gweni::NodeGraphDemo>(control);

    if(!demo || !demo->graph())
    {
        std::cerr<<"ControlTest: node-graph: NodeGraphDemo has no graph\n";
        return false;
    }

    NodeGraph *graph=demo->graph();
    Canvas *canvas=s.canvas;
    GraphEvents events;

    events.graph=graph;
    graph->onNodesMoved.add(&events, &GraphEvents::onMoved);
    graph->onLinkRequested.add(&events, &GraphEvents::onLinkRequested);
    graph->onLinkCreated.add(&events, &GraphEvents::onLinkCreated);
    graph->onLinkDroppedOnEmpty.add(&events, &GraphEvents::onDroppedOnEmpty);
    graph->onSelectionChanged.add(&events, &GraphEvents::onSelection);
    graph->onNodeRemoveRequested.add(&events, &GraphEvents::onNodeRemoveRequested);
    graph->onNodeRemoved.add(&events, &GraphEvents::onNodeRemoved);

    s.label="node-graph";
    renderFrames(s, s.frames);

    auto check=[&](bool good, const std::string &what) {
        if(good)
            return;
        std::cerr<<"ControlTest: node-graph: "<<what<<"\n";
        passed=false;
    };

    auto findNode=[&](const std::string &title) -> Node * {
        for(Node *node : graph->getNodes())
            if(node->getTitle() == title)
                return node;
        return nullptr;
    };
    auto findPin=[&](Node *node, const std::string &label, int which=0) -> Pin * {
        for(Pin *pin : node?node->getPins():std::vector<Pin *>())
            if((pin->getLabel() == label) && (which-- == 0))
                return pin;
        return nullptr;
    };
    // a graph point on the canvas, on the unit it falls in
    auto canvasAt=[&](const gweni::PointF &point) {
        const gweni::PointF at=graph->localPosToCanvasF(graph->graphToLocal(point));

        return gweni::Point(int(std::floor(at.x)), int(std::floor(at.y)));
    };
    auto titleOf=[&](Node *node) {
        Base *bar=node->getTitleBar();

        return bar->localPosToCanvas(gweni::Point(bar->getWidth()/2, bar->getHeight()/2));
    };
    auto record=[&](const std::function<void()> &input) {
        GraphFrame frame;

        input();
        renderFrames(s, 1);
        frame.controls=getChangeStats().controls;
        frame.passes=canvas->getLayoutStats().passes;
        frame.visited=canvas->getLayoutStats().visited;
        for(size_t id : s.renderer->writtenPrimitiveIds())
            frame.primitives.insert(id);
        for(const gweni::renderer::WrittenText &text : s.renderer->writtenTextIds())
            frame.texts.insert(text.textId);
        return frame;
    };
    // the ids written that are none of the controls' subtrees'
    auto strays=[&](const GraphFrame &frame, const std::vector<Base *> &controls) {
        std::set<size_t> primitives, texts;

        for(Base *allowed : controls)
            if(allowed)
                subtreeIds(allowed, primitives, texts);

        size_t count=0;

        for(size_t id : frame.primitives)
            count+=primitives.count(id)?0:1;
        for(size_t id : frame.texts)
            count+=texts.count(id)?0:1;
        return count;
    };
    auto countControls=[&](Base *root) {
        std::vector<Base *> subtree{root};

        for(size_t i=0; i < subtree.size(); ++i)
            for(Base *child : subtree[i]->getActualChildren())
                subtree.push_back(child);
        return subtree.size();
    };
    auto depthOf=[&](Base *node) {
        size_t depth=0;

        for(Base *parent=node->getActualParent(); parent; parent=parent->getActualParent())
            ++depth;
        return depth;
    };
    auto report=[&](const std::string &name, const GraphFrame &frame, size_t strayCount) {
        std::cout<<"node-graph "<<name<<" prim_slots="<<frame.primitives.size()<<" text_ids="<<frame.texts.size()
                 <<" controls_changed="<<frame.controls<<" layout_passes="<<frame.passes
                 <<" layout_visited="<<frame.visited<<" strays="<<strayCount<<"\n";
    };
    auto key=[&](int glfwKeyCode) {
        glfwKey(s, glfwKeyCode, GLFW_PRESS, 0);
        glfwKey(s, glfwKeyCode, GLFW_RELEASE, 0);
    };
    auto withModifier=[&](int modifierKey, int mod, const std::function<void()> &input) {
        glfwKey(s, modifierKey, GLFW_PRESS, 0);
        input();
        glfwKey(s, modifierKey, GLFW_RELEASE, mod);
    };

    Node *coord=findNode("Texture Coordinate");
    Node *noise=findNode("Noise Texture");
    Node *ramp=findNode("Color Ramp");
    Node *mix=findNode("Mix Shader");
    Pin *noiseColor=findPin(noise, "Color");
    Pin *mixShader=findPin(mix, "Shader", 1);

    if(!coord || !noise || !ramp || !mix || !noiseColor || !mixShader)
    {
        std::cerr<<"ControlTest: node-graph: NodeGraphDemo is missing a node or pin\n";
        return false;
    }

    // a point of the graph no node or link is near: below the nodes, on the left
    const gweni::Point empty=canvasAt(gweni::PointF(60.0f, 420.0f));

    moveMouse(s, empty.x, empty.y);
    renderFrames(s, 2);
    check(graph->getNodes().size() == 5, "NodeGraphDemo has "+std::to_string(graph->getNodes().size())+" nodes");
    check(graph->getLinks().size() == 4, "NodeGraphDemo has "+std::to_string(graph->getLinks().size())+" links");

    // Moving nodes: Noise Texture and Color Ramp selected, dragged by Noise's
    // title bar. Each frame writes their subtrees' and their links' slots
    // alone, changes no more controls than they hold and lays out in one pass
    // entering no more than them and their ancestors.
    {
        clickMouse(s, titleOf(noise).x, titleOf(noise).y);
        renderFrames(s, 1);
        withModifier(GLFW_KEY_LEFT_SHIFT, GLFW_MOD_SHIFT, [&]() { clickMouse(s, titleOf(ramp).x, titleOf(ramp).y); });
        renderFrames(s, 1);
        check(graph->getSelectedNodes().size() == 2, "the click and the Shift-click selected "
            +std::to_string(graph->getSelectedNodes().size())+" nodes");

        std::vector<Base *> moved{noise, ramp};
        size_t held=countControls(noise)+countControls(ramp);

        for(Link *link : noise->getLinks())
            moved.push_back(link);
        for(Link *link : ramp->getLinks())
            moved.push_back(link);
        held+=moved.size()-2;

        const gweni::Point from=titleOf(noise);
        const gweni::Point noiseStart=noise->getGraphPosition();
        const gweni::Point rampStart=ramp->getGraphPosition();

        mouseDown(s, from.x, from.y, true);
        renderFrames(s, 1);
        for(int step=1; step <= 6; ++step)
        {
            const GraphFrame frame=record([&]() { moveMouse(s, from.x+step*9, from.y+step*5); });
            const size_t strayCount=strays(frame, moved);

            report("drag_"+std::to_string(step), frame, strayCount);
            check(strayCount == 0, "a drag frame wrote slots outside the moved nodes and their links");
            check(frame.controls <= held, "a drag frame changed "+std::to_string(frame.controls)+" controls");
            check(frame.passes <= 1, "a drag frame ran more than one layout pass");
            check(frame.visited <= 4*(held+depthOf(noise)+depthOf(ramp)), "a drag frame's layout entered "
                +std::to_string(frame.visited)+" controls");
            check(events.moved == 0, "onNodesMoved fired before the release");
        }
        mouseUp(s, from.x+54, from.y+30);
        renderFrames(s, 1);
        check(events.moved == 1, "onNodesMoved fired "+std::to_string(events.moved)+" times for one drag");
        check((noise->getGraphPosition().x == noiseStart.x+54) && (ramp->getGraphPosition().x == rampStart.x+54),
            "the drag did not move both selected nodes by the mouse's motion");

        // the app puts them back: the same costs, with no event
        const GraphFrame back=record([&]() {
            noise->setGraphPosition(noiseStart);
            ramp->setGraphPosition(rampStart);
        });
        const size_t strayCount=strays(back, moved);

        report("app_move", back, strayCount);
        check(strayCount == 0, "an app's move wrote slots outside the moved nodes and their links");
        check(back.passes <= 1, "an app's move ran more than one layout pass");
        check(events.moved == 1, "an app's move fired onNodesMoved");
    }

    // Hover: a pin's marker, a link within HoverDistance of its curve and
    // not beyond; each frame writes the hovered item's slots and the left one's.
    {
        moveMouse(s, empty.x, empty.y);
        renderFrames(s, 2);

        const gweni::Point pinAt=canvasAt(noiseColor->getAnchor());
        const GraphFrame onPin=record([&]() { moveMouse(s, pinAt.x, pinAt.y); });
        const size_t pinStrays=strays(onPin, {noiseColor});

        report("hover_pin", onPin, pinStrays);
        check(gweni::HoveredControl == noiseColor, "the mouse on Noise's Color pin hovers "
            +traceName(gweni::HoveredControl));
        check(pinStrays == 0, "the pin's hover wrote slots outside the pin");
        check(!onPin.primitives.empty(), "the pin's hover wrote nothing");

        // the Fac -> Fac link, at a point of its curve, off it across
        Link *link=nullptr;

        for(Link *candidate : noise->getLinks())
            if(candidate->getTo() && (candidate->getTo()->getNode() == ramp))
                link=candidate;
        check(link != nullptr, "Noise has no link to Color Ramp");
        if(link)
        {
            std::vector<gweni::PointF> points;

            link->getPoints(points);

            const size_t middle=points.size()/2;
            const gweni::PointF a=graph->localPosToCanvasF(graph->graphToLocal(points[middle-1]));
            const gweni::PointF b=graph->localPosToCanvasF(graph->graphToLocal(points[middle+1]));
            const gweni::PointF on=graph->localPosToCanvasF(graph->graphToLocal(points[middle]));
            const float length=std::hypot(b.x-a.x, b.y-a.y);
            const gweni::PointF across(-(b.y-a.y)/length, (b.x-a.x)/length);
            auto off=[&](float units) {
                return gweni::Point(int(std::lround(on.x+across.x*units-0.5f)), int(std::lround(on.y+across.y*units-0.5f)));
            };

            const GraphFrame onLink=record([&]() { moveMouse(s, off(2.0f).x, off(2.0f).y); });
            const size_t linkStrays=strays(onLink, {link, noiseColor});

            report("hover_link", onLink, linkStrays);
            check(gweni::HoveredControl == link, "2 units off its curve the link is not hovered");
            check(linkStrays == 0, "the link's hover wrote slots outside it and the pin it left");

            moveMouse(s, off(6.0f).x, off(6.0f).y);
            renderFrames(s, 1);
            check(gweni::HoveredControl != link, "6 units off its curve the link is still hovered");

            // at zoom 4, where a graph unit is 4 on-screen units, the band is
            // still HoverDistance on screen: each canvas unit across the curve
            // is hovered exactly when its centre is that near, at segments of
            // the curve's middle part (other slopes, other phases of the
            // graph's whole units), the zoom about each
            const float zoomBefore=graph->zoom();
            const gweni::PointF panBefore=graph->pan();
            size_t wrong=0;
            size_t tested=0;

            for(size_t sample=0; sample < 5; ++sample)
            {
                const size_t i=points.size()*(3+sample)/10;
                const gweni::PointF centre((points[i].x+points[i+1].x)*0.5f, (points[i].y+points[i+1].y)*0.5f);

                graph->setZoom(zoomBefore);
                graph->setPan(panBefore);
                graph->zoomAt(4.0f, graph->graphToLocal(centre));
                renderFrames(s, 1);

                const gweni::PointF a4=graph->localPosToCanvasF(graph->graphToLocal(points[i]));
                const gweni::PointF b4=graph->localPosToCanvasF(graph->graphToLocal(points[i+1]));
                const gweni::PointF on4=graph->localPosToCanvasF(graph->graphToLocal(centre));
                const float length4=std::hypot(b4.x-a4.x, b4.y-a4.y);
                const gweni::PointF across4(-(b4.y-a4.y)/length4, (b4.x-a4.x)/length4);

                for(int step=-14; step <= 14; ++step)
                {
                    const float units=float(step)*0.5f;
                    const gweni::Point at(int(std::floor(on4.x+across4.x*units)), int(std::floor(on4.y+across4.y*units)));
                    const float distance=link->distanceTo(graph->canvasToGraph(
                        gweni::PointF(float(at.x)+0.5f, float(at.y)+0.5f)))*graph->zoom();

                    if(std::abs(distance-Link::HoverDistance) < 0.05f)
                        continue;
                    moveMouse(s, at.x, at.y);
                    renderFrames(s, 1);
                    ++tested;
                    if((gweni::HoveredControl == link) != (distance <= Link::HoverDistance))
                    {
                        std::cerr<<"ControlTest: node-graph: at zoom 4, "<<distance<<" units off its curve the link is "
                            <<((gweni::HoveredControl == link)?"":"not ")<<"hovered\n";
                        ++wrong;
                    }
                }
            }
            std::cout<<"node-graph hover_link_zoom4 tested="<<tested<<" wrong="<<wrong<<"\n";
            check(wrong == 0, "at zoom 4 the link's hover band is not HoverDistance on screen");

            graph->setZoom(zoomBefore);
            graph->setPan(panBefore);
            moveMouse(s, empty.x, empty.y);
            renderFrames(s, 2);
        }
    }

    // Linking: a drag out of Noise's Color shows a preview at the mouse; a drop
    // on Mix's second Shader input asks and makes the link, vetoed it does
    // not; a drop on empty space makes nothing and tells where; Escape ends a
    // drag with nothing.
    {
        const size_t links=graph->getLinks().size();
        const gweni::Point from=canvasAt(noiseColor->getAnchor());
        const gweni::Point to=canvasAt(mixShader->getAnchor());
        auto dragTo=[&](const gweni::Point &target) {
            mouseDown(s, from.x, from.y, true);
            renderFrames(s, 1);
            moveMouse(s, (from.x+target.x)/2, (from.y+target.y)/2);
            renderFrames(s, 1);
            moveMouse(s, target.x, target.y);
            renderFrames(s, 1);
        };

        events.vetoLink=true;
        dragTo(to);
        Link *preview=graph->getPreviewLink();
        check(preview && preview->getParent() == graph->getOverlayLayer(), "a pin drag shows no preview over the nodes");
        if(preview)
        {
            const gweni::PointF end=graph->canvasToGraph(gweni::PointF(to));

            check((std::fabs(preview->getFreeEnd().x-end.x) < 0.01f) && (std::fabs(preview->getFreeEnd().y-end.y) < 0.01f),
                "the preview does not end at the mouse");
        }
        mouseUp(s, to.x, to.y);
        renderFrames(s, 1);
        check((events.linkRequests == 1) && (events.linksMade == 0) && (graph->getLinks().size() == links),
            "a vetoed link request made a link");

        events.vetoLink=false;
        dragTo(to);
        mouseUp(s, to.x, to.y);
        renderFrames(s, 1);
        check((events.linkRequests == 2) && (events.linksMade == 1) && (graph->getLinks().size() == links+1),
            "a drop on a compatible pin made no link");
        check(noiseColor->isConnected() && mixShader->isConnected(), "the new link's pins are not connected");

        // where no node is: the demo offers nodes there, and Escape closes it
        dragTo(empty);
        mouseUp(s, empty.x, empty.y);
        renderFrames(s, 1);
        check((events.emptyDrops == 1) && (graph->getLinks().size() == links+1), "a drop on empty space made a link "
            "or told nothing");
        const gweni::PointF dropped=graph->canvasToGraph(gweni::PointF(empty));
        check((std::fabs(graph->getDropPosition().x-dropped.x) < 0.01f)
            && (std::fabs(graph->getDropPosition().y-dropped.y) < 0.01f), "the drop's graph position is not the mouse's");
        check(canvas->shownContextMenu() == graph->getContextMenu(), "the demo's Add node menu did not open at the drop");
        key(GLFW_KEY_ESCAPE);
        renderFrames(s, 1);

        dragTo(to);
        check(graph->draggingLink(), "a pin drag is not going on");
        key(GLFW_KEY_ESCAPE);
        renderFrames(s, 1);
        check(!graph->draggingLink() && (canvas->inputCapture() == nullptr), "Escape did not end the pin drag");
        mouseUp(s, to.x, to.y);
        renderFrames(s, 1);
        check((events.linkRequests == 2) && (graph->getLinks().size() == links+1), "a cancelled drag made a link");
        moveMouse(s, empty.x, empty.y);
        renderFrames(s, 2);
    }

    // Selection by click: alone, Shift adds, Ctrl toggles, empty space clears.
    {
        clickMouse(s, titleOf(coord).x, titleOf(coord).y);
        renderFrames(s, 1);
        check(graph->getSelectedNodes() == std::vector<Node *>{coord}, "a click did not select the node alone");
        withModifier(GLFW_KEY_LEFT_SHIFT, GLFW_MOD_SHIFT, [&]() { clickMouse(s, titleOf(mix).x, titleOf(mix).y); });
        renderFrames(s, 1);
        check(coord->isSelected() && mix->isSelected(), "Shift-click did not add");
        withModifier(GLFW_KEY_LEFT_CONTROL, GLFW_MOD_CONTROL, [&]() { clickMouse(s, titleOf(coord).x, titleOf(coord).y); });
        renderFrames(s, 1);
        check(!coord->isSelected() && mix->isSelected(), "Ctrl-click did not toggle");
        const int before=events.selections;
        clickMouse(s, empty.x, empty.y);
        renderFrames(s, 1);
        check(graph->getSelectedNodes().empty() && graph->getSelectedLinks().empty(), "a click on empty space did not clear");
        check(events.selections == before+1, "the clear did not fire onSelectionChanged once");
    }

    // The marquee: each frame writes the marquee's slots and those of the
    // items whose selection changed, nothing else; Escape gives the selection
    // back.
    {
        graph->selectNode(mix);
        renderFrames(s, 1);

        auto selection=[&]() {
            std::map<Base *, bool> state;

            for(Node *node : graph->getNodes())
                state[node]=node->isSelected();
            for(Link *link : graph->getLinks())
                state[link]=link->isSelected();
            return state;
        };
        const std::map<Base *, bool> start=selection();
        const gweni::Point from=canvasAt(gweni::PointF(400.0f, 200.0f));
        bool changedAny=false;

        mouseDown(s, from.x, from.y, true);
        renderFrames(s, 1);
        check(graph->marqueeActive(), "a press on empty space started no marquee");
        for(int step=1; step <= 6; ++step)
        {
            const std::map<Base *, bool> before=selection();
            const GraphFrame frame=record([&]() { moveMouse(s, from.x+step*40, from.y+step*25); });
            const std::map<Base *, bool> after=selection();
            std::vector<Base *> allowed;

            for(Base *part : graph->getOverlayLayer()->getChildren())
                if(gweni::gweni_cast<internal::NodeGraphMarquee>(part))
                    allowed.push_back(part);
            for(Base *part : graph->getLinkLayer()->getChildren())
                if(gweni::gweni_cast<internal::NodeGraphMarqueeFill>(part))
                    allowed.push_back(part);
            for(const auto &entry : after)
            {
                const auto was=before.find(entry.first);

                if((was == before.end()) || (was->second != entry.second))
                {
                    allowed.push_back(entry.first);
                    changedAny=true;
                }
            }

            const size_t strayCount=strays(frame, allowed);

            report("marquee_"+std::to_string(step), frame, strayCount);
            check(strayCount == 0, "a marquee frame wrote slots outside the marquee and the items it (de)selected");
            check(frame.passes <= 1, "a marquee frame ran more than one layout pass");
        }
        check(changedAny, "the marquee selected nothing");
        check(ramp->isSelected(), "the marquee did not select Color Ramp");
        key(GLFW_KEY_ESCAPE);
        renderFrames(s, 1);
        check(selection() == start, "Escape did not give the selection from before the marquee back");
        check(!graph->marqueeActive(), "Escape did not end the marquee");
        mouseUp(s, from.x+240, from.y+150);
        renderFrames(s, 2);
    }

    // Context menus: empty space, a node and a link each open theirs, with the
    // right press's graph position; a right drag pans and opens none.
    {
        rightClick(s, empty);
        renderFrames(s, 1);
        const gweni::PointF at=graph->canvasToGraph(gweni::PointF(empty));
        check(canvas->shownContextMenu() == graph->getContextMenu(), "a right click on empty space opened "
            "no graph menu");
        check((std::fabs(graph->getContextMenuPosition().x-at.x) < 0.01f)
            && (std::fabs(graph->getContextMenuPosition().y-at.y) < 0.01f), "the menu's graph position is not the click's");
        key(GLFW_KEY_ESCAPE);
        renderFrames(s, 1);

        rightClick(s, titleOf(ramp));
        renderFrames(s, 1);
        check(canvas->shownContextMenu() && (canvas->shownContextMenu() == ramp->getContextMenu()),
            "a right click on a node opened no node menu");
        key(GLFW_KEY_ESCAPE);
        renderFrames(s, 1);

        Link *link=graph->getLinks().front();
        std::vector<gweni::PointF> points;

        link->getPoints(points);
        const gweni::Point onLink=canvasAt(points[points.size()/2]);
        moveMouse(s, onLink.x, onLink.y);
        renderFrames(s, 1);
        rightClick(s, onLink);
        renderFrames(s, 1);
        check(canvas->shownContextMenu() && (canvas->shownContextMenu() == link->getContextMenu()),
            "a right click on a link opened no link menu");
        key(GLFW_KEY_ESCAPE);
        renderFrames(s, 1);

        const gweni::PointF pan=graph->pan();

        mouseDown(s, empty.x, empty.y, true, GLFW_MOUSE_BUTTON_RIGHT);
        moveMouse(s, empty.x+40, empty.y-20);
        renderFrames(s, 1);
        mouseUp(s, empty.x+40, empty.y-20, GLFW_MOUSE_BUTTON_RIGHT);
        renderFrames(s, 1);
        check(canvas->shownContextMenu() == nullptr, "a right drag opened a menu");
        check(std::fabs(graph->pan().x-(pan.x+40.0f)) < 0.01f, "a right drag did not pan");
        graph->setPan(pan);
        moveMouse(s, empty.x, empty.y);
        renderFrames(s, 2);
    }

    // Pan and zoom: every node's slots move, with no layout pass.
    {
        const GraphFrame panned=record([&]() { graph->setPan(gweni::PointF(graph->pan().x+15.0f, graph->pan().y)); });
        const GraphFrame zoomed=record([&]() { scrollWheel(s, 60); });

        report("pan", panned, 0);
        report("zoom", zoomed, 0);
        check((panned.passes == 0) && (zoomed.passes == 0), "a pan or zoom ran a layout pass");
        check(!panned.primitives.empty() && !zoomed.primitives.empty(), "a pan or zoom wrote nothing");
        graph->setZoom(1.0f);
        graph->setPan(gweni::PointF(10.0f, 10.0f));
        renderFrames(s, 2);
    }

    // Deleting: a vetoed node stays; one with links goes with them, its
    // events firing and every id it took coming back.
    {
        const size_t nodes=graph->getNodes().size();

        clickMouse(s, titleOf(mix).x, titleOf(mix).y);
        renderFrames(s, 1);
        events.vetoRemove=true;
        key(GLFW_KEY_DELETE);
        renderFrames(s, 1);
        check((events.nodeRemoveRequests == 1) && (graph->getNodes().size() == nodes), "a vetoed node was removed");
        events.vetoRemove=false;

        const size_t idsWithout=livePrimitiveIds()+liveTextIds();
        Node *added=demo->addNodeOfKind(1, gweni::PointF(120.0f, 330.0f), findPin(coord, "UV"));

        renderFrames(s, 2);
        check(added && (added->getLinks().size() == 1), "the demo's added node is not linked");
        clickMouse(s, titleOf(added).x, titleOf(added).y);
        renderFrames(s, 1);
        key(GLFW_KEY_DELETE);
        renderFrames(s, 2);
        check((events.nodeRemoveRequests == 2) && (events.nodesRemoved == 1) && (graph->getNodes().size() == nodes),
            "Delete did not remove the selected node");
        const size_t idsAfter=livePrimitiveIds()+liveTextIds();
        check(idsAfter == idsWithout, "the deleted node and its link gave "+std::to_string(idsWithout)
            +" ids back as "+std::to_string(idsAfter));
    }

    // a settled frame writes nothing, and the demo gives every id back
    {
        const GraphFrame settled=record([]() {});

        check(settled.primitives.empty() && settled.texts.empty(), "a settled frame wrote slots");
        if(!shoot(s, shotDir+"/node-graph.png"))
            return false;
    }

    dropControl(s, control);
    renderFrames(s, 2);
    check(livePrimitiveIds()+liveTextIds() == idsBefore, "deleting NodeGraphDemo did not give its ids back");
    std::cout<<"node-graph moved="<<events.moved<<" link_requests="<<events.linkRequests
             <<" links_made="<<events.linksMade<<" empty_drops="<<events.emptyDrops
             <<" node_remove_requests="<<events.nodeRemoveRequests<<" passed="<<(passed?"yes":"no")<<"\n";
    return true;
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
// --scenario dialog: dialogs::Dialog and the standard dialogs, on DialogDemo
// and LargeDialogDemo, the clock frozen, the mouse off the canvas so no hover
// moves. Each open frame and each settled frame is held to the dialog by the
// ids it writes and the controls its update phase leaves pending; a change
// behind an open dialog to the control changed; each close to the frame
// before the open.
//-----------------------------------------------------------------------------

// One frame, its update phase inspected (frameCheckingStrays): the ids it
// wrote, what it wrote and rendered, its pixels, and the controls with a change
// pending outside `allowed`.
struct DialogFrame
{
    std::set<size_t> primitives;
    std::set<size_t> texts;
    size_t written=0;
    size_t visited=0;
    std::vector<std::string> strays;
    std::vector<uint8_t> pixels;
};

static DialogFrame dialogFrame(Session &s, const std::vector<gweni::controls::Base *> &allowed)
{
    DialogFrame frame;

    frameCheckingStrays(s, allowed, frame.strays, true);
    for(size_t id : s.renderer->writtenPrimitiveIds())
        frame.primitives.insert(id);
    for(const gweni::renderer::WrittenText &text : s.renderer->writtenTextIds())
        frame.texts.insert(text.textId);
    const gweni::renderer::FrameCounts counts=s.renderer->frameStats();
    frame.written=counts.primitiveSlotsWritten+counts.textSlotsWritten;
    frame.visited=gweni::controls::getChangeStats().visited;
    frame.pixels=framePixels(s);
    return frame;
}

// The ids `frame` wrote that no control under any of `roots` holds, and the
// controls pending outside them, as a list ("" for none).
static std::string writtenOutside(const DialogFrame &frame, const std::vector<gweni::controls::Base *> &roots)
{
    std::set<size_t> primitives, texts;
    for(gweni::controls::Base *root : roots)
        subtreeIds(root, primitives, texts);

    std::string outside;
    for(size_t id : frame.primitives)
    {
        if(!primitives.count(id))
            outside+=" prim#"+std::to_string(id);
    }
    for(size_t id : frame.texts)
    {
        if(!texts.count(id))
            outside+=" text#"+std::to_string(id);
    }
    for(const std::string &stray : frame.strays)
        outside+=" pending:"+stray;
    return outside;
}

// The pixels of two frames that differ inside `rect`.
static size_t differencesInsideRect(const Session &s, const std::vector<uint8_t> &a,
    const std::vector<uint8_t> &b, const gweni::Rect &rect)
{
    return differencesOutsideRects(s, a, b, {})-differencesOutsideRects(s, a, b, {rect});
}

static bool runDialogScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    InteractionChecks checks;
    checks.scenario="dialog";
    bool ok=true;
    gweni::controls::Canvas *canvas=s.canvas;
    auto shot=[&](const std::string &name)
    {
        if(!shoot(s, shotDir+"/dialog-"+name+".png"))
            ok=false;
    };
    auto settle=[&]()
    {
        forgetMouse(s);
        renderFrames(s, s.frames);
        return framePixels(s);
    };

    // DialogDemo: each standard dialog opened by its button's action, a
    // settled frame, then closed by Enter, Escape or a typed text and Enter.
    gweni::controls::Base *demo=addControl(s, "DialogDemo");
    auto *page=gweni::gweni_cast<gweni::DialogDemo>(demo);
    if(!page || !page->m_message || !page->m_yesNo || !page->m_okCancel || !page->m_prompt || !page->m_result)
    {
        checks.check("demo_found", false, "DialogDemo has no four dialog buttons and a result label");
        dropControl(s, demo);
        passed=false;
        return ok;
    }

    struct Case
    {
        const char *name;
        gweni::controls::Button *button;
        //! the keys and text that close it
        std::vector<std::string> keys;
        const char *result;
    };
    const std::vector<Case> cases={
        {"message", page->m_message, {"return"}, "Result: message Ok"},
        {"yes_no", page->m_yesNo, {"escape"}, "Result: yes/no No"},
        {"ok_cancel", page->m_okCancel, {"return"}, "Result: ok/cancel Ok"},
        {"prompt", page->m_prompt, {"a", "b", "c", "return"}, "Result: prompt Ok \"abc\""},
    };

    for(const Case &one : cases)
    {
        std::vector<uint8_t> before=settle();
        const size_t idsBefore=livePrimitiveIds()+liveTextIds();
        const gweni::Rect resultRect=canvasRectOf(page->m_result);

        one.button->doAction();
        gweni::controls::Base *dialog=canvas->topModal();
        if(!dialog)
        {
            checks.check(std::string(one.name)+"_opens", false, "its button opened no dialog");
            continue;
        }
        const DialogFrame opened=dialogFrame(s, {dialog});
        const std::string openOutside=writtenOutside(opened, {dialog});
        checks.check(std::string(one.name)+"_open_alone", opened.written > 0 && openOutside.empty(),
            "the frame that opened it wrote "+std::to_string(opened.written)+" slots, outside the dialog:"
            +openOutside);
        renderFrames(s, s.frames);
        shot(std::string(one.name)+"-open");
        const DialogFrame settled=dialogFrame(s, {});
        checks.check(std::string(one.name)+"_settled", settled.written == 0 && settled.visited == 0
            && settled.strays.empty(),
            "a settled frame with it open wrote "+std::to_string(settled.written)+" slots and rendered "
            +std::to_string(settled.visited)+" controls");

        for(const std::string &key : one.keys)
        {
            sendKey(s, key);
            renderFrames(s, 1);
        }
        const std::vector<uint8_t> after=settle();
        // the label as it was and as it is: its text's width changed
        const size_t outside=differencesOutsideRects(s, before, after, {resultRect, canvasRectOf(page->m_result)});
        const bool closed=(canvas->topModal() == nullptr);
        checks.check(std::string(one.name)+"_closes", closed && page->m_result->getText() == one.result
            && outside == 0 && livePrimitiveIds()+liveTextIds() == idsBefore,
            std::string(closed ? "" : "it stayed open; ")+"the result reads \""+page->m_result->getText()
            +"\", want \""+one.result+"\"; "+std::to_string(outside)
            +" pixels outside the result label unlike the frame before it opened; live ids "
            +std::to_string(idsBefore)+" before, "+std::to_string(livePrimitiveIds()+liveTextIds())+" after");
    }
    forgetMouse(s);
    dropControl(s, demo);

    // LargeDialogDemo: 1200 units by 0.92 of the canvas, clamped to it.
    demo=addControl(s, "LargeDialogDemo");
    auto *large=gweni::gweni_cast<gweni::LargeDialogDemo>(demo);
    if(!large || !large->m_open || !large->m_behind)
    {
        checks.check("large_demo_found", false, "LargeDialogDemo has no open button and label behind");
        dropControl(s, demo);
        passed=false;
        return ok;
    }

    std::vector<uint8_t> before=settle();
    shot("large-before");
    const size_t idsBefore=livePrimitiveIds()+liveTextIds();

    for(int round=0; round < 2; ++round)
    {
        const std::string closeKey=(round == 0) ? "escape" : "return";

        large->m_open->doAction();
        gweni::dialogs::Dialog *dialog=large->dialog();
        if(!dialog || canvas->topModal() != dialog)
        {
            checks.check("large_opens", false, "\"Open task\" opened no dialog on the modal stack");
            break;
        }
        const DialogFrame opened=dialogFrame(s, {dialog});
        const std::string openOutside=writtenOutside(opened, {dialog});
        const gweni::Rect rect=canvasRectOf(dialog);
        const int wantH=static_cast<int>(std::lround(0.92*s.size.y));
        if(round == 0)
        {
            checks.check("large_size", rect.w == std::min(1200, s.size.x) && rect.h == wantH
                && rect.x == (s.size.x-rect.w)/2 && rect.y == (s.size.y-rect.h)/2,
                "the dialog is at "+rectText(rect)+", want "+std::to_string(std::min(1200, s.size.x))+"x"
                +std::to_string(wantH)+" centred");
            checks.check("large_open_alone", opened.written > 0 && openOutside.empty(),
                "the frame that opened it wrote outside it:"+openOutside);
        }
        renderFrames(s, s.frames);
        if(round == 0)
            shot("large-open");
        const DialogFrame settled=dialogFrame(s, {});
        const std::vector<uint8_t> open=settled.pixels;

        if(round == 0)
        {
            checks.check("large_settled", settled.written == 0 && settled.visited == 0,
                "a settled frame with it open wrote "+std::to_string(settled.written)+" slots and rendered "
                +std::to_string(settled.visited)+" controls");

            // a change behind it: the Label's own slots, nothing pending
            // elsewhere, no pixel inside the dialog
            const gweni::String was=large->m_behind->getText();
            large->m_behind->setText("Ticks behind the dialog: 999");
            const DialogFrame behind=dialogFrame(s, {large->m_behind});
            const std::string behindOutside=writtenOutside(behind, {large->m_behind});
            const size_t inside=differencesInsideRect(s, open, behind.pixels, rect);
            checks.check("large_behind_alone", behind.written > 0 && behindOutside.empty() && inside == 0,
                "a change behind it wrote "+std::to_string(behind.written)+" slots, outside the label:"
                +behindOutside+"; "+std::to_string(inside)+" pixels inside the dialog changed");
            large->m_behind->setText(was);
            renderFrames(s, s.frames);

            // a confirm stacked on it: modal to it, Escape gives it back, and
            // its Yes is the large dialog's to log
            large->m_forceRelease->doAction();
            gweni::controls::Base *confirm=canvas->topModal();
            renderFrames(s, s.frames);
            shot("large-confirm");
            const bool stacked=confirm && (confirm != dialog) && canvas->modalCount() == 2
                && confirm->getZIndex() > dialog->getZIndex();
            sendKey(s, "escape");
            renderFrames(s, s.frames);
            // (the dialog and its parts are gone if Escape closed it instead)
            const bool backAfterEscape=large->dialog() && (canvas->topModal() == dialog) && dialog->isOpen();
            bool yesLogged=false;
            if(backAfterEscape)
            {
                const size_t rows=large->m_log->getRowCount();
                large->m_forceRelease->doAction();
                renderFrames(s, s.frames);
                sendKey(s, "return");
                renderFrames(s, s.frames);
                yesLogged=large->dialog() && (canvas->topModal() == dialog) && large->m_log->getRowCount() == rows+1;
            }
            checks.check("large_confirm", stacked && backAfterEscape && yesLogged,
                std::string(stacked ? "" : "Force release opened no confirm stacked over the dialog; ")
                +(backAfterEscape ? "" : "Escape did not hand the input back to the dialog; ")
                +(yesLogged ? "" : "Enter (Yes) did not log the release with the dialog on top"));
        }

        sendKey(s, closeKey);
        const std::vector<uint8_t> after=settle();
        checks.check("large_"+closeKey+"_closes", !large->dialog() && !canvas->topModal() && after == before
            && livePrimitiveIds()+liveTextIds() == idsBefore,
            closeKey+" left "+(large->dialog() ? "the dialog open" : "the frame unlike the one before it opened")
            +"; live ids "+std::to_string(idsBefore)+" before, "+std::to_string(livePrimitiveIds()+liveTextIds())
            +" after");
    }
    forgetMouse(s);
    dropControl(s, demo);

    std::cout<<"dialog";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second ? "yes" : "no");
    std::cout<<"\n";

    passed=checks.allPassed();
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario toast: Canvas::notify's toasts on ToastDemo, the clock stepping
// (1/60 s a frame). Each check reads the canvas's toasts, their canvas rects,
// what a frame wrote and its whole pixels.
//-----------------------------------------------------------------------------

static bool runToastScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=false;
    s.statsClockSteps=true;

    InteractionChecks checks;
    checks.scenario="toast";
    bool ok=true;
    gweni::controls::Canvas *canvas=s.canvas;
    auto shot=[&](const std::string &name)
    {
        if(!shoot(s, shotDir+"/toast-"+name+".png"))
            ok=false;
    };
    // frames until nothing more is written, at most `limit`
    auto settle=[&](int limit)
    {
        for(int i=0; i < limit; ++i)
        {
            renderFrames(s, 1);
            if(s.renderer->frameStats().primitiveSlotsWritten+s.renderer->frameStats().textSlotsWritten == 0
                && gweni::controls::getChangeStats().controls == 0)
                break;
        }
    };

    gweni::controls::Base *demo=addControl(s, "ToastDemo");
    auto *page=gweni::gweni_cast<gweni::ToastDemo>(demo);
    if(!page || !page->m_info || !page->m_positive || !page->m_long || !page->m_sticky)
    {
        checks.check("demo_found", false, "ToastDemo has no Info, Positive, Multi-line and Sticky buttons");
        dropControl(s, demo);
        passed=false;
        return ok;
    }
    forgetMouse(s);
    renderFrames(s, s.frames);
    const std::vector<uint8_t> before=framePixels(s);
    shot("before");
    const size_t idsBefore=livePrimitiveIds()+liveTextIds();

    // two toasts, the newer above the older, in the top-right corner
    page->m_info->doAction();
    renderFrames(s, 1);
    page->m_positive->doAction();
    renderFrames(s, s.frames);
    shot("two");
    std::vector<gweni::controls::Toast *> shown=canvas->toasts();
    if(shown.size() != 2)
    {
        checks.check("stack", false, std::to_string(shown.size())+" toasts shown after two buttons, want 2");
        dropControl(s, demo);
        passed=false;
        return ok;
    }
    gweni::controls::Toast *older=shown[0];
    gweni::controls::Toast *newer=shown[1];
    const gweni::Rect olderRect=canvasRectOf(older);
    const gweni::Rect newerRect=canvasRectOf(newer);
    // read now: the toasts are gone by the time the multi-line one is checked
    const int newerTextHeight=newer->getTextLabel()->getHeight();
    const int right=s.size.x-gweni::controls::Toast::Gap;
    checks.check("stack", newerRect.y == gweni::controls::Toast::Gap
        && olderRect.y == newerRect.y+newerRect.h+gweni::controls::Toast::Gap
        && newerRect.x+newerRect.w == right && olderRect.x+olderRect.w == right,
        "the newer toast is at "+rectText(newerRect)+" and the older at "+rectText(olderRect)
        +"; want the newer at the top, "+std::to_string(gweni::controls::Toast::Gap)+" in from the corner");

    // settled frames: nothing written, nothing changed, no animation
    const DialogFrame settled=dialogFrame(s, {});
    checks.check("settled", settled.written == 0 && settled.visited == 0
        && canvas->getUpdateStats().animsRun == 0,
        "a settled frame with toasts shown wrote "+std::to_string(settled.written)+" slots, rendered "
        +std::to_string(settled.visited)+" controls and ran "+std::to_string(canvas->getUpdateStats().animsRun)
        +" animations");

    // the mouse onto the older one: the entry frame writes nothing, and the
    // stack stays past both timeouts (4 s) for as long as it rests there
    const gweni::Point onOlder(olderRect.x+olderRect.w/3, olderRect.y+olderRect.h/2);
    moveMouse(s, onOlder.x, onOlder.y);
    const DialogFrame entry=dialogFrame(s, {});
    const bool paused=canvas->toastsPaused();
    renderFrames(s, 6*60);
    const bool held=(canvas->toasts().size() == 2);
    size_t heldWritten=0;
    for(int i=0; i < 3; ++i)
        heldWritten+=dialogFrame(s, {}).written;
    // off it, to where nothing is: the exit frame writes nothing
    moveMouse(s, s.size.x/2, s.size.y-10);
    const DialogFrame exit=dialogFrame(s, {});
    checks.check("hover_holds", paused && held && entry.written == 0 && exit.written == 0 && heldWritten == 0
        && canvas->getUpdateStats().animsRun == 0,
        std::string(paused ? "" : "the hover did not pause the stack; ")
        +(held ? "" : "a toast expired under the mouse; ")
        +"the entry frame wrote "+std::to_string(entry.written)+", the held frames "+std::to_string(heldWritten)
        +" and the exit frame "+std::to_string(exit.written)+" slots");

    // each resumes with the time it had left: gone within a second or so of
    // their 4 s, not at once
    renderFrames(s, 60);
    const bool notAtOnce=(canvas->toasts().size() == 2);
    renderFrames(s, 4*60);
    forgetMouse(s);
    settle(30);
    const std::vector<uint8_t> afterExpiry=framePixels(s);
    shot("expired");
    // the host stays once made (it draws nothing, but holds its control's id)
    size_t hostIds=0;
    if(canvas->toastHost() && canvas->toastHost()->getSkinControl())
    {
        // its own, not its children's: a toast left in it is a leak
        std::vector<size_t> primitives, texts;
        canvas->toastHost()->getSkinControl()->appendIds(primitives, texts);
        hostIds=primitives.size()+texts.size();
    }
    checks.check("expiry_gives_back", notAtOnce && canvas->toasts().empty() && afterExpiry == before
        && livePrimitiveIds()+liveTextIds() == idsBefore+hostIds,
        std::string(notAtOnce ? "" : "the toasts went as soon as the mouse left; ")
        +std::to_string(canvas->toasts().size())+" toasts left, the frame "
        +(afterExpiry == before ? "" : "unlike the one before them, ")+"live ids "+std::to_string(idsBefore)
        +" before and the host's "+std::to_string(hostIds)+", "+std::to_string(livePrimitiveIds()+liveTextIds())
        +" after");

    // a multi-line toast is taller than a one-line one; a sticky one stays
    // past every default timeout and goes with its close button
    page->m_long->doAction();
    page->m_sticky->doAction();
    renderFrames(s, s.frames);
    shot("long-sticky");
    shown=canvas->toasts();
    const bool longTaller=(shown.size() == 2) && (shown[0]->getHeight() > newerRect.h)
        && (shown[0]->getTextLabel()->getHeight() > newerTextHeight);
    renderFrames(s, 9*60);
    const std::vector<gweni::controls::Toast *> afterNine=canvas->toasts();
    const bool stickyStays=(afterNine.size() == 1) && (afterNine[0]->getTimeout() == 0.0);
    if(stickyStays)
    {
        const gweni::Point close=centreOf(afterNine[0]->getCloseButton());
        clickMouse(s, close.x, close.y);
    }
    forgetMouse(s);
    settle(30);
    const std::vector<uint8_t> afterClose=framePixels(s);
    checks.check("multi_line_sticky", longTaller && stickyStays && canvas->toasts().empty() && afterClose == before
        && livePrimitiveIds()+liveTextIds() == idsBefore+hostIds,
        std::string(longTaller ? "" : "the multi-line toast is no taller than a one-line one; ")
        +(stickyStays ? "" : "the sticky toast did not outlast the others; ")
        +(canvas->toasts().empty() ? "" : "its close button left it shown; ")
        +(afterClose == before ? "" : "the frame is unlike the one before the toasts; ")
        +"live ids "+std::to_string(idsBefore)+" before, "+std::to_string(livePrimitiveIds()+liveTextIds())+" after");

    forgetMouse(s);
    dropControl(s, demo);

    std::cout<<"toast";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second ? "yes" : "no");
    std::cout<<"\n";

    passed=checks.allPassed();
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario code-editor: CodeEditorDemo's 5000-line document, the clock
// frozen, scrolled to its middle. Each input is one frame, held to the part of
// the editor it may touch by the ids it writes, the controls it changes and the
// pixels it changes: a typed character (its line's row, the gutter and the
// caret), Return mid-screen (the editor's rows and gutter on screen), a typing
// burst undone (the frame before it, byte for byte) and a one-line scroll (the
// rows on screen). Then wrapped, through the demo's toggle: a typed character
// that keeps its line's rows (its row, the gutter and the caret), one that
// adds a row (the rows below moved, nothing outside the editor), a resize and
// the toggle (the caret's line on screen) and a wrapped line's selection (a
// rect of its fill on each visual row, again after a resize). Last a plain
// auto-height field of CodeEditorFormDemo grown by a row in one pass. Every
// settled frame writes and changes nothing, and the demos give their
// primitive ids back.
//-----------------------------------------------------------------------------

namespace
{

// One frame's counters and pixels.
struct CodeEditorFrame
{
    std::set<size_t> primitives;
    std::set<size_t> texts;
    size_t glyphSlots=0;
    size_t written=0;
    size_t changed=0;
    size_t visited=0;
    int passes=0;
    std::vector<uint8_t> pixels;
};

size_t subtreeControls(gweni::controls::Base *control);

bool idsWithin(const std::set<size_t> &written, const std::set<size_t> &allowed)
{
    for(size_t id : written)
    {
        if(!allowed.count(id))
            return false;
    }
    return true;
}

// The glyphs the shown rows and the gutter's shown numbers hold.
size_t shownGlyphs(gweni::controls::CodeEditor *editor)
{
    size_t glyphs=0;

    for(gweni::controls::Base *row : editor->getRows())
    {
        if(auto *line=gweni::gweni_cast<gweni::controls::CodeLine>(row); line && !line->hidden())
            glyphs+=line->getText().size();
    }
    for(gweni::controls::internal::CodeLineNumber *number : editor->getLineNumberLabels())
    {
        if(!number->hidden())
            glyphs+=number->getText().size();
    }
    return glyphs;
}

}//namespace

static std::vector<uint8_t> framePixels(const Session &s);
static void subtreeIds(gweni::controls::Base *control, std::set<size_t> &primitives, std::set<size_t> &texts);

static CodeEditorFrame codeEditorFrame(Session &s)
{
    renderFrames(s, 1);

    CodeEditorFrame frame;
    const gweni::renderer::FrameCounts counts=s.renderer->frameStats();

    for(size_t id : s.renderer->writtenPrimitiveIds())
        frame.primitives.insert(id);
    for(const gweni::renderer::WrittenText &text : s.renderer->writtenTextIds())
    {
        frame.texts.insert(text.textId);
        frame.glyphSlots+=text.glyphSlots;
    }
    frame.written=counts.primitiveSlotsWritten+counts.textSlotsWritten;
    frame.changed=gweni::controls::getChangeStats().controls;
    frame.visited=s.canvas->getLayoutStats().visited;
    frame.passes=s.canvas->getLayoutStats().passes;
    frame.pixels=framePixels(s);
    return frame;
}

// Two frames with nothing to do: what they wrote and changed (0 when settled),
// the last one's pixels in `pixels`.
static size_t settleCodeEditor(Session &s, std::vector<uint8_t> &pixels)
{
    size_t work=0;

    for(int i=0; i < 2; ++i)
    {
        CodeEditorFrame frame=codeEditorFrame(s);

        work+=frame.written+frame.changed;
        pixels=std::move(frame.pixels);
    }
    return work;
}

// A press at the end of a line, in canvas coordinates: past its last glyph, so
// the caret goes to the line's end.
static void clickLineEnd(Session &s, gweni::controls::CodeEditor *editor, size_t line)
{
    const gweni::Rect &viewport=editor->getViewport();
    const int y=viewport.y+static_cast<int>(line)*editor->getLineHeight()-editor->getVerticalScrollOffset()
        +editor->getLineHeight()/2;
    const int x=viewport.x+editor->columnX(editor->getLineCells(line))+3*static_cast<int>(editor->getCellWidth());
    const gweni::Point at=editor->localPosToCanvas(gweni::Point(x, y));

    clickMouse(s, at.x, at.y);
}

static gweni::Rect canvasRect(gweni::controls::Base *control, const gweni::Rect &local)
{
    const gweni::Point origin=control->localPosToCanvas(gweni::Point(local.x, local.y));

    return gweni::Rect(origin.x, origin.y, local.w, local.h);
}

// A press at the middle of a control, in canvas coordinates.
static void clickCentre(Session &s, gweni::controls::Base *control)
{
    const gweni::Rect bounds=control->getCanvasBounds();

    clickMouse(s, bounds.x+bounds.w/2, bounds.y+bounds.h/2);
}

// The caret's line is shown by a row and the caret's rect is whole in the
// viewport (a line high).
static bool caretLineOnScreen(gweni::controls::CodeEditor *editor)
{
    const gweni::controls::CodeLine *row=editor->rowForLine(editor->getCaret().line);

    return row && !row->hidden() && editor->getCaretBounds().h == editor->getLineHeight();
}

// A line below the viewport's top, shown whole in \p rows visual rows, whose
// text starts with \p prefix (the line count when none is).
static size_t shownLineStarting(gweni::controls::CodeEditor *editor, const std::string &prefix, size_t rows)
{
    const size_t top=editor->firstVisibleItem();

    for(size_t line=top+1; line < top+40 && line < editor->getLineCount(); ++line)
    {
        const gweni::controls::CodeLine *row=editor->rowForLine(line);

        if(row && !row->hidden() && row->getRowCount() == rows && row->bottom() < editor->getViewportHeight()
            && editor->getDocument().line(line).rfind(prefix, 0) == 0)
            return line;
    }
    return editor->getLineCount();
}

// A press past the end of a wrapped line's last visual row: the caret goes
// to the line's end.
static void clickWrappedLineEnd(Session &s, gweni::controls::CodeEditor *editor, size_t line)
{
    const gweni::controls::CodeLine *row=editor->rowForLine(line);
    const gweni::Rect bounds=row->getCanvasBounds();
    const int lineHeight=editor->getLineHeight();

    clickMouse(s, bounds.x+bounds.w-4, bounds.y+bounds.h-lineHeight/2);
}

// A press at the end of the last line whose row is whole in the viewport:
// the caret on the view's bottom line, where wrapping or a smaller view
// pushes it out unless the editor keeps it on screen.
static size_t clickBottomLine(Session &s, gweni::controls::CodeEditor *editor)
{
    size_t bottom=editor->getLineCount();

    for(gweni::controls::Base *base : editor->getRows())
    {
        const auto *row=gweni::gweni_cast<gweni::controls::CodeLine>(base);

        if(row && !row->hidden() && row->getY() >= 0 && row->bottom() <= editor->getViewportHeight()
            && (bottom == editor->getLineCount() || row->getLine() > bottom))
            bottom=row->getLine();
    }
    // unwrapped, a row is the content's width, its end out of the viewport
    if(bottom < editor->getLineCount() && editor->isWrapped())
        clickWrappedLineEnd(s, editor, bottom);
    else if(bottom < editor->getLineCount())
        clickLineEnd(s, editor, bottom);
    return bottom;
}

// How many pixels of a rect (canvas units) are the opaque CodeSelection fill.
static size_t selectionPixels(const Session &s, const std::vector<uint8_t> &pixels, const gweni::Rect &rect)
{
    const gweni::Color &fill=gweni::skin::textured::tokyonight::CodeSelection.top;
    const gweni::Point topLeft=devicePixels(gweni::Point(rect.x, rect.y), s.scale);
    const gweni::Point bottomRight=devicePixels(gweni::Point(rect.x+rect.w, rect.y+rect.h), s.scale);
    size_t count=0;

    for(int y=std::max(0, topLeft.y); y < std::min(s.pixels.y, bottomRight.y); ++y)
    {
        for(int x=std::max(0, topLeft.x); x < std::min(s.pixels.x, bottomRight.x); ++x)
        {
            const uint8_t *p=&pixels[(static_cast<size_t>(y)*s.pixels.x+x)*4];

            if(p[0] == fill.r && p[1] == fill.g && p[2] == fill.b)
                ++count;
        }
    }
    return count;
}

// Every selection rect of a line's row is mostly the selection's fill (the
// glyphs over it take the rest); the rects, in canvas units, in \p rects.
static bool selectionDrawn(const Session &s, const std::vector<uint8_t> &pixels, gweni::controls::CodeLine *row,
    std::vector<gweni::Rect> &rects)
{
    std::vector<gweni::Rect> local;

    rects.clear();
    row->getSelectionRects(local);
    for(const gweni::Rect &rect : local)
    {
        const gweni::Rect onCanvas=canvasRect(row, rect);
        const gweni::Point a=devicePixels(gweni::Point(onCanvas.x, onCanvas.y), s.scale);
        const gweni::Point b=devicePixels(gweni::Point(onCanvas.x+onCanvas.w, onCanvas.y+onCanvas.h), s.scale);
        const size_t area=static_cast<size_t>(std::max(0, b.x-a.x))*static_cast<size_t>(std::max(0, b.y-a.y));

        rects.push_back(onCanvas);
        if(area == 0 || selectionPixels(s, pixels, onCanvas)*2 < area)
            return false;
    }
    return !local.empty();
}

//-----------------------------------------------------------------------------
// --scenario chip-badge: ChipBadgeDemo's badges and chips, the clock frozen
//
// A Badge is an attached child (Base::attachTo): placed by Base at its
// anchor's corner, out of the anchor's size, drawn at the z its anchor's
// parent reserves. So a new count writes the badge's slots alone and lays out
// the way down to the badge only when its width changes; a move of its anchor
// enters neither the badge nor the anchor's Text, the badge's pixels moving
// with the anchor's. A Chip's hover writes its slot alone, and a removed one's
// frame lays its row out in one pass and gives its ids back.
//-----------------------------------------------------------------------------

namespace
{

// The controls from the canvas down to `control`, both included.
size_t pathLength(gweni::controls::Base *control)
{
    size_t length=0;

    for(gweni::controls::Base *at=control; at; at=at->getActualParent())
        ++length;
    return length;
}

// The pixels of `rect` (logical units) in `before` that differ, by more than 1
// in a channel, from those of `after` `shift` device pixels further right and
// down.
size_t differencesMoved(const Session &s, const std::vector<uint8_t> &before,
    const std::vector<uint8_t> &after, const gweni::Rect &rect, const gweni::Point &shift)
{
    const gweni::Point topLeft=devicePixels(gweni::Point(rect.x, rect.y), s.scale);
    const gweni::Point bottomRight=devicePixels(gweni::Point(rect.x+rect.w, rect.y+rect.h), s.scale);
    size_t count=0;

    for(int y=std::max(topLeft.y, 0); y < std::min(bottomRight.y, s.pixels.y); ++y)
    {
        for(int x=std::max(topLeft.x, 0); x < std::min(bottomRight.x, s.pixels.x); ++x)
        {
            const int mx=x+shift.x;
            const int my=y+shift.y;

            if(mx < 0 || my < 0 || mx >= s.pixels.x || my >= s.pixels.y)
            {
                ++count;
                continue;
            }

            const size_t at=(static_cast<size_t>(y)*s.pixels.x+x)*4;
            const size_t movedAt=(static_cast<size_t>(my)*s.pixels.x+mx)*4;

            for(int c=0; c < 4; ++c)
            {
                if(std::abs(int(before[at+c])-int(after[movedAt+c])) > 1)
                {
                    ++count;
                    break;
                }
            }
        }
    }
    return count;
}

gweni::Rect unionRect(const gweni::Rect &a, const gweni::Rect &b)
{
    const int x0=std::min(a.x, b.x);
    const int y0=std::min(a.y, b.y);
    const int x1=std::max(a.x+a.w, b.x+b.w);
    const int y1=std::max(a.y+a.h, b.y+b.h);

    return gweni::Rect(x0, y0, x1-x0, y1-y0);
}

}//namespace

static bool runChipBadgeScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    InteractionChecks checks;
    checks.scenario="chip-badge";

    const size_t idsBefore=livePrimitiveIds()+liveTextIds();
    gweni::controls::Base *control=addControl(s, "ChipBadgeDemo");
    auto *demo=gweni::gweni_cast<gweni::ChipBadgeDemo>(control);

    if(!demo)
    {
        std::cerr<<"ControlTest: chip-badge: ChipBadgeDemo was not built\n";
        return false;
    }

    s.label="chip-badge";
    renderFrames(s, s.frames);

    bool ok=shoot(s, shotDir+"/chip-badge.png");
    std::vector<uint8_t> pixels;
    size_t idleWork=settleCodeEditor(s, pixels);

    // (1) a new count: the badge's slots alone, nothing laid out while its
    // width stays, the way down to it alone when it grows
    gweni::controls::Badge *badge=demo->navBadge();
    std::set<size_t> badgePrims, badgeTexts;

    subtreeIds(badge, badgePrims, badgeTexts);

    badge->setCount(badge->getCount()+1);
    const gweni::Rect sameRect=badge->getCanvasBounds();
    const CodeEditorFrame same=codeEditorFrame(s);
    const size_t sameOutside=differencesOutsideRects(s, pixels, same.pixels, {sameRect});

    checks.check("count_same_width",
        !same.texts.empty() && idsWithin(same.primitives, badgePrims) && idsWithin(same.texts, badgeTexts)
            && same.passes == 0 && same.changed == 1 && sameOutside == 0,
        "a count of the same width wrote "+std::to_string(same.primitives.size())+" primitive and "
        +std::to_string(same.texts.size())+" text ids (the badge's alone allowed), ran "
        +std::to_string(same.passes)+" layout passes, changed "+std::to_string(same.changed)
        +" controls and "+std::to_string(sameOutside)+" pixels outside it");
    idleWork+=settleCodeEditor(s, pixels);

    badge->setCount(9);
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);

    const gweni::Rect narrow=badge->getCanvasBounds();

    badge->setCount(10);

    const CodeEditorFrame wider=codeEditorFrame(s);
    const gweni::Rect wide=badge->getCanvasBounds();
    const size_t widerOutside=differencesOutsideRects(s, pixels, wider.pixels, {narrow, wide});
    const size_t badgePath=pathLength(badge);

    checks.check("count_wider",
        wide.w > narrow.w && idsWithin(wider.primitives, badgePrims) && idsWithin(wider.texts, badgeTexts)
            && wider.passes == 1 && wider.visited <= 4*badgePath && widerOutside == 0,
        "9 -> 10 wrote outside the badge's ids, ran "+std::to_string(wider.passes)+" passes entering "
        +std::to_string(wider.visited)+" (the way down to the badge, "+std::to_string(4*badgePath)
        +", allowed) or changed "+std::to_string(widerOutside)+" pixels outside it");
    badge->setCount(4);
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);

    // (2) the anchor moved: neither its Text nor its badges entered, and what
    // they draw moved with it
    gweni::controls::Button *corner=demo->cornerButton();
    gweni::Rect region=corner->getCanvasBounds();

    for(gweni::controls::Badge *cornerBadge : demo->cornerBadges())
        region=unionRect(region, cornerBadge->getCanvasBounds());
    region=gweni::Rect(region.x-1, region.y-1, region.w+2, region.h+2);

    const gweni::Point step(16, 8);

    corner->setPos(corner->getX()+step.x, corner->getY()+step.y);

    const CodeEditorFrame moved=codeEditorFrame(s);
    const size_t cornerPath=pathLength(corner);
    const size_t movedDiff=differencesMoved(s, pixels, moved.pixels, region,
        devicePixels(step, s.scale));

    checks.check("anchor_moved",
        moved.passes == 1 && moved.visited <= 4*cornerPath && movedDiff == 0,
        "the anchor's move ran "+std::to_string(moved.passes)+" passes entering "
        +std::to_string(moved.visited)+" (the way down to the anchor alone, "+std::to_string(4*cornerPath)
        +", allowed), and "+std::to_string(movedDiff)+" of its and its badges' pixels did not move with it");
    corner->setPos(corner->getX()-step.x, corner->getY()-step.y);
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);

    // (3) a clickable chip's hover: its slot alone, and moving off gives the
    // frame back
    gweni::controls::Chip *chip=demo->clickableChip();
    std::set<size_t> chipPrims, chipTexts;

    subtreeIds(chip, chipPrims, chipTexts);

    const gweni::Rect chipRect=chip->getCanvasBounds();

    moveMouse(s, chipRect.x+chipRect.w/2, chipRect.y+chipRect.h/2);

    const CodeEditorFrame hover=codeEditorFrame(s);
    const size_t hoverOutside=differencesOutsideRects(s, pixels, hover.pixels, {chipRect});

    moveMouse(s, s.size.x-4, s.size.y-4);

    const CodeEditorFrame off=codeEditorFrame(s);

    checks.check("chip_hover",
        chip->isHovered() == false && !hover.primitives.empty() && idsWithin(hover.primitives, chipPrims)
            && hover.texts.empty() && hoverOutside == 0 && off.pixels == pixels,
        "a chip's hover wrote "+std::to_string(hover.primitives.size())+" primitive ids (its own alone allowed) and "
        +std::to_string(hover.texts.size())+" text ids, changed "+std::to_string(hoverOutside)
        +" pixels outside it, or moving off did not give the frame back");
    idleWork+=settleCodeEditor(s, pixels);

    // (4) a removal: one pass, the chip's ids back
    gweni::controls::Chip *removed=demo->removableChip();
    std::set<size_t> removedPrims, removedTexts;

    subtreeIds(removed, removedPrims, removedTexts);

    const size_t idsWithChip=livePrimitiveIds()+liveTextIds();
    const gweni::Rect removeRect=removed->getRemoveButton()->getCanvasBounds();

    clickMouse(s, removeRect.x+removeRect.w/2, removeRect.y+removeRect.h/2);

    const CodeEditorFrame removal=codeEditorFrame(s);
    const size_t idsWithout=livePrimitiveIds()+liveTextIds();
    const bool gone=demo->removableChip() != removed;
    const size_t rowEntries=4*(pathLength(demo->chipRow())+subtreeControls(demo->chipRow())-1);

    checks.check("chip_removed",
        gone && removal.passes == 1 && removal.visited <= rowEntries
            && idsWithChip-idsWithout == removedPrims.size()+removedTexts.size(),
        std::string(gone?"":"the chip is still there; ")+"the removal ran "+std::to_string(removal.passes)
        +" passes entering "+std::to_string(removal.visited)+" (the row and the way down to it, "
        +std::to_string(rowEntries)+", allowed) and gave back "+std::to_string(idsWithChip-idsWithout)
        +" of the chip's "+std::to_string(removedPrims.size()+removedTexts.size())+" ids");

    forgetMouse(s);
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);
    checks.check("settled", idleWork == 0,
        "settled frames wrote or changed "+std::to_string(idleWork));

    dropControl(s, control);
    renderFrames(s, 1);

    const size_t idsLeft=livePrimitiveIds()+liveTextIds()-idsBefore;

    checks.check("ids_back", idsLeft == 0, std::to_string(idsLeft)+" ids not given back");

    passed=checks.allPassed();
    std::cout<<"chip-badge";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second?"yes":"no");
    std::cout<<" same_prims="<<same.primitives.size()<<" wider_visited="<<wider.visited
             <<" moved_visited="<<moved.visited<<" hover_prims="<<hover.primitives.size()
             <<" removal_visited="<<removal.visited<<"\n";
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario switch: SwitchDemo's switches, the clock stepping
//
// A Switch draws its track and its thumb in two slots of its own, its label
// being its own Text. A hover lights the track alone; a toggle starts the
// thumb's slide, an animation registered only while it moves, each of whose
// frames writes the thumb's slot alone and lays nothing out; a settled switch
// runs no animation. Every look is compared with the switch focused (the
// focus border stays the same throughout), the mouse off it.
//-----------------------------------------------------------------------------

namespace
{

// What a toggle's frames did: the input frame, then each slide frame until
// one runs no animation.
struct SwitchSlideRun
{
    size_t frames=0;            // slide frames that ran the animation
    size_t thumbFrames=0;       // of those, the ones that wrote the thumb's slot
    bool inputWithin=true;      // the input frame wrote the switch's ids alone
    bool slideThumbOnly=true;   // every slide frame wrote the thumb's slot alone
    bool slideNoLayout=true;    // and ran no layout pass
    size_t outside=0;           // pixels changed outside the switch, every frame
    bool ended=false;           // a frame ran no animation and the switch is not sliding
};

SwitchSlideRun switchSlideFrames(Session &s, gweni::controls::Switch *control,
    const std::vector<uint8_t> &before)
{
    SwitchSlideRun run;
    std::set<size_t> prims, texts;

    subtreeIds(control, prims, texts);

    const size_t thumb=control->getSkinControl()->getPrimitives().at(1);
    const gweni::Rect rect=control->getCanvasBounds();
    const CodeEditorFrame input=codeEditorFrame(s);

    run.inputWithin=idsWithin(input.primitives, prims) && input.texts.empty();
    run.outside+=differencesOutsideRects(s, before, input.pixels, {rect});

    for(int i=0; i < 60; ++i)
    {
        const CodeEditorFrame frame=codeEditorFrame(s);

        run.outside+=differencesOutsideRects(s, before, frame.pixels, {rect});
        if(s.canvas->getUpdateStats().animsRun == 0)
        {
            run.ended=!control->isSliding() && frame.written == 0;
            break;
        }
        ++run.frames;
        if(frame.primitives.count(thumb))
            ++run.thumbFrames;
        if(!idsWithin(frame.primitives, {thumb}) || !frame.texts.empty())
            run.slideThumbOnly=false;
        if(frame.passes != 0)
            run.slideNoLayout=false;
    }
    return run;
}

bool switchSlideGood(const SwitchSlideRun &run)
{
    return run.inputWithin && run.slideThumbOnly && run.slideNoLayout && run.outside == 0
        && run.ended && run.thumbFrames >= 3;
}

std::string switchSlideReport(const SwitchSlideRun &run)
{
    return "input frame within the switch's ids: "+std::string(run.inputWithin?"yes":"no")
        +"; "+std::to_string(run.frames)+" slide frames, "+std::to_string(run.thumbFrames)
        +" writing the thumb (3 or more wanted), thumb alone: "+std::string(run.slideThumbOnly?"yes":"no")
        +", no layout: "+std::string(run.slideNoLayout?"yes":"no")+"; "+std::to_string(run.outside)
        +" pixels changed outside the switch; ended: "+std::string(run.ended?"yes":"no");
}

gweni::Point switchTrackCentre(gweni::controls::Switch *control)
{
    const gweni::Rect track=control->getTrackRect();

    return control->localPosToCanvas(gweni::Point(track.x+track.w/2, track.y+track.h/2));
}

gweni::Point switchLabelPoint(gweni::controls::Switch *control)
{
    const gweni::Rect track=control->getTrackRect();
    const int x=(track.x == 0)?control->getWidth()-4:4;

    return control->localPosToCanvas(gweni::Point(x, control->getHeight()/2));
}

}//namespace

static bool runSwitchScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=false;
    s.statsClockSteps=true;

    InteractionChecks checks;
    checks.scenario="switch";

    const size_t idsBefore=livePrimitiveIds()+liveTextIds();
    gweni::controls::Base *control=addControl(s, "SwitchDemo");
    auto *demo=gweni::gweni_cast<gweni::SwitchDemo>(control);

    if(!demo || !demo->labelRightSwitch() || !demo->disabledOffSwitch() || !demo->enablingSwitch())
    {
        std::cerr<<"ControlTest: switch: SwitchDemo was not built\n";
        return false;
    }

    s.label="switch";
    renderFrames(s, s.frames);

    gweni::controls::Switch *target=demo->labelRightSwitch();
    const gweni::Point off(s.size.x-4, s.size.y-4);

    // every look below is the focused one
    target->focus();
    renderFrames(s, 1);

    std::vector<uint8_t> offLook;
    size_t idleWork=settleCodeEditor(s, offLook);
    bool ok=shoot(s, shotDir+"/switch.png");

    checks.check("settled_start", idleWork == 0 && s.canvas->getUpdateStats().animsRun == 0,
        "the demo's first settled frames wrote or changed "+std::to_string(idleWork)
        +" and ran "+std::to_string(s.canvas->getUpdateStats().animsRun)+" animations");

    // (1) a hover: the switch's track alone, on the track and on the label alike
    std::set<size_t> prims, texts;

    subtreeIds(target, prims, texts);

    const gweni::Rect rect=target->getCanvasBounds();
    const size_t track=target->getSkinControl()->getPrimitives().at(0);
    const gweni::Point onTrack=switchTrackCentre(target);
    const gweni::Point onLabel=switchLabelPoint(target);

    moveMouse(s, onTrack.x, onTrack.y);

    const CodeEditorFrame hover=codeEditorFrame(s);
    const size_t hoverOutside=differencesOutsideRects(s, offLook, hover.pixels, {rect});

    moveMouse(s, onLabel.x, onLabel.y);

    const CodeEditorFrame labelHover=codeEditorFrame(s);

    moveMouse(s, off.x, off.y);

    const CodeEditorFrame leave=codeEditorFrame(s);

    checks.check("hover",
        hover.primitives == std::set<size_t>{track} && hover.texts.empty() && hoverOutside == 0
            && hover.pixels != offLook && labelHover.written == 0 && labelHover.pixels == hover.pixels
            && leave.pixels == offLook,
        "the hover wrote "+std::to_string(hover.primitives.size())+" primitive ids (the track's alone wanted) and "
        +std::to_string(hover.texts.size())+" text ids, changed "+std::to_string(hoverOutside)
        +" pixels outside the switch, the label's hover wrote "+std::to_string(labelHover.written)
        +(labelHover.pixels == hover.pixels?"":" and looked unlike the track's")
        +(leave.pixels == offLook?"":", and moving off did not give the frame back"));
    idleWork+=settleCodeEditor(s, offLook);

    // (2) a click on the track: its slide, the thumb's slot alone
    clickMouse(s, onTrack.x, onTrack.y);

    const SwitchSlideRun clickOn=switchSlideFrames(s, target, offLook);

    checks.check("click_slide", target->isOn() && switchSlideGood(clickOn),
        std::string(target->isOn()?"":"the click did not turn it on; ")+switchSlideReport(clickOn));

    moveMouse(s, off.x, off.y);
    renderFrames(s, 1);   // the leave frame: the hover goes

    std::vector<uint8_t> onLook;

    idleWork+=settleCodeEditor(s, onLook);
    checks.check("settled_on", s.canvas->getUpdateStats().animsRun == 0 && onLook != offLook
        && differencesOutsideRects(s, offLook, onLook, {rect}) == 0,
        "on, settled, it ran "+std::to_string(s.canvas->getUpdateStats().animsRun)
        +" animations, looked as off, or changed pixels outside it");
    ok=shoot(s, shotDir+"/switch-on.png") && ok;

    // (3) a second click gives the first look back
    clickMouse(s, onTrack.x, onTrack.y);

    const SwitchSlideRun clickOff=switchSlideFrames(s, target, offLook);
    std::vector<uint8_t> back;

    moveMouse(s, off.x, off.y);
    renderFrames(s, 1);   // the leave frame: the hover goes
    idleWork+=settleCodeEditor(s, back);
    checks.check("second_click", !target->isOn() && switchSlideGood(clickOff) && back == offLook,
        std::string(target->isOn()?"the second click left it on; ":"")+switchSlideReport(clickOff)
        +(back == offLook?"":"; the off look did not come back"));

    // (4) a click on the label and Space: the same toggle, the same looks
    clickMouse(s, onLabel.x, onLabel.y);

    const SwitchSlideRun labelOn=switchSlideFrames(s, target, offLook);
    std::vector<uint8_t> labelOnLook, labelOffLook;

    moveMouse(s, off.x, off.y);
    renderFrames(s, 1);   // the leave frame: the hover goes
    idleWork+=settleCodeEditor(s, labelOnLook);
    clickMouse(s, onLabel.x, onLabel.y);

    const SwitchSlideRun labelOff=switchSlideFrames(s, target, offLook);

    moveMouse(s, off.x, off.y);
    renderFrames(s, 1);   // the leave frame: the hover goes
    idleWork+=settleCodeEditor(s, labelOffLook);
    checks.check("label_click", switchSlideGood(labelOn) && switchSlideGood(labelOff)
        && labelOnLook == onLook && labelOffLook == offLook,
        switchSlideReport(labelOn)+"; back: "+switchSlideReport(labelOff)
        +(labelOnLook == onLook?"":"; on, unlike the click's on look")
        +(labelOffLook == offLook?"":"; off, unlike the first look"));

    glfwKey(s, GLFW_KEY_SPACE, GLFW_PRESS, 0);
    glfwKey(s, GLFW_KEY_SPACE, GLFW_RELEASE, 0);

    const SwitchSlideRun spaceOn=switchSlideFrames(s, target, offLook);
    std::vector<uint8_t> spaceOnLook, spaceOffLook;

    idleWork+=settleCodeEditor(s, spaceOnLook);
    glfwKey(s, GLFW_KEY_SPACE, GLFW_PRESS, 0);
    glfwKey(s, GLFW_KEY_SPACE, GLFW_RELEASE, 0);

    const SwitchSlideRun spaceOff=switchSlideFrames(s, target, offLook);

    idleWork+=settleCodeEditor(s, spaceOffLook);
    checks.check("space", switchSlideGood(spaceOn) && switchSlideGood(spaceOff)
        && spaceOnLook == onLook && spaceOffLook == offLook,
        switchSlideReport(spaceOn)+"; back: "+switchSlideReport(spaceOff)
        +(spaceOnLook == onLook?"":"; on, unlike the click's on look")
        +(spaceOffLook == offLook?"":"; off, unlike the first look"));

    // (5) the disabled switches: neither a hover nor a click changes them. A
    // click on one drops the focus as a click on anything that takes no keys
    // does, so they are compared with the focus dropped first.
    clickMouse(s, off.x, off.y);
    moveMouse(s, off.x, off.y);

    std::vector<uint8_t> unfocused;

    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, unfocused);

    bool disabledStill=gweni::KeyboardFocus == nullptr;
    std::string disabledReport=disabledStill?"":"a click off the switch kept the focus; ";

    for(gweni::controls::Switch *disabled : {demo->disabledOffSwitch(), demo->disabledOnSwitch()})
    {
        const bool wasOn=disabled->isOn();
        const gweni::Point at=switchTrackCentre(disabled);

        moveMouse(s, at.x, at.y);

        const CodeEditorFrame hovered=codeEditorFrame(s);

        clickMouse(s, at.x, at.y);

        const CodeEditorFrame clicked=codeEditorFrame(s);
        const CodeEditorFrame after=codeEditorFrame(s);

        moveMouse(s, off.x, off.y);

        const CodeEditorFrame left=codeEditorFrame(s);
        const bool still=disabled->isOn() == wasOn && !disabled->isSliding() && hovered.written == 0
            && clicked.written == 0 && after.written == 0 && left.written == 0
            && hovered.pixels == unfocused && clicked.pixels == unfocused && left.pixels == unfocused
            && gweni::KeyboardFocus == nullptr;

        if(!still)
        {
            disabledStill=false;
            disabledReport+=std::string(wasOn?"the disabled on switch":"the disabled off switch")
                +" wrote "+std::to_string(hovered.written)+" slots hovered, "+std::to_string(clicked.written)
                +" and "+std::to_string(after.written)+" clicked, is "+(disabled->isOn()?"on":"off")
                +(gweni::KeyboardFocus == nullptr?"":", took the focus")
                +(clicked.pixels == unfocused?"; ":", changed pixels; ");
        }
    }
    checks.check("disabled", disabledStill, disabledReport);
    idleWork+=settleCodeEditor(s, back);

    // (6) the switch that enables the Apply button
    gweni::controls::Switch *enabling=demo->enablingSwitch();
    const gweni::Point enablingAt=switchTrackCentre(enabling);
    const bool disabledFirst=demo->enabledButton()->isDisabled();

    clickMouse(s, enablingAt.x, enablingAt.y);
    renderFrames(s, 1);

    const bool enabled=!demo->enabledButton()->isDisabled();

    clickMouse(s, enablingAt.x, enablingAt.y);
    renderFrames(s, 1);

    const bool disabledAgain=demo->enabledButton()->isDisabled();

    checks.check("enables", disabledFirst && enabled && disabledAgain,
        "the Apply button was "+std::string(disabledFirst?"disabled":"enabled")+", then "
        +(enabled?"enabled":"disabled")+", then "+(disabledAgain?"disabled":"enabled"));
    moveMouse(s, off.x, off.y);
    renderFrames(s, 30);

    // (7) the demo hidden mid-slide: the next frame runs no animation
    clickMouse(s, onTrack.x, onTrack.y);
    renderFrames(s, 2);

    const bool slidingBeforeHide=target->isSliding();

    control->setHidden(true);
    renderFrames(s, 1);

    const size_t hideFrameAnims=s.canvas->getUpdateStats().animsRun;

    renderFrames(s, 1);

    const size_t nextAnims=s.canvas->getUpdateStats().animsRun;

    checks.check("hidden_mid_slide", slidingBeforeHide && !target->isSliding() && nextAnims == 0
        && target->getThumbPosition() == 1.0f,
        "sliding before the hide: "+std::string(slidingBeforeHide?"yes":"no")+"; after it "
        +(target->isSliding()?"still sliding":"not sliding")+", thumb at "+std::to_string(target->getThumbPosition())
        +", the hide frame ran "+std::to_string(hideFrameAnims)+" and the next "+std::to_string(nextAnims)
        +" animations");
    control->setHidden(false);
    renderFrames(s, 1);

    forgetMouse(s);
    renderFrames(s, 1);
    std::vector<uint8_t> last;
    idleWork+=settleCodeEditor(s, last);
    checks.check("settled", idleWork == 0 && s.canvas->getUpdateStats().animsRun == 0,
        "settled frames wrote or changed "+std::to_string(idleWork));

    dropControl(s, control);
    renderFrames(s, 1);

    const size_t idsLeft=livePrimitiveIds()+liveTextIds()-idsBefore;

    checks.check("ids_back", idsLeft == 0, std::to_string(idsLeft)+" ids not given back");

    passed=checks.allPassed();
    std::cout<<"switch";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second?"yes":"no");
    std::cout<<" hover_prims="<<hover.primitives.size()<<" slide_frames="<<clickOn.frames
             <<" thumb_frames="<<clickOn.thumbFrames<<" hide_frame_anims="<<hideFrameAnims<<"\n";
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario range-slider: RangeSliderDemo's sliders, the clock frozen
//
// A RangeSlider's thumbs are drawn, and hit, at their rects (getThumbRect),
// their controls never moved, so a drag frame writes the dragged thumb's slot,
// the fill's and its text's glyphs alone and lays nothing out; a thumb's
// position is rounded from the value read off where the mouse put it, so a
// drag step moves it by exactly the mouse's step until it stops at the other
// thumb less (plus) the gap or at the range's end.
//-----------------------------------------------------------------------------

namespace
{

// The ids a frame of `thumb`'s drag or key may write: its slot, the fill's
// (the slider's second) and its value text's glyphs.
struct RangeThumbIds
{
    std::set<size_t> primitives;
    std::set<size_t> texts;
};

RangeThumbIds rangeThumbIds(gweni::controls::RangeSlider *slider, int thumb)
{
    RangeThumbIds ids;
    std::set<size_t> unused;

    ids.primitives.insert(slider->getThumb(thumb)->getSkinControl()->getPrimitives().at(0));
    ids.primitives.insert(slider->getSkinControl()->getPrimitives().at(1));
    if(gweni::controls::internal::Text *text=slider->getValueTextControl(thumb))
        subtreeIds(text, unused, ids.texts);
    return ids;
}

int rangeAlong(gweni::controls::RangeSlider *slider, int thumb)
{
    const gweni::Rect r=slider->getThumbRect(thumb);

    return slider->isHorizontal()?r.x:r.y;
}

gweni::Point rangeThumbCentre(gweni::controls::RangeSlider *slider, int thumb)
{
    const gweni::Rect r=slider->getThumbRect(thumb);

    return slider->localPosToCanvas(gweni::Point(r.x+r.w/2, r.y+r.h/2));
}

// The width of `thumb`'s value text, 0 with none.
int textWidth(gweni::controls::RangeSlider *slider, int thumb)
{
    gweni::controls::internal::Text *text=slider->getValueTextControl(thumb);

    return text?text->getWidth():0;
}

// A layout pass for a text of a new width: the canvas, the demo, the slider and
// its texts, each measured and arranged in both axes.
constexpr size_t RangeResizeEntries=24;

// One frame of a drag or a key: what it did against what it may do.
struct RangeStep
{
    std::string failures;
    size_t glyphFramesKept=0;   // frames keeping the string, writing no glyph
    size_t frames=0;
};

// Checks the frame after an input that moved `thumb` (or meant to, by `step`
// units along the axis): its ids, its layout, the pixels outside the slider,
// the controls it changed, and the thumb at its old place plus the step, or at
// the limit it stopped at.
void checkRangeFrame(Session &s, gweni::controls::RangeSlider *slider, int thumb, int step,
    int before, const gweni::String &textBefore, int widthBefore, const std::vector<uint8_t> &previous,
    const std::string &what, RangeStep &run, std::vector<uint8_t> &pixels)
{
    const RangeThumbIds ids=rangeThumbIds(slider, thumb);
    const CodeEditorFrame frame=codeEditorFrame(s);
    const int at=rangeAlong(slider, thumb);
    const double value=slider->getValue(thumb);
    const double lower=slider->getLowerLimit(thumb);
    const double upper=slider->getUpperLimit(thumb);
    const bool atLimit=(at == slider->positionOf(lower)) || (at == slider->positionOf(upper));
    const gweni::String text=slider->getValueText(thumb);
    const size_t outside=differencesOutsideRects(s, previous, frame.pixels, {slider->getCanvasBounds()});
    std::string bad;

    ++run.frames;
    if(!idsWithin(frame.primitives, ids.primitives) || !idsWithin(frame.texts, ids.texts))
        bad+=" wrote "+std::to_string(frame.primitives.size())+" primitive and "
            +std::to_string(frame.texts.size())+" text ids outside the thumb, the fill and its text;";
    if(text == textBefore && !frame.texts.empty())
        bad+=" kept \""+text+"\" and wrote "+std::to_string(frame.glyphSlots)+" glyph slots;";
    else if(text == textBefore)
        ++run.glyphFramesKept;
    // a string of another width is laid out again (the text's size changed):
    // one pass, entering no more than the way down to the slider and its texts
    const bool resized=(text.size() != textBefore.size()) || (textWidth(slider, thumb) != widthBefore);

    if(frame.passes != 0 && !(resized && frame.passes == 1 && frame.visited <= RangeResizeEntries))
        bad+=" ran "+std::to_string(frame.passes)+" layout passes entering "+std::to_string(frame.visited)
            +(resized?"":", its strings' widths the same")+";";
    if(!resized && frame.changed > 3)
        bad+=" changed "+std::to_string(frame.changed)+" controls (the thumb, the slider and its text wanted);";
    if(outside != 0)
        bad+=" changed "+std::to_string(outside)+" pixels outside the slider;";
    // a thumb short of both limits moved by exactly the step
    if(at != before+step && !(atLimit && (value == lower || value == upper)))
        bad+=" moved the thumb from "+std::to_string(before)+" to "+std::to_string(at)+" for a step of "
            +std::to_string(step)+";";
    if(!bad.empty())
        run.failures+=" "+what+":"+bad;
    pixels=frame.pixels;
}

// Drags `thumb` from its centre by `dir` (+1 or -1 along the axis, in canvas
// units): 12 one-unit steps along the track, 2 steps of a tenth of its length
// along it and 2 more off it (past its far side), then `last` units more,
// each a frame (`last` past the other thumb, to stop it at the gap); then the
// release where the mouse is.
RangeStep dragRangeThumb(Session &s, gweni::controls::RangeSlider *slider, int thumb, int dir, int last,
    std::vector<uint8_t> &previous, bool &hoveredWhileHeld, bool &releaseUnhovered)
{
    RangeStep run;
    const bool horizontal=slider->isHorizontal();
    const gweni::Point grab=rangeThumbCentre(slider, thumb);
    const gweni::Rect bounds=slider->getCanvasBounds();
    const int length=horizontal?bounds.w:bounds.h;
    const int coarse=std::max(1, length/10);

    moveMouse(s, grab.x, grab.y);
    renderFrames(s, 1);
    mouseDown(s, grab.x, grab.y, true);
    previous=codeEditorFrame(s).pixels;

    gweni::Point mouse=grab;

    for(int i=1; i <= 17; ++i)
    {
        const int step=dir*(i <= 12?1:(i <= 16?coarse:last));
        const bool offTrack=(i > 14);
        const int before=rangeAlong(slider, thumb);
        const gweni::String textBefore=slider->getValueText(thumb);
        const int widthBefore=textWidth(slider, thumb);

        if(horizontal)
        {
            mouse.x+=step;
            mouse.y=offTrack?bounds.y+bounds.h+15:grab.y;
        }
        else
        {
            mouse.y+=step;
            mouse.x=offTrack?bounds.x+bounds.w+15:grab.x;
        }
        moveMouse(s, mouse.x, mouse.y);
        checkRangeFrame(s, slider, thumb, step, before, textBefore, widthBefore, previous,
            "step "+std::to_string(i), run, previous);
    }

    hoveredWhileHeld=slider->getThumb(thumb)->isHovered();
    mouseUp(s, mouse.x, mouse.y);
    renderFrames(s, 1);
    releaseUnhovered=!slider->getThumb(thumb)->isHovered() && gweni::MouseFocus == nullptr
        && slider->getDraggedThumb() == -1;
    return run;
}

std::string rangeReport(const RangeStep &run)
{
    return std::to_string(run.frames)+" frames, "+std::to_string(run.glyphFramesKept)
        +" keeping the string;"+run.failures;
}

}//namespace

static bool runRangeSliderScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    InteractionChecks checks;
    checks.scenario="range-slider";

    const size_t idsBefore=livePrimitiveIds()+liveTextIds();
    gweni::controls::Base *control=addControl(s, "RangeSliderDemo");
    auto *demo=gweni::gweni_cast<gweni::RangeSliderDemo>(control);

    if(!demo || !demo->price || !demo->vertical || !demo->joined)
    {
        std::cerr<<"ControlTest: range-slider: RangeSliderDemo was not built\n";
        return false;
    }

    s.label="range-slider";
    renderFrames(s, s.frames);

    const gweni::Point off(s.size.x-4, s.size.y-4);

    moveMouse(s, off.x, off.y);
    renderFrames(s, 1);

    std::vector<uint8_t> rest;
    size_t idleWork=settleCodeEditor(s, rest);
    bool ok=shoot(s, shotDir+"/range-slider.png");

    checks.check("settled_start", idleWork == 0, "the demo's first settled frames wrote or changed "
        +std::to_string(idleWork));

    gweni::controls::RangeSlider *price=demo->price;

    // (1) a hover writes the hovered thumb's slot alone; off gives it back
    {
        const size_t slot=price->getThumb(0)->getSkinControl()->getPrimitives().at(0);
        const gweni::Point at=rangeThumbCentre(price, 0);

        moveMouse(s, at.x, at.y);

        const CodeEditorFrame hover=codeEditorFrame(s);

        moveMouse(s, off.x, off.y);

        const CodeEditorFrame leave=codeEditorFrame(s);

        checks.check("hover", hover.primitives == std::set<size_t>{slot} && hover.texts.empty()
            && hover.passes == 0 && hover.pixels != rest && leave.pixels == rest,
            "the hover wrote "+std::to_string(hover.primitives.size())+" primitive ids (the thumb's alone wanted) and "
            +std::to_string(hover.texts.size())+" text ids"+(leave.pixels == rest?"":", and moving off did not give the frame back"));
    }

    // (2) drags: each price thumb toward the other, stopping at the gap; the
    // vertical one's low thumb up and its high one down; the joined one's high
    // thumb toward the minimum
    struct Drag
    {
        const char *name;
        gweni::controls::RangeSlider *slider;
        int thumb;
        int dir;
    };

    const Drag drags[]=
    {
        {"price_low", price, 0, 1},
        {"price_high", price, 1, -1},
        {"vertical_low", demo->vertical, 0, -1},
        {"vertical_high", demo->vertical, 1, 1},
        {"joined_high", demo->joined, 1, -1},
    };
    size_t keptFrames=0;
    std::vector<uint8_t> previous;

    for(const Drag &drag : drags)
    {
        // each from the demo's values, so a drag that ended on the other
        // thumb leaves the next one its own thumbs apart
        const double startLow=drag.slider->getLow(), startHigh=drag.slider->getHigh();
        bool hoveredWhileHeld=false, releaseUnhovered=false;
        const RangeStep run=dragRangeThumb(s, drag.slider, drag.thumb, drag.dir, 200,
            previous, hoveredWhileHeld, releaseUnhovered);
        gweni::controls::RangeSlider *r=drag.slider;
        // the last step went past the other thumb: it stopped at the gap
        const double gap=r->getHigh()-r->getLow();
        const bool stopped=std::abs(gap-r->getMinimumGap()) < 1e-9;

        keptFrames+=run.glyphFramesKept;
        checks.check(std::string("drag_")+drag.name, run.failures.empty() && hoveredWhileHeld,
            rangeReport(run)+(hoveredWhileHeld?"":" the held thumb was not hovered"));
        checks.check(std::string("gap_")+drag.name, stopped,
            "the thumbs ended "+std::to_string(gap)+" apart, the gap is "+std::to_string(r->getMinimumGap()));
        checks.check(std::string("release_")+drag.name, releaseUnhovered,
            "the thumb let go off itself kept the hover, the mouse or the drag");
        moveMouse(s, off.x, off.y);
        renderFrames(s, 1);
        idleWork+=settleCodeEditor(s, previous);
        if(drag.thumb == 0 && drag.slider == price)
            ok=shoot(s, shotDir+"/range-slider-gap.png") && ok;
        drag.slider->setValues(startLow, startHigh);
        renderFrames(s, 2);
    }
    checks.check("glyphs_kept", keptFrames > 0,
        "no drag frame kept its string, so none showed a frame writing no glyph");

    // (3) keys step the focused thumb the same way: Right, Left, PageUp,
    // PageDown, Home, End on the price's low thumb, then Tab to the high one
    price->setValues(200, 800);
    price->getThumb(0)->focus();
    renderFrames(s, 1);

    std::vector<uint8_t> focused;

    idleWork+=settleCodeEditor(s, focused);

    RangeStep keys;
    const double keyStep=price->getKeyStep();
    struct Key
    {
        int key;
        const char *name;
        double expect;  // the low value after it
    };
    const Key keyList[]=
    {
        {GLFW_KEY_RIGHT, "right", 200+keyStep},
        {GLFW_KEY_LEFT, "left", 200},
        {GLFW_KEY_UP, "up", 200+keyStep},
        {GLFW_KEY_DOWN, "down", 200},
        {GLFW_KEY_PAGE_UP, "pageup", 200+10*keyStep},
        {GLFW_KEY_PAGE_DOWN, "pagedown", 200},
        {GLFW_KEY_END, "end", 800-price->getMinimumGap()},
        {GLFW_KEY_HOME, "home", 0},
    };
    std::string keyValues;
    std::vector<uint8_t> keyPixels=focused;

    for(const Key &key : keyList)
    {
        const int before=rangeAlong(price, 0);
        const gweni::String textBefore=price->getValueText(0);
        const int widthBefore=textWidth(price, 0);
        const int step=price->positionOf(key.expect)-before;

        glfwKey(s, key.key, GLFW_PRESS, 0);
        glfwKey(s, key.key, GLFW_RELEASE, 0);
        checkRangeFrame(s, price, 0, step, before, textBefore, widthBefore, keyPixels, key.name, keys, keyPixels);
        if(std::abs(price->getLow()-key.expect) > 1e-9)
            keyValues+=" "+std::string(key.name)+": low "+std::to_string(price->getLow())+", "
                +std::to_string(key.expect)+" wanted;";
    }
    checks.check("keys", keys.failures.empty() && keyValues.empty() && price->getHigh() == 800,
        rangeReport(keys)+keyValues);

    glfwKey(s, GLFW_KEY_TAB, GLFW_PRESS, 0);
    glfwKey(s, GLFW_KEY_TAB, GLFW_RELEASE, 0);
    renderFrames(s, 1);

    const bool tabbed=gweni::KeyboardFocus == price->getThumb(1) && price->getFrontThumb() == 1;

    glfwKey(s, GLFW_KEY_HOME, GLFW_PRESS, 0);
    glfwKey(s, GLFW_KEY_HOME, GLFW_RELEASE, 0);
    renderFrames(s, 1);
    checks.check("tab_high", tabbed && price->getHigh() == price->getLow()+price->getMinimumGap(),
        std::string(tabbed?"":"Tab did not focus the high thumb, in front; ")+"Home put the high value at "
        +std::to_string(price->getHigh()));

    // (4) settled, and the ids back
    price->getThumb(1)->unfocus();
    renderFrames(s, 1);
    forgetMouse(s);
    renderFrames(s, 2);

    std::vector<uint8_t> last;

    idleWork+=settleCodeEditor(s, last);
    checks.check("settled", idleWork == 0, "settled frames wrote or changed "+std::to_string(idleWork));

    dropControl(s, control);
    renderFrames(s, 1);

    const size_t idsLeft=livePrimitiveIds()+liveTextIds()-idsBefore;

    checks.check("ids_back", idsLeft == 0, std::to_string(idsLeft)+" ids not given back");

    passed=checks.allPassed();
    std::cout<<"range-slider";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second?"yes":"no");
    std::cout<<" glyph_kept_frames="<<keptFrames<<"\n";
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario segmented: SegmentedButtonsDemo's rows, the clock frozen
//
// A segment's state is drawn by its own slot alone: a joint between two
// segments is its left one's border column (the right one is drawn from a
// Joint in, no border on its left), so a hover writes the hovered segment's
// slot, an exclusive selection the two segments it leaves and reaches (their
// fills and their glyphs' colours), a multi toggle its one segment, and none
// of them lays anything out (the check's room is kept in every segment of a
// row that shows checks). The demo's selection label is the app's reply to
// onSelectionChange: its new text is laid out, so the cost checks are made
// with its updates off and the label is checked on its own at the end. The
// mouse moves off the canvas between the inputs, so no press leaves the page
// behind the rows hovered (its own repaint).
//-----------------------------------------------------------------------------

namespace
{

// The device-pixel runs of `border` (each channel within 2; the segment
// border's colour by default) on device row `y` from x0 up to x1: their
// starts and lengths.
std::vector<std::pair<int, int>> borderRuns(const Session &s, const std::vector<uint8_t> &pixels, int y,
    int x0, int x1, const gweni::Color &border=gweni::skin::textured::tokyonight::Segment.border)
{
    std::vector<std::pair<int, int>> runs;

    if(y < 0 || y >= s.pixels.y)
        return runs;
    for(int x=std::max(x0, 0); x < std::min(x1, s.pixels.x); ++x)
    {
        const size_t at=(static_cast<size_t>(y)*s.pixels.x+x)*4;
        const bool match=std::abs(int(pixels[at])-int(border.r)) <= 2 && std::abs(int(pixels[at+1])-int(border.g)) <= 2
            && std::abs(int(pixels[at+2])-int(border.b)) <= 2;

        if(!match)
            continue;
        if(!runs.empty() && runs.back().first+runs.back().second == x)
            ++runs.back().second;
        else
            runs.push_back({x, 1});
    }
    return runs;
}

}//namespace

static bool runSegmentedScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    InteractionChecks checks;
    checks.scenario="segmented";

    const size_t idsBefore=livePrimitiveIds()+liveTextIds();
    gweni::controls::Base *control=addControl(s, "SegmentedButtonsDemo");
    auto *demo=gweni::gweni_cast<gweni::SegmentedButtonsDemo>(control);

    if(!demo)
    {
        std::cerr<<"ControlTest: segmented: SegmentedButtonsDemo was not built\n";
        return false;
    }

    s.label="segmented";
    renderFrames(s, s.frames);

    bool ok=shoot(s, shotDir+"/segmented.png");
    std::vector<uint8_t> pixels;
    size_t idleWork=settleCodeEditor(s, pixels);
    gweni::controls::SegmentedButtons *row=demo->textRow();
    auto idsOf=[](std::initializer_list<gweni::controls::Base *> controls, std::set<size_t> &primitives,
        std::set<size_t> &texts) {
        for(gweni::controls::Base *part : controls)
            subtreeIds(part, primitives, texts);
    };

    // (1) every inner joint of the view row is one border column, as wide as
    // the row's outer left border: the joints just under the top border
    // (above the text), the outer one at mid-height (clear of its corner)
    const int borderPx=std::max(1, gweni::renderer::Base::snap(gweni::skin::textured::tokyonight::Segment.borderWidth,
        s.scale));
    bool jointsOk=true;
    std::string joints;

    {
        const gweni::Rect first=row->getSegment(0)->getCanvasBounds();
        const int y=devicePixels(gweni::Point(0, first.y), s.scale).y+borderPx+1;
        const int left=devicePixels(gweni::Point(first.x, 0), s.scale).x;
        // the outer border at mid-height, clear of the rounded corner
        const auto outer=borderRuns(s, pixels, devicePixels(gweni::Point(0, first.y+first.h/2), s.scale).y,
            left-2, left+4*borderPx);

        jointsOk=(outer.size() == 1) && (outer[0].second == borderPx);
        joints="outer="+std::to_string(outer.empty()?0:outer[0].second);
        for(int i=0; i+1 < row->segmentCount(); ++i)
        {
            const gweni::Rect a=row->getSegment(i)->getCanvasBounds();
            const gweni::Rect b=row->getSegment(i+1)->getCanvasBounds();
            const auto runs=borderRuns(s, pixels, y, devicePixels(gweni::Point(a.x+a.w/2, 0), s.scale).x,
                devicePixels(gweni::Point(b.x+b.w/2, 0), s.scale).x);

            joints+=" "+std::to_string(i)+"|"+std::to_string(i+1)+"=";
            for(size_t r=0; r < runs.size(); ++r)
                joints+=(r?",":"")+std::to_string(runs[r].second);
            if(runs.size() != 1 || runs[0].second != borderPx)
                jointsOk=false;
        }
    }
    checks.check("joints", jointsOk, "an inner joint is not one border column of "+std::to_string(borderPx)
        +" device px: "+joints);

    // (1b) each segment after the first focused in turn: its left joint is
    // one run of the focus border as wide as the outer border, with none of
    // the neighbour's border left beside it, whichever device px the 1-unit
    // joint lands on (at 1.5 it is 1 or 2 by its position; Day|Week is the
    // odd one there); its right joint the same
    bool focusJointsOk=true;
    std::string focusJoints;

    {
        const gweni::Color &focus=gweni::skin::textured::tokyonight::SegmentFocus.border;
        const gweni::Rect first=row->getSegment(0)->getCanvasBounds();
        const int y=devicePixels(gweni::Point(0, first.y), s.scale).y+borderPx+1;
        auto middle=[&](int i) {
            const gweni::Rect bounds=row->getSegment(i)->getCanvasBounds();

            return devicePixels(gweni::Point(bounds.x+bounds.w/2, 0), s.scale).x;
        };
        auto jointOk=[&](const std::vector<uint8_t> &shown, int i, std::string &what) {
            const auto line=borderRuns(s, shown, y, middle(i), middle(i+1));
            const auto blue=borderRuns(s, shown, y, middle(i), middle(i+1), focus);

            what+=std::to_string(i)+"|"+std::to_string(i+1)+"=";
            for(size_t r=0; r < blue.size(); ++r)
                what+=(r?",":"")+std::to_string(blue[r].second);
            what+="/"+std::to_string(line.size());
            return line.empty() && (blue.size() == 1) && (blue[0].second == borderPx);
        };

        for(int i=1; i < row->segmentCount(); ++i)
        {
            std::vector<uint8_t> shown;

            row->getSegment(i)->focus();
            renderFrames(s, 1);
            s.renderer->readPixels(s.pixels.x, s.pixels.y, shown);
            focusJoints+=" "+std::to_string(i)+":";
            if(!jointOk(shown, i-1, focusJoints))
                focusJointsOk=false;
            if(i+1 < row->segmentCount())
            {
                focusJoints+=",";
                if(!jointOk(shown, i, focusJoints))
                    focusJointsOk=false;
            }
        }
        row->getSegment(row->segmentCount()-1)->unfocus();
        renderFrames(s, 1);

        std::vector<uint8_t> back;

        s.renderer->readPixels(s.pixels.x, s.pixels.y, back);
        if(back != pixels)
        {
            focusJointsOk=false;
            focusJoints+=" unfocused_differs";
        }
    }
    checks.check("focus_joints", focusJointsOk, "a focused segment's joint is not one focus-border column of "
        +std::to_string(borderPx)+" device px (blue runs/line runs):"+focusJoints);

    demo->setLabelUpdates(false);

    // (2) a hover on an unselected segment: its slot alone, no glyph, no pixel
    // outside it, and moving off gives the frame back
    gweni::controls::SegmentedButton *hovered=row->getSegment(2);
    std::set<size_t> hoverPrims, hoverTexts;

    idsOf({hovered}, hoverPrims, hoverTexts);

    const gweni::Rect hoverRect=hovered->getCanvasBounds();

    moveMouse(s, hoverRect.x+hoverRect.w/2, hoverRect.y+hoverRect.h/2);

    const CodeEditorFrame hover=codeEditorFrame(s);
    const size_t hoverOutside=differencesOutsideRects(s, pixels, hover.pixels, {hoverRect});

    forgetMouse(s);

    const CodeEditorFrame off=codeEditorFrame(s);

    checks.check("hover",
        hover.primitives.size() == 1 && idsWithin(hover.primitives, hoverPrims) && hover.texts.empty()
            && hover.changed <= subtreeControls(hovered) && hover.passes == 0 && hoverOutside == 0 && off.pixels == pixels,
        "a hover wrote "+std::to_string(hover.primitives.size())+" primitive ids (the segment's one allowed) and "
        +std::to_string(hover.texts.size())+" text ids, changed "+std::to_string(hover.changed)
        +" controls (its "+std::to_string(subtreeControls(hovered))+" allowed), ran "+std::to_string(hover.passes)+" layout passes, changed "
        +std::to_string(hoverOutside)+" pixels outside it, or moving off did not give the frame back");
    idleWork+=settleCodeEditor(s, pixels);

    // (3) an exclusive click from Week to Month: the two segments' slots and
    // glyphs alone, no layout, no pixel outside them
    const int selectedBefore=row->getSelectedIndex();
    gweni::controls::SegmentedButton *from=row->getSegment(selectedBefore);
    std::set<size_t> pairPrims, pairTexts;

    idsOf({from, hovered}, pairPrims, pairTexts);
    // the focus on the selection, as Tab leaves it: the press moves it along
    from->focus();
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);
    clickMouse(s, hoverRect.x+hoverRect.w/2, hoverRect.y+hoverRect.h/2);

    // a focused segment after the first paints its left joint, the left
    // neighbour's border column, in the focus colour: at a fractional scale
    // part of that column lies outside its bounds, so the focus moving on is
    // allowed there too (a Joint wider on the left)
    auto withJoint=[](gweni::controls::SegmentedButton *segment) {
        gweni::Rect bounds=segment->getCanvasBounds();
        gweni::controls::SegmentedButtons *owner=segment->getSegmentedButtons();

        if(owner && owner->indexOf(segment) > 0)
        {
            bounds.x-=gweni::controls::SegmentedButtons::Joint;
            bounds.w+=gweni::controls::SegmentedButtons::Joint;
        }
        return bounds;
    };
    const CodeEditorFrame click=codeEditorFrame(s);
    const size_t clickOutside=differencesOutsideRects(s, pixels, click.pixels,
        {withJoint(from), withJoint(hovered)});

    checks.check("exclusive_click",
        selectedBefore == 1 && row->getSelectedIndex() == 2 && !click.primitives.empty()
            && idsWithin(click.primitives, pairPrims) && idsWithin(click.texts, pairTexts) && click.passes == 0
            && click.changed <= subtreeControls(from)+subtreeControls(hovered)
            && clickOutside == 0,
        "the click selected "+std::to_string(row->getSelectedIndex())+" (2 expected), wrote "
        +std::to_string(click.primitives.size())+" primitive and "+std::to_string(click.texts.size())
        +" text ids (the two segments' allowed), changed "+std::to_string(click.changed)+" controls, ran "+std::to_string(click.passes)+" layout passes and changed "
        +std::to_string(clickOutside)+" pixels outside them");
    forgetMouse(s);
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);

    // (4) Right then Left: the focus and the selection to Year and back, the
    // two segments alone each time, and Left gives the frame before Right back
    gweni::controls::SegmentedButton *next=row->getSegment(3);
    std::set<size_t> keyPrims, keyTexts;

    idsOf({hovered, next}, keyPrims, keyTexts);

    const std::vector<gweni::Rect> keyRects={withJoint(hovered), withJoint(next)};

    sendKey(s, "right");

    const CodeEditorFrame right=codeEditorFrame(s);
    const size_t rightOutside=differencesOutsideRects(s, pixels, right.pixels, keyRects);
    const int afterRight=row->getSelectedIndex();

    sendKey(s, "left");

    const CodeEditorFrame left=codeEditorFrame(s);

    checks.check("keys",
        afterRight == 3 && row->getSelectedIndex() == 2 && !right.primitives.empty()
            && idsWithin(right.primitives, keyPrims) && idsWithin(right.texts, keyTexts)
            && idsWithin(left.primitives, keyPrims) && idsWithin(left.texts, keyTexts)
            && right.passes == 0 && left.passes == 0 && rightOutside == 0 && left.pixels == pixels
            && right.changed <= subtreeControls(hovered)+subtreeControls(next)
            && left.changed <= subtreeControls(hovered)+subtreeControls(next),
        "Right selected "+std::to_string(afterRight)+" and Left "+std::to_string(row->getSelectedIndex())
        +" (3 and 2 expected), wrote "+std::to_string(right.primitives.size())+"+"+std::to_string(left.primitives.size())
        +" primitive ids (the two segments' allowed), changed "+std::to_string(right.changed)+"+"
        +std::to_string(left.changed)+" controls, ran "+std::to_string(right.passes+left.passes)
        +" layout passes, changed "+std::to_string(rightOutside)
        +" pixels outside the two, or Left did not give the frame before Right back");
    idleWork+=settleCodeEditor(s, pixels);

    // (5) a multi toggle by Space and by a click: the one segment, and a
    // second toggle gives the first frame back
    gweni::controls::SegmentedButtons *multi=demo->multiRow();
    gweni::controls::SegmentedButton *toggled=multi->getSegment(2);
    std::set<size_t> togglePrims, toggleTexts;

    idsOf({toggled}, togglePrims, toggleTexts);
    toggled->focus();
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);

    const gweni::Rect toggleRect=toggled->getCanvasBounds();
    const std::vector<int> selectedMulti=multi->getSelectedIndices();

    sendKey(s, "space");

    const CodeEditorFrame on=codeEditorFrame(s);
    const size_t onOutside=differencesOutsideRects(s, pixels, on.pixels, {toggleRect});
    const bool isOn=multi->isSelected(2);

    sendKey(s, "space");

    const CodeEditorFrame back=codeEditorFrame(s);

    clickMouse(s, toggleRect.x+toggleRect.w/2, toggleRect.y+toggleRect.h/2);

    const CodeEditorFrame clicked=codeEditorFrame(s);
    const bool clickedOn=multi->isSelected(2);

    clickMouse(s, toggleRect.x+toggleRect.w/2, toggleRect.y+toggleRect.h/2);

    const CodeEditorFrame clickedBack=codeEditorFrame(s);

    forgetMouse(s);

    const CodeEditorFrame offAgain=codeEditorFrame(s);

    checks.check("multi_toggle",
        isOn && clickedOn && multi->getSelectedIndices() == selectedMulti && !on.primitives.empty()
            && idsWithin(on.primitives, togglePrims) && idsWithin(on.texts, toggleTexts)
            && idsWithin(back.primitives, togglePrims) && idsWithin(back.texts, toggleTexts)
            && idsWithin(clicked.primitives, togglePrims) && idsWithin(clicked.texts, toggleTexts)
            && idsWithin(clickedBack.primitives, togglePrims) && idsWithin(clickedBack.texts, toggleTexts)
            && on.passes+back.passes+clicked.passes+clickedBack.passes == 0 && onOutside == 0
            && std::max({on.changed, back.changed, clicked.changed, clickedBack.changed}) <= subtreeControls(toggled)
            && back.pixels == pixels && offAgain.pixels == pixels,
        "a toggle did not turn Underline on and off, wrote outside its segment's ids or changed controls outside it, ran "
        +std::to_string(on.passes+back.passes+clicked.passes+clickedBack.passes)+" layout passes, changed "
        +std::to_string(onOutside)+" pixels outside it, or a second toggle did not give the first frame back");
    idleWork+=settleCodeEditor(s, pixels);

    // (6) the label names the selection once it follows it again
    demo->setLabelUpdates(true);
    renderFrames(s, 1);

    const gweni::Rect day=row->getSegment(0)->getCanvasBounds();

    clickMouse(s, day.x+day.w/2, day.y+day.h/2);
    renderFrames(s, 1);

    const std::string label=demo->selectionLabel()->getText();

    checks.check("label", label.find("View: Day") != std::string::npos,
        "the selection label says \""+label+"\"");

    forgetMouse(s);
    if(gweni::KeyboardFocus)
        gweni::KeyboardFocus->unfocus();
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);
    checks.check("settled", idleWork == 0, "settled frames wrote or changed "+std::to_string(idleWork));

    dropControl(s, control);
    renderFrames(s, 1);

    const size_t idsLeft=livePrimitiveIds()+liveTextIds()-idsBefore;

    checks.check("ids_back", idsLeft == 0, std::to_string(idsLeft)+" ids not given back");

    passed=checks.allPassed();
    std::cout<<"segmented";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second?"yes":"no");
    std::cout<<" border_px="<<borderPx<<" "<<joints<<" focus"<<focusJoints<<" hover_prims="<<hover.primitives.size()
             <<" hover_changed="<<hover.changed<<" click_changed="<<click.changed
             <<" click_prims="<<click.primitives.size()<<" click_texts="<<click.texts.size()
             <<" toggle_prims="<<on.primitives.size()<<"\n";
    return ok;
}

//-----------------------------------------------------------------------------
// NavigationRailDemo: app callbacks first, then the rail alone. Updating a
// page is the app's work, so detach its callback before bounding selection to
// two rows and their glyphs. Counts are frozen; only tooltip waits step time.
//-----------------------------------------------------------------------------
static bool runNavigationRailScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;
    if(!resizeSession(s, 640, 480))
        return false;
    InteractionChecks checks;
    checks.scenario="navigation-rail";
    auto *control=addControl(s, "NavigationRailDemo");
    auto *demo=gweni::gweni_cast<gweni::NavigationRailDemo>(control);
    if(!demo || demo->items().size() != 6)
        return false;
    auto *rail=demo->rail();
    auto *home=demo->items()[0];
    auto *inbox=demo->items()[1];
    auto *activity=demo->items()[2];
    auto *footer=demo->footerItem();
    auto *badge=inbox->getBadge();
    s.label="navigation-rail";
    renderFrames(s, s.frames);
    bool ok=shoot(s, shotDir+"/navigation-rail.png");

    std::vector<uint8_t> pixels;
    size_t idleWork=0;
    auto settle=[&]()
    {
        const auto before=framePixels(s);
        for(int i=0; i < 2; ++i)
        {
            const auto frame=codeEditorFrame(s);
            const auto counts=s.renderer->frameStats();
            idleWork+=frame.written+frame.changed+frame.visited+frame.passes
                +counts.uploadCalls+s.canvas->getUpdateStats().animsRun;
            if(frame.pixels != before) ++idleWork;
            pixels=frame.pixels;
        }
    };
    auto click=[&](gweni::controls::Base *item)
    {
        const auto r=item->getCanvasBounds();
        clickMouse(s, r.x+r.w/2, r.y+r.h/2);
    };
    auto off=[&]() { moveMouse(s, s.size.x-4, s.size.y-4); };
    auto rowIds=[](gweni::controls::NavigationItem *item,
        std::set<size_t> &primitives, std::set<size_t> &texts)
    {
        std::vector<size_t> p, t;
        item->getSkinControl()->appendIds(p, t);
        primitives.insert(p.begin(), p.end());
        subtreeIds(item->getLabel(), primitives, texts);
    };
    struct Changes: gweni::event::Handler
    {
        int calls=0;
        void changed(gweni::event::Info) { ++calls; }
    } changes;

    // The real demo callback and buttons are exercised before cost isolation.
    click(inbox);
    renderFrames(s, 1);
    checks.check("page_callback", rail->getSelected() == inbox && demo->pageLabel()->getText() == "Inbox",
        "click did not show the selected Inbox page");
    click(demo->countButton());
    renderFrames(s, 1);
    checks.check("count_button", badge->getCount() == 10,
        "New inbox message expected count 10, got "+std::to_string(badge->getCount()));
    click(demo->menuButton());
    renderFrames(s, 1);
    const bool menuCollapsed=rail->isCollapsed();
    click(demo->menuButton());
    renderFrames(s, 1);
    checks.check("menu_button", menuCollapsed && !rail->isCollapsed(),
        "two menu clicks did not collapse then expand");
    rail->onSelectionChange.removeHandler(demo);
    rail->onSelectionChange.add(&changes, &Changes::changed);
    rail->setSelected(home);
    demo->pageLabel()->setText("Home");
    inbox->setBadgeCount(9);
    rail->focus();
    off();
    renderFrames(s, 1);
    changes.calls=0;
    settle();

    std::set<size_t> inboxPrims, inboxTexts, selectedPrims, selectedTexts;
    rowIds(inbox, inboxPrims, inboxTexts);
    rowIds(home, selectedPrims, selectedTexts);
    rowIds(inbox, selectedPrims, selectedTexts);
    const auto homeRect=home->getCanvasBounds();
    const auto inboxRect=inbox->getCanvasBounds();
    moveMouse(s, inboxRect.x+16, inboxRect.y+inboxRect.h/2);
    const auto hover=codeEditorFrame(s);
    off();
    const auto hoverOff=codeEditorFrame(s);
    checks.check("hover", !hover.primitives.empty() && hover.primitives.size() <= inboxPrims.size()
        && idsWithin(hover.primitives, inboxPrims) && hover.texts.empty()
        && hover.changed <= 2 && hover.passes == 0 && hover.visited == 0
        && differencesOutsideRects(s, pixels, hover.pixels, {inboxRect}) == 0
        && differencesIn(s, pixels, hover.pixels, inboxRect) > 0 && hoverOff.pixels == pixels
        && idsWithin(hoverOff.primitives, inboxPrims) && hoverOff.primitives.size() <= inboxPrims.size()
        && hoverOff.texts.empty() && hoverOff.changed <= 2 && hoverOff.passes == 0 && hoverOff.visited == 0,
        "hover wrote "+std::to_string(hover.primitives.size())+" primitives/"
        +std::to_string(hover.glyphSlots)+" glyphs, changed "+std::to_string(hover.changed)
        +" controls, visited "+std::to_string(hover.visited)+" layouts, or pixels escaped/did not return");
    settle();

    // Hover is settled before clicking, so this measures selection alone.
    moveMouse(s, inboxRect.x+16, inboxRect.y+inboxRect.h/2);
    renderFrames(s, 1);
    settle();
    click(inbox);
    std::vector<std::string> clickStrays;
    pendingOutside(s.canvas, {home, inbox}, clickStrays);
    const auto selected=codeEditorFrame(s);
    checks.check("click", rail->getSelected() == inbox && changes.calls == 1
        && selected.primitives.size() <= selectedPrims.size()
        && idsWithin(selected.primitives, selectedPrims) && idsWithin(selected.texts, selectedTexts)
        && selected.glyphSlots <= home->getText().size()+inbox->getText().size()
        // Each row also inherits Button's empty, hidden Text; it follows the
        // row colour but holds no glyphs. All changed controls stay in rows.
        && clickStrays.empty() && selected.changed <= 6 && selected.passes == 0 && selected.visited == 0
        && differencesOutsideRects(s, pixels, selected.pixels, {homeRect, inboxRect}) == 0,
        "click expected Inbox and one event, got "+std::to_string(changes.calls)+" events; wrote "
        +std::to_string(selected.primitives.size())+" primitives/"+std::to_string(selected.glyphSlots)
        +" glyphs, changed "+std::to_string(selected.changed)+" controls, visited "
        +std::to_string(selected.visited)+" layouts, or changed pixels outside the two rows");
    click(inbox);
    const auto repeated=codeEditorFrame(s);
    checks.check("repeat", changes.calls == 1 && repeated.glyphSlots == 0 && repeated.passes == 0,
        "repeat click selected again or changed label glyphs/layout");
    off();
    renderFrames(s, 1);
    settle();

    std::set<size_t> keyboardPrims, keyboardTexts;
    rowIds(inbox, keyboardPrims, keyboardTexts);
    rowIds(activity, keyboardPrims, keyboardTexts);
    sendKey(s, "down");
    const auto down=codeEditorFrame(s);
    checks.check("focus", rail->getFocused() == activity && rail->getSelected() == inbox
        && changes.calls == 1 && idsWithin(down.primitives, keyboardPrims)
        && down.primitives.size() <= keyboardPrims.size() && down.texts.empty()
        && down.changed <= 2 && down.passes == 0 && down.visited == 0,
        "Down selected a page or wrote/laid out beyond the two focus rows");
    settle();
    sendKey(s, "return");
    std::vector<std::string> keyboardStrays;
    pendingOutside(s.canvas, {inbox, activity}, keyboardStrays);
    const auto keyboard=codeEditorFrame(s);
    checks.check("keyboard", rail->getSelected() == activity && changes.calls == 2
        && idsWithin(keyboard.primitives, keyboardPrims) && idsWithin(keyboard.texts, keyboardTexts)
        && keyboard.primitives.size() <= keyboardPrims.size()
        && keyboard.glyphSlots <= inbox->getText().size()+activity->getText().size()
        && keyboardStrays.empty() && keyboard.changed <= 6 && keyboard.passes == 0 && keyboard.visited == 0
        && differencesOutsideRects(s, pixels, keyboard.pixels, {inboxRect, activity->getCanvasBounds()}) == 0,
        "Return expected Activity and two events, got "+std::to_string(changes.calls)+" events; wrote "
        +std::to_string(keyboard.primitives.size())+" primitives/"+std::to_string(keyboard.glyphSlots)
        +" glyphs, changed "+std::to_string(keyboard.changed)+" controls, visited "
        +std::to_string(keyboard.visited)+" layouts, or pixels escaped the two rows");
    settle();
    sendKey(s, "end");
    sendKey(s, "return");
    renderFrames(s, 1);
    const bool footerSelected=rail->getSelected() == footer;
    sendKey(s, "up");
    renderFrames(s, 1);
    checks.check("footer_disabled", footerSelected && footer->getGroup() == home->getGroup()
        && rail->getFocused() == demo->items()[4],
        "End/Return did not select the footer, or Up did not skip disabled Archive");
    rail->setSelected(home);
    demo->countButton()->focus();
    rail->focus();
    renderFrames(s, 1);
    settle();

    std::set<size_t> badgePrims, badgeTexts;
    subtreeIds(badge, badgePrims, badgeTexts);
    auto counts=[&](const char *state)
    {
        inbox->setBadgeCount(8);
        const auto same=codeEditorFrame(s);
        checks.check(std::string(state)+"_badge_same", badge->getCount() == 8
            && !same.texts.empty() && idsWithin(same.primitives, badgePrims)
            && idsWithin(same.texts, badgeTexts) && same.changed == 1 && same.passes == 0
            && same.visited == 0 && differencesOutsideRects(s, pixels, same.pixels, {badge->getCanvasBounds()}) == 0,
            "same-width count wrote outside badge or visited layout");
        inbox->setBadgeCount(9);
        renderFrames(s, 1);
        settle();
        const auto narrow=badge->getCanvasBounds();
        inbox->setBadgeCount(10);
        const auto wider=codeEditorFrame(s);
        checks.check(std::string(state)+"_badge_wider", badge->getCount() == 10
            && badge->getWidth() > narrow.w && idsWithin(wider.primitives, badgePrims)
            && idsWithin(wider.texts, badgeTexts) && wider.changed == 1 && wider.passes == 1
            && wider.visited <= 4*pathLength(badge)
            && differencesOutsideRects(s, pixels, wider.pixels, {narrow, badge->getCanvasBounds()}) == 0,
            "wider count visited "+std::to_string(wider.visited)+" layouts or wrote outside badge");
        inbox->setBadgeCount(9);
        renderFrames(s, 1);
        settle();
    };
    counts("expanded");
    const auto beforeCollapse=pixels;
    rail->setCollapsed(true);
    const auto collapsed=codeEditorFrame(s);
    bool labelsHidden=footer->getLabel()->hidden() && footer->getToolTipText() == footer->getText();
    for(auto *item : demo->items())
        labelsHidden=labelsHidden && item->getLabel()->hidden() && item->getToolTipText() == item->getText();
    checks.check("collapse", labelsHidden
        && rail->getWidth() == gweni::controls::NavigationItem::IconSize+2*gweni::controls::NavigationItem::PaddingX
        && collapsed.passes == 1
        && collapsed.visited <= 4*(pathLength(rail)+subtreeControls(rail)-1)
        && badge->visible() && activity->getBadge()->visible(),
        "collapse labels/tooltips/badges/width wrong, or "+std::to_string(collapsed.passes)
        +" passes visited "+std::to_string(collapsed.visited)+" layouts (rail subtree and parent path allowed)");
    settle();
    counts("collapsed");

    // A stationary mouse waits for the canvas ToolTip; no delay is bypassed.
    const auto collapsedInbox=inbox->getCanvasBounds();
    moveMouse(s, collapsedInbox.x+16, collapsedInbox.y+collapsedInbox.h/2);
    renderFrames(s, 1);
    const bool early=s.canvas->toolTipShown();
    s.freezeClock=false;
    renderFrames(s, int(std::ceil(s.canvas->getToolTipDelay()/g_frameStep))+2);
    s.freezeClock=true;
    auto *tip=s.canvas->toolTipControl();
    checks.check("tooltip", !early && tip && s.canvas->toolTipShown()
        && s.canvas->toolTipOwner() == inbox && tip->getText() == inbox->getText(),
        "stationary mouse did not show Inbox as the canvas tooltip after its delay");
    ok=shoot(s, shotDir+"/navigation-rail-collapsed-tooltip.png") && ok;
    settle();
    off();
    renderFrames(s, 1);
    settle();
    rail->setCollapsed(false);
    const auto expanded=codeEditorFrame(s);
    bool labelsShown=!footer->getLabel()->hidden() && footer->getToolTipText().empty();
    for(auto *item : demo->items())
        labelsShown=labelsShown && !item->getLabel()->hidden() && item->getToolTipText().empty();
    checks.check("expand", labelsShown && rail->getWidth() == gweni::NavigationRailDemo::ExpandedWidth
        && expanded.passes == 1 && expanded.pixels == beforeCollapse,
        "expanded labels/width wrong, or pixels did not return byte for byte");
    settle();
    resizeSession(s, gweni::NavigationRailDemo::CollapseBelow-1, 480);
    const auto narrow=codeEditorFrame(s);
    const bool responsive=rail->isCollapsed();
    resizeSession(s, 640, 480);
    const auto restored=codeEditorFrame(s);
    checks.check("app_width", responsive && !rail->isCollapsed()
        && narrow.passes == 1 && restored.passes == 1 && restored.pixels == beforeCollapse,
        "app width threshold did not collapse/restore the rail in one pass");
    settle();
    checks.check("settled", idleWork == 0, "settled frames did "+std::to_string(idleWork)+" writes/layouts/uploads/animations/pixel changes");
    dropControl(s, control);
    renderFrames(s, 1);
    passed=checks.allPassed();
    std::cout<<"navigation-rail";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second?"yes":"no");
    std::cout<<" hover_prims="<<hover.primitives.size()<<" click_prims="<<selected.primitives.size()
        <<" click_glyphs="<<selected.glyphSlots<<" keyboard_prims="<<keyboard.primitives.size()
        <<" keyboard_glyphs="<<keyboard.glyphSlots<<" collapse_visited="<<collapsed.visited
        <<" collapse_passes="<<collapsed.passes<<" idle_work="<<idleWork<<"\n";
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario spinner: SpinnerDemo's spinners, the clock stepping
//
// A Spinner draws its arc in one slot (and a track ring in another), turned
// by an animation registered only while it spins and is visible(); a step
// marks it only when the arc's slot bits change. So a spinning frame writes
// arc slots alone, lays nothing out and changes no pixel outside the
// spinners; a stopped, hidden or hidden-under-a-hidden-host spinner runs no
// animation from the next frame, and one shown or started again turns from
// where the clock says. The mouse is off the canvas but for the clicks.
//-----------------------------------------------------------------------------

namespace
{

// What a run of spinning frames did.
struct SpinnerRun
{
    size_t frames=0;
    size_t arcFrames=0;         // frames that wrote some arc slot
    bool arcsOnly=true;         // every frame wrote arc slots alone, no text
    bool changedOnly=true;      // and each arc slot written was a spinner whose bits changed
    bool noLayout=true;         // no layout pass
    size_t outside=0;           // pixels changed outside the spinners, from the first frame
    std::set<size_t> anims;     // animsRun of each frame
};

SpinnerRun spinnerFrames(Session &s, const std::vector<gweni::controls::Spinner *> &spinners,
    const std::vector<uint8_t> &reference, int count)
{
    SpinnerRun run;
    std::vector<gweni::Rect> rects;
    std::set<size_t> arcs;

    for(gweni::controls::Spinner *spinner : spinners)
    {
        rects.push_back(spinner->getCanvasBounds());
        arcs.insert(spinner->getSkinControl()->getPrimitives().at(1));
    }

    for(int i=0; i < count; ++i)
    {
        std::vector<uint32_t> bits;

        for(gweni::controls::Spinner *spinner : spinners)
            bits.push_back((uint32_t(spinner->getStartBits())<<16)|spinner->getSweepBits());

        const CodeEditorFrame frame=codeEditorFrame(s);

        ++run.frames;
        run.anims.insert(s.canvas->getUpdateStats().animsRun);
        if(!frame.primitives.empty())
            ++run.arcFrames;
        if(!idsWithin(frame.primitives, arcs) || !frame.texts.empty())
            run.arcsOnly=false;
        for(size_t j=0; j < spinners.size(); ++j)
        {
            const uint32_t now=(uint32_t(spinners[j]->getStartBits())<<16)|spinners[j]->getSweepBits();

            if(frame.primitives.count(spinners[j]->getSkinControl()->getPrimitives().at(1)) && (now == bits[j]))
                run.changedOnly=false;
        }
        if(frame.passes != 0)
            run.noLayout=false;
        run.outside+=differencesOutsideRects(s, reference, frame.pixels, rects);
    }
    return run;
}

std::string spinnerRunReport(const SpinnerRun &run)
{
    std::string anims;

    for(size_t n : run.anims)
        anims+=(anims.empty()?"":",")+std::to_string(n);
    return std::to_string(run.frames)+" frames, "+std::to_string(run.arcFrames)+" writing arcs; arc slots alone: "
        +(run.arcsOnly?"yes":"no")+", only changed arcs: "+(run.changedOnly?"yes":"no")+", no layout: "
        +(run.noLayout?"yes":"no")+"; "+std::to_string(run.outside)+" pixels changed outside the spinners; anims_run "
        +anims;
}

bool spinnerRunGood(const SpinnerRun &run, size_t anims)
{
    return run.arcsOnly && run.changedOnly && run.noLayout && run.outside == 0 && run.arcFrames > 0
        && run.anims == std::set<size_t>{anims};
}

gweni::Point controlCentre(gweni::controls::Base *control)
{
    const gweni::Rect r=control->getCanvasBounds();

    return gweni::Point(r.x+r.w/2, r.y+r.h/2);
}

// The arc of the last frame drawn: renderFrames() steps the clock after it.
bool spinnerAtClock(gweni::controls::Spinner *spinner)
{
    const gweni::controls::Spinner::Arc arc=gweni::controls::Spinner::arcAt(
        double(g_clockSteps-1)*g_frameStep, spinner->getSpeed());

    return spinner->getStartBits() == gweni::renderer::arcStartBits(float(arc.start))
        && spinner->getSweepBits() == gweni::renderer::arcSweepBits(float(arc.sweep));
}

}//namespace

static bool runSpinnerScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=false;
    s.statsClockSteps=true;

    InteractionChecks checks;
    checks.scenario="spinner";

    const size_t idsBefore=livePrimitiveIds()+liveTextIds();
    gweni::controls::Base *control=addControl(s, "SpinnerDemo");
    auto *demo=gweni::gweni_cast<gweni::SpinnerDemo>(control);

    if(!demo || demo->spinners().empty() || !demo->cardSpinner() || !demo->hideTarget())
    {
        std::cerr<<"ControlTest: spinner: SpinnerDemo was not built\n";
        return false;
    }

    s.label="spinner";
    renderFrames(s, s.frames);

    const std::vector<gweni::controls::Spinner *> &all=demo->spinners();
    const size_t count=all.size();
    gweni::controls::Spinner *card=demo->cardSpinner();
    gweni::controls::Spinner *target=demo->hideTarget();
    std::vector<gweni::controls::Spinner *> others;

    for(gweni::controls::Spinner *spinner : all)
    {
        if(spinner != card)
            others.push_back(spinner);
    }

    // (1) every spinner spinning: arc slots alone, only those whose bits moved
    std::vector<uint8_t> reference=framePixels(s);
    bool ok=shoot(s, shotDir+"/spinner.png");
    const SpinnerRun spinning=spinnerFrames(s, all, reference, 20);

    checks.check("spinning", spinnerRunGood(spinning, count), spinnerRunReport(spinning)
        +" ("+std::to_string(count)+" wanted)");

    // (2) Stop: the card's spinner stands still as its ring, writes nothing more
    const gweni::Point off(s.size.x-4, s.size.y-4);
    const gweni::Point stop=controlCentre(demo->stopButton());

    clickMouse(s, stop.x, stop.y);
    moveMouse(s, off.x, off.y);
    renderFrames(s, 2);     // the press, the release and the leave drawn

    const bool stopped=!card->isSpinning() && !card->isTurning() && card->visible();
    std::set<size_t> cardPrims, cardTexts;

    subtreeIds(card, cardPrims, cardTexts);

    std::vector<uint8_t> stoppedLook=framePixels(s);
    const SpinnerRun afterStop=spinnerFrames(s, others, stoppedLook, 10);

    // the card's spinner is one of the "outside" ones now: nothing of it moves
    checks.check("stop", stopped && spinnerRunGood(afterStop, count-1),
        std::string(stopped?"":"the card's spinner still spins, turns or is hidden; ")+spinnerRunReport(afterStop)
        +" ("+std::to_string(count-1)+" wanted)");
    ok=shoot(s, shotDir+"/spinner-stopped.png") && ok;

    const gweni::Point start=controlCentre(demo->startButton());

    clickMouse(s, start.x, start.y);
    moveMouse(s, off.x, off.y);
    renderFrames(s, 2);

    const SpinnerRun afterStart=spinnerFrames(s, all, framePixels(s), 10);

    checks.check("start", card->isSpinning() && card->isTurning() && spinnerAtClock(card)
        && spinnerRunGood(afterStart, count), spinnerRunReport(afterStart));

    // (3) the Hide button: the next frame runs no animation for it; Show
    // resumes it where the clock says
    const gweni::Point hide=controlCentre(demo->hideButton());

    clickMouse(s, hide.x, hide.y);
    moveMouse(s, off.x, off.y);
    renderFrames(s, 1);

    const bool hidden=!target->visible() && !target->isTurning();

    renderFrames(s, 1);

    const size_t hiddenAnims=s.canvas->getUpdateStats().animsRun;
    std::vector<gweni::controls::Spinner *> shown;

    for(gweni::controls::Spinner *spinner : all)
    {
        if(spinner != target)
            shown.push_back(spinner);
    }

    const SpinnerRun whileHidden=spinnerFrames(s, shown, framePixels(s), 10);

    checks.check("hide", hidden && hiddenAnims == count-1 && spinnerRunGood(whileHidden, count-1),
        std::string(hidden?"":"the spinner is still visible or turning; ")+"the frame after the hide ran "
        +std::to_string(hiddenAnims)+" animations; "+spinnerRunReport(whileHidden));

    // the row lays out without the hidden spinner: the button moved
    const gweni::Point show=controlCentre(demo->hideButton());

    clickMouse(s, show.x, show.y);
    moveMouse(s, off.x, off.y);
    renderFrames(s, 1);

    const bool resumed=target->visible() && target->isTurning() && spinnerAtClock(target);

    const size_t shownAnims=s.canvas->getUpdateStats().animsRun;
    const SpinnerRun afterShow=spinnerFrames(s, all, framePixels(s), 10);

    checks.check("show", resumed && shownAnims == count && spinnerRunGood(afterShow, count),
        std::string(resumed?"":"the shown spinner does not turn from the clock's arc; ")+"the show frame ran "
        +std::to_string(shownAnims)+" animations; "+spinnerRunReport(afterShow));

    // (4) the demo's host hidden: the next frame runs none; shown, all again
    control->setHidden(true);
    renderFrames(s, 1);
    renderFrames(s, 1);

    const size_t hostHiddenAnims=s.canvas->getUpdateStats().animsRun;
    bool noneTurning=true;

    for(gweni::controls::Spinner *spinner : all)
        noneTurning=noneTurning && !spinner->isTurning();

    std::vector<uint8_t> hostHidden;
    const size_t hostHiddenWork=settleCodeEditor(s, hostHidden);
    const size_t hostSettledAnims=s.canvas->getUpdateStats().animsRun;

    control->setHidden(false);
    renderFrames(s, 1);

    const size_t hostShownAnims=s.canvas->getUpdateStats().animsRun;
    bool allAtClock=true;

    for(gweni::controls::Spinner *spinner : all)
        allAtClock=allAtClock && spinner->isTurning() && spinnerAtClock(spinner);

    checks.check("host_hidden", hostHiddenAnims == 0 && noneTurning && hostHiddenWork == 0
        && hostSettledAnims == 0,
        "with the demo hidden the next frame ran "+std::to_string(hostHiddenAnims)+" animations"
        +(noneTurning?"":", a spinner kept its animation")+", settled frames wrote or changed "
        +std::to_string(hostHiddenWork));
    checks.check("host_shown", hostShownAnims == count && allAtClock,
        "shown again the frame ran "+std::to_string(hostShownAnims)+" animations ("+std::to_string(count)
        +" wanted)"+(allAtClock?"":", a spinner is not turning from the clock's arc"));

    // (5) every spinner stopped (the card's as its ring, the rest hidden)
    // or hidden: settled frames write nothing and run no animation
    for(gweni::controls::Spinner *spinner : all)
        spinner->stop();
    renderFrames(s, 1);

    std::vector<uint8_t> still;
    const size_t stillWork=settleCodeEditor(s, still);
    const size_t stillAnims=s.canvas->getUpdateStats().animsRun;

    checks.check("all_stopped", stillWork == 0 && stillAnims == 0 && card->visible() && !target->visible(),
        "with every spinner stopped settled frames wrote or changed "+std::to_string(stillWork)+" and ran "
        +std::to_string(stillAnims)+" animations");

    dropControl(s, control);
    renderFrames(s, 1);

    const size_t idsLeft=livePrimitiveIds()+liveTextIds()-idsBefore;

    checks.check("ids_back", idsLeft == 0, std::to_string(idsLeft)+" ids not given back");

    passed=checks.allPassed();
    std::cout<<"spinner";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second?"yes":"no");
    std::cout<<" spinners="<<count<<" arc_frames="<<spinning.arcFrames<<"/"<<spinning.frames<<"\n";
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario field-validation: FieldValidationDemo's fields, the clock frozen
//
// A TextBox's message line is a Text of its own under the field part, its
// room reserved in the box's height (TextBox::setReserveMessageLine, a
// helper text, a validator), so a message set, changed or cleared lays out
// the box alone and writes its frame's slot (the error look) and the
// message's glyphs, and changes no pixel outside it.
//-----------------------------------------------------------------------------

namespace
{

// The frame's slot and the message's text id of a field with a message line.
struct FieldIds
{
    size_t frame=0;
    std::set<size_t> message;
    std::set<size_t> primitives;    // the field's subtree's
    std::set<size_t> texts;
};

FieldIds fieldIds(gweni::controls::TextBox *box)
{
    FieldIds ids;
    std::set<size_t> unused;

    ids.frame=box->getSkinControl()->getPrimitives().at(0);
    // no line yet: nothing of it may be written
    if(box->getMessageText())
        subtreeIds(const_cast<gweni::controls::internal::Text *>(box->getMessageText()), unused, ids.message);
    subtreeIds(box, ids.primitives, ids.texts);
    return ids;
}

// The controls a layout of `box` alone enters: the way down to it and its
// subtree, four entries each.
size_t fieldEntries(gweni::controls::Base *box)
{
    size_t path=0;

    for(gweni::controls::Base *at=box->getActualParent(); at; at=at->getActualParent())
        ++path;
    return 4*(path+subtreeControls(box));
}

}//namespace

static bool runFieldValidationScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    InteractionChecks checks;
    checks.scenario="field-validation";

    const size_t idsBefore=livePrimitiveIds()+liveTextIds();
    gweni::controls::Base *control=addControl(s, "FieldValidationDemo");
    auto *demo=gweni::gweni_cast<gweni::FieldValidationDemo>(control);

    if(!demo || !demo->keyField() || !demo->passwordField() || !demo->hostField())
    {
        std::cerr<<"ControlTest: field-validation: FieldValidationDemo was not built\n";
        return false;
    }

    s.label="field-validation";
    renderFrames(s, s.frames);
    forgetMouse(s);

    bool ok=shoot(s, shotDir+"/field-validation.png");
    std::vector<uint8_t> pixels;
    size_t idleWork=settleCodeEditor(s, pixels);

    // (1) the app's set and clear: the field alone, its frame's slot and the
    // message's glyphs, no pixel outside it; the clear gives the frame back
    gweni::controls::TextBox *key=demo->keyField();
    const FieldIds keyIds=fieldIds(key);
    const gweni::Rect keyRect=key->getCanvasBounds();
    const int keyHeight=key->getHeight();
    const size_t keyEntries=fieldEntries(key);
    const std::vector<uint8_t> beforeSet=pixels;

    demo->setKeyError();

    const CodeEditorFrame set=codeEditorFrame(s);
    const size_t setOutside=differencesOutsideRects(s, beforeSet, set.pixels, {keyRect});

    checks.check("app_set",
        key->hasError() && set.passes == 1 && set.visited <= keyEntries
            && idsWithin(set.primitives, {keyIds.frame}) && set.primitives.count(keyIds.frame)
            && idsWithin(set.texts, keyIds.message) && !set.texts.empty()
            && setOutside == 0 && key->getHeight() == keyHeight && set.pixels != beforeSet,
        "the set wrote "+std::to_string(set.primitives.size())+" primitive ids (the frame's alone allowed) and "
        +std::to_string(set.texts.size())+" text ids (the message's alone), ran "+std::to_string(set.passes)
        +" passes entering "+std::to_string(set.visited)+" (the field's way and subtree, "
        +std::to_string(keyEntries)+", allowed) and changed "+std::to_string(setOutside)
        +" pixels outside the field");
    shoot(s, shotDir+"/field-validation-error.png");
    idleWork+=settleCodeEditor(s, pixels);

    demo->clearKeyError();

    const CodeEditorFrame clear=codeEditorFrame(s);
    const size_t clearOutside=differencesOutsideRects(s, pixels, clear.pixels, {keyRect});

    checks.check("app_clear",
        !key->hasError() && clear.passes == 1 && clear.visited <= keyEntries
            && idsWithin(clear.primitives, {keyIds.frame}) && idsWithin(clear.texts, keyIds.message)
            && clearOutside == 0 && key->getHeight() == keyHeight,
        "the clear wrote "+std::to_string(clear.primitives.size())+" primitive and "
        +std::to_string(clear.texts.size())+" text ids, ran "+std::to_string(clear.passes)+" passes entering "
        +std::to_string(clear.visited)+" and changed "+std::to_string(clearOutside)+" pixels outside the field");
    checks.check("clear_gives_back", clear.pixels == beforeSet,
        "the frame after the clear is not the one before the set");
    idleWork+=settleCodeEditor(s, pixels);

    // (2) the buttons take the same path: each click's frame lays out the
    // field alone, writes the field's frame slot and message glyphs and the
    // clicked button's own (its hover and press look), and changes no pixel
    // outside the two; with the mouse gone the frame before the set is back
    const std::vector<uint8_t> beforeButtons=pixels;

    auto buttonClick=[&](const char *name, gweni::controls::Button *button, bool wantError)
    {
        std::set<size_t> primitives{keyIds.frame};
        std::set<size_t> texts=keyIds.message;

        subtreeIds(button, primitives, texts);

        const gweni::Rect rect=button->getCanvasBounds();
        const size_t entries=keyEntries;

        clickMouse(s, rect.x+rect.w/2, rect.y+rect.h/2);

        const CodeEditorFrame frame=codeEditorFrame(s);
        const size_t outside=differencesOutsideRects(s, pixels, frame.pixels, {keyRect, rect});
        const bool state=wantError?key->getError() == "a key already in this file":!key->hasError();

        checks.check(name,
            state && frame.passes == 1 && frame.visited <= entries && frame.primitives.count(keyIds.frame)
                && idsWithin(frame.primitives, primitives) && idsWithin(frame.texts, texts)
                && (!wantError || !frame.texts.empty()) && outside == 0 && key->getHeight() == keyHeight,
            std::string("the ")+button->getText().c_str()+" click "+(state?"did":"did not")
            +" what it says, wrote "+std::to_string(frame.primitives.size())
            +" primitive ids (the frame's and the button's allowed) and "+std::to_string(frame.texts.size())
            +" text ids (the message's and the button's), ran "+std::to_string(frame.passes)+" passes entering "
            +std::to_string(frame.visited)+" ("+std::to_string(entries)+" allowed) and changed "
            +std::to_string(outside)+" pixels outside the field and the button");
        forgetMouse(s);
        renderFrames(s, 1);
        idleWork+=settleCodeEditor(s, pixels);
    };

    buttonClick("button_set", demo->setErrorButton(), true);
    buttonClick("button_clear", demo->clearErrorButton(), false);
    checks.check("buttons_give_back", pixels == beforeButtons,
        "the frame after the buttons' set and clear, the mouse gone, is not the one before them");

    // (3) typing into the Change-validated password: the frame that makes it
    // invalid shows the error, within the field's ids (its typed glyphs, caret
    // and frame, the message's glyphs), one that keeps the message writes no
    // message glyph, and the one that makes it valid clears it
    gweni::controls::PasswordTextBox *password=demo->passwordField();
    const gweni::Rect passwordRect=password->getCanvasBounds();

    clickMouse(s, passwordRect.x+passwordRect.w/2, passwordRect.y+password->getFieldHeight()/2);
    forgetMouse(s);
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);

    const FieldIds passwordIds=fieldIds(password);

    typeText(s, "a");

    const CodeEditorFrame invalid=codeEditorFrame(s);
    const size_t invalidOutside=differencesOutsideRects(s, pixels, invalid.pixels, {passwordRect});

    checks.check("typed_invalid",
        password->getError() == gweni::FieldValidationDemo::PasswordTooShort
            && idsWithin(invalid.primitives, passwordIds.primitives) && idsWithin(invalid.texts, passwordIds.texts)
            && invalid.texts.size() >= 2 && invalidOutside == 0 && invalid.passes <= 1
            && invalid.visited <= fieldEntries(password),
        "typing \"a\" set the error \""+password->getError()+"\", wrote "+std::to_string(invalid.primitives.size())
        +" primitive and "+std::to_string(invalid.texts.size())+" text ids (the field's, its typed text and its message, wanted), ran "
        +std::to_string(invalid.passes)+" passes entering "+std::to_string(invalid.visited)+" and changed "
        +std::to_string(invalidOutside)+" pixels outside the field");
    idleWork+=settleCodeEditor(s, pixels);

    typeText(s, "b");

    const CodeEditorFrame kept=codeEditorFrame(s);
    bool messageKept=true;

    for(size_t id : kept.texts)
    {
        if(passwordIds.message.count(id))
            messageKept=false;
    }
    checks.check("typed_kept",
        messageKept && idsWithin(kept.primitives, passwordIds.primitives) && idsWithin(kept.texts, passwordIds.texts),
        "a keystroke that keeps the message wrote "+std::to_string(kept.texts.size())+" text ids, "
        +std::string(messageKept?"none":"one")+" of them the message's");
    idleWork+=settleCodeEditor(s, pixels);

    typeText(s, "cdefg");
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);
    typeText(s, "h");

    const CodeEditorFrame valid=codeEditorFrame(s);
    const size_t validOutside=differencesOutsideRects(s, pixels, valid.pixels, {passwordRect});

    checks.check("typed_valid",
        !password->hasError() && idsWithin(valid.primitives, passwordIds.primitives)
            && idsWithin(valid.texts, passwordIds.texts) && validOutside == 0,
        "the eighth character left the error \""+password->getError()+"\" or wrote outside the field ("
        +std::to_string(validOutside)+" pixels outside it)");
    idleWork+=settleCodeEditor(s, pixels);

    // (4) Tab off the FocusLoss host field, empty: "required"
    gweni::controls::TextBox *host=demo->hostField();
    const gweni::Rect hostRect=host->getCanvasBounds();
    const bool hostHadError=host->hasError();

    clickMouse(s, hostRect.x+hostRect.w/2, hostRect.y+host->getFieldHeight()/2);
    forgetMouse(s);
    renderFrames(s, 1);
    const bool typingClean=!host->hasError();
    glfwKey(s, GLFW_KEY_TAB, GLFW_PRESS, 0);
    glfwKey(s, GLFW_KEY_TAB, GLFW_RELEASE, 0);
    renderFrames(s, 1);

    checks.check("tab_required", !hostHadError && typingClean && host->getError() == "required" && !host->isFocussed(),
        "Tab off the empty host field left the error \""+host->getError()+"\"");
    shoot(s, shotDir+"/field-validation-required.png");
    idleWork+=settleCodeEditor(s, pixels);

    checks.check("settled", idleWork == 0, "settled frames wrote or changed "+std::to_string(idleWork));

    dropControl(s, control);
    renderFrames(s, 1);

    const size_t idsLeft=livePrimitiveIds()+liveTextIds()-idsBefore;

    checks.check("ids_back", idsLeft == 0, std::to_string(idsLeft)+" ids not given back");

    passed=checks.allPassed();
    std::cout<<"field-validation";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second?"yes":"no");
    std::cout<<" set_prims="<<set.primitives.size()<<" set_texts="<<set.texts.size()<<" set_visited="<<set.visited
             <<" field_entries="<<keyEntries<<" typed_texts="<<invalid.texts.size()<<"\n";
    return ok;
}

static bool runCodeEditorScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    InteractionChecks checks;
    checks.scenario="code-editor";
    bool ok=true;

    const size_t idsBefore=livePrimitiveIds();
    gweni::controls::Base *control=addControl(s, "CodeEditorDemo");
    auto *demo=gweni::gweni_cast<gweni::CodeEditorDemo>(control);

    if(!demo || !demo->m_editor || !demo->m_editor->getGutter())
    {
        std::cerr<<"ControlTest: code-editor: CodeEditorDemo has no editor with line numbers\n";
        return false;
    }

    gweni::controls::CodeEditor *editor=demo->m_editor;
    gweni::controls::Base *gutter=editor->getGutter();

    s.label="CodeEditorDemo";
    renderFrames(s, s.frames);

    const int lineHeight=editor->getLineHeight();

    editor->setVerticalScrollOffset(2500*lineHeight);
    renderFrames(s, s.frames);

    const size_t top=editor->firstVisibleItem();
    // a short line ("    model: ...", an agent's second) well inside the view
    size_t line=top+3;

    while(line < top+20 && editor->getDocument().line(line).rfind("    model:", 0) != 0)
        ++line;

    clickLineEnd(s, editor, line);
    renderFrames(s, s.frames);

    std::vector<uint8_t> idle;

    checks.check("settled_focused", settleCodeEditor(s, idle) == 0, "a settled frame with the caret placed wrote or changed something");
    checks.check("caret_placed", editor->getCaret().line == line
        && editor->getCaret().byte == editor->getDocument().line(line).size(),
        "the press did not put the caret at the end of line "+std::to_string(line));

    // A typed character: its line's row, the gutter and the caret alone.
    {
        gweni::controls::CodeLine *row=editor->rowForLine(line);
        std::set<size_t> allowedPrims, allowedTexts;

        if(row)
            subtreeIds(row, allowedPrims, allowedTexts);
        subtreeIds(gutter, allowedPrims, allowedTexts);
        if(const gweni::skin::Control *own=editor->getSkinControl())
        {
            std::vector<size_t> prims, texts;

            own->appendIds(prims, texts);
            allowedPrims.insert(prims.begin(), prims.end());
        }

        const gweni::Rect caretBefore=canvasRect(editor, editor->getCaretBounds());
        const size_t tokenizedBefore=editor->getHighlightCache().tokenizedLines();

        typeText(s, "x");

        const CodeEditorFrame typed=codeEditorFrame(s);
        const gweni::Rect caretAfter=canvasRect(editor, editor->getCaretBounds());
        const size_t outside=differencesOutsideRects(s, idle, typed.pixels,
            {row?row->getCanvasBounds():gweni::Rect(), gutter->getCanvasBounds(), caretBefore, caretAfter});

        checks.check("typed_slots", row && typed.written > 0 && idsWithin(typed.primitives, allowedPrims)
            && idsWithin(typed.texts, allowedTexts)
            && typed.glyphSlots <= editor->getDocument().line(line).size(),
            "the typed character wrote "+std::to_string(typed.primitives.size())+" primitive and "
            +std::to_string(typed.glyphSlots)+" glyph slots, some outside its row, the gutter and the caret");
        checks.check("typed_pixels", outside == 0,
            std::to_string(outside)+" pixels changed outside the line, the gutter and the caret");
        checks.check("typed_one_pass", typed.passes <= 1, "the typing frame took "+std::to_string(typed.passes)+" passes");
        // the value keeps the state the next line starts in: its line alone
        checks.check("typed_tokenized", editor->getHighlightCache().tokenizedLines() == tokenizedBefore+1,
            "the typed character tokenized "+std::to_string(editor->getHighlightCache().tokenizedLines()-tokenizedBefore)
            +" lines, not its own alone");
        checks.check("typed_text", editor->getDocument().line(line).back() == 'x', "the character did not go in");
        checks.check("settled_typed", settleCodeEditor(s, idle) == 0, "a settled frame after typing wrote or changed something");
    }

    // A typing burst undone gives the frame before it back.
    {
        clickLineEnd(s, editor, line+2);
        renderFrames(s, s.frames);

        std::vector<uint8_t> before;

        settleCodeEditor(s, before);

        const std::string textBefore=editor->getDocument().line(line+2);

        typeText(s, "burst");
        renderFrames(s, 1);
        sendKey(s, "ctrl+z");

        const CodeEditorFrame undone=codeEditorFrame(s);

        checks.check("undo_text", editor->getDocument().line(line+2) == textBefore, "the undo did not give the line back");
        checks.check("undo_pixels", undone.pixels == before, "the frame after the undo is not the frame before the burst");
        checks.check("settled_undo", settleCodeEditor(s, idle) == 0, "a settled frame after the undo wrote or changed something");
    }

    // A block scalar opened at an agent's entry low on screen ("  - name: ..."
    // made "  - |"): every line of the entry below it is in the block, past the
    // screen's last line. The rows on screen below it get new colours, and
    // nothing else: the slots written are the rows', the gutter's and the
    // caret's, no more glyphs than those rows hold, in one pass, no pixel
    // changed outside the editor, and the lines past the screen are not
    // tokenized. Its undo gives the frame before it back.
    {
        gweni::text::HighlightCache &cache=editor->getHighlightCache();
        size_t lastShown=0;
        std::map<size_t, gweni::controls::CodeLine *> shown;
        size_t entry=gweni::controls::CodeLine::NoLine;
        auto findShown=[&]()
            {
                shown.clear();
                lastShown=0;
                entry=gweni::controls::CodeLine::NoLine;
                for(gweni::controls::Base *base : editor->getRows())
                {
                    auto *row=gweni::gweni_cast<gweni::controls::CodeLine>(base);

                    if(row && !row->hidden() && row->getLine() != gweni::controls::CodeLine::NoLine)
                    {
                        shown[row->getLine()]=row;
                        lastShown=std::max(lastShown, row->getLine());
                    }
                }
                // the last entry with a row on screen below it
                for(const auto &item : shown)
                {
                    if(item.first < lastShown && editor->getDocument().line(item.first).rfind("  - name:", 0) == 0)
                        entry=item.first;
                }
            };

        findShown();
        // scrolled so that 8 of the entry's 15 lines below it are on screen
        // (when the next entry's line is the screen's last, that entry's)
        if(entry != gweni::controls::CodeLine::NoLine && (lastShown-entry)%16 < 8)
        {
            editor->scrollByRows(static_cast<int>(8-(lastShown-entry)%16));
            renderFrames(s, s.frames);
            settleCodeEditor(s, idle);
            findShown();
        }

        const bool found=(entry != gweni::controls::CodeLine::NoLine) && entry+16 > lastShown;

        checks.check("block_entry", found, "no agent entry on screen with a row below it");
        if(found)
        {
            // the value and the space before it selected, settled, then replaced
            editor->setCaret(gweni::text::Position{entry, 3});
            editor->setCaret(gweni::text::Position{entry, editor->getDocument().line(entry).size()}, true);
            renderFrames(s, s.frames);

            std::vector<uint8_t> before;

            settleCodeEditor(s, before);

            std::map<size_t, std::vector<gweni::renderer::ColorSpan>> spansBefore;
            std::set<size_t> allowedPrims, allowedTexts;
            size_t rowGlyphs=0;

            for(const auto &item : shown)
            {
                spansBefore[item.first]=item.second->getColorSpans();
                if(item.first < entry)
                    continue;
                subtreeIds(item.second, allowedPrims, allowedTexts);
                rowGlyphs+=item.second->getText().size();
            }
            subtreeIds(gutter, allowedPrims, allowedTexts);
            if(const gweni::skin::Control *own=editor->getSkinControl())
            {
                std::vector<size_t> prims, texts;

                own->appendIds(prims, texts);
                allowedPrims.insert(prims.begin(), prims.end());
            }

            // the space first (the entry's line alone), then the '|' that opens it
            typeText(s, " ");
            renderFrames(s, 1);
            for(auto &item : spansBefore)
            {
                if(gweni::controls::CodeLine *row=editor->rowForLine(item.first))
                    item.second=row->getColorSpans();
            }

            const size_t tokenizedBefore=cache.tokenizedLines();

            typeText(s, "|");

            const CodeEditorFrame opened=codeEditorFrame(s);
            const size_t outside=differencesOutsideRects(s, before, opened.pixels, {editor->getCanvasBounds()});
            const size_t lastVisible=cache.visibleFirst()+cache.visibleCount()-1;
            size_t recoloured=0, same=0;

            for(const auto &item : shown)
            {
                if(item.first <= entry)
                    continue;
                if(editor->rowForLine(item.first) && editor->rowForLine(item.first)->getColorSpans() != spansBefore[item.first])
                    ++recoloured;
                else
                    ++same;
            }
            checks.check("block_text", editor->getDocument().line(entry) == "  - |", "the entry did not become \"  - |\"");
            checks.check("block_recoloured", same == 0 && recoloured == lastShown-entry,
                std::to_string(recoloured)+" rows below the entry got new colours and "+std::to_string(same)
                +" did not, of "+std::to_string(lastShown-entry));
            checks.check("block_slots", idsWithin(opened.primitives, allowedPrims) && idsWithin(opened.texts, allowedTexts)
                && opened.glyphSlots <= rowGlyphs+2 && opened.passes <= 1,
                "the opened block wrote "+std::to_string(opened.glyphSlots)+" glyph slots (the rows from it down hold "
                +std::to_string(rowGlyphs)+"), "+std::to_string(opened.passes)+" passes, or a slot outside them, the gutter and the caret");
            checks.check("block_pixels", outside == 0, std::to_string(outside)+" pixels changed outside the editor");
            checks.check("block_past_screen", lastVisible == lastShown && cache.frontier() == lastVisible+1
                && !cache.isFresh(lastVisible+1) && cache.tokenizedLines()-tokenizedBefore <= lastVisible-entry+1,
                "the opened block at line "+std::to_string(entry)+" tokenized "
                +std::to_string(cache.tokenizedLines()-tokenizedBefore)+" lines up to "
                +std::to_string(cache.frontier())+" (fresh past it: "+(cache.isFresh(lastVisible+1)?"yes":"no")
                +"), the screen's last line "+std::to_string(lastVisible)+" (shown "+std::to_string(lastShown)+")");
            checks.check("settled_block", settleCodeEditor(s, idle) == 0, "a settled frame after the block opened wrote or changed something");

            sendKey(s, "ctrl+z");

            const CodeEditorFrame undone=codeEditorFrame(s);

            checks.check("block_undo", undone.pixels == before, "the frame after the block's undo is not the frame before it");
            checks.check("settled_block_undo", settleCodeEditor(s, idle) == 0,
                "a settled frame after the block's undo wrote or changed something");
            editor->setCaret(editor->getCaret());
            renderFrames(s, s.frames);
            settleCodeEditor(s, idle);
        }
    }

    // Return mid-screen: inside the editor, no more glyphs than its rows and
    // numbers on screen, nothing outside it changed.
    {
        std::set<size_t> allowedPrims, allowedTexts;

        subtreeIds(editor, allowedPrims, allowedTexts);

        const size_t lines=editor->getLineCount();

        sendKey(s, "return");

        const CodeEditorFrame broken=codeEditorFrame(s);
        const size_t outside=differencesOutsideRects(s, idle, broken.pixels, {editor->getCanvasBounds()});
        std::set<size_t> afterPrims, afterTexts;

        subtreeIds(editor, afterPrims, afterTexts);
        checks.check("return_line", editor->getLineCount() == lines+1, "Return did not add a line");
        // the pool takes no id and gives none back: the rows on screen are re-bound
        checks.check("return_ids", afterPrims == allowedPrims && afterTexts == allowedTexts,
            "Return changed the ids the editor holds");
        checks.check("return_rows", idsWithin(broken.primitives, afterPrims) && idsWithin(broken.texts, afterTexts)
            && broken.glyphSlots <= shownGlyphs(editor) && broken.passes <= 1
            && broken.visited <= 4*(subtreeControls(editor)+2),
            "Return wrote "+std::to_string(broken.glyphSlots)+" glyph slots (the rows on screen hold "
            +std::to_string(shownGlyphs(editor))+"), "+std::to_string(broken.visited)+" layout entries, "
            +std::to_string(broken.passes)+" passes, or a slot outside the editor");
        checks.check("return_pixels", outside == 0, std::to_string(outside)+" pixels changed outside the editor");
        checks.check("settled_return", settleCodeEditor(s, idle) == 0, "a settled frame after Return wrote or changed something");
    }

    // A one-line scroll: the rows on screen and the gutter, and the bar.
    {
        std::set<size_t> allowedPrims, allowedTexts;

        subtreeIds(editor, allowedPrims, allowedTexts);
        editor->scrollByRows(1);

        const CodeEditorFrame scrolled=codeEditorFrame(s);
        const size_t outside=differencesOutsideRects(s, idle, scrolled.pixels, {editor->getCanvasBounds()});

        checks.check("scroll_rows", scrolled.written > 0 && idsWithin(scrolled.primitives, allowedPrims)
            && idsWithin(scrolled.texts, allowedTexts) && scrolled.glyphSlots <= shownGlyphs(editor)
            && scrolled.passes <= 1,
            "the scroll wrote "+std::to_string(scrolled.glyphSlots)+" glyph slots (the rows on screen hold "
            +std::to_string(shownGlyphs(editor))+") or a slot outside the editor");
        checks.check("scroll_pixels", outside == 0, std::to_string(outside)+" pixels changed outside the editor");
        checks.check("settled_scroll", settleCodeEditor(s, idle) == 0, "a settled frame after the scroll wrote or changed something");
    }

    // Wrap on, through the demo's toggle, the caret on the view's bottom line
    // (the lines above it wrapping push it down): it stays on screen.
    {
        const size_t bottom=clickBottomLine(s, editor);

        renderFrames(s, s.frames);

        const size_t caretLine=editor->getCaret().line;

        checks.check("wrap_caret_bottom", caretLine == bottom, "the press did not put the caret on the view's bottom line");

        clickCentre(s, demo->m_wrapToggle->checkBox());
        renderFrames(s, s.frames);
        editor->focus();
        renderFrames(s, s.frames);
        checks.check("wrap_toggle", editor->isWrapped() && editor->getCaret().line == caretLine
            && caretLineOnScreen(editor), "turning wrap on lost the caret's line from the screen");
        checks.check("settled_wrap", settleCodeEditor(s, idle) == 0, "a settled frame after wrapping wrote or changed something");
    }

    // Wrapped, a typed character that keeps its line's rows: its row (all of
    // its visual rows), the gutter and the caret alone.
    const size_t wrapLine=shownLineStarting(editor, "    host:", 1);

    checks.check("wrap_line_found", wrapLine < editor->getLineCount(), "no one-row host: line on screen");
    if(wrapLine < editor->getLineCount())
    {
        clickWrappedLineEnd(s, editor, wrapLine);
        renderFrames(s, s.frames);
        settleCodeEditor(s, idle);

        gweni::controls::CodeLine *row=editor->rowForLine(wrapLine);
        std::set<size_t> allowedPrims, allowedTexts;

        subtreeIds(row, allowedPrims, allowedTexts);
        subtreeIds(gutter, allowedPrims, allowedTexts);
        if(const gweni::skin::Control *own=editor->getSkinControl())
        {
            std::vector<size_t> prims, texts;

            own->appendIds(prims, texts);
            allowedPrims.insert(prims.begin(), prims.end());
        }

        const gweni::Rect caretBefore=canvasRect(editor, editor->getCaretBounds());
        const gweni::Rect rowBefore=row->getCanvasBounds();

        typeText(s, "y");

        const CodeEditorFrame typed=codeEditorFrame(s);
        const gweni::Rect caretAfter=canvasRect(editor, editor->getCaretBounds());
        const size_t outside=differencesOutsideRects(s, idle, typed.pixels,
            {rowBefore, gutter->getCanvasBounds(), caretBefore, caretAfter});

        checks.check("wrap_typed_slots", editor->getCaret().line == wrapLine && row->getRowCount() == 1
            && typed.written > 0 && idsWithin(typed.primitives, allowedPrims) && idsWithin(typed.texts, allowedTexts)
            && typed.glyphSlots <= editor->getDocument().line(wrapLine).size() && typed.passes <= 1,
            "the wrapped typed character wrote "+std::to_string(typed.primitives.size())+" primitive and "
            +std::to_string(typed.glyphSlots)+" glyph slots, some outside its row, the gutter and the caret");
        checks.check("wrap_typed_pixels", outside == 0,
            std::to_string(outside)+" pixels changed outside the wrapped line, the gutter and the caret");
        checks.check("settled_wrap_typed", settleCodeEditor(s, idle) == 0, "a settled frame after wrapped typing wrote or changed something");

        // The line filled to its last cell (still one row), then one more: a
        // second row, the rows below on screen moved down a line, nothing
        // outside the editor changed.
        const size_t room=editor->getWrapCells()-editor->getLineCells(wrapLine);

        typeText(s, std::string(room, 'z'));
        renderFrames(s, s.frames);
        settleCodeEditor(s, idle);

        gweni::controls::CodeLine *nextRow=editor->rowForLine(wrapLine+1);
        const int nextY=nextRow?nextRow->getY():0;
        const bool fullOneRow=(row->getRowCount() == 1);

        typeText(s, "z");

        const CodeEditorFrame grown=codeEditorFrame(s);
        // the ids the editor holds after the frame: the new visual row is a
        // Text of its own
        std::set<size_t> editorPrims, editorTexts;

        subtreeIds(editor, editorPrims, editorTexts);
        const size_t outsideEditor=differencesOutsideRects(s, idle, grown.pixels, {editor->getCanvasBounds()});
        gweni::controls::CodeLine *movedRow=editor->rowForLine(wrapLine+1);

        checks.check("wrap_row_added", fullOneRow && row->getRowCount() == 2 && movedRow
            && movedRow->getY() == nextY+lineHeight && grown.passes <= 1
            && idsWithin(grown.primitives, editorPrims) && idsWithin(grown.texts, editorTexts)
            && grown.glyphSlots <= shownGlyphs(editor),
            "the character that wrapped the line did not give it a second row and move the rows below by a line"
            " in one pass inside the editor ("+std::to_string(grown.glyphSlots)+" glyph slots, "
            +std::to_string(grown.passes)+" passes)");
        checks.check("wrap_row_pixels", outsideEditor == 0,
            std::to_string(outsideEditor)+" pixels changed outside the editor when the line wrapped");
        checks.check("settled_wrap_row", settleCodeEditor(s, idle) == 0, "a settled frame after the new row wrote or changed something");
    }

    // A resize keeps the caret's line on screen, and back: the caret on the
    // view's bottom line, which a smaller canvas cuts off unless kept.
    {
        const gweni::Point size=s.size;

        clickBottomLine(s, editor);
        renderFrames(s, s.frames);
        settleCodeEditor(s, idle);

        resizeSession(s, size.x*3/4, size.y*3/4);
        renderFrames(s, s.frames);
        checks.check("wrap_resize_caret", caretLineOnScreen(editor), "a smaller canvas lost the caret's line from the screen");
        resizeSession(s, size.x, size.y);
        renderFrames(s, s.frames);
        checks.check("wrap_resize_back", caretLineOnScreen(editor), "the canvas back to its size lost the caret's line");
        checks.check("settled_wrap_resize", settleCodeEditor(s, idle) == 0, "a settled frame after the resize wrote or changed something");
    }

    // The wrapped selection: a line of several visual rows selected draws one
    // rect of the selection's fill on each, and after a resize wraps it again
    // the new rows' rects are drawn.
    {
        const size_t longLine=[&]()
            {
                const size_t top=editor->firstVisibleItem();

                for(size_t line=top; line < top+40 && line < editor->getLineCount(); ++line)
                {
                    const gweni::controls::CodeLine *row=editor->rowForLine(line);

                    if(row && !row->hidden() && row->getRowCount() >= 2 && row->bottom() < editor->getViewportHeight())
                        return line;
                }
                return editor->getLineCount();
            }();

        checks.check("wrap_long_line", longLine < editor->getLineCount(), "no wrapped line on screen");
        if(longLine < editor->getLineCount())
        {
            const gweni::text::Position from{longLine, 0};
            const gweni::text::Position to{longLine, editor->getDocument().line(longLine).size()};

            editor->setCaret(from);
            editor->setCaret(to, true);

            const CodeEditorFrame selected=codeEditorFrame(s);
            gweni::controls::CodeLine *row=editor->rowForLine(longLine);
            std::vector<gweni::Rect> rects;
            const bool drawn=row && selectionDrawn(s, selected.pixels, row, rects);

            checks.check("wrap_selection_rects", drawn && rects.size() == row->getRowCount(),
                "the selection of a wrapped line did not draw one rect of its fill on each of its "
                +std::to_string(row?row->getRowCount():0)+" visual rows ("+std::to_string(rects.size())+" rects)");

            const gweni::Point size=s.size;

            resizeSession(s, size.x*3/4, size.y);
            renderFrames(s, s.frames);

            const std::vector<uint8_t> narrow=framePixels(s);
            gweni::controls::CodeLine *rewrapped=editor->rowForLine(longLine);
            const bool redrawn=rewrapped && selectionDrawn(s, narrow, rewrapped, rects);

            checks.check("wrap_selection_rewrap", redrawn && rects.size() == rewrapped->getRowCount()
                && editor->getSelectedText() == editor->getDocument().line(longLine),
                "after the resize the wrapped selection's "+std::to_string(rects.size())
                +" rects are not drawn on the line's new rows, or the selection moved");
            resizeSession(s, size.x, size.y);
            editor->setCaret(to);
            renderFrames(s, s.frames);
            checks.check("settled_wrap_selection", settleCodeEditor(s, idle) == 0,
                "a settled frame after the selection wrote or changed something");
        }
    }

    // Wrap off again, through the toggle: the caret's line stays on screen.
    {
        const size_t caretLine=editor->getCaret().line;

        clickCentre(s, demo->m_wrapToggle->checkBox());
        renderFrames(s, s.frames);
        checks.check("unwrap_toggle", !editor->isWrapped() && editor->getCaret().line == caretLine
            && caretLineOnScreen(editor), "turning wrap off lost the caret's line from the screen");
        checks.check("settled_unwrap", settleCodeEditor(s, idle) == 0, "a settled frame after unwrapping wrote or changed something");
    }

    if(!shoot(s, shotDir+"/code-editor.png"))
        ok=false;

    forgetMouse(s);
    dropControl(s, control);
    renderFrames(s, 1);
    checks.check("ids_back", livePrimitiveIds() == idsBefore, "the demo did not give its primitive ids back");

    // An auto-height field in a form growing by a row: one pass, inside the
    // form, the fields below it moved down a line.
    {
        gweni::controls::Base *formControl=addControl(s, "CodeEditorFormDemo");
        auto *form=gweni::gweni_cast<gweni::CodeEditorFormDemo>(formControl);

        if(!form || !form->m_comment)
        {
            std::cerr<<"ControlTest: code-editor: CodeEditorFormDemo has no comment field\n";
            return false;
        }

        gweni::controls::CodeEditor *comment=form->m_comment;

        renderFrames(s, s.frames);
        clickCentre(s, comment);
        renderFrames(s, s.frames);

        // two rows of letters: still its least rows, 2
        typeText(s, std::string(2*comment->getWrapCells(), 'q'));
        renderFrames(s, s.frames);

        std::vector<uint8_t> formIdle;

        settleCodeEditor(s, formIdle);

        const int height=comment->getHeight();
        gweni::controls::Base *panel=comment->getParent();
        const gweni::controls::Base *lastField=panel->getChildren().back();
        const int lastY=lastField->getY();
        std::set<size_t> formPrims, formTexts;
        size_t depth=0;
        size_t below=0;
        bool after=false;

        for(gweni::controls::Base *up=comment->getParent(); up; up=up->getParent())
            ++depth;
        for(gweni::controls::Base *child : panel->getChildren())
        {
            if(after)
                ++below;
            after=after || child == comment;
        }
        subtreeIds(formControl, formPrims, formTexts);

        const gweni::Rect above(0, 0, s.size.x, comment->getCanvasBounds().y);

        typeText(s, "q");

        const CodeEditorFrame grown=codeEditorFrame(s);
        const size_t aboveChanged=differencesIn(s, formIdle, grown.pixels, above);
        // the field's subtree and its ancestors entered, each at most twice an
        // axis, and each field below once (the dock moves it)
        const size_t entries=4*(subtreeControls(comment)+depth)+below;

        checks.check("auto_height_grow", comment->getHeight() == height+comment->getLineHeight()
            && lastField->getY() == lastY+comment->getLineHeight() && grown.passes == 1
            && grown.visited <= entries && idsWithin(grown.primitives, formPrims) && idsWithin(grown.texts, formTexts),
            "the auto-height field did not grow a row in one pass inside the form ("+std::to_string(grown.passes)
            +" passes, "+std::to_string(grown.visited)+" layout entries of at most "+std::to_string(entries)+")");
        checks.check("auto_height_pixels", aboveChanged == 0,
            std::to_string(aboveChanged)+" pixels changed above the growing field");
        checks.check("settled_auto_height", settleCodeEditor(s, formIdle) == 0,
            "a settled frame after the field grew wrote or changed something");

        forgetMouse(s);
        dropControl(s, formControl);
        renderFrames(s, 1);
        checks.check("form_ids_back", livePrimitiveIds() == idsBefore, "the form demo did not give its primitive ids back");
    }

    std::cout<<"code-editor";
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
    // the text measures the frame made (virtual-list-variable compares them)
    size_t measures=0;

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
    const size_t measuresBefore=gweni::controls::measureTextCalls();

    renderFrames(s, 1);

    VirtualListStep step;

    step.measures=gweni::controls::measureTextCalls()-measuresBefore;
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
// --scenario virtual-list-variable: VirtualListVariableDemo at 1 000 and
// 100 000 items
//
// A variable-height list's frame costs what its rows on screen cost, as a
// fixed one's does: the pool, the ids and a one-row scroll and a wheel notch
// (with their measures) are the same at both counts, with the idle re-measure
// off (budget 0) so no frame measures off screen. Then from the middle of the
// list: an item above the viewport growing and one inserted above it move the
// offset, not a pixel inside the viewport; a narrower list keeps the item at
// the top where it was; and the idle re-measure, given a budget, ends with the
// content height the sum of every item's measure, no pixel inside the
// viewport moving meanwhile.
//-----------------------------------------------------------------------------

namespace
{

struct VariableListRun
{
    size_t items=0;
    size_t rows=0;
    size_t subtree=0;
    size_t primIds=0;
    size_t idleSlots=0;
    size_t idleChanged=0;
    size_t idsLeft=0;
    VirtualListStep row, wheel;
    // pixels inside the viewport a growth and an insert above it changed, and
    // whether each moved the offset
    size_t growChanged=0;
    size_t insertChanged=0;
    bool growMoved=false;
    bool insertMoved=false;
    // a narrower list: the generation bumped and the top item's row where it
    // was
    bool resizeKeepsTop=false;
    // the idle re-measure: its frames, whether it ended, the pixels inside the
    // viewport it changed, and the content against the sum of the measures
    int remeasureFrames=0;
    bool remeasureEnded=false;
    size_t remeasureChanged=0;
    int64_t contentHeight=0;
    int64_t measuredSum=0;
    size_t animsAfter=0;
};

gweni::Rect viewportOnCanvas(gweni::controls::VirtualList *list)
{
    const gweni::Point origin=list->localPosToCanvas();
    gweni::Rect view=list->getViewport();

    view.x+=origin.x;
    view.y+=origin.y;
    return view;
}

// What a one-row scroll or a wheel notch cost at the two counts, less the
// thumb's one slot where it moved.
bool sameStepCost(const VirtualListStep &a, const VirtualListStep &b)
{
    return a.primSlots-(a.thumbMoved?1:0) == b.primSlots-(b.thumbMoved?1:0)
        && a.textSlots == b.textSlots && a.zVisited == b.zVisited && a.passes == b.passes
        && a.requests == b.requests && a.rowPrims == b.rowPrims && a.rowGlyphs == b.rowGlyphs
        && a.measures == b.measures
        && a.visited-(a.thumbMoved?1:0) == b.visited-(b.thumbMoved?1:0)
        && a.changed-(a.thumbMoved?1:0) == b.changed-(b.thumbMoved?1:0);
}

}//namespace

static void settleVariableList(Session &s, gweni::controls::VirtualList *list, VariableListRun &run)
{
    for(int i=0; i<2; ++i)
    {
        const VirtualListStep idle=virtualListFrame(s, list);

        run.idleSlots+=idle.primSlots+idle.textSlots;
        run.idleChanged+=idle.changed;
    }
}

static VirtualListStep variableListStep(Session &s, gweni::controls::VirtualList *list,
    const std::function<void()> &input)
{
    gweni::controls::Base *bar=findControl(list, "VerticalScrollBar");
    gweni::controls::Base *thumb=bar?findControl(bar, "ScrollBarBar"):nullptr;
    const gweni::Rect thumbBefore=thumb?thumb->getBounds():gweni::Rect();

    input();

    VirtualListStep step=virtualListFrame(s, list);

    step.thumbMoved=thumb && !(thumb->getBounds() == thumbBefore);
    return step;
}

static bool runVariableList(Session &s, const std::string &shotDir, size_t items, size_t budget,
    VariableListRun &run)
{
    const size_t idsBefore=livePrimitiveIds();
    gweni::controls::Base *control=addControl(s, "VirtualListVariableDemo");
    auto *demo=gweni::gweni_cast<gweni::VirtualListVariableDemo>(control);

    if(!demo || !demo->m_list)
    {
        std::cerr<<"ControlTest: virtual-list-variable: VirtualListVariableDemo has no list\n";
        return false;
    }

    gweni::controls::VirtualList *list=demo->m_list;

    s.label="VirtualListVariableDemo-"+std::to_string(items);
    // only what shows is measured, so no frame does work off screen
    list->setRemeasureBudget(0);
    demo->setItems(items);
    renderFrames(s, s.frames);

    run.items=items;
    run.rows=list->getRows().size();
    run.subtree=subtreeControls(list);
    run.primIds=livePrimitiveIds()-idsBefore;
    settleVariableList(s, list, run);

    run.row=variableListStep(s, list, [&] { list->scrollByRows(1); });
    settleVariableList(s, list, run);

    const gweni::Point origin=list->localPosToCanvas();

    moveMouse(s, origin.x+list->getWidth()/2, origin.y+list->getHeight()/2);
    renderFrames(s, 1);
    settleVariableList(s, list, run);
    run.wheel=variableListStep(s, list, [&] { scrollWheel(s, -60); });
    settleVariableList(s, list, run);
    forgetMouse(s);
    renderFrames(s, 1);

    // from the middle: edits above the viewport
    list->scrollToItem(items/2);
    renderFrames(s, s.frames);
    settleVariableList(s, list, run);

    gweni::Rect view=viewportOnCanvas(list);
    std::vector<uint8_t> before=framePixels(s);
    int offset=list->getVerticalScrollOffset();

    demo->growItem(list->getAnchorItem()-5);
    renderFrames(s, 1);
    run.growChanged=differencesIn(s, before, framePixels(s), view);
    run.growMoved=list->getVerticalScrollOffset() != offset;
    settleVariableList(s, list, run);

    before=framePixels(s);
    offset=list->getVerticalScrollOffset();
    demo->insertAtTop(1);
    renderFrames(s, 1);
    run.insertChanged=differencesIn(s, before, framePixels(s), view);
    run.insertMoved=list->getVerticalScrollOffset() != offset;
    settleVariableList(s, list, run);

    // narrower: the rows on screen measured again, the top one where it was
    const size_t top=list->getAnchorItem();
    gweni::controls::Base *topRow=list->rowForItem(top);
    const int topY=topRow?topRow->getY():-1;
    const uint32_t generation=list->getMeasureGeneration();

    list->setWidth(list->getWidth()-60);
    renderFrames(s, 1);
    topRow=list->rowForItem(list->getAnchorItem());
    run.resizeKeepsTop=list->getMeasureGeneration() == generation+1 && list->getAnchorItem() == top
        && topRow && topRow->getY() == topY;
    renderFrames(s, s.frames);

    // the idle re-measure, to its end
    view=viewportOnCanvas(list);
    before=framePixels(s);
    list->setRemeasureBudget(budget);
    while(list->isRemeasuring() && run.remeasureFrames < 1000)
    {
        renderFrames(s, 1);
        ++run.remeasureFrames;
        run.remeasureChanged+=differencesIn(s, before, framePixels(s), view);
    }
    run.remeasureEnded=!list->isRemeasuring();

    const gweni::controls::HeightIndex &index=list->getHeightIndex();
    const gweni::Font &font=demo->getSkin()->getDefaultFont();
    const int width=list->getMeasureWidth()-2*gweni::VirtualListVariableDemo::RowPadX;

    run.contentHeight=list->getVirtualContentHeight();
    for(size_t i=0; i < index.size(); ++i)
        run.measuredSum+=list->wrappedTextHeight(font, demo->itemText(i), width)
            +2*gweni::VirtualListVariableDemo::RowPadY;
    if(index.minGeneration() != list->getMeasureGeneration())
        run.measuredSum=-1;
    settleVariableList(s, list, run);
    run.animsAfter=s.canvas->getUpdateStats().animsRun;

    // the end, for the picture
    list->scrollToItem(list->getItemCount()-1);
    renderFrames(s, s.frames);

    const bool shot=shoot(s, shotDir+"/virtual-list-variable-"+std::to_string(items)+".png");

    dropControl(s, control);
    renderFrames(s, 1);
    run.idsLeft=livePrimitiveIds()-idsBefore;
    return shot;
}

static bool runVirtualListVariableScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    VariableListRun small, large;
    bool ok=runVariableList(s, shotDir, 1000, 64, small);

    ok=runVariableList(s, shotDir, 100000, 8192, large) && ok;

    // A wrapped row given an item of another line count gains or loses line
    // Texts, which the z renumber enters (within the list, as many at both
    // counts), so z is bounded by the list's subtree rather than 0.
    auto withinRows=[](const VariableListRun &run, const VirtualListStep &step)
    {
        return step.passes == 1 && step.zVisited <= run.subtree && step.requests == 0
            && step.visited <= run.subtree+2 && step.changed <= run.subtree
            && step.primSlots+step.textSlots > 0
            && step.primSlots <= step.rowPrims+(step.thumbMoved?1:0) && step.textSlots <= step.rowGlyphs;
    };
    auto aboveStill=[](const VariableListRun &run)
    {
        return run.growChanged == 0 && run.insertChanged == 0 && run.growMoved && run.insertMoved;
    };
    auto remeasured=[](const VariableListRun &run)
    {
        return run.remeasureEnded && run.remeasureChanged == 0 && run.animsAfter == 0
            && run.contentHeight == std::min<int64_t>(run.measuredSum, INT_MAX);
    };

    const bool sameCost=small.rows == large.rows && small.subtree == large.subtree
        && small.primIds == large.primIds && sameStepCost(small.row, large.row)
        && sameStepCost(small.wheel, large.wheel);
    const bool settled=small.idleSlots == 0 && small.idleChanged == 0
        && large.idleSlots == 0 && large.idleChanged == 0;
    const bool rowsOnly=withinRows(small, small.row) && withinRows(small, small.wheel)
        && withinRows(large, large.row) && withinRows(large, large.wheel);
    const bool editsAbove=aboveStill(small) && aboveStill(large);
    const bool resizeKeeps=small.resizeKeepsTop && large.resizeKeepsTop;
    const bool remeasureEnds=remeasured(small) && remeasured(large);
    const bool idsBack=small.idsLeft == 0 && large.idsLeft == 0;

    for(const VariableListRun *run : {&small, &large})
    {
        std::cout<<"virtual-list-variable items="<<run->items<<" rows="<<run->rows<<" subtree="<<run->subtree
                 <<" prim_ids="<<run->primIds<<" idle_slots="<<run->idleSlots
                 <<" idle_changed="<<run->idleChanged;
        printVirtualListStep("row", run->row);
        std::cout<<" row_measures="<<run->row.measures;
        printVirtualListStep("wheel", run->wheel);
        std::cout<<" wheel_measures="<<run->wheel.measures
                 <<" grow_changed="<<run->growChanged<<" grow_moved="<<(run->growMoved?1:0)
                 <<" insert_changed="<<run->insertChanged<<" insert_moved="<<(run->insertMoved?1:0)
                 <<" resize_keeps_top="<<(run->resizeKeepsTop?1:0)
                 <<" remeasure_frames="<<run->remeasureFrames
                 <<" remeasure_ended="<<(run->remeasureEnded?1:0)
                 <<" remeasure_changed="<<run->remeasureChanged
                 <<" content_height="<<run->contentHeight<<" measured_sum="<<run->measuredSum
                 <<" anims_after="<<run->animsAfter<<" ids_left="<<run->idsLeft<<"\n";
    }

    passed=sameCost && settled && rowsOnly && editsAbove && resizeKeeps && remeasureEnds && idsBack;

    std::cout<<"virtual-list-variable same_cost="<<(sameCost?"yes":"no")<<" settled="<<(settled?"yes":"no")
             <<" rows_only="<<(rowsOnly?"yes":"no")<<" edits_above_still="<<(editsAbove?"yes":"no")
             <<" resize_keeps_top="<<(resizeKeeps?"yes":"no")
             <<" remeasure_ends="<<(remeasureEnds?"yes":"no")<<" ids_back="<<(idsBack?"yes":"no")<<"\n";
    if(!passed)
        std::cerr<<"ControlTest: virtual-list-variable: "
                 <<(!sameCost ? "a frame at 100 000 items did not cost what it did at 1 000"
                     : !settled ? "a settled frame wrote slots or changed controls"
                     : !rowsOnly ? "a scroll wrote more than its shown rows' slots, entered more than"
                         " the list, renumbered z or took more than one layout pass"
                     : !editsAbove ? "an edit above the viewport changed a pixel in it, or moved no offset"
                     : !resizeKeeps ? "a narrower list moved the item at the top"
                     : !remeasureEnds ? "the idle re-measure did not end, moved a pixel in the viewport,"
                         " or left the content height off the sum of the measures"
                     : "the demo did not give its primitive ids back")<<"\n";
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario flow: FlowDemo's cards in a layout::Flow, in a stable-gutter
// vertical ScrollControl, the clock frozen and the canvas 480 high. The
// canvas widths are worked out from what the canvas less the Flow's width is
// at 640 (the bar, its margins, the inner panel's margin), so the Flow's own
// width is each breakpoint less 1 and each breakpoint (1023, 1024, 1535,
// 1536); then the canvas is 640, 2560 and 640 again. Each size must show the
// breakpoints' column count, read from the --dump-bounds listing (the cards'
// distinct x), the Flow that wide and its full rows ending at its right
// edge; lay out in one update-phase pass; and settle to frames writing and
// changing nothing. The last 640 must be the first one's listing, renderer
// mirror (slot by owner: a wrapped Text's lines are made and deleted as the
// widths change, so their text ids can come back in another order) and
// pixels. Then at 2560 a card's body grown by a few words that
// keep its row's height (card 0, one sentence, beside card 1's four) must
// move no card, change no pixel outside it and enter no more than the way
// down to it and its subtree (5 entries a control: the ScrollControl measures
// its content in Y again). Last, at 640 scrolled 200 down: a card's text
// growing keeps the offset; the cards replaced in one frame (each
// delayedDelete()d, a new set added) keep it, clamped only to a shorter new
// content's end; and the cards removed in one frame and added in a later one
// clamp it to 0 in between, where it stays.
//-----------------------------------------------------------------------------

namespace
{

// The columns the listing shows for FlowDemo's cards (their distinct x), the
// Flow's width, and whether every full row ends at the Flow's right edge.
struct FlowListing
{
    size_t columns=0;
    int flowWidth=-1;
    bool rowsEnd=true;
    std::vector<gweni::Rect> cards;
};

bool parseBounds(const std::string &line, gweni::Rect &rect)
{
    const size_t at=line.find(" bounds=(");

    return at != std::string::npos
        && std::sscanf(line.c_str()+at, " bounds=(%d, %d, %d, %d)", &rect.x, &rect.y, &rect.w, &rect.h) == 4;
}

FlowListing flowListing(const std::string &listing)
{
    FlowListing result;
    std::istringstream in(listing);
    std::string line;
    std::set<int> xs;

    while(std::getline(in, line))
    {
        const size_t start=line.find_first_not_of(' ');
        gweni::Rect rect;

        if(start == std::string::npos || !parseBounds(line, rect))
            continue;
        if(line.compare(start, 5, "Flow ") == 0)
            result.flowWidth=rect.w;
        else if(line.compare(start, 13, "FlowDemoCard ") == 0)
        {
            result.cards.push_back(rect);
            xs.insert(rect.x);
        }
    }

    result.columns=xs.size();
    for(size_t i=0; result.columns > 0 && i+result.columns <= result.cards.size(); i+=result.columns)
    {
        const gweni::Rect &last=result.cards[i+result.columns-1];

        if(last.x+last.w != result.flowWidth)
            result.rowsEnd=false;
    }
    return result;
}

int expectedFlowColumns(int width)
{
    return (width < gweni::FlowDemo::TwoColumns)?1:(width < gweni::FlowDemo::ThreeColumns)?2:3;
}

std::vector<gweni::Rect> cardBounds(gweni::FlowDemo *demo)
{
    std::vector<gweni::Rect> bounds;

    for(gweni::FlowDemoCard *card : demo->cards())
        bounds.push_back(card->getBounds());
    return bounds;
}

}//namespace

static void collectMirrorByOwner(gweni::controls::Base *control, const std::string &path,
    const gweni::renderer::SlotRenderer::MirrorSnapshot &mirror,
    std::map<std::string, std::vector<uint8_t>> &slots);
static bool mirrorsMatchByOwner(Session &s, const ResizeState &before,
    const std::map<std::string, std::vector<uint8_t>> &beforeSlots, const ResizeState &after);

//-----------------------------------------------------------------------------
// --scenario image-grid: ImageGridDemo's Replace and Free buttons, the clock
// frozen
//
// A click on Replace swaps the first cell's image for another and back, a
// click on Free clears the second cell's and loads it again. Each frame writes
// no slot outside the button and the cell, changes no pixel outside them, and
// keeps the renderer's live image count (the old image freed as the new one
// loads, the cleared one given back and taken again); the second Replace and
// Free give the cell's first pixels back; settled frames write nothing; and
// the demo gives its images and ids back.
//-----------------------------------------------------------------------------

static bool runImageGridScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    InteractionChecks checks;
    checks.scenario="image-grid";

    const size_t idsBefore=livePrimitiveIds()+liveTextIds();
    const size_t imagesBefore=s.renderer->loadedTextureCount();
    gweni::controls::Base *control=addControl(s, "ImageGridDemo");
    auto *demo=gweni::gweni_cast<gweni::ImageGridDemo>(control);

    if(!demo)
    {
        std::cerr<<"ControlTest: image-grid: ImageGridDemo was not built\n";
        return false;
    }

    s.label="image-grid";
    renderFrames(s, s.frames);

    bool ok=shoot(s, shotDir+"/image-grid.png");
    std::vector<uint8_t> pixels;
    size_t idleWork=settleCodeEditor(s, pixels);
    const size_t imagesLoaded=s.renderer->loadedTextureCount();
    const size_t cellImages=gweni::ImageGridDemo::EmptyCell;

    checks.check("images_loaded", imagesLoaded == imagesBefore+cellImages,
        std::to_string(imagesLoaded-imagesBefore)+" images loaded, "+std::to_string(cellImages)+" expected");
    checks.check("empty_and_bad", demo->panels()[gweni::ImageGridDemo::EmptyCell]->isEmpty()
            && demo->panels()[gweni::ImageGridDemo::BadDataCell]->failedToLoad(),
        "the empty cell is not empty or the bad-data cell did not fail");

    // One click on a button, then the mouse off the canvas: what the click's
    // frame wrote, and the pixels once settled.
    struct Click
    {
        CodeEditorFrame frame;
        size_t outside=0;
        bool within=false;
        size_t images=0;
    };
    auto click=[&](gweni::controls::Base *button, size_t cell) -> Click
    {
        std::set<size_t> allowedPrims, allowedTexts;

        subtreeIds(button, allowedPrims, allowedTexts);
        subtreeIds(demo->cells()[cell], allowedPrims, allowedTexts);
        allowedPrims.insert(demo->panels()[cell]->primitiveId());

        const gweni::Rect buttonRect=button->getCanvasBounds();
        const gweni::Rect cellRect=demo->cells()[cell]->getCanvasBounds();

        clickMouse(s, buttonRect.x+buttonRect.w/2, buttonRect.y+buttonRect.h/2);

        Click result;

        result.frame=codeEditorFrame(s);
        forgetMouse(s);
        renderFrames(s, 1);
        // a new caption's glyphs can take new text ids: the cell's after the click count too
        subtreeIds(demo->cells()[cell], allowedPrims, allowedTexts);
        result.within=idsWithin(result.frame.primitives, allowedPrims) && idsWithin(result.frame.texts, allowedTexts);
        result.outside=differencesOutsideRects(s, pixels, result.frame.pixels, {buttonRect, cellRect});
        result.images=s.renderer->loadedTextureCount();
        idleWork+=settleCodeEditor(s, pixels);
        return result;
    };

    const std::vector<uint8_t> first=pixels;
    const gweni::Rect cell0=demo->cells()[0]->getCanvasBounds();
    const gweni::Rect cell1=demo->cells()[1]->getCanvasBounds();

    // (1) Replace and back
    const Click replace=click(demo->replaceButton(), 0);
    const bool replacedPixels=differencesIn(s, first, pixels, cell0) > 0;

    checks.check("replace", demo->replaced() && replace.within && replace.outside == 0
            && replace.images == imagesLoaded && replacedPixels,
        "Replace wrote outside the button and the cell, changed "+std::to_string(replace.outside)
        +" pixels outside them, left "+std::to_string(replace.images)+" images ("+std::to_string(imagesLoaded)
        +" before) or did not change the cell");

    const Click replaceBack=click(demo->replaceButton(), 0);

    checks.check("replace_back", !demo->replaced() && replaceBack.within && replaceBack.outside == 0
            && replaceBack.images == imagesLoaded && differencesIn(s, first, pixels, cell0) == 0,
        "Replace again did not give the cell's first pixels back, wrote outside the button and the cell, or left "
        +std::to_string(replaceBack.images)+" images");

    // (2) Free and load again
    const Click freed=click(demo->freeButton(), 1);
    const bool freedPixels=differencesIn(s, first, pixels, cell1) > 0;

    checks.check("free", demo->freed() && demo->panels()[1]->isEmpty() && freed.within && freed.outside == 0
            && freed.images == imagesLoaded-1 && freedPixels,
        "Free wrote outside the button and the cell, changed "+std::to_string(freed.outside)
        +" pixels outside them, left "+std::to_string(freed.images)+" images ("+std::to_string(imagesLoaded-1)
        +" expected) or did not change the cell");

    const Click loaded=click(demo->freeButton(), 1);

    checks.check("free_back", !demo->freed() && loaded.within && loaded.outside == 0
            && loaded.images == imagesLoaded && differencesIn(s, first, pixels, cell1) == 0,
        "Free again did not give the cell's first pixels back, wrote outside the button and the cell, or left "
        +std::to_string(loaded.images)+" images");

    ok=shoot(s, shotDir+"/image-grid-back.png") && ok;
    checks.check("settled", idleWork == 0, "settled frames wrote or changed "+std::to_string(idleWork));

    dropControl(s, control);
    renderFrames(s, 1);

    const size_t idsLeft=livePrimitiveIds()+liveTextIds()-idsBefore;
    const size_t imagesLeft=s.renderer->loadedTextureCount()-imagesBefore;

    checks.check("ids_back", idsLeft == 0 && imagesLeft == 0,
        std::to_string(idsLeft)+" ids and "+std::to_string(imagesLeft)+" images not given back");

    passed=checks.allPassed();
    std::cout<<"image-grid";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second?"yes":"no");
    std::cout<<" images="<<imagesLoaded-imagesBefore<<" replace_prims="<<replace.frame.primitives.size()
             <<" replace_texts="<<replace.frame.texts.size()<<" free_prims="<<freed.frame.primitives.size()<<"\n";
    return ok;
}

static bool runFlowScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    InteractionChecks checks;
    checks.scenario="flow";

    if(!resizeSession(s, 640, 480))
        return false;

    const size_t idsBefore=livePrimitiveIds()+liveTextIds();
    gweni::controls::Base *control=addControl(s, "FlowDemo");
    auto *demo=gweni::gweni_cast<gweni::FlowDemo>(control);

    if(!demo)
    {
        std::cerr<<"ControlTest: flow: FlowDemo was not built\n";
        return false;
    }

    s.label="flow";

    // the first frame: one pass (--require-one-pass), then settled
    const CodeEditorFrame built=codeEditorFrame(s);
    std::vector<uint8_t> pixels;
    size_t idleWork=settleCodeEditor(s, pixels);
    bool ok=shoot(s, shotDir+"/flow-640.png");
    ResizeState firstState, lastState;

    captureResizeState(s, shotDir+"/flow-640.png", firstState);

    // by owner: a wrapped Text's lines come and go with the widths, so which
    // line holds which text id at the end depends on the sizes in between
    std::map<std::string, std::vector<uint8_t>> firstSlots;

    collectMirrorByOwner(s.canvas, s.canvas->getTypeName(), firstState.mirror, firstSlots);

    // what the canvas has beside the Flow: the bar and the insets
    const int inset=s.size.x-demo->flow()->getWidth();
    std::vector<int> widths;

    for(int boundary : {gweni::FlowDemo::TwoColumns, gweni::FlowDemo::ThreeColumns})
    {
        widths.push_back(boundary-1+inset);
        widths.push_back(boundary+inset);
    }
    widths.push_back(640);
    widths.push_back(2560);
    widths.push_back(640);

    bool sizesOk=true;
    int maxPasses=built.passes;
    std::ostringstream shown;

    for(size_t i=0; i < widths.size(); ++i)
    {
        const int width=widths[i];

        if(!resizeSession(s, width, 480))
            return false;

        const CodeEditorFrame resized=codeEditorFrame(s);
        const size_t work=settleCodeEditor(s, pixels);
        const FlowListing listing=flowListing(boundsListing(s));
        const int flowWidth=width-inset;
        const size_t expected=static_cast<size_t>(expectedFlowColumns(flowWidth));
        const std::string name="flow-"+std::to_string(i+1)+"-"+std::to_string(width)+".png";

        ok=shoot(s, shotDir+"/"+name) && ok;
        maxPasses=std::max(maxPasses, resized.passes);
        idleWork+=work;
        shown<<" "<<listing.flowWidth<<":"<<listing.columns;

        const bool sizeOk=resized.passes == 1 && listing.flowWidth == flowWidth
            && listing.columns == expected && listing.rowsEnd
            && listing.cards.size() == static_cast<size_t>(gweni::FlowDemo::Cards);

        if(!sizeOk)
            std::cerr<<"ControlTest: flow: at a "<<width<<" wide canvas the Flow is "<<listing.flowWidth
                     <<" wide (expected "<<flowWidth<<") with "<<listing.columns<<" columns (expected "
                     <<expected<<"), its full rows "<<(listing.rowsEnd?"":"not ")<<"ending at its edge, "
                     <<listing.cards.size()<<" cards listed, laid out in "<<resized.passes<<" passes\n";
        sizesOk=sizesOk && sizeOk;
    }
    checks.check("columns", sizesOk, "a size showed the wrong columns or took more than one pass");
    checks.check("first_frame", built.passes == 1,
        "the first frame took "+std::to_string(built.passes)+" passes");

    captureResizeState(s, shotDir+"/flow-"+std::to_string(widths.size())+"-640.png", lastState);

    const bool listingBack=listingsMatch(firstState, lastState);
    const bool mirrorBack=mirrorsMatchByOwner(s, firstState, firstSlots, lastState);
    const bool pixelsBack=pixelsMatch(firstState, lastState);

    checks.check("round_trip", listingBack && mirrorBack && pixelsBack,
        "the last 640 differs from the first in its listing, mirror or pixels");

    // a card's text that keeps its row's height: only it, the way down to
    // it, and nothing else moves or changes
    if(!resizeSession(s, 2560, 480))
        return false;
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);

    gweni::FlowDemoCard *card=demo->cards()[0];
    const std::vector<gweni::Rect> before=cardBounds(demo);

    card->setBody(gweni::FlowDemo::cardBody(0, 0)+" It changed.");

    const CodeEditorFrame edit=codeEditorFrame(s);
    const std::vector<gweni::Rect> after=cardBounds(demo);
    const size_t editOutside=differencesOutsideRects(s, pixels, edit.pixels, {card->getCanvasBounds()});
    const size_t editBudget=5*(pathLength(card)+subtreeControls(card));
    size_t allCards=0;

    for(gweni::FlowDemoCard *each : demo->cards())
        allCards+=subtreeControls(each);
    checks.check("card_edit",
        edit.passes == 1 && before == after && editOutside == 0 && edit.visited <= editBudget,
        "a card's edit ran "+std::to_string(edit.passes)+" passes entering "+std::to_string(edit.visited)
        +" (the way down to it and its subtree, "+std::to_string(editBudget)+", allowed; the cards hold "
        +std::to_string(allCards)+" controls), moved "+(before == after?"no card":"a card")+" and changed "
        +std::to_string(editOutside)+" pixels outside it");
    idleWork+=settleCodeEditor(s, pixels);

    // scrolled down at 640: a card growing keeps the offset
    if(!resizeSession(s, 640, 480))
        return false;
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);

    gweni::controls::ScrollControl *scroll=demo->scroll();
    const int offset=200;

    scroll->setVerticalScrollOffset(offset);
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);

    const int maxBefore=scroll->getMaxVerticalScrollOffset();

    demo->cards()[1]->setBody(gweni::FlowDemo::cardBody(1, 0)+" "+gweni::FlowDemo::cardBody(2, 0));

    const CodeEditorFrame grow=codeEditorFrame(s);

    checks.check("grow_keeps_offset",
        maxBefore > offset && grow.passes == 1 && scroll->getVerticalScrollOffset() == offset
            && scroll->getMaxVerticalScrollOffset() > maxBefore,
        "a card's growth left the offset at "+std::to_string(scroll->getVerticalScrollOffset())+" (of "
        +std::to_string(offset)+", the range "+std::to_string(maxBefore)+" -> "
        +std::to_string(scroll->getMaxVerticalScrollOffset())+") in "+std::to_string(grow.passes)+" passes");
    idleWork+=settleCodeEditor(s, pixels);

    // the cards replaced in one frame: the offset kept, clamped only to the
    // new content's end
    demo->rebuildCards();

    const CodeEditorFrame rebuilt=codeEditorFrame(s);
    const int rebuiltOffset=scroll->getVerticalScrollOffset();

    idleWork+=settleCodeEditor(s, pixels);

    const int rebuiltMax=scroll->getMaxVerticalScrollOffset();

    checks.check("rebuild_keeps_offset",
        rebuilt.passes == 1 && rebuiltOffset == std::min(offset, rebuiltMax)
            && scroll->getVerticalScrollOffset() == rebuiltOffset && rebuiltMax > 0,
        "the rebuild left the offset at "+std::to_string(rebuiltOffset)+" then "
        +std::to_string(scroll->getVerticalScrollOffset())+" (of "+std::to_string(offset)
        +", the new range "+std::to_string(rebuiltMax)+") in "+std::to_string(rebuilt.passes)+" passes");

    // removed in one frame and added in a later one: the empty frame clamps
    // the offset to 0, and the new cards start at the top
    demo->removeCards();

    const CodeEditorFrame emptied=codeEditorFrame(s);
    const int emptyOffset=scroll->getVerticalScrollOffset();

    demo->addCards();

    const CodeEditorFrame refilled=codeEditorFrame(s);

    idleWork+=settleCodeEditor(s, pixels);
    checks.check("remove_then_add_clamps",
        emptied.passes == 1 && refilled.passes == 1 && emptyOffset == 0
            && scroll->getVerticalScrollOffset() == 0 && scroll->getMaxVerticalScrollOffset() > offset,
        "the offset was "+std::to_string(emptyOffset)+" with no cards and "
        +std::to_string(scroll->getVerticalScrollOffset())+" once they were back (0 both)");

    checks.check("settled", idleWork == 0, "settled frames wrote or changed "+std::to_string(idleWork));

    dropControl(s, control);
    renderFrames(s, 1);

    const size_t idsLeft=livePrimitiveIds()+liveTextIds()-idsBefore;

    checks.check("ids_back", idsLeft == 0, std::to_string(idsLeft)+" ids not given back");

    passed=checks.allPassed();
    std::cout<<"flow";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second?"yes":"no");
    std::cout<<" inset="<<inset<<" columns="<<shown.str().substr(1)<<" max_passes="<<maxPasses
             <<" edit_visited="<<edit.visited<<" rebuilt_offset="<<rebuiltOffset<<"\n";
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario board: BoardDemo at 640x480, the clock frozen. Every wheel is
// one frame inspected before it renders: the controls with a change pending
// must all be under the ScrollControl it scrolls (a column's body, or the
// Board), and the slots it writes must be the shown slots of that subtree.
// A vertical wheel over the overflowing column's body scrolls it alone; at
// its bottom, at its top and over the fitting column it is consumed with the
// board and the body still and nothing pending anywhere. A vertical wheel
// over the board's inset, a gap between columns and a column header scrolls
// the board; a horizontal wheel and a Shift+wheel over a body scroll the
// board and leave the body. Then, the board and two bodies scrolled, a card
// added to the overflowing column, one added to the fitting column and a
// card's text grown keep every offset, each in one pass; every body's bar
// shows, disabled exactly while its cards fit, and the fitting column's
// cards keep their widths, also once cards added to it make it overflow
// (its bar enabled then). Settled frames write nothing, and the demo gives
// its ids back.
//-----------------------------------------------------------------------------

namespace
{

// One frame of the board scenario: the controls pending outside the allowed
// subtrees as the update phase ends, and what the frame wrote.
struct BoardFrame
{
    std::vector<std::string> strays;
    std::set<size_t> primitives;
    std::set<size_t> texts;
    size_t primitiveSlots=0;
    int passes=0;
};

// The ids the shown controls under `control` hold.
void shownSubtreeIds(gweni::controls::Base *control, std::set<size_t> &primitives, std::set<size_t> &texts)
{
    if(control->hidden())
        return;
    if(const gweni::skin::Control *skinControl=control->getSkinControl())
    {
        std::vector<size_t> primitiveIds, textIds;

        skinControl->appendIds(primitiveIds, textIds);
        primitives.insert(primitiveIds.begin(), primitiveIds.end());
        texts.insert(textIds.begin(), textIds.end());
    }
    for(gweni::controls::Base *child : control->getActualChildren())
        shownSubtreeIds(child, primitives, texts);
}

gweni::controls::BaseScrollBar *boardVerticalBar(gweni::controls::ScrollControl *body)
{
    for(gweni::controls::Base *child : body->getActualChildren())
        if(auto *bar=gweni::gweni_cast<gweni::controls::VerticalScrollBar>(child))
            return bar;
    return nullptr;
}

// Every offset of the board: the board's own, then each body's.
std::vector<int> boardOffsets(gweni::BoardDemo *demo)
{
    std::vector<int> offsets{demo->board()->getHorizontalScrollOffset()};

    for(gweni::controls::BoardColumn *column : demo->columns())
        offsets.push_back(column->getBody()->getVerticalScrollOffset());
    return offsets;
}

std::string offsetsText(const std::vector<int> &offsets)
{
    std::string text;

    for(int offset : offsets)
        text+=(text.empty()?"":",")+std::to_string(offset);
    return text;
}

}//namespace

// The controls with a change pending outside `root`'s subtree, walking the
// tree as it renders: a column's cards are the BoardColumn's children
// (getParent()) but sit in its body's inner panel.
static void boardPendingOutside(gweni::controls::Base *control, gweni::controls::Base *root,
    std::vector<std::string> &strays)
{
    if(control == root)
        return;
    if(control->getStateChange() != gweni::controls::StateChange_Nothing)
        strays.push_back(std::string(control->getTypeName())+"#"+std::to_string(control->getControlId()));
    for(gweni::controls::Base *child : control->getActualChildren())
        boardPendingOutside(child, root, strays);
}

static BoardFrame boardFrame(Session &s, gweni::controls::Base *root)
{
    BoardFrame frame;

    // frameCheckingStrays, with the strays taken by the rendered tree
    gweni::controls::resetChangeStats();
    verifyIdleBefore(s);
    s.canvas->updateCanvas();
    boardPendingOutside(s.canvas, root, frame.strays);

    s.renderer->clearTarget(s.background);
    s.canvas->renderCanvas();
    verifyIdleAfter(s);

    if(s.stats)
        printFrameStats(s.frame, s.label, gweni::controls::getChangeStats(),
            s.renderer->getFrameStats(), s.canvas->getLayoutStats(),
            s.canvas->getUpdateStats());
    checkOnePass(s, std::to_string(s.frame));
    ++s.frame;
    ++g_clockSteps;

    for(size_t id : s.renderer->writtenPrimitiveIds())
        frame.primitives.insert(id);
    for(const gweni::renderer::WrittenText &text : s.renderer->writtenTextIds())
        frame.texts.insert(text.textId);
    frame.primitiveSlots=s.renderer->frameStats().primitiveSlotsWritten;
    frame.passes=s.canvas->getLayoutStats().passes;
    return frame;
}

static bool runBoardScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    InteractionChecks checks;
    checks.scenario="board";

    if(!resizeSession(s, 640, 480))
        return false;

    const size_t idsBefore=livePrimitiveIds()+liveTextIds();
    gweni::controls::Base *control=addControl(s, "BoardDemo");
    auto *demo=gweni::gweni_cast<gweni::BoardDemo>(control);

    if(!demo || demo->columns().size() != gweni::BoardDemo::ColumnCount)
    {
        std::cerr<<"ControlTest: board: BoardDemo was not built\n";
        return false;
    }

    s.label="board";

    std::vector<uint8_t> pixels;
    renderFrames(s, 1);
    size_t idleWork=settleCodeEditor(s, pixels);
    bool ok=shoot(s, shotDir+"/board.png");

    gweni::controls::Board *board=demo->board();
    gweni::controls::ScrollControl *overflowing=demo->columns()[gweni::BoardDemo::OverflowingColumn]->getBody();
    gweni::controls::ScrollControl *fitting=demo->columns()[gweni::BoardDemo::FittingColumn]->getBody();

    checks.check("board_overflows", board->getMaxHorizontalScrollOffset() > 0
            && overflowing->getMaxVerticalScrollOffset() > 0 && fitting->getMaxVerticalScrollOffset() == 0,
        "the board's range is "+std::to_string(board->getMaxHorizontalScrollOffset())
        +", the overflowing body's "+std::to_string(overflowing->getMaxVerticalScrollOffset())
        +" and the fitting body's "+std::to_string(fitting->getMaxVerticalScrollOffset()));

    // the mouse at a point, and the hover it gives settled before the wheel
    auto pointAt=[&](const gweni::Point &point)
    {
        moveMouse(s, point.x, point.y);
        renderFrames(s, 1);
        idleWork+=settleCodeEditor(s, pixels);
    };
    auto bodyPoint=[&](gweni::controls::ScrollControl *body)
    {
        const gweni::Rect &viewport=body->getViewport();

        return body->localPosToCanvas(gweni::Point(viewport.x+viewport.w/2, viewport.y+viewport.h/2));
    };

    // one wheel and its frame: held to `scroll`'s subtree, writing only its
    // shown slots; `moved` the ScrollControl expected to move, or null
    struct WheelResult
    {
        bool consumed=false;
        bool held=false;
        std::vector<int> before, after;
    };
    auto wheel=[&](const std::string &name, gweni::controls::ScrollControl *scroll, gweni::Dim dim,
        bool shift, int delta) -> WheelResult
    {
        WheelResult result;

        result.before=boardOffsets(demo);
        if(shift)
            s.canvas->inputModifierKey(gweni::keys::Shift, true);
        result.consumed=dim == gweni::Dim::X?s.canvas->inputMouseWheelH(delta):s.canvas->inputMouseWheel(delta);
        if(shift)
            s.canvas->inputModifierKey(gweni::keys::Shift, false);

        const BoardFrame frame=boardFrame(s, scroll);
        std::set<size_t> primitives, texts;

        shownSubtreeIds(scroll, primitives, texts);
        result.after=boardOffsets(demo);
        result.held=frame.strays.empty() && idsWithin(frame.primitives, primitives)
            && idsWithin(frame.texts, texts) && frame.primitiveSlots <= primitives.size() && frame.passes <= 1;
        if(!result.held)
            std::cerr<<"ControlTest: board: "<<name<<": "<<frame.strays.size()<<" controls pending outside "
                     <<scroll->getTypeName()<<"#"<<scroll->getControlId()
                     <<(frame.strays.empty()?"":" (first "+frame.strays.front()+")")<<", "
                     <<frame.primitiveSlots<<" primitive slots written of the "<<primitives.size()
                     <<" it shows, ids within it: "<<(idsWithin(frame.primitives, primitives)?"yes":"no")
                     <<"/"<<(idsWithin(frame.texts, texts)?"yes":"no")<<", "<<frame.passes<<" passes\n";
        idleWork+=settleCodeEditor(s, pixels);
        return result;
    };
    // the board's offset is index 0, the overflowing body's 1, the fitting 2
    auto changedOnly=[](const WheelResult &result, size_t index)
    {
        for(size_t i=0; i < result.before.size(); ++i)
        {
            if((i == index) != (result.before[i] != result.after[i]))
                return false;
        }
        return true;
    };
    auto still=[](const WheelResult &result) { return result.before == result.after; };
    auto describe=[](const WheelResult &result)
    {
        return std::string(result.consumed?"consumed":"not consumed")+", offsets "+offsetsText(result.before)
            +" -> "+offsetsText(result.after);
    };
    const size_t boardIndex=0;
    const size_t overflowingIndex=1+gweni::BoardDemo::OverflowingColumn;

    // a vertical wheel over the overflowing body scrolls it alone
    pointAt(bodyPoint(overflowing));
    const WheelResult down=wheel("body_wheel", overflowing, gweni::Dim::Y, false, -120);

    checks.check("body_wheel", down.consumed && down.held && changedOnly(down, overflowingIndex)
            && overflowing->getVerticalScrollOffset() > 0,
        "a wheel over the overflowing body: "+describe(down));
    ok=shoot(s, shotDir+"/board-body-scrolled.png") && ok;

    // at the bottom, at the top and in the fitting body it stops there
    overflowing->setVerticalScrollOffset(overflowing->getMaxVerticalScrollOffset());
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);

    const WheelResult bottom=wheel("body_bottom", overflowing, gweni::Dim::Y, false, -120);

    overflowing->setVerticalScrollOffset(0);
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);

    const WheelResult top=wheel("body_top", overflowing, gweni::Dim::Y, false, 120);

    pointAt(bodyPoint(fitting));

    const WheelResult fits=wheel("body_fits", fitting, gweni::Dim::Y, false, -120);

    checks.check("body_ends_consumed", bottom.consumed && bottom.held && still(bottom)
            && top.consumed && top.held && still(top) && fits.consumed && fits.held && still(fits),
        "at the bottom: "+describe(bottom)+"; at the top: "+describe(top)+"; fitting: "+describe(fits));

    // a vertical wheel over the board's inset, a gap and a header scrolls the board
    auto boardWheel=[&](const std::string &name, const gweni::Point &point,
        const std::function<bool(gweni::controls::Base *)> &hovered, int delta)
    {
        pointAt(point);

        const bool onIt=gweni::HoveredControl && hovered(gweni::HoveredControl);
        const WheelResult result=wheel(name, board, gweni::Dim::Y, false, delta);

        checks.check(name, onIt && result.consumed && result.held && changedOnly(result, boardIndex),
            std::string("the mouse ")+(onIt?"on":"not on")+" its target ("
            +(gweni::HoveredControl?gweni::HoveredControl->getTypeName():"nothing")+"), "+describe(result));
    };

    boardWheel("inset_wheel", board->localPosToCanvas(gweni::Point(2, 20)),
        [&](gweni::controls::Base *at) { return at == board; }, -120);

    gweni::controls::BoardColumn *first=demo->columns()[0];

    board->setHorizontalScrollOffset(0);
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);
    boardWheel("gap_wheel", first->localPosToCanvas(gweni::Point(first->getWidth()+board->getGap()/2,
            first->getHeight()/2)),
        [&](gweni::controls::Base *at) { return at == board->getInnerPanel(); }, -120);
    boardWheel("header_wheel", first->getHeader()->localPosToCanvas(gweni::Point(
            first->getHeader()->getWidth()/2, first->getHeader()->getHeight()/2)),
        [&](gweni::controls::Base *at) { return isUnder(at, first->getHeader()); }, 120);

    // a horizontal wheel and a Shift+wheel over a body scroll the board
    board->setHorizontalScrollOffset(0);
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);
    pointAt(bodyPoint(overflowing));

    const WheelResult horizontal=wheel("horizontal_wheel", board, gweni::Dim::X, false, -120);

    checks.check("horizontal_wheel", horizontal.consumed && horizontal.held
            && changedOnly(horizontal, boardIndex),
        "a horizontal wheel over a body: "+describe(horizontal));
    ok=shoot(s, shotDir+"/board-scrolled.png") && ok;

    const WheelResult shifted=wheel("shift_wheel", board, gweni::Dim::Y, true, -120);

    checks.check("shift_wheel", shifted.consumed && shifted.held && changedOnly(shifted, boardIndex),
        "a Shift+wheel over a body: "+describe(shifted));

    // offsets kept as cards come and text grows
    board->setHorizontalScrollOffset(board->getMaxHorizontalScrollOffset()/2);
    overflowing->setVerticalScrollOffset(overflowing->getMaxVerticalScrollOffset()/2);

    gweni::controls::ScrollControl *third=demo->columns()[2]->getBody();

    third->setVerticalScrollOffset(third->getMaxVerticalScrollOffset());
    renderFrames(s, 1);
    idleWork+=settleCodeEditor(s, pixels);

    const std::vector<int> kept=boardOffsets(demo);
    bool offsetsKept=kept[0] > 0 && kept[overflowingIndex] > 0 && kept[3] > 0;
    std::string offsetsWhy="the offsets "+offsetsText(kept);
    auto keepsOffsets=[&](const std::string &name)
    {
        const CodeEditorFrame frame=codeEditorFrame(s);
        const std::vector<int> now=boardOffsets(demo);

        if(now != kept || frame.passes != 1)
        {
            offsetsKept=false;
            offsetsWhy+="; "+name+" gave "+offsetsText(now)+" in "+std::to_string(frame.passes)+" passes";
        }
        idleWork+=settleCodeEditor(s, pixels);
    };

    demo->addCard(gweni::BoardDemo::OverflowingColumn, "Added card",
        "A card added below the others leaves every scroll position where it was.");
    keepsOffsets("a card added to the overflowing column");

    std::vector<gweni::Rect> fittingBefore;

    for(gweni::BoardDemoCard *card : demo->cards(gweni::BoardDemo::FittingColumn))
        fittingBefore.push_back(card->getBounds());
    demo->addCard(gweni::BoardDemo::FittingColumn, "Fits too", "One more short card.");
    keepsOffsets("a card added to the fitting column");

    bool widthsKept=true;

    for(size_t i=0; i < fittingBefore.size(); ++i)
        widthsKept=widthsKept && demo->cards(gweni::BoardDemo::FittingColumn)[i]->getBounds().w == fittingBefore[i].w;

    gweni::BoardDemoCard *grown=demo->cards(gweni::BoardDemo::OverflowingColumn)[1];

    grown->setBody(grown->body()->getText()+" It grew by a sentence that wraps onto more lines than before.");
    keepsOffsets("a card's text grown");
    checks.check("offsets_kept", offsetsKept, offsetsWhy);
    checks.check("fitting_widths_kept", widthsKept, "a card added to the fitting column changed a card's width");

    // every body's bar shows, disabled exactly while its cards fit
    bool barsOk=true;
    std::string barsWhy;

    for(gweni::controls::BoardColumn *column : demo->columns())
    {
        gweni::controls::ScrollControl *body=column->getBody();
        gweni::controls::BaseScrollBar *bar=boardVerticalBar(body);
        const bool fitsNow=body->getMaxVerticalScrollOffset() == 0;

        if(!bar || !bar->visible() || bar->isDisabled() != fitsNow)
        {
            barsOk=false;
            barsWhy+=" "+column->getText()+(bar && bar->visible()?"":" has no shown bar")
                +(bar && bar->isDisabled() != fitsNow?" bar disabled="+std::to_string(bar->isDisabled())
                    +" with range "+std::to_string(body->getMaxVerticalScrollOffset()):"");
        }
    }
    checks.check("bars", barsOk && fitting->getMaxVerticalScrollOffset() == 0, "the bars:"+barsWhy
        +" (the fitting body's range "+std::to_string(fitting->getMaxVerticalScrollOffset())+")");
    ok=shoot(s, shotDir+"/board-edited.png") && ok;

    // cards added to the fitting column until it overflows: its bar enables,
    // and no card it had changes its width
    gweni::controls::BaseScrollBar *fittingBar=boardVerticalBar(fitting);
    int added=0;
    bool overflowWidthsKept=true;

    for(; added < 20 && fitting->getMaxVerticalScrollOffset() == 0; ++added)
    {
        demo->addCard(gweni::BoardDemo::FittingColumn, "Overflow "+std::to_string(added),
            "One more card, until the column no longer fits its cards.");
        keepsOffsets("card "+std::to_string(added)+" added to the fitting column");
    }
    for(size_t i=0; i < fittingBefore.size(); ++i)
        overflowWidthsKept=overflowWidthsKept
            && demo->cards(gweni::BoardDemo::FittingColumn)[i]->getBounds().w == fittingBefore[i].w;
    checks.check("overflow_widths_kept", fitting->getMaxVerticalScrollOffset() > 0 && overflowWidthsKept
            && fittingBar && fittingBar->visible() && !fittingBar->isDisabled() && offsetsKept,
        std::to_string(added)+" cards added to the fitting column gave it a range of "
        +std::to_string(fitting->getMaxVerticalScrollOffset())+", its cards' widths "
        +(overflowWidthsKept?"kept":"changed")+", its bar "
        +(fittingBar && fittingBar->visible()?(fittingBar->isDisabled()?"disabled":"enabled"):"not shown")
        +"; "+offsetsWhy);

    checks.check("settled", idleWork == 0, "settled frames wrote or changed "+std::to_string(idleWork));

    moveMouse(s, -1, -1);
    dropControl(s, control);
    renderFrames(s, 1);

    const size_t idsLeft=livePrimitiveIds()+liveTextIds()-idsBefore;

    checks.check("ids_back", idsLeft == 0, std::to_string(idsLeft)+" ids not given back");

    passed=checks.allPassed();
    std::cout<<"board";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second?"yes":"no");
    std::cout<<" offsets="<<offsetsText(kept)<<"\n";
    return ok;
}

//-----------------------------------------------------------------------------
// --scenario log-view: LogViewDemo's log filled with 1 000 and then 100 000
// events, the clock frozen (the demo streams nothing) and the idle re-measure
// off, so no frame measures off screen. Each input is one frame, held to the
// part of the view it may touch by the ids it writes and the pixels it
// changes: appends while following (the rows and the bar), a wheel notch up
// (detached: appends then write the bar and the jump button alone), the jump
// button (following again), a replace above the viewport (no pixel in it), a
// canvas resize (the anchor row where it was), a block expanded (that row and
// the rows below it), three rows selected and copied, the selection scrolled
// off screen and back (the same pixels), characters of them dragged over (a
// frame inside one row its highlight alone), copied, scrolled off and back, a
// double-clicked word and a Shift+click, and the cap dropping rows while
// detached (nothing on screen moving). Every settled frame writes and changes
// nothing, the pool and the ids the view holds are the same at both counts,
// and the demo gives every primitive id back.
//-----------------------------------------------------------------------------

namespace
{

struct LogViewRun
{
    size_t count=0;
    size_t rows=0;
    size_t subtree=0;
    size_t prims=0;
    size_t texts=0;
};

}//namespace

static size_t settleLogView(Session &s)
{
    std::vector<uint8_t> pixels;

    return settleCodeEditor(s, pixels);
}

// The ids of the pooled rows' subtrees, and of the bar's.
static void logRowIds(gweni::controls::LogView *log, std::set<size_t> &prims, std::set<size_t> &texts)
{
    for(gweni::controls::Base *row : log->getRows())
        subtreeIds(row, prims, texts);
}

static bool logAtEnd(gweni::controls::LogView *log)
{
    gweni::controls::Base *last=log->rowControl(log->lastRowId());

    return last && last->getY()+last->getHeight() == log->getViewportHeight();
}

// A press in the middle of a row's text, below a block's title line.
static gweni::Point logRowPoint(gweni::controls::LogView *log, gweni::controls::LogView::RowId id)
{
    gweni::controls::Base *row=log->rowControl(id);

    if(!row)
        return gweni::Point(-1, -1);
    return row->localPosToCanvas(gweni::Point(row->getWidth()/2, row->getHeight()-2));
}

// A press past the end of a row's text, at its right edge on its last line.
static gweni::Point logRowEndPoint(gweni::controls::LogView *log, gweni::controls::LogView::RowId id)
{
    gweni::controls::Base *row=log->rowControl(id);

    if(!row)
        return gweni::Point(-1, -1);
    return row->localPosToCanvas(gweni::Point(row->getWidth()-2, row->getHeight()-2));
}

// The canvas point of the caret before a character of a plain row on screen,
// in the middle of its wrapped line.
static gweni::Point logCharPoint(gweni::controls::LogView *log, gweni::controls::LogView::RowId id, size_t character)
{
    gweni::controls::internal::LogViewRow *row=log->rowControl(id);

    if(!row)
        return gweni::Point(-1, -1);

    const gweni::Rect caret=row->getCharacterPosition(static_cast<int>(character));

    return row->localPosToCanvas(gweni::Point(caret.x, caret.y+caret.h/2));
}

// The run of letters, digits and '_' round a byte of an ASCII text.
static std::string wordAround(const std::string &text, size_t at)
{
    const auto word=[](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
    size_t begin=at;
    size_t end=at;

    while(begin > 0 && word(text[begin-1]))
        --begin;
    while(end < text.size() && word(text[end]))
        ++end;
    return text.substr(begin, end-begin);
}

// Characters of three plain rows a, b, c on screen: a drag inside a (each
// frame writing a's highlight slots alone, changing no pixel outside it),
// on from there to the middle of c and Ctrl+C (exactly the characters), the
// selection scrolled off screen and back (the same pixels), a double-click
// on a word of b (that word) and a Shift+click on c (from the word's start).
template<typename Check>
static void runLogViewCharacters(Session &s, gweni::controls::LogView *log, gweni::controls::LogView::RowId a,
    gweni::controls::LogView::RowId b, gweni::controls::LogView::RowId c, const Check &check)
{
    const std::string textA=log->rowText(a);
    const std::string textB=log->rowText(b);
    const std::string textC=log->rowText(c);
    const size_t fromA=textA.size()/2;
    const size_t toC=textC.size()/2;
    const gweni::Rect view=viewportOnCanvas(log);

    check("char_rows", textA.size() > 12 && textC.size() > 4 && log->rowControl(a) && log->rowControl(c),
        "rows too short for a drag inside one");
    if(textA.size() <= 12 || textC.size() <= 4 || !log->rowControl(a) || !log->rowControl(c))
        return;

    // the mouse at the press's place first, so the frames compared hover the
    // same control
    gweni::Point at=logCharPoint(log, a, fromA);

    moveMouse(s, at.x, at.y);
    renderFrames(s, 1);
    settleLogView(s);
    mouseDown(s, at.x, at.y, true);
    renderFrames(s, 1);
    at=logCharPoint(log, a, fromA+2);
    moveMouse(s, at.x, at.y);
    renderFrames(s, 1);
    check("settled_char_drag", settleLogView(s) == 0, "a settled frame in a drag wrote something");

    // the drag inside row a: its own slots alone
    {
        gweni::controls::internal::LogViewRow *row=log->rowControl(a);
        const gweni::Rect rowRect=row->getCanvasBounds();
        const std::vector<uint8_t> before=framePixels(s);

        at=logCharPoint(log, a, fromA+5);
        moveMouse(s, at.x, at.y);

        const CodeEditorFrame frame=codeEditorFrame(s);
        std::vector<size_t> own, ownTexts;

        if(const gweni::skin::Control *skinControl=row->getSkinControl())
            skinControl->appendIds(own, ownTexts);

        const std::set<size_t> allowed(own.begin(), own.end());
        const size_t outside=differencesOutsideRects(s, before, frame.pixels, {rowRect});

        check("char_drag_row", !frame.primitives.empty() && idsWithin(frame.primitives, allowed)
            && frame.texts.empty() && frame.passes <= 1 && outside == 0
            && log->getSelectedText() == textA.substr(fromA, 5),
            "a drag inside a row wrote "+std::to_string(frame.primitives.size())+" primitive and "
            +std::to_string(frame.texts.size())+" text ids not all its highlight's, or changed "
            +std::to_string(outside)+" pixels outside it, or selected \""+log->getSelectedText()+"\"");
    }

    // on to the middle of row c, two rows down, and Ctrl+C
    at=logCharPoint(log, c, toC);
    moveMouse(s, at.x, at.y);
    renderFrames(s, 1);
    mouseUp(s, at.x, at.y);
    renderFrames(s, 1);
    gweni::platform::setClipboardText("");
    sendKey(s, "ctrl+c");
    renderFrames(s, 1);

    const std::string expected=textA.substr(fromA)+"\n"+textB+"\n"+textC.substr(0, toC);

    check("char_copy", gweni::platform::getClipboardText() == expected,
        "Ctrl+C gave \""+gweni::platform::getClipboardText()+"\", not \""+expected+"\"");
    check("settled_char_select", settleLogView(s) == 0, "a settled frame with the selection wrote something");

    // scrolled off screen and back: the same pixels, row a still in part
    {
        const std::vector<uint8_t> selected=framePixels(s);
        const size_t anchor=log->getAnchorItem();
        const int anchorOffset=log->getAnchorOffset();

        log->setVerticalScrollOffset(0);
        renderFrames(s, s.frames);

        // the rows the pool gave the top's rows show none of it
        bool off=!log->rowControl(a);

        for(gweni::controls::Base *control : log->getRows())
        {
            auto *row=gweni::gweni_cast<gweni::controls::internal::LogViewRow>(control);
            const size_t item=log->itemForRow(control);

            if(row && !row->hidden() && item != gweni::controls::VirtualList::NoItem)
                off=off && row->getSelection().selected == log->isRowSelected(log->rowIdAt(item));
        }

        log->setVerticalScrollOffset(static_cast<int>(log->getHeightIndex().offsetOf(anchor))+anchorOffset);
        renderFrames(s, s.frames);

        gweni::controls::internal::LogViewRow *row=log->rowControl(a);

        check("char_selection_back", off && differencesIn(s, selected, framePixels(s), view) == 0 && row
            && row->getSelection().selected && !row->getSelection().whole,
            "the character selection scrolled off screen and back does not show as it did");
        settleLogView(s);
    }

    // a double-click on a word of row b, then Shift+click on row c
    {
        const size_t space=textB.find(' ', textB.size()/3);
        const size_t inWord=(space == std::string::npos)?1:space+2;
        const std::string word=wordAround(textB, inWord);

        at=logCharPoint(log, b, inWord);
        doubleClickMouse(s, at.x, at.y);
        renderFrames(s, 1);
        check("double_click_word", !word.empty() && log->getSelectedText() == word,
            "a double-click selected \""+log->getSelectedText()+"\", not the word \""+word+"\"");
        check("settled_word", settleLogView(s) == 0, "a settled frame with the word selected wrote something");

        const size_t wordStart=textB.find(word, inWord+1 > word.size()?inWord+1-word.size():0);
        const size_t toShift=toC/2;

        at=logCharPoint(log, c, toShift);
        glfwKey(s, GLFW_KEY_LEFT_SHIFT, GLFW_PRESS, GLFW_MOD_SHIFT);
        clickMouse(s, at.x, at.y);
        glfwKey(s, GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE, 0);
        renderFrames(s, 1);

        const std::string extended=textB.substr(wordStart)+"\n"+textC.substr(0, toShift);

        check("shift_click_extends", wordStart != std::string::npos && log->getSelectedText() == extended,
            "Shift+click selected \""+log->getSelectedText()+"\", not \""+extended+"\"");
        check("settled_shift_click", settleLogView(s) == 0, "a settled frame after Shift+click wrote something");
    }
}

static bool runLogViewCount(Session &s, const std::string &shotDir, size_t count, InteractionChecks &checks,
    LogViewRun &run)
{
    using RowId=gweni::controls::LogView::RowId;

    const size_t idsBefore=livePrimitiveIds();
    gweni::controls::Base *control=addControl(s, "LogViewDemo");
    auto *demo=gweni::gweni_cast<gweni::LogViewDemo>(control);

    if(!demo || !demo->m_log)
    {
        std::cerr<<"ControlTest: log-view: LogViewDemo has no log\n";
        return false;
    }

    gweni::controls::LogView *log=demo->m_log;
    const std::string n=std::to_string(count);

    s.label="LogViewDemo-"+n;
    // the scenario's rows are its own: a press steps the clock (for a
    // double-click), which the demo's stream would read
    gweni::anim::cancel(demo);
    log->setRemeasureBudget(0);
    log->clear();
    for(size_t i=0; i < count; ++i)
        log->append(gweni::LogViewDemo::event(static_cast<long long>(i)));
    renderFrames(s, s.frames);

    run.count=count;
    run.rows=log->getRows().size();
    run.subtree=subtreeControls(log);
    {
        std::set<size_t> prims, texts;

        subtreeIds(log, prims, texts);
        run.prims=prims.size();
        run.texts=texts.size();
    }

    gweni::controls::Base *bar=findControl(log, "VerticalScrollBar", 0);
    const auto check=[&](const std::string &name, bool passed, const std::string &why)
    {
        checks.check(name+"_"+n, passed, why+" ("+n+" rows)");
    };

    // the keyboard is the log's from the start, so no press below changes
    // its field's look
    log->focus();
    renderFrames(s, 1);
    check("settled", settleLogView(s) == 0, "a settled frame wrote or changed something");
    check("following", log->isFollowing() && logAtEnd(log), "the filled log does not follow its end");

    // Appends while following: the rows and the bar alone, the last row at the
    // bottom in the frame.
    {
        std::vector<uint8_t> before=framePixels(s);
        std::set<size_t> prims, texts;

        logRowIds(log, prims, texts);
        if(bar)
            subtreeIds(bar, prims, texts);
        for(int i=0; i < 3; ++i)
            log->append(gweni::LogViewDemo::event(static_cast<long long>(count)+i));

        const CodeEditorFrame frame=codeEditorFrame(s);
        const size_t outside=differencesOutsideRects(s, before, frame.pixels, {log->getCanvasBounds()});

        // a row given a text of more lines makes line Texts: their ids are
        // the rows' too
        logRowIds(log, prims, texts);
        check("follow_slots", frame.written > 0 && idsWithin(frame.primitives, prims) && idsWithin(frame.texts, texts)
            && frame.passes <= 1, "an append while following wrote a slot outside the rows and the bar");
        check("follow_end", logAtEnd(log), "the last row is not at the viewport's bottom in the append's frame");
        check("follow_pixels", outside == 0, std::to_string(outside)+" pixels changed outside the log");
        check("settled_follow", settleLogView(s) == 0, "a settled frame after the appends wrote something");
    }

    // A wheel notch up detaches; appends then write no row slot and move
    // nothing on screen but the bar and the jump button.
    {
        const gweni::Rect bounds=log->getCanvasBounds();

        moveMouse(s, bounds.x+bounds.w/3, bounds.y+bounds.h/2);
        renderFrames(s, 1);
        settleLogView(s);
        scrollWheel(s, 60);
        renderFrames(s, 1);
        check("wheel_detaches", !log->isFollowing() && log->getJumpButton() && !log->getJumpButton()->hidden(),
            "a wheel notch up did not detach the view and show the jump button");
        check("settled_detached", settleLogView(s) == 0, "a settled detached frame wrote something");

        gweni::controls::Base *jump=log->getJumpButton();
        std::set<size_t> rowPrims, rowTexts, allowedPrims, allowedTexts;

        logRowIds(log, rowPrims, rowTexts);
        if(bar)
            subtreeIds(bar, allowedPrims, allowedTexts);
        if(jump)
            subtreeIds(jump, allowedPrims, allowedTexts);

        const std::vector<uint8_t> before=framePixels(s);
        const gweni::Rect jumpBefore=jump?jump->getCanvasBounds():gweni::Rect();

        for(int i=0; i < 5; ++i)
            log->append(gweni::LogViewDemo::event(static_cast<long long>(count)+3+i));

        const CodeEditorFrame frame=codeEditorFrame(s);
        bool rowWritten=false;

        for(size_t id : frame.primitives)
            rowWritten=rowWritten || rowPrims.count(id);
        for(size_t id : frame.texts)
            rowWritten=rowWritten || rowTexts.count(id);

        const size_t outside=differencesOutsideRects(s, before, frame.pixels,
            {bar?bar->getCanvasBounds():gweni::Rect(), jumpBefore, jump?jump->getCanvasBounds():gweni::Rect()});

        check("detached_slots", !rowWritten && idsWithin(frame.primitives, allowedPrims)
            && idsWithin(frame.texts, allowedTexts) && frame.passes <= 1,
            "an append while detached wrote a row's slot, or one outside the bar and the jump button");
        check("detached_pixels", outside == 0,
            std::to_string(outside)+" pixels changed outside the bar and the jump button");
        check("jump_count", jump && jump->getValue() == gweni::controls::LogView::jumpText(5),
            "the jump button does not count the 5 rows appended");
        check("settled_detached_append", settleLogView(s) == 0, "a settled frame after the appends wrote something");

        // the jump button follows again
        if(jump)
        {
            const gweni::Rect at=jump->getCanvasBounds();

            clickMouse(s, at.x+at.w/2, at.y+at.h/2);
        }
        renderFrames(s, 1);
        check("jump_follows", log->isFollowing() && logAtEnd(log) && jump && jump->hidden(),
            "the jump button did not follow the log again");
        check("settled_jump", settleLogView(s) == 0, "a settled frame after the jump wrote something");
    }

    // Detached in the middle: a replace above the viewport changes no pixel
    // in it.
    const RowId middle=log->rowIdAt(log->getRowCount()/2);

    log->scrollToRow(middle);
    renderFrames(s, s.frames);
    settleLogView(s);

    const gweni::Rect view=viewportOnCanvas(log);
    {
        const std::vector<uint8_t> before=framePixels(s);
        const int offset=log->getVerticalScrollOffset();
        const RowId above=log->rowIdAt(log->rowPosition(middle)-5);

        log->replace(above, gweni::controls::LogRow(log->rowText(above)+", now with a sentence or two more, so"
            " that it takes more lines than it did at the log's width, and then some more"));
        renderFrames(s, 1);
        check("replace_above_still", differencesIn(s, before, framePixels(s), view) == 0
            && log->getVerticalScrollOffset() != offset && log->rowControl(middle)
            && log->rowControl(middle)->getY() == 0,
            "a replace above the viewport changed a pixel in it, or moved no offset");
        check("settled_replace", settleLogView(s) == 0, "a settled frame after the replace wrote something");
    }

    // A canvas resize keeps the anchor row at the top.
    {
        const gweni::Point size=s.size;
        const RowId anchor=log->rowIdAt(log->getAnchorItem());

        resizeSession(s, size.x-120, size.y);
        renderFrames(s, s.frames);

        const bool narrower=log->rowIdAt(log->getAnchorItem()) == anchor && log->rowControl(anchor)
            && log->rowControl(anchor)->getY() == 0;

        resizeSession(s, size.x, size.y);
        renderFrames(s, s.frames);
        check("resize_keeps_anchor", narrower && log->rowIdAt(log->getAnchorItem()) == anchor
            && log->rowControl(anchor) && log->rowControl(anchor)->getY() == 0,
            "a canvas resize moved the row at the top");
        check("settled_resize", settleLogView(s) == 0, "a settled frame after the resize wrote something");
    }

    // A block expanded: that row and the rows below it on screen alone.
    {
        RowId block=gweni::controls::LogView::NoRow;

        for(size_t i=log->getAnchorItem()+1; i < log->getRowCount() && log->rowForItem(i); ++i)
        {
            const gweni::controls::LogRow *row=log->getRow(log->rowIdAt(i));

            if(row && row->kind == gweni::controls::LogRow::Block && row->collapsed
                && row->lines.size() > log->getBlockPreviewLines())
            {
                block=log->rowIdAt(i);
                break;
            }
        }

        gweni::controls::Base *row=log->rowControl(block);

        check("block_on_screen", row != nullptr, "no collapsed block on screen below the top row");
        if(row)
        {
            const gweni::Rect rowRect=row->getCanvasBounds();
            const int height=row->getHeight();
            const gweni::Point title=row->localPosToCanvas(gweni::Point(
                gweni::controls::LogView::RowPadX+gweni::controls::LogView::ChevronColumn+20,
                gweni::controls::LogView::RowPadY+log->getLineHeight()/2));

            // the mouse there first, so the frames compared hover the same
            // control (the jump button the click on it left hovered)
            moveMouse(s, title.x, title.y);
            renderFrames(s, 1);
            settleLogView(s);

            const std::vector<uint8_t> before=framePixels(s);

            clickMouse(s, title.x, title.y);

            const CodeEditorFrame frame=codeEditorFrame(s);
            const gweni::Rect below(view.x, rowRect.y, view.w, view.y+view.h-rowRect.y);
            const size_t outside=differencesOutsideRects(s, before, frame.pixels,
                {below, bar?bar->getCanvasBounds():gweni::Rect()});

            check("expand_grows", !log->getRow(block)->collapsed && log->rowControl(block)
                && log->rowControl(block)->getHeight() > height
                && log->rowControl(block)->getCanvasBounds().y == rowRect.y,
                "a press on the block's title did not expand it in place");
            check("expand_pixels", outside == 0 && frame.passes <= 1,
                std::to_string(outside)+" pixels changed above the block or outside the log");
            check("settled_expand", settleLogView(s) == 0, "a settled frame after the expand wrote something");

            clickMouse(s, title.x, title.y);
            renderFrames(s, 1);
            check("collapse_back", log->getRow(block)->collapsed && framePixels(s) == before,
                "a second press did not give the collapsed block's frame back");
            settleLogView(s);
        }
    }

    // Three rows selected and copied; the selection scrolled off screen and
    // back shows as it did.
    {
        size_t first=gweni::controls::VirtualList::NoItem;

        for(size_t i=log->getAnchorItem()+1; i+2 < log->getRowCount(); ++i)
        {
            bool plain=true;

            for(size_t k=0; k < 3; ++k)
            {
                const gweni::controls::LogRow *row=log->getRow(log->rowIdAt(i+k));

                plain=plain && row && row->kind == gweni::controls::LogRow::Plain && log->rowForItem(i+k);
            }
            if(plain)
            {
                first=i;
                break;
            }
        }
        check("plain_rows_on_screen", first != gweni::controls::VirtualList::NoItem,
            "no three plain rows on screen together");
        if(first != gweni::controls::VirtualList::NoItem)
        {
            const RowId a=log->rowIdAt(first);
            const RowId c=log->rowIdAt(first+2);
            const gweni::Point at=logRowPoint(log, a);
            const gweni::Point to=logRowEndPoint(log, c);

            clickMouse(s, at.x, at.y);
            renderFrames(s, 1);
            glfwKey(s, GLFW_KEY_LEFT_SHIFT, GLFW_PRESS, GLFW_MOD_SHIFT);
            clickMouse(s, to.x, to.y);
            glfwKey(s, GLFW_KEY_LEFT_SHIFT, GLFW_RELEASE, 0);
            renderFrames(s, 1);
            gweni::platform::setClipboardText("");
            sendKey(s, "ctrl+c");
            renderFrames(s, 1);

            const std::string expected=log->rowText(a)+"\n"+log->rowText(log->rowIdAt(first+1))+"\n"
                +log->rowText(c);

            check("copy_three", gweni::platform::getClipboardText() == expected,
                "Ctrl+C gave \""+gweni::platform::getClipboardText()+"\", not the three rows' text");
            check("settled_select", settleLogView(s) == 0, "a settled frame with the selection wrote something");

            const std::vector<uint8_t> selected=framePixels(s);
            const size_t anchor=log->getAnchorItem();
            const int anchorOffset=log->getAnchorOffset();

            // to the top (whose rows are measured there, moving every offset
            // below them) and back to the same row at the top, by the heights
            // as they are then; the bar's thumb is the new heights', so the
            // viewport is compared
            log->setVerticalScrollOffset(0);
            renderFrames(s, s.frames);

            // the rows the pool gave the top's rows are marked as their rows are
            bool marks=true;

            for(gweni::controls::Base *control : log->getRows())
            {
                auto *row=gweni::gweni_cast<gweni::controls::internal::LogViewRow>(control);
                const size_t item=log->itemForRow(control);

                if(row && !row->hidden() && item != gweni::controls::VirtualList::NoItem)
                    marks=marks && row->isSelected() == log->isRowSelected(log->rowIdAt(item));
            }
            check("selection_off_screen", marks && !log->rowControl(a),
                "a row shown at the top is marked selected otherwise than its row is");
            log->setVerticalScrollOffset(static_cast<int>(log->getHeightIndex().offsetOf(anchor))+anchorOffset);
            renderFrames(s, s.frames);
            check("selection_back", differencesIn(s, selected, framePixels(s), view) == 0
                && log->getAnchorItem() == anchor && log->rowControl(a) && log->rowControl(a)->isSelected(),
                "the selection scrolled off screen and back does not show as it did");
            settleLogView(s);

            runLogViewCharacters(s, log, a, log->rowIdAt(first+1), c, check);
        }
    }

    // The cap drops rows above while detached: nothing on screen moves.
    {
        gweni::controls::Base *jump=log->getJumpButton();
        const std::vector<uint8_t> before=framePixels(s);
        const gweni::Rect jumpBefore=jump?jump->getCanvasBounds():gweni::Rect();
        const RowId firstBefore=log->firstRowId();

        log->setMaxRows(log->getRowCount());
        for(int i=0; i < 10; ++i)
            log->append(gweni::LogViewDemo::event(static_cast<long long>(count)+100+i));
        renderFrames(s, 1);

        const size_t outside=differencesOutsideRects(s, before, framePixels(s),
            {bar?bar->getCanvasBounds():gweni::Rect(), jumpBefore, jump?jump->getCanvasBounds():gweni::Rect()});

        check("cap_still", !log->isFollowing() && log->firstRowId() == firstBefore+10 && outside == 0,
            std::to_string(outside)+" pixels changed outside the bar and the jump button as the cap dropped rows");
        check("settled_cap", settleLogView(s) == 0, "a settled frame after the cap's drop wrote something");
        log->setMaxRows(0);
    }

    const bool shot=shoot(s, shotDir+"/log-view-"+n+".png");

    forgetMouse(s);
    dropControl(s, control);
    renderFrames(s, 1);
    check("ids_back", livePrimitiveIds() == idsBefore, "the demo did not give its primitive ids back");
    return shot;
}

static bool runLogViewScenario(Session &s, const std::string &shotDir, bool &passed)
{
    s.freezeClock=true;

    InteractionChecks checks;
    checks.scenario="log-view";

    LogViewRun small, large;
    bool ok=runLogViewCount(s, shotDir, 1000, checks, small);

    ok=runLogViewCount(s, shotDir, 100000, checks, large) && ok;
    checks.check("same_pool", small.rows == large.rows && small.subtree == large.subtree
        && small.prims == large.prims && small.texts == large.texts,
        "the view holds "+std::to_string(large.rows)+" rows, "+std::to_string(large.subtree)+" controls, "
        +std::to_string(large.prims)+" primitive and "+std::to_string(large.texts)+" text ids at 100 000 rows, "
        +std::to_string(small.rows)+", "+std::to_string(small.subtree)+", "+std::to_string(small.prims)+" and "
        +std::to_string(small.texts)+" at 1 000");

    for(const LogViewRun *run : {&small, &large})
        std::cout<<"log-view count="<<run->count<<" rows="<<run->rows<<" subtree="<<run->subtree
                 <<" prim_ids="<<run->prims<<" text_ids="<<run->texts<<"\n";
    std::cout<<"log-view";
    for(const auto &result : checks.results)
        std::cout<<" "<<result.first<<"="<<(result.second ? "yes" : "no");
    std::cout<<"\n";

    passed=checks.allPassed();
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
    g_clockSteps=0;
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
    verifyIdleBefore(s);
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
    verifyIdleAfter(s);
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
        // a Control whose ids are all text ids (a Text, a CodeEditor line's
        // CodeLineText) is read from the text mirror
        std::vector<size_t> primitiveIds, textIds;

        skinControl->appendIds(primitiveIds, textIds);

        const bool text=primitiveIds.empty() && !textIds.empty();
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
    g_clockSteps=0;
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

//! Posts to the window canvas (Canvas::post) a function that marks it for a
//! frame (redraw()), as a callback of the page's own would: an idle page draws
//! one frame for it. Returns "posted", or "no canvas" before the window has one.
extern "C" EMSCRIPTEN_KEEPALIVE const char *gweni_web_post()
{
    if(!g_canvas || !g_canvas->post([]() {
            if(g_canvas)
                g_canvas->redraw();
        }))
        return "no canvas";
    return "posted";
}

//! The keyboard-focused control's text (a Label's, which every TextBox is, or
//! a CodeEditor's caret line), or an empty string if it has none.
extern "C" EMSCRIPTEN_KEEPALIVE const char *gweni_web_focused_text()
{
    static std::string text;

    text.clear();
    focusedText(text);
    return text.c_str();
}

//! The window canvas's ImageGridDemo, or null
static gweni::ImageGridDemo *windowImageGrid(gweni::controls::Base *control)
{
    if(auto *grid=gweni::gweni_cast<gweni::ImageGridDemo>(control))
        return grid;
    for(gweni::controls::Base *child : control->getChildren())
    {
        if(gweni::ImageGridDemo *grid=windowImageGrid(child))
            return grid;
    }
    return nullptr;
}

//! Gives cell \p cell of the window's ImageGridDemo (ControlTest.html?control=
//! ImageGridDemo) the \p size encoded bytes at \p bytes (a PNG or JPEG), kept as
//! the demo keeps its own (ImagePanel::setImageData, at its max size): what a
//! page that fetched an image does with it, having copied the response into a
//! buffer from Module._malloc, which it frees after the call (the bytes are not
//! kept). Returns "status=loaded size=<w>x<h> rect=<x>,<y>,<w>,<h>" (the
//! image's texture size and the panel's canvas rect, logical units),
//! "status=failed rect=...", or "no grid" / "no cell".
extern "C" EMSCRIPTEN_KEEPALIVE const char *gweni_web_set_image(int cell, const uint8_t *bytes, int size)
{
    static std::string result;

    gweni::ImageGridDemo *grid=g_canvas?windowImageGrid(g_canvas):nullptr;

    if(!grid)
        return "no grid";
    if(cell < 0 || (size_t)cell >= grid->panels().size())
        return "no cell";

    gweni::controls::ImagePanel *panel=grid->panels()[cell];

    panel->setImageData(bytes, size > 0 ? (size_t)size : 0, gweni::ImageGridDemo::MaxImageSize);

    const gweni::Rect rect=panel->getCanvasBounds();

    result=panel->failedToLoad()?"status=failed":
        "status=loaded size="+std::to_string(panel->textureWidth())+"x"+std::to_string(panel->textureHeight());
    result+=" rect="+std::to_string(rect.x)+","+std::to_string(rect.y)+","+std::to_string(rect.w)+","
        +std::to_string(rect.h);
    return result.c_str();
}

//! Opens the window canvas's depth view (\p on 1) or closes it (0); any other
//! value leaves it as it is. Returns "view=open" or "view=closed", after. The
//! gallery page's "Depth view" checkbox calls it (tools/site/controltest.html).
extern "C" EMSCRIPTEN_KEEPALIVE const char *gweni_web_depth_view(int on)
{
    using gweni::tools::DepthView;

    static std::string state;

    if(g_canvas)
    {
        if(on == 1 && !DepthView::find(g_canvas))
            DepthView::open(g_canvas);
        else if(on == 0 && DepthView::find(g_canvas))
            DepthView::close(g_canvas);
    }
    state=std::string("view=")+(g_canvas && DepthView::find(g_canvas) ? "open" : "closed");
    return state.c_str();
}

//! Tells the page when the window canvas's depth view opens or closes, however
//! (F9, Escape, --depth-view, gweni_web_depth_view), by calling
//! Module.gweniDepthViewChanged(open) if the page set one, so its checkbox
//! follows the view.
struct WebDepthViewNotice: public gweni::event::Handler
{
    void onOpenChanged(gweni::event::Info info)
    {
        if(info.control != g_canvas)
            return;
        EM_ASM({
            if(Module['gweniDepthViewChanged'])
                Module['gweniDepthViewChanged']($0 != 0);
        }, (int)info.integer);
    }
};
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
            ++g_clockSteps;
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
    g_clockSteps=0;

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

//-----------------------------------------------------------------------------
// --idle-test: the idle window loop, measured (window_idle, tests/windowIdle.cmake).
// It runs the window's own loop (Device::runWindowLoop, input::GlfwLoop, idle)
// over --control with no input callbacks, and from another thread: once the
// canvas has settled (an iteration after which it has no work due) it lets the
// window idle for kIdleSeconds and then posts to the canvas (Canvas::post). The
// post, on the UI thread, reads the loop's counts: no frame drawn and no swap or
// present while idle, and the process's CPU time (getrusage, every thread, the
// rasteriser's included) under kIdleCpuSeconds. It sets a Button's text, so the
// frame it wakes writes that Button's glyphs, and a canvas timer closes the window
// kAfterPostSeconds later, after it counts exactly one frame (and one swap or
// present) drawn for the post.
//-----------------------------------------------------------------------------

#if !defined(__EMSCRIPTEN__)
static constexpr double kIdleSeconds=2.0;
static constexpr double kAfterPostSeconds=1.0;
static constexpr double kIdleCpuSeconds=0.1;

static double processCpuSeconds()
{
    rusage usage{};

    getrusage(RUSAGE_SELF, &usage);
    return double(usage.ru_utime.tv_sec)+double(usage.ru_utime.tv_usec)*1e-6
        +double(usage.ru_stime.tv_sec)+double(usage.ru_stime.tv_usec)*1e-6;
}

static bool runIdleTest(gweni::capture::Device &device, gweni::controls::Canvas *canvas,
    gweni::renderer::SlotRenderer *renderer, const gweni::Color &background,
    gweni::controls::Base *control, gweni::platform::LoopOptions options)
{
    typedef std::chrono::steady_clock Clock;

    GLFWwindow *window=device.context().window;
    const gweni::capture::Device::WindowStats *stats=device.windowStats();
    // a GL window swaps once a drawn frame; a Vulkan one counts its presents
    auto presents=[&](const gweni::platform::IdleLoop &loop) {
        return stats ? stats->presents : long(loop.frames());
    };

    std::mutex mutex;
    std::condition_variable changed;
    bool settled=false;
    std::atomic<bool> postRan{false};
    std::atomic<bool> ok{true};

    const gweni::platform::IdleLoop *loop=nullptr;
    long baseFrames=0;
    long basePresents=0;
    double baseCpu=0.0;
    Clock::time_point baseWall;
    long postFrames=0;
    long postPresents=0;
    bool checkPostFrame=false;

    gweni::controls::Base *button=findControl(control, "Button");

    options.mode=gweni::platform::LoopMode::Idle;

    std::thread worker([&]() {
        {
            std::unique_lock<std::mutex> lock(mutex);

            if(!changed.wait_for(lock, std::chrono::seconds(30), [&]() { return settled; }))
            {
                std::cerr<<"ControlTest: --idle-test: the canvas never settled (it had work after every frame for 30 s)\n";
                ok=false;
            }
        }
        std::this_thread::sleep_for(std::chrono::duration<double>(kIdleSeconds));

        // what the loop did while idle, read on the UI thread when the post runs
        canvas->post([&]() {
            postRan=true;

            // the frame this post runs in was counted as it began
            const long idleFrames=long(loop->frames())-1-baseFrames;
            const long idlePresents=presents(*loop)-basePresents-(stats ? 0 : 1);
            const double cpu=processCpuSeconds()-baseCpu;
            const double wall=std::chrono::duration<double>(Clock::now()-baseWall).count();

            std::cout<<"idle-test: idle frames="<<idleFrames<<" presents="<<idlePresents
                     <<" cpu_s="<<cpu<<" wall_s="<<wall<<" waits="<<loop->waits()<<"\n";
            if(idleFrames != 0 || idlePresents != 0)
            {
                std::cerr<<"ControlTest: --idle-test: "<<idleFrames<<" frame(s) and "<<idlePresents
                         <<" swap(s) or present(s) while idle\n";
                ok=false;
            }
            if(cpu > kIdleCpuSeconds)
            {
                std::cerr<<"ControlTest: --idle-test: "<<cpu<<" s of CPU over "<<wall
                         <<" s idle (more than "<<kIdleCpuSeconds<<" s)\n";
                ok=false;
            }

            postFrames=long(loop->frames());
            postPresents=presents(*loop);
            checkPostFrame=true;
            if(button)
                static_cast<gweni::controls::Button *>(button)->setText("Posted");

            canvas->after(kAfterPostSeconds, [&]() {
                // postFrames counted the post's frame and postPresents did not
                // (it presents after this update), so the frames since are the
                // ones after it, this timer's included, and the presents the
                // post frame's and any after it (on GL a present is a frame)
                const long between=long(loop->frames())-postFrames-1;
                const long shown=presents(*loop)-postPresents;

                std::cout<<"idle-test: after post frames="<<between<<" presents="<<shown<<"\n";
                if(between != 0 || shown != 1)
                {
                    std::cerr<<"ControlTest: --idle-test: after the post's frame, "<<between
                             <<" frame(s) were drawn before the timer's and "<<shown
                             <<" swap(s) or present(s) made from the post's frame on; none and one were wanted\n";
                    ok=false;
                }
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            });
        });

        // a loop the post did not wake would wait for an event forever
        const Clock::time_point posted=Clock::now();

        while(!postRan && (Clock::now()-posted) < std::chrono::seconds(10))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if(!postRan)
        {
            std::cerr<<"ControlTest: --idle-test: the post did not wake the loop within 10 s\n";
            ok=false;
            glfwPostEmptyEvent();
        }
    });

    device.runWindowLoop(canvas, renderer, options, [canvas, renderer, background]()
    {
        canvas->updateCanvas();
        renderer->clearTarget(background);
        canvas->renderCanvas();
    },
    [&](const gweni::platform::IdleLoop &iterated) {
        loop=&iterated;
        if(checkPostFrame)
        {
            checkPostFrame=false;
            if(renderer->frameStats().textSlotsWritten == 0)
            {
                std::cerr<<"ControlTest: --idle-test: the frame the post woke wrote no glyph slot\n";
                ok=false;
            }
        }

        std::lock_guard<std::mutex> lock(mutex);

        if(settled || (canvas->nextWorkTime() <= gweni::platform::getTimeInSeconds()))
            return;
        settled=true;
        baseFrames=long(iterated.frames());
        basePresents=presents(iterated);
        baseCpu=processCpuSeconds();
        baseWall=Clock::now();
        changed.notify_all();
    });
    worker.join();

    if(!button)
    {
        std::cerr<<"ControlTest: --idle-test: the control has no Button to mark\n";
        ok=false;
    }
    std::cout<<"idle-test: "<<(ok ? "passed" : "failed")<<"\n";
    return ok;
}
#endif

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
    bool idleTest=false;
    gweni::platform::LoopOptions loopOptions=gweni::capture::loopOptions();
    g_verifyIdle=loopOptions.verify;
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
    bool clipboardTest=false;
    bool waitsForClipboard=false;

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
        else if(arg == "--idle-test") idleTest=true;
        else if(arg == "--frames" && hasValue)
        {
            frames=std::atoi(argv[++i]);
            steps.push_back({StepKind::Frames, frames, 0, {}});
        }
        else if(arg == "--shot-all") shotAll=true;
        else if(arg == "--headless") forceHeadless=true;
        else if(arg == "--stats-overlay") g_statsOverlay=gweni::capture::onOffArgument(argc, argv, i);
        else if(arg == "--depth-view") g_depthView=gweni::capture::onOffArgument(argc, argv, i);
        else if(arg == "--loop")
        {
            if(!gweni::capture::loopArgument(argc, argv, i, loopOptions))
                return EXIT_FAILURE;
        }
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
        else if(arg == "--verify-idle") g_verifyIdle=true;
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
        else if(arg == "--clipboard-test") clipboardTest=true;
        else if(arg == "--wait-clipboard-change" && hasValue)
        {
            scripted=true;
            waitsForClipboard=true;
            steps.push_back({StepKind::WaitClipboard, std::atoi(argv[++i]), 0, {}});
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
                           " text-size, scale, tooltip, context-menu, popup, dialog, toast,"
                           " virtual-list, hide-show, demo-labels or plot-histogram\n";
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

    if(idleTest && (capturing || !windowTestDir.empty()))
    {
        std::cerr<<"ControlTest: --idle-test is the window, measured; drop the capture switches"
                   " and --window-test\n";
        return EXIT_FAILURE;
    }

    if(!windowTestDir.empty() && (capturing || scale != 1.0f))
    {
        std::cerr<<"ControlTest: --window-test is the window, scripted at --scale 1;"
                   " drop the capture switches and --scale\n";
        return EXIT_FAILURE;
    }

    // a script, in the window: the steps run as in a capture, drawing into
    // the window's framebuffer, and the clipboard is the system's
    if(clipboardTest && (!scripted || forceHeadless || !windowTestDir.empty() || backend != gweni::capture::Backend::GL))
    {
        std::cerr<<"ControlTest: --clipboard-test runs a script (--key, --type, ...) in a GL window;"
                   " drop --headless, --window-test and --renderer vulkan\n";
        return EXIT_FAILURE;
    }
    if(waitsForClipboard && !clipboardTest)
    {
        std::cerr<<"ControlTest: --wait-clipboard-change waits on the system clipboard;"
                   " it needs --clipboard-test\n";
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
    deviceOptions.capturing=capturing && !clipboardTest;
    deviceOptions.headless=forceHeadless;
    // the scripted window reads nothing from the host's clipboard
    deviceOptions.systemClipboard=windowTestDir.empty();

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

        case Scenario::Chart:
        {
            bool passed=false;
            ok=runChartScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a change wrote more than the slots it moves: a failing assertion
            break;
        }

        case Scenario::Timeline:
        {
            bool passed=false;
            ok=runTimelineScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a change wrote more than the slots it moves: a failing assertion
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

        case Scenario::ZoomPanel:
        {
            bool passed=false;
            ok=runZoomPanelScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a zoomed panel did not draw as a scaled canvas
            break;
        }

        case Scenario::RangeSlider:
        {
            bool passed=false;
            ok=runRangeSliderScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a drag, a hover or a key did more than its thumb, or missed a step
            break;
        }

        case Scenario::Segmented:
        {
            bool passed=false;
            ok=runSegmentedScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a hover, a click, a key or a toggle did more than its segments, or a joint doubled
            break;
        }

        case Scenario::ChipBadge:
        {
            bool passed=false;
            ok=runChipBadgeScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a count, a move, a hover or a removal did more than its part
            break;
        }

        case Scenario::Switch:
        {
            bool passed=false;
            ok=runSwitchScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a hover, a slide or a disabled switch did more than its part
            break;
        }

        case Scenario::Spinner:
        {
            bool passed=false;
            ok=runSpinnerScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a turn, a stop or a hide did more than its part, or ran on
            break;
        }

        case Scenario::NavigationRail:
        {
            bool passed=false;
            ok=runNavigationRailScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;
            break;
        }

        case Scenario::FieldValidation:
        {
            bool passed=false;
            ok=runFieldValidationScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a message set or cleared did more than its field's part
            break;
        }

        case Scenario::Flow:
        {
            bool passed=false;
            ok=runFlowScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a size, a card's edit or a rebuild did more than its part
            break;
        }

        case Scenario::Board:
        {
            bool passed=false;
            ok=runBoardScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a wheel reached the wrong ScrollControl, or an edit moved an offset
            break;
        }

        case Scenario::ImageGrid:
        {
            bool passed=false;
            ok=runImageGridScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a button's image change did more than its cell, or kept an image
            break;
        }

        case Scenario::NodeGraph:
        {
            bool passed=false;
            ok=runNodeGraphScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // an input wrote, changed or laid out more than its part of the graph
            break;
        }

        case Scenario::DepthView:
        {
            bool passed=false;
            ok=runDepthViewScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // the view did work in the UI or changed what it shows
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

        case Scenario::Dialog:
        {
            bool passed=false;
            ok=runDialogScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a dialog opened, sat or closed wrongly: a failing assertion
            break;
        }

        case Scenario::Toast:
        {
            bool passed=false;
            ok=runToastScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a toast stacked, sat, paused or went wrongly: a failing assertion
            break;
        }

        case Scenario::CodeEditor:
        {
            bool passed=false;
            ok=runCodeEditorScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // an edit or a scroll touched more than its lines: a failing assertion
            break;
        }

        case Scenario::LogView:
        {
            bool passed=false;
            ok=runLogViewScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // an append, a scroll or an edit touched more than it may: a failing assertion
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

        case Scenario::VirtualListVariable:
        {
            bool passed=false;
            ok=runVirtualListVariableScenario(session, shotDir, passed);
            if(ok && !passed)
                ok=false;   // a variable-height frame cost more at 100 000 items, or moved what is on screen
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
        // never deleted: the window loop does not return in the browser
        gweni::tools::DepthView::onOpenChangedCaller.add(new WebDepthViewNotice,
            &WebDepthViewNotice::onOpenChanged);
#endif

        // --window-test and --idle-test take no input: the X pointer is
        // wherever the server left it, maybe over a button
        if(windowTestDir.empty() && !idleTest)
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
        else if(idleTest)
        {
#if !defined(__EMSCRIPTEN__)
            if(!runIdleTest(*device, canvas, renderer, background, control, loopOptions))
                result=EXIT_FAILURE;
#else
            std::cerr<<"ControlTest: --idle-test needs threads, which the web build has not\n";
            result=EXIT_FAILURE;
#endif
        }
        else
        {
            // The window's loop until it closes (in the browser, per animation
            // frame, never returning): idle, a frame only when the canvas has
            // work, continuous one per displayed frame (--loop). The frame
            // holds only the canvas and the renderer, heap objects, and the
            // colour, a copy, as the browser's loop needs.
            loopOptions.verify=loopOptions.verify || g_verifyIdle;
            device->runWindowLoop(canvas, renderer, loopOptions, [canvas, renderer, background]()
            {
                canvas->updateCanvas();
                if(g_depthView)
                    gweni::capture::openDepthViewOnce(*canvas);
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

    // verifyIdleAfter printed each one as it found it.
    if(g_idleVerifyFailures)
    {
        std::cerr<<"ControlTest: "<<g_idleVerifyFailures
                 <<" frame(s) an idle loop would have skipped drew something\n";
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
