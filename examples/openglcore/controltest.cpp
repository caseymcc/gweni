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
 *                       children_scanned= the children the render walk's
 *                       loop passed over in the controls it entered
 *                       (visited=), pending or not;
 *                       layout_visited= the controls the frame's layout
 *                       passes entered, one per calculateSize() and one per
 *                       arrange(), each axis counted (0 with no pass);
 *                       measure_calls= the renderer measureText() and
 *                       lineHeight() calls the controls made in the update
 *                       phase and any render-phase layout;
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
 *    --trace            log each scripted input on stderr with what it hit
 *    --list             print the control demo names and exit
 *    --help, -h         print the option list and exit
 *    --scenario <name>  per-control | resize | docked | scroll | interaction |
 *                       typing | clip | text-size | scale | tooltip |
 *                       context-menu | popup | virtual-list | hide-show |
 *                       demo-labels (see below)
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
 *                       control, alt, command, or a single printable
 *                       character. A named key is pressed and released as
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
 *                   nothing. TextBoxDemo: clicking TextBox 1 changes it, and
 *                   once the mouse has moved on and clicked TextBox 2, TextBox
 *                   1 looks as it did before any click.
 *                   TreeControlDemo: collapsing a node repaints and moves the
 *                   node below it up. SelectableDemo: hovering a free row
 *                   changes it and moving off gives its idle look back, a
 *                   click selects it and a second one deselects it back to
 *                   that look; with group row A selected and the mouse on row
 *                   B, the click on B changes pixels inside A and B only, and
 *                   that frame changes only A, B and their labels
 *                   (background=2), with at most 2 primitive slots and their
 *                   glyphs written (a line reports the counts). WindowDemo:
 *                   the close button of the window "Normal Window" opens
 *                   draws something at rest, hover and press change its
 *                   pixels and moving off gives the rest look back, the hover
 *                   frame writes 1..2 primitive slots, a press let go off it
 *                   leaves the window open and the button at rest, and a
 *                   click closes the window with the live primitive ids back
 *                   where they were before it opened. SliderDemo: each frame
 *                   of a thumb drag, along the track and off it, changes the
 *                   thumb only and moves it, and the release off it takes
 *                   its hover away. Exits non-zero if any check is no. The
 *                   clock is frozen as under --stats.
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
 *                 slots too, less the thumb's: its three rows move the bar's
 *                 thumb a pixel at 1 000 items only, thumb_moved=1);
 *                 settled frames write and change nothing;
 *                 each scroll writes no more than its shown rows' primitive
 *                 slots and glyphs, enters no more than the list's subtree
 *                 and the two controls above it, renumbers no z and lays out
 *                 in one pass with no request; the demo gives every primitive
 *                 id back. Exits non-zero if any check is no.
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
 *  depends on the frame number and not on how fast the process ran. --stats
 *  freezes the per-frame step instead, so an idle frame is genuinely idle and
 *  the counters settle to zero; --stats-clock step keeps it moving.
 */

#include <gweni/platforms/renderers/glApi.h>
#include <gweni/platforms/renderers/openglCore.h>
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
#include <gweni/controls/comboBox.h>
#include <gweni/controls/virtualList.h>
#include <GLFW/glfw3.h>
#include <gweni/utils/filesystem.h>
#include "capture.h"
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <fstream>
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
#include <demo/groupBox.h>
#include <demo/imagePanel.h>
#include <demo/label.h>
#include <demo/labelMultiline.h>
#include <demo/layoutDemo.h>
#include <demo/listBox.h>
#include <demo/menuStrip.h>
#include <demo/numeric.h>
#include <demo/pageControl.h>
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
#include <demo/statusBar.h>
#include <demo/tabControl.h>
#include <demo/treeControl.h>
#include <demo/textBox.h>
#include <demo/textDemo.h>
#include <demo/textSizes.h>
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

static void key_callback(GLFWwindow*, int key, int scancode, int action, int mods)
{   const gweni::input::GLFW3::KeyEvent e={key,scancode,action,mods}; g_glfwInput.processKeyEvent(e); }
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
    X(GroupBoxDemo) X(ImagePanelDemo) X(StatusBarDemo) X(SeparatorDemo) X(ComboBoxDemo) \
    X(TextBoxDemo) X(ListBoxDemo) X(SelectableDemo) X(VirtualListDemo) X(ContextMenuDemo) X(PopupDemo) X(CrossSplitterDemo) X(RadioButtonDemo) \
    X(CheckBoxDemo) X(NumericDemo) X(SliderDemo) X(MenuStripDemo) \
    X(WindowDemo) X(WindowButtonsDemo) X(TreeControlDemo) X(PropertiesDemo) X(TabControlDemo) \
    X(ScrollControlDemo) X(PageControlDemo) X(CollapsibleListDemo) X(ColorPickerDemo) \
    X(UnicodeTextDemo) X(TextSizesDemo) X(TextDemo) X(LayoutDemo)

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
         "  --stats            print one key=value line per frame; prim_slots_written=\n"
         "                     and text_slots_written= are the distinct slots its\n"
         "                     flush uploads (a draw, hide, show or release each)\n"
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
         "                     | context-menu | popup | virtual-list | hide-show\n"
         "                     | demo-labels;\n"
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
         "                     hide-show whether a demo hidden and shown again, and\n"
         "                     one built hidden and shown, are the one built shown,\n"
         "                     demo-labels whether every demo-list label fits;\n"
         "                     all twelve fail on a no\n"
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

using gweni::capture::Context;
using gweni::capture::Framebuffer;
using gweni::capture::createFramebuffer;
using gweni::capture::destroyContext;
using gweni::capture::destroyFramebuffer;
using gweni::capture::ensureDirectory;
using gweni::capture::initGlfwContext;
#if GWENI_EXAMPLE_EGL
using gweni::capture::initEglContext;
#endif
using gweni::capture::readFramebufferRgba;
using gweni::capture::writeFramebufferPng;
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

// One grep-able line per frame, frame index first. The renderer numbers are read
// after renderCanvas(), so they describe the frame just flushed.
static void printFrameStats(int frame, const std::string &control,
    const gweni::controls::ChangeStats &changes,
    const gweni::renderer::SlotRenderer::FrameStats &gpu,
    const gweni::controls::LayoutStats &layout,
    const gweni::controls::UpdateStats &update)
{
    std::printf("frame=%d control=%s controls_changed=%zu created=%zu visibility=%zu"
        " geometry=%zu background=%zu text=%zu prim_slots_written=%zu text_slots_written=%zu"
        " upload_calls=%zu upload_bytes=%zu mirror_bytes=%zu draw_calls=%zu"
        " atlas_upload_calls=%zu atlas_upload_bytes=%zu buffer_regrows=%zu"
        " layout_passes=%d redirtied_by=%s render_layout=%d layout_requests=%d requested_by=%s"
        " layout_visited=%zu measure_calls=%zu"
        " visited=%zu children_scanned=%zu z_visited=%zu anims_run=%zu prim_ids=%zu\n",
        frame, control.c_str(),
        changes.controls, changes.created, changes.visibility,
        changes.geometry, changes.background, changes.text,
        gpu.primitiveSlotsWritten, gpu.textSlotsWritten,
        gpu.uploadCalls, gpu.uploadBytes, gpu.mirrorBytes, gpu.drawCalls,
        gpu.atlasUploadCalls, gpu.atlasUploadBytes, gpu.bufferRegrows,
        layout.passes, redirtiedBy(layout).c_str(), layout.renderLayout ? 1 : 0,
        layout.requests, requestedBy(layout).c_str(),
        layout.visited, layout.measureCalls,
        changes.visited, changes.childrenScanned, update.zVisited, update.animsRun,
        gweni::primitiveIdCount()-gweni::releasedPrimitiveIdCount());
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
    gweni::renderer::OpenGLCore *renderer=nullptr;
    gweni::controls::Canvas *canvas=nullptr;
    Framebuffer *fb=nullptr;
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
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
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
    if(!writeFramebufferPng(s.pixels.x, s.pixels.y, path))
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

    destroyFramebuffer(*s.fb);
    if(!createFramebuffer(*s.fb, s.pixels.x, s.pixels.y))
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

    destroyFramebuffer(*s.fb);
    if(!createFramebuffer(*s.fb, s.pixels.x, s.pixels.y))
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
static void trace(const Session &s, const std::string &what)
{
    if(!s.trace)
        return;
    std::cerr<<"trace "<<what
             <<" clock="<<g_clockSeconds
             <<" hovered="<<(gweni::HoveredControl
                 ? gweni::HoveredControl->getTypeName() : "(none)");
    for(gweni::controls::Base *p=gweni::HoveredControl?gweni::HoveredControl->getParent():nullptr; p; p=p->getParent())
        std::cerr<<" < "<<p->getTypeName();
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
    {"kp-enter", GLFW_KEY_KP_ENTER},
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
    ToolTip, ContextMenu, Popup, VirtualList, HideShow, DemoLabels };

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
    else if(name == "hide-show") scenario=Scenario::HideShow;
    else if(name == "demo-labels") scenario=Scenario::DemoLabels;
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

static void captureResizeState(Session &s, const std::string &path, ResizeState &state)
{
    state.name=fs::path(path).filename().string();
    state.listing=boundsListing(s);
    readFramebufferRgba(s.pixels.x, s.pixels.y, state.pixels);
    state.mirror=s.renderer->snapshotMirror();
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
    readFramebufferRgba(s.size.x, s.size.y, fromPixels);

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
    readFramebufferRgba(s.size.x, s.size.y, toPixels);

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
    readFramebufferRgba(s.pixels.x, s.pixels.y, frame);

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
    readFramebufferRgba(s.pixels.x, s.pixels.y, before);
    const gweni::Rect belowBefore=below->getBounds();

    const gweni::Point at=centreOf(toggle);
    clickMouse(s, at.x, at.y);
    renderFrames(s, s.frames);
    if(!shoot(s, shotDir+"/interaction-tree-collapsed.png"))
        ok=false;
    readFramebufferRgba(s.pixels.x, s.pixels.y, after);
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
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
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

// SliderDemo: dragging the first slider's thumb, along the track and off it,
// changes the thumb alone on every drag frame (the mouse is held, so the
// controls it crosses get no hover), the thumb follows the mouse, and the
// release off the thumb takes its hover away.
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

    // towards the track's far end, in steps that stay inside it; the second
    // half of the steps runs below the track, off the slider
    const gweni::Point sliderAt=slider->localPosToCanvas();
    const int direction=(grab.x < sliderAt.x+slider->getWidth()/2) ? 1 : -1;
    const int step=std::max(1, slider->getWidth()/10);
    const int below=sliderAt.y+slider->getHeight()+5;

    std::string frames;
    bool onlyThumb=true;
    bool followed=true;
    for(int i=1; i <= 4; ++i)
    {
        const int thumbX=thumb->getX();
        moveMouse(s, grab.x+direction*step*i, i <= 2 ? grab.y : below);
        renderFrames(s, 1);
        const gweni::controls::ChangeStats changes=gweni::controls::getChangeStats();

        frames+=" controls_changed="+std::to_string(changes.controls)
            +" background="+std::to_string(changes.background);
        if(changes.controls != 1)
            onlyThumb=false;
        if(thumb->getX() == thumbX)
            followed=false;
    }
    const bool hoveredWhileHeld=thumb->isHovered();
    if(!shoot(s, shotDir+"/interaction-slider-drag.png"))
        ok=false;

    mouseUp(s, s.mouse.x, s.mouse.y);
    renderFrames(s, s.frames);
    const bool unhovered=!thumb->isHovered() && gweni::MouseFocus == nullptr;

    forgetMouse(s);
    dropControl(s, demo);

    checks.check("slider_drag_one_control", onlyThumb,
        "a drag frame changed other controls than the thumb:"+frames+"; want controls_changed=1 each");
    checks.check("slider_drag_follows", followed && hoveredWhileHeld,
        std::string(followed ? "" : "the thumb did not move with every drag step; ")
        +(hoveredWhileHeld ? "" : "the held thumb was not hovered"));
    checks.check("slider_drag_release", unhovered,
        "the thumb let go off itself kept the hover or the mouse");
}

static bool runInteractionScenario(Session &s, const std::string &shotDir, bool &passed)
{
    // Frozen like --stats, so a frame with no input is genuinely idle (no
    // caret blink); a scripted press still steps it.
    s.freezeClock=true;

    InteractionChecks checks;
    bool ok=true;
    checkButton(s, shotDir, checks, ok);
    checkFocusHandOff(s, shotDir, checks, ok);
    checkTreeCollapse(s, shotDir, checks, ok);
    checkSelectable(s, shotDir, checks, ok);
    checkWindowClose(s, shotDir, checks, ok);
    checkSliderDrag(s, shotDir, checks, ok);

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
    readFramebufferRgba(s.pixels.x, s.pixels.y, frame);
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

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
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

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
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

    forgetMouse(s);
    dropControl(s, demo);

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

    // The wheel's three rows move the bar's thumb a pixel at 1 000 items and
    // not at 100 000, so that step's controls differ by the thumb, and its
    // primitive slots by the thumb's one slot; the rest of its slots do not.
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

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
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

// A hidden control loses the keyboard focus (input::onCanvasThink drops it)
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
    s.label="(empty)";

    renderFrames(s, s.frames);
    readFramebufferRgba(s.pixels.x, s.pixels.y, pixels);
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

int main(int argc, char *argv[])
{
    gweni::capture::setToolName("ControlTest");
    gweni::capture::installGlfwErrorHandler();

    std::string which="ButtonDemo";
    std::string shotPath;
    std::string shotDir="/tmp";
    std::string resourceDir;
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
        else if(arg == "--frames" && hasValue)
        {
            frames=std::atoi(argv[++i]);
            steps.push_back({StepKind::Frames, frames, 0, {}});
        }
        else if(arg == "--shot-all") shotAll=true;
        else if(arg == "--headless") forceHeadless=true;
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
                           " hide-show or demo-labels\n";
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

    // Declared before the renderer, which keeps a reference to it. A missing
    // directory is reported here and the run carries on: what loads from it
    // degrades below.
    bool resourcesFound=true;
    const std::unique_ptr<gweni::ResourcePaths> paths=gweni::capture::resourcePaths(resourceDir, resourcesFound);

    Context ctx;
    if(forceHeadless)
    {
#if GWENI_EXAMPLE_EGL
        if(!initEglContext(ctx)) return EXIT_FAILURE;
#else
        std::cerr<<"ControlTest: --headless needs the EGL path, which this platform lacks\n";
        return EXIT_FAILURE;
#endif
    }
    else if(!initGlfwContext(ctx, screenPixels, "Gweni ControlTest - "+which, !capturing))
    {
        if(!capturing)
        {
            std::cerr<<"ControlTest: GLFW could not open a window;"
                       " interactive mode needs a display\n";
            return EXIT_FAILURE;
        }
#if GWENI_EXAMPLE_EGL
        std::cerr<<"ControlTest: GLFW could not open a window"
                   " (no display?); falling back to surfaceless EGL\n";
        if(!initEglContext(ctx)) return EXIT_FAILURE;
#else
        std::cerr<<"ControlTest: GLFW could not open a window\n";
        return EXIT_FAILURE;
#endif
    }

    if(!gweni::capture::initGL(ctx))
    {
        destroyContext(ctx);
        return EXIT_FAILURE;
    }

    Framebuffer fb;
    if(capturing && !createFramebuffer(fb, screenPixels.x, screenPixels.y))
    {
        destroyContext(ctx);
        return EXIT_FAILURE;
    }

    gweni::renderer::OpenGLCore *renderer=
        new gweni::renderer::OpenGLCore(*paths, gweni::Rect(gweni::Point(0,0), screenPixels));
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
    // out), so the clear color is what actually shows.
    const gweni::Color background=gweni::skin::textured::tokyonight::Canvas.top;
    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, background.a/255.0f);

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

        Session session;
        session.renderer=renderer;
        session.canvas=canvas;
        session.fb=&fb;
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

        g_glfwInput.initialize(canvas);
        glfwSetKeyCallback(ctx.window, key_callback);
        glfwSetCharCallback(ctx.window, char_callback);
        glfwSetWindowFocusCallback(ctx.window, window_focus_callback);
        glfwSetCursorPosCallback(ctx.window, cursor_pos_callback);
        glfwSetCursorEnterCallback(ctx.window, cursor_enter_callback);
        glfwSetMouseButtonCallback(ctx.window, mouse_button_callback);
        glfwSetScrollCallback(ctx.window, scroll_callback);
        glfwSetFramebufferSizeCallback(ctx.window, framebuffer_size_callback);
        glfwSetWindowContentScaleCallback(ctx.window, content_scale_callback);

        // The window's own content scale, and its real framebuffer size.
        fitCanvas(ctx.window);

        while(!glfwWindowShouldClose(ctx.window))
        {
            canvas->updateCanvas();
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            canvas->renderCanvas();
            glfwSwapBuffers(ctx.window);
            glfwPollEvents();
            gweni::platform::sleep(0);
        }

        delete control;
    }

    delete canvas;
    delete skin;
    delete renderer;
    if(capturing) destroyFramebuffer(fb);
    destroyContext(ctx);

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
