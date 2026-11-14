#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  gweni tests, run with: ctest --test-dir build_linux_x64_debug
#
#  Tests labelled "gpu" open a GL context (the examples' surfaceless EGL path,
#  --headless) or a Vulkan device, so skip them with ctest -LE gpu; the Vulkan
#  ones are labelled "vulkan" too (ctest -LE vulkan runs the rest). The examples load the
#  resources/ the build copies next to them, so they run from any working
#  directory (controltest_any_cwd checks it); their tests still run from the
#  repo root, where relative --shot paths are meant. gweni_unittests loads
#  resources/ relative to the working directory, so it must run there.
#
#  In the web tree (EMSCRIPTEN) the executables are pages, run in headless
#  Chromium by web/run.mjs: none of the native registrations below apply there,
#  and the web ones are in the if(EMSCRIPTEN) blocks.
#

# gweni_whole_app_test(<name> <timeout> COMMAND ... [WORKING_DIRECTORY ...])
#
# A whole-app check (ControlTest, OpenGLCore, a script running them), label
# gpu, registered once per backend: <name> on GL, and with GWENI_RENDER_VULKAN
# <name>_vulkan, the same command on renderer::Vulkan in the Vulkan test
# environment (cmake/Vulkan.cmake: the validation layer on, any message
# aborting; lavapipe; labels gpu and vulkan). The backend is chosen by
# GWENI_RENDERER, which the exes read as their --renderer default, so a script's
# own ControlTest and OpenGLCore children (two runs of determinism.cmake, one
# per control in eachControl.cmake) take it too and no script needs an argument
# for it; the GL run sets it to gl, so a GWENI_RENDERER of the caller's own
# changes neither. In the arguments, @VARIANT@ is "" for <name> and "_vulkan"
# for <name>_vulkan: every scratch and --shot-dir directory carries it, so the
# two can run side by side (ctest -j).
function(gweni_whole_app_test name timeout)
    set(variants "-")
    if(GWENI_RENDER_VULKAN)
        list(APPEND variants "_vulkan")
    endif()

    foreach(variant IN LISTS variants)
        if(variant STREQUAL "-")
            set(suffix "")
            set(backend gl)
        else()
            set(suffix "${variant}")
            set(backend vulkan)
        endif()
        string(REPLACE "@VARIANT@" "${suffix}" arguments "${ARGN}")
        add_test(NAME ${name}${suffix} ${arguments})
        set_tests_properties(${name}${suffix} PROPERTIES
            ENVIRONMENT GWENI_RENDERER=${backend})
        if(backend STREQUAL "gl")
            set_tests_properties(${name}${suffix} PROPERTIES
                LABELS gpu
                TIMEOUT ${timeout})
        else()
            # lavapipe under the validation layer: each process starts slower
            math(EXPR vulkan_timeout "${timeout}*2")
            set_tests_properties(${name}${suffix} PROPERTIES
                TIMEOUT ${vulkan_timeout})
            gweni_vulkan_test_environment(${name}${suffix})
        endif()
    endforeach()
endfunction()

if(TARGET ControlTest AND NOT EMSCRIPTEN)
    # every --shot-all PNG and --dump-bounds listing, byte-identical across two
    # separate ControlTest processes
    gweni_whole_app_test(controltest_determinism 600
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/determinism@VARIANT@
            -P ${CMAKE_CURRENT_SOURCE_DIR}/determinism.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # the same at --scale 1.5, where every edge is rounded to the device-pixel
    # grid, so a rounding that depends on anything but the input shows here
    gweni_whole_app_test(controltest_determinism_scale150 600
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/determinism_scale150@VARIANT@
            -DSCALE=1.5
            -P ${CMAKE_CURRENT_SOURCE_DIR}/determinism.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # --shot-all --dump-bounds at --scale 1 and 2: the listings (logical units)
    # are identical and every PNG (device pixels) is exactly twice the size
    gweni_whole_app_test(controltest_scale_bounds 600
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/scale_bounds@VARIANT@
            -P ${CMAKE_CURRENT_SOURCE_DIR}/scaleBounds.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # every --shot-all PNG (and OpenGLCore's --screenshot) byte-identical with the
    # draws split into chunks of 64 slots (GWENI_INDEX_CHUNK_SLOTS) and without
    set(index_chunks_opengl)
    if(TARGET OpenGLCore)
        set(index_chunks_opengl -DOPENGLCORE=$<TARGET_FILE:OpenGLCore>)
    endif()
    gweni_whole_app_test(controltest_index_chunks 600
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            ${index_chunks_opengl}
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/index_chunks@VARIANT@
            -DCHUNK_SLOTS=64
            -P ${CMAKE_CURRENT_SOURCE_DIR}/indexChunks.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # the README samples in docs/images/, byte-identical to a fresh capture
    # (tools/capture_samples.sh --check; regenerate them in the same commit)
    if(TARGET OpenGLCore)
        add_test(NAME docs_samples
            COMMAND bash ${GWENI_SOURCE_DIR}/tools/capture_samples.sh
                --check --bin-dir $<TARGET_FILE_DIR:ControlTest>
            WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
        set_tests_properties(docs_samples PROPERTIES
            LABELS gpu
            TIMEOUT 300)
    endif()

    # build, render and destroy every demo; exits non-zero unless the canvas
    # renders as the empty canvas it started as, and every frame of it takes
    # one layout pass. Since it builds every demo in --list, it is also the
    # test that fails when a control type has no skin renderer (ControlTest
    # exits non-zero after "gweni: no skin renderer for <TypeName>")
    gweni_whole_app_test(controltest_teardown 300
        COMMAND $<TARGET_FILE:ControlTest> --headless --check-teardown
            --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/teardown@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # the whole Demo at 640x480, 1024x768 and 640x480 again: the listing, the
    # renderer mirror (per id) and the pixels of the last shot match the first
    # and a canvas built directly at 640x480, and every frame takes one layout
    # pass (issue #58)
    gweni_whole_app_test(controltest_resize 120
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario resize
            --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/resize@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # The modes that build a control again in the same process and compare the
    # two (resize, scale, hide-show) leave InspectorDemo and StatsOverlayDemo
    # out: the inspector shows control ids, which are process-wide and never
    # reused, so the second build's captions are other strings, and the stats
    # overlay shows the process's live ids and the frames before it, which the
    # second build sees otherwise. determinism (two processes), one_pass_each,
    # teardown, controltest_inspector and controltest_stats_overlay cover them.
    # the same round trip for every other control in --list, one process each
    gweni_whole_app_test(controltest_resize_each 600
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/resize_each@VARIANT@
            -DMODE=resize
            -DSKIP=Demo,InspectorDemo,StatsOverlayDemo
            -P ${CMAKE_CURRENT_SOURCE_DIR}/eachControl.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # the plain run of every control in --list (Demo included): every frame
    # takes one layout pass and the render phase lays out nothing
    gweni_whole_app_test(controltest_one_pass_each 600
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/one_pass_each@VARIANT@
            -DMODE=one-pass
            -P ${CMAKE_CURRENT_SOURCE_DIR}/eachControl.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # Canvas::setScale 1 -> 2 -> 1 for every control in --list (Demo included),
    # one process each: the last 1x shot, its listing and the renderer mirror
    # are the first one's, the 2x shot is a canvas built at scale 2, and every
    # frame takes one layout pass
    gweni_whole_app_test(controltest_scale_roundtrip 600
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/scale_roundtrip@VARIANT@
            -DMODE=scale
            -DSKIP=InspectorDemo,StatsOverlayDemo
            -P ${CMAKE_CURRENT_SOURCE_DIR}/eachControl.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # every control in --list (Demo included), one process each, built into a
    # host page: the host hidden and shown again, and on a fresh canvas the
    # demo built into a host that is hidden already, as the shell builds every
    # page from index 1 on, then shown. Both have to be the demo built shown
    # (listing, renderer mirror, pixels), the parts a control hid itself still
    # hidden, and Demo's shell has to show PropertiesDemo and ComboBoxDemo
    # with every ComboBox's list closed
    gweni_whole_app_test(controltest_hide_show_each 600
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/hide_show_each@VARIANT@
            -DMODE=hide-show
            -DSKIP=InspectorDemo,StatsOverlayDemo
            -P ${CMAKE_CURRENT_SOURCE_DIR}/eachControl.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # the plain run of every control in --list (Demo included) with every
    # frame's layout checked against a full pass (--verify-layout): a pass may
    # skip only what running would leave as it is, and a full pass over a
    # converged tree must change nothing
    add_test(NAME controltest_verify_layout_each
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/verify_layout_each
            -DMODE=verify
            -P ${CMAKE_CURRENT_SOURCE_DIR}/eachControl.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_verify_layout_each PROPERTIES
        LABELS gpu
        TIMEOUT 600)

    # the same check through the scenarios that change layout at run time:
    # input, a canvas resize and a scale round trip (both on Demo), a Window
    # dragged past its parent's edges, the tooltip box, the context menu, the
    # stats overlay, the depth view, the zoom panel, the node graph and a
    # variable-height VirtualList, the code editor, the log view, the
    # badges' counts and anchor moves, the dialogs, the toasts, a Flow's
    # resizes and rebuilds, the switches' toggles, the navigation rail's
    # collapse/selection, the board's wheels and edits, the fields' messages
    # and the segmented buttons' selections
    foreach(verify_scenario interaction resize scale clip tooltip context-menu stats-overlay
            depth-view zoom-panel node-graph virtual-list-variable code-editor log-view chip-badge
            dialog toast flow switch navigation-rail board field-validation segmented)
        string(REPLACE "-" "_" verify_name "${verify_scenario}")
        set(verify_flags)
        if(verify_scenario STREQUAL "navigation-rail")
            list(APPEND verify_flags --require-one-pass)
        endif()
        add_test(NAME controltest_verify_layout_${verify_name}
            COMMAND $<TARGET_FILE:ControlTest> --headless --scenario ${verify_scenario}
                --verify-layout ${verify_flags}
                --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/verify_layout_${verify_name}
            WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
        set_tests_properties(controltest_verify_layout_${verify_name} PROPERTIES
            LABELS gpu
            TIMEOUT 300)
    endforeach()

    # every frame an idle window loop would have skipped (the canvas's
    # nextWorkTime() past the clock before its update) drawn anyway through
    # the scenarios that change what is shown at run time, and failed if it
    # wrote a slot, uploaded anything or changed a pixel (--verify-idle): a
    # change the canvas does not report as work would show late in a window
    foreach(idle_scenario interaction tooltip context-menu depth-view stats-overlay)
        string(REPLACE "-" "_" idle_name "${idle_scenario}")
        add_test(NAME controltest_verify_idle_${idle_name}
            COMMAND $<TARGET_FILE:ControlTest> --headless --scenario ${idle_scenario}
                --verify-idle
                --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/verify_idle_${idle_name}
            WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
        set_tests_properties(controltest_verify_idle_${idle_name} PROPERTIES
            LABELS gpu
            TIMEOUT 300)
    endforeach()

    # hover, press, disabled and focus repaint the control (and only it), a
    # settled frame does nothing, and collapsing a tree node still moves the
    # nodes below; exits non-zero on any failed check
    gweni_whole_app_test(controltest_interaction 300
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario interaction
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/interaction@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # the canvas's tooltip on ButtonDemo, on the stepped clock: it waits for
    # its delay, shows below the button above every other control (an open
    # Menu included), writes nothing once settled or for a move inside the
    # button, moves the same box straight to the next button, hides on leaving
    # (the frame as before hovering) and on a press, and goes above a button in
    # the bottom-right corner; exits non-zero on any failed check
    gweni_whole_app_test(controltest_tooltip 300
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario tooltip --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/tooltip@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # right-clicks on ContextMenuDemo (Base::setContextMenu): the menu opens with
    # its top-left at the cursor changing nothing visible outside it and its box,
    # a settled frame writes nothing, a click outside and Escape give the frame
    # before opening back, it flips inside the canvas in the bottom-right
    # corner, a right-click on the other box moves to its menu, a disabled item
    # does nothing and an enabled one is chosen, the Help tooltip draws over an
    # open menu, and hiding or deleting the box closes it; exits non-zero on any
    # failed check
    gweni_whole_app_test(controltest_context_menu 300
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario context-menu --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/context-menu@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # PopupDemo: a click on Options opens its popup below it, changing nothing
    # outside the popup and the button; a settled frame with it open writes
    # nothing; a click outside and Escape each give the frame before opening
    # back; the button toggles it; the Corner popup opens above its button,
    # inside the canvas; one popup at a time; a ComboBox list opens over it;
    # a tooltip draws over it; hiding its button closes it; exits non-zero on
    # any failed check
    gweni_whole_app_test(controltest_popup 300
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario popup --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/popup@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # DialogDemo and LargeDialogDemo, the clock frozen: each standard dialog's
    # open frame writes only the dialog's ids, a settled frame writes and
    # renders nothing, Enter, Escape or a typed prompt closes it with only the
    # result label changed; the large dialog is 1200 x 0.92 clamped to the
    # canvas and centred, a change behind it writes only that Label's ids and
    # no pixel inside it, a confirm stacks on it and hands it back, and Escape
    # and Enter each close it with the frame before it opened and the ids
    # back; exits non-zero on any failed check
    gweni_whole_app_test(controltest_dialog 300
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario dialog --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/dialog@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # ToastDemo, the clock stepping: two toasts stack in the top-right
    # corner, newest at the top; a settled frame writes and renders nothing
    # and runs no animation; the mouse on a toast holds the stack past its
    # timeout, the entry, held and exit frames writing nothing; off it they
    # expire some seconds later, giving the frame before them and the ids
    # back; a multi-line toast is taller, and a sticky one outlasts it and
    # goes with its close button; exits non-zero on any failed check
    gweni_whole_app_test(controltest_toast 300
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario toast --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/toast@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # VirtualListDemo at 1 000 and 100 000 items: the same pool, ids and
    # per-frame counters at both, a settled frame writing nothing, and a
    # one-row scroll and a wheel notch writing no more than the shown rows'
    # slots, entering only the list's subtree in one layout pass with no z
    # renumber; exits non-zero on any failed check
    # CodeEditorDemo's 5000 lines, highlighted, scrolled to the middle: a typed
    # character tokenizes its line alone, writes its row's, the gutter's and the
    # caret's slots alone and changes no pixel outside them, a typing burst
    # undone gives the frame before it back, a block scalar opened re-colours the
    # rows on screen below it and tokenizes nothing past the screen (and its undo
    # gives the frame back), Return and a one-line scroll stay inside the
    # editor's rows on screen, and settled frames write nothing
    gweni_whole_app_test(controltest_code_editor 120
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario code-editor --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/code-editor@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    # LogViewDemo's log at 1 000 and 100 000 events: appends while following
    # write the rows and the bar alone, a wheel notch up detaches and appends
    # then write no row slot, the jump button follows again, a replace above
    # the viewport, a resize, an expand, a 3-row selection copied and
    # scrolled off screen and back, and the cap while detached each touch only
    # what they may, settled frames write nothing, and the pool and ids are the
    # same at both counts
    gweni_whole_app_test(controltest_log_view 120
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario log-view --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/log-view@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    gweni_whole_app_test(controltest_virtual_list 120
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario virtual-list --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/virtual-list@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # VirtualListVariableDemo (wrapped rows, each its measurer's height) at
    # 1 000 and 100 000 items: the same pool, ids and one-row scroll and wheel
    # counters at both, a settled frame writing nothing; an item growing and
    # one inserted above the viewport changing no pixel in it; a narrower list
    # keeping its top row in place; the idle re-measure ending with the content
    # the sum of the measures; exits non-zero on any failed check
    gweni_whole_app_test(controltest_virtual_list_variable 300
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario virtual-list-variable --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/virtual-list-variable@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # DataTableVirtualDemo at 1 000 and 100 000 records: the source-mode
    # table's pool, ids and per-frame counters the same at both, a settled
    # frame writing nothing, a one-row scroll and a wheel notch writing no more
    # than the shown rows' slots in one layout pass with no z renumber, and a
    # header click (the app sorting and rebinding) and a column drag each laid
    # out in one pass; exits non-zero on any failed check
    gweni_whole_app_test(controltest_data_table_virtual 120
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario data-table-virtual --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/data-table-virtual@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # keys handled in one frame, after an edit and before the layout that
    # re-wraps it, move the caret through the edited text's lines (#209): the
    # same script with no frame between the keys and with one after each edit
    # ends with the same text. Click on TextBoxMultiline's blank row, then Home,
    # X, Up, Y, Up, Z.
    set(SAME_FRAME_TEXT [=[I think we're losing sight of the real isZsue here, which is: what are we gYonna call ourselves?
X
Erm, and I think it comes down to a choice between "The League Against Salivating Monsters" or my own personal preference, which is "The Committee for the Liberation and Integration of Terrifying Organisms and their Rehabilitation Into Society." Erm, one drawback with that: the abbreviation is "CLITORIS."]=])
    foreach(variant same_frame layout_between)
        if(variant STREQUAL "same_frame")
            set(between)
        else()
            set(between --frames 1)
        endif()
        gweni_whole_app_test(controltest_multiline_keys_${variant} 60
            COMMAND $<TARGET_FILE:ControlTest> --headless --control TextBoxDemo
                --frames 3 --click 5,261 --frames 2
                --key home --type X ${between} --key up --type Y ${between}
                --key up --type Z ${between} --expect-text "${SAME_FRAME_TEXT}"
            WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    endforeach()

    # TextBox undo and redo (#597) through the keys: in TextBoxDemo's
    # TextBoxMultiline, "abc" typed on its blank row is one step that Ctrl+Z
    # takes back and Ctrl+Shift+Z and Ctrl+Y do again, and a Return starts a
    # step of its own; in its TextBoxNumeric ("2004"), "15" typed and a
    # Backspace are two steps, undone and redone one at a time
    set(UNDO_TEXT_ABC [=[I think we're losing sight of the real issue here, which is: what are we gonna call ourselves?
abc
Erm, and I think it comes down to a choice between "The League Against Salivating Monsters" or my own personal preference, which is "The Committee for the Liberation and Integration of Terrifying Organisms and their Rehabilitation Into Society." Erm, one drawback with that: the abbreviation is "CLITORIS."]=])
    set(UNDO_TEXT_EMPTY [=[I think we're losing sight of the real issue here, which is: what are we gonna call ourselves?

Erm, and I think it comes down to a choice between "The League Against Salivating Monsters" or my own personal preference, which is "The Committee for the Liberation and Integration of Terrifying Organisms and their Rehabilitation Into Society." Erm, one drawback with that: the abbreviation is "CLITORIS."]=])
    set(UNDO_TEXT_RETURN [=[I think we're losing sight of the real issue here, which is: what are we gonna call ourselves?
abc
de
Erm, and I think it comes down to a choice between "The League Against Salivating Monsters" or my own personal preference, which is "The Committee for the Liberation and Integration of Terrifying Organisms and their Rehabilitation Into Society." Erm, one drawback with that: the abbreviation is "CLITORIS."]=])
    gweni_whole_app_test(controltest_textbox_undo 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --control TextBoxDemo
            --frames 3 --click 5,261 --frames 2
            --type abc --expect-text "${UNDO_TEXT_ABC}"
            --key ctrl+z --expect-text "${UNDO_TEXT_EMPTY}"
            --key ctrl+shift+z --expect-text "${UNDO_TEXT_ABC}"
            --key ctrl+z --key ctrl+y --expect-text "${UNDO_TEXT_ABC}"
            --key return --type de --expect-text "${UNDO_TEXT_RETURN}"
            --key ctrl+z --expect-text "${UNDO_TEXT_ABC}"
            --key ctrl+y --expect-text "${UNDO_TEXT_RETURN}"
            --click 100,98 --frames 2 --key end
            --type 15 --expect-text 200415
            --key backspace --expect-text 20041
            --key ctrl+z --expect-text 200415
            --key ctrl+z --expect-text 2004
            --key ctrl+shift+z --expect-text 200415
            --key ctrl+y --expect-text 20041
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # script mode settles a script that starts with input and renders a frame
    # before a --shot that follows input, so --move 30,5 --shot matches the same
    # with --frames 3 / --frames 1 around the move; a shot before any frame
    # (--frames 0 --shot) fails and writes nothing
    gweni_whole_app_test(controltest_script_first_frame 120
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/scriptFirstFrame@VARIANT@
            -P ${CMAKE_CURRENT_SOURCE_DIR}/scriptFirstFrame.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # children are cut at their clipping parent's edges: content overflowing a
    # ScrollControl and a Window dragged past each edge of its parent leave the
    # pixels outside unchanged, and moved right out they leave nothing behind;
    # exits non-zero on any failed check
    gweni_whole_app_test(controltest_clip 300
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario clip --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/clip@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # the same at --scale 2, where the skin's 11 px text is 22 device px and so
    # SDF text: the Window dragged around and back must still look as it did,
    # which holds only if a moved text run is exactly a fresh layout
    gweni_whole_app_test(controltest_clip_scale200 300
        COMMAND $<TARGET_FILE:ControlTest> --headless --scale 2 --scenario clip --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/clip_scale200@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # a Label's font changed from 24 to 36 px, both drawn from the face's SDF
    # atlas, uploads nothing to an atlas and writes only that Label's glyph slots
    gweni_whole_app_test(controltest_text_size 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario text-size
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/text_size@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # A ZoomPanel draws and takes input as a canvas at setScale(zoom) would,
    # its zoom and pan frames lay nothing out and write only its slots, and
    # what opens from inside it opens at its controls as drawn
    gweni_whole_app_test(controltest_zoom_panel 120
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario zoom-panel --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/zoom_panel@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # NodeGraphDemo through the mouse and keys: a drag, a hover, linking,
    # selection, the marquee, context menus, a pan and a zoom, and Delete each
    # write, change and lay out no more than their part of the graph
    gweni_whole_app_test(controltest_node_graph 120
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario node-graph --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/node_graph@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # PlotHistogramDemo's series: a sample with a new height writes its bar's
    # slot alone, one that keeps its height nothing, a hover the hovered bars'
    gweni_whole_app_test(controltest_plot_histogram 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario plot-histogram --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/plot_histogram@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # PlotLinesDemo's series: a moved sample writes at most the four segments
    # around it (the two it ends and the two holding it as a neighbour), one
    # that keeps its point nothing, a push no layout, a hover nothing
    gweni_whole_app_test(controltest_plot_lines 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario plot-lines --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/plot_lines@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # ChartTimeBarsDemo, ChartLinesDemo and ChartAreasDemo with their ranges
    # fixed: a changed point writes the slots around it (its stack's bars, a
    # line's four segments, a step line's five, an area's two trapezoids and
    # those of the bands stacked on it), an append its own, a marker its slot
    # and caption, a hover the crosshair and the ToolTip box, a move within
    # one x nothing; legend, point and band clicks fire their events; ids come
    # back
    gweni_whole_app_test(controltest_chart 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario chart --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/chart@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # TimelineDemo's 1000 runs with its day fixed: a changed or added bar
    # writes its own slot, a hover the hover look's slot and the ToolTip box,
    # a move within a bar nothing; a click fires onBarClicked with the lane,
    # index and id; a bar added over another shows, hit-tests and clicks in
    # front of it; a wheel notch writes no more than the lanes in view (the
    # bars move: bars_moved=yes); ids come back
    gweni_whole_app_test(controltest_timeline 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario timeline --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/timeline@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # FlowDemo: canvas widths that put the Flow's own width at each
    # breakpoint and one less, 640, 2560 and 640 again, each its breakpoint's
    # columns (from the bounds listing) in one pass, settling to nothing, the
    # last 640 the first; a card's edit enters only the way down to it; a
    # growth and a one-frame rebuild keep the scroll offset, and the cards
    # removed then added a frame later clamp it to 0 in between
    gweni_whole_app_test(controltest_flow 120
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario flow --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/flow@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # BoardDemo: a vertical wheel over a column's body scrolls it alone and is
    # consumed at either end and in the fitting column; over the board's
    # inset, a gap and a header it scrolls the board, as do a horizontal wheel
    # and Shift+wheel over a body; each wheel frame has nothing pending outside
    # the ScrollControl it scrolls and writes only that subtree's shown slots;
    # cards added and text grown keep every offset, the fitting column's cards
    # their widths, and every body shows its bar, disabled while its cards fit
    gweni_whole_app_test(controltest_board 120
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario board --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/board@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # ChipBadgeDemo: a badge's new count writes its slots alone and lays out
    # the way down to it only when it grows, its anchor's move enters neither
    # it nor the anchor's Text, a chip's hover writes its slot alone and a
    # removal lays the row out in one pass and gives the chip's ids back
    gweni_whole_app_test(controltest_chip_badge 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario chip-badge --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/chip_badge@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # SwitchDemo, the clock stepping: a hover writes the track's slot alone,
    # each frame of a toggle's slide the thumb's alone with no layout, a
    # settled switch runs no animation, the label and Space toggle it as a
    # click does, a disabled one changes nothing, and hidden mid-slide it
    # runs no animation from the next frame on
    gweni_whole_app_test(controltest_switch 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario switch --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/switch@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # NavigationRailDemo: bounded hover/selection, keyboard/footer, isolated
    # badge counts, one-pass collapse, delayed tooltip and exact expansion.
    gweni_whole_app_test(controltest_navigation_rail 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario navigation-rail --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/navigation_rail@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # FieldValidationDemo, the clock frozen: the app's set and clear of a
    # field's error lay out the field alone, write its frame's slot and the
    # message's glyphs and change no pixel outside it, the clear giving the
    # frame back; typing into a Change-validated field shows and clears its
    # error within the field; Tab off an empty FocusLoss field shows
    # "required"; settled frames write nothing
    gweni_whole_app_test(controltest_field_validation 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario field-validation --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/field_validation@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # ImageGridDemo: Replace and Free change their cell alone and keep the
    # renderer's live image count, the second clicks give the cells back, and
    # the demo gives its images and ids back
    gweni_whole_app_test(controltest_image_grid 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario image-grid --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/image_grid@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # SegmentedButtonsDemo: every inner joint of a row is one border column,
    # a hover writes its segment's slot alone, an exclusive click and Right
    # and Left the two segments' slots and glyphs alone with no layout, a
    # multi toggle its one segment, and a second toggle gives the first frame
    # back; at scale 1 and at 1.5, where a joint is 2 device px
    gweni_whole_app_test(controltest_segmented 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario segmented --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/segmented@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    gweni_whole_app_test(controltest_segmented_scale150 60
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario segmented --require-one-pass --scale 1.5
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/segmented_scale150@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # gweni::tools::Inspector over the settled Demo, opened and closed by F12:
    # it settles to 0 and a frame that writes nothing, its own slots included,
    # shows a hover's slots on the next frame, and gives its ids back
    gweni_whole_app_test(controltest_inspector 120
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario inspector --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/inspector@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # gweni::tools::StatsOverlay over the settled Demo, the clock stepping,
    # opened and closed by F10: it writes nothing between its refreshes and
    # only its own slots on one, shows what each window's frames counted and a
    # hover on the next refresh, lets the mouse through to a Button under it,
    # gives its ids back, and Demo's --stats counters are the same with it
    gweni_whole_app_test(controltest_stats_overlay 240
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/stats_overlay@VARIANT@
            -P ${CMAKE_CURRENT_SOURCE_DIR}/statsOverlay.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # gweni::tools::DepthView over ButtonDemo, the clock frozen, opened and
    # closed by F9: opening it, moving its camera, a click while it is open and
    # closing it do no work in the UI and change no hover, focus, tree or id,
    # the frame after it closes is the one before it opened, two runs'
    # shots through its cameras are byte-identical, and the demo shell's Depth
    # button is on exactly while the view is open
    gweni_whole_app_test(controltest_depth_view 120
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/depth_view@VARIANT@
            -P ${CMAKE_CURRENT_SOURCE_DIR}/depthView.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # the textured skin's theme changed under the settled Demo and applied live
    # (tools::ThemeEditor::applyToCanvas) draws what a Demo built under it
    # draws; saved, the defaults given back and the file loaded, the first
    # Demo and then the changed one again, byte for byte
    gweni_whole_app_test(controltest_theme 120
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario theme --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/theme@VARIANT@
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})

    # every entry of the shell's demo list fits its row at the README hero's
    # 1024x768, at scale 1 and 1.5: a label wider than its button's inner rect
    # (or running under the list's scrollbar) fails, naming the entry's key
    add_test(NAME controltest_demo_labels
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario demo-labels --size 1024x768
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/demo_labels
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    add_test(NAME controltest_demo_labels_scale150
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario demo-labels --size 1024x768
            --scale 1.5 --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/demo_labels_scale150
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_demo_labels controltest_demo_labels_scale150 PROPERTIES
        LABELS gpu
        TIMEOUT 60)

    # the resources/ next to the exe are found from a directory that has none
    add_test(NAME controltest_any_cwd
        COMMAND $<TARGET_FILE:ControlTest> --headless --control ButtonDemo
            --shot ${CMAKE_CURRENT_BINARY_DIR}/any_cwd/ButtonDemo.png
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(controltest_any_cwd PROPERTIES
        LABELS gpu
        TIMEOUT 60)

    # a capture from a --resources directory that is missing, or lacks the skin
    # config, the default font or the mono font, names what is missing, still writes its
    # (degraded) frame and exits 2; from a complete copy it exits 0
    set(missing_resources_tools ControlTest)
    if(TARGET OpenGLCore)
        list(APPEND missing_resources_tools OpenGLCore)
    endif()
    foreach(tool ${missing_resources_tools})
        string(TOLOWER ${tool} tool_lower)
        foreach(case nodir noskin nofont nomono complete)
            if(case STREQUAL "nodir")
                set(test_name ${tool_lower}_missing_resources)
            else()
                set(test_name ${tool_lower}_missing_resources_${case})
            endif()
            gweni_whole_app_test(${test_name} 60
                COMMAND ${CMAKE_COMMAND}
                    -DEXE=$<TARGET_FILE:${tool}>
                    -DTOOL=${tool}
                    -DSOURCE_DIR=${GWENI_SOURCE_DIR}
                    -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/${test_name}@VARIANT@
                    -DCASE=${case}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/missingResources.cmake)
        endforeach()
    endforeach()
elseif(NOT EMSCRIPTEN)
    message(STATUS "tests: ControlTest tests skipped, GWENI_BUILD_EXAMPLES is off")
endif()

# The mirror that keeps the resources/ next to the examples in step with the
# repo's (cmake/mirrorDirectory.cmake), on scratch directories: cmake alone, no
# exe and no context, so it is registered in every tree, with no gpu label.
add_test(NAME mirror_directory
    COMMAND ${CMAKE_COMMAND}
        -DMIRROR=${GWENI_SOURCE_DIR}/cmake/mirrorDirectory.cmake
        -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/mirror_directory
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mirrorDirectory.cmake)
set_tests_properties(mirror_directory PROPERTIES TIMEOUT 30)
if(EMSCRIPTEN)
    set_tests_properties(mirror_directory PROPERTIES LABELS web)
endif()

# Every public header compiles as the only include of a translation unit: one
# generated header_check/gweni/<rel>.cpp holding #include <gweni/<rel>> per
# header under include/gweni/, built in every tree as the OBJECT library
# gweni_header_check, part of `all`, so a header that does not include what it
# uses fails cmake --build, naming its .cpp. The glob covers a new header with
# no edit here, and a .cpp is written only when its text changes, so
# re-configuring an unchanged tree rebuilds nothing.
file(GLOB_RECURSE gweni_public_headers CONFIGURE_DEPENDS
    RELATIVE ${GWENI_SOURCE_DIR}/include
    ${GWENI_SOURCE_DIR}/include/gweni/*.h)
# gweni/controls.h is the umbrella of the controls: configuring fails, naming
# each one, if a header under include/gweni/controls/ is not one of its
# #include lines. An edit of controls.h re-runs the check.
set(gweni_umbrella ${GWENI_SOURCE_DIR}/include/gweni/controls.h)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${gweni_umbrella})
file(STRINGS ${gweni_umbrella} gweni_umbrella_lines
    REGEX "^#include <gweni/controls/[^>]+>")
set(gweni_umbrella_includes)
foreach(line IN LISTS gweni_umbrella_lines)
    string(REGEX REPLACE "^#include <([^>]+)>.*" "\\1" included "${line}")
    list(APPEND gweni_umbrella_includes ${included})
endforeach()
foreach(header IN LISTS gweni_public_headers)
    if(header MATCHES "^gweni/controls/" AND NOT header IN_LIST gweni_umbrella_includes)
        message(SEND_ERROR "controls.h does not include ${header}")
    endif()
endforeach()
# A backend's headers are checked only in a tree that builds it, the one that
# has its dependencies.
set(gweni_header_check_excluded)
if(NOT GWENI_RENDER_OPENGL_CORE)
    # freetype-gl and glew or the GLES headers come with the GL renderer
    list(APPEND gweni_header_check_excluded
        gweni/platforms/renderers/glApi.h
        gweni/platforms/renderers/openglCore.h
        gweni/platforms/renderers/slotRenderer.h)
endif()
if(NOT GWENI_RENDER_VULKAN)
    # volk comes with the vcpkg feature vulkan
    list(APPEND gweni_header_check_excluded gweni/platforms/renderers/vulkan.h)
endif()
set(gweni_header_check_sources)
foreach(header IN LISTS gweni_public_headers)
    if(header IN_LIST gweni_header_check_excluded)
        continue()
    endif()
    set(source ${CMAKE_CURRENT_BINARY_DIR}/header_check/${header}.cpp)
    set(content "#include <${header}>\n")
    set(written "")
    if(EXISTS ${source})
        file(READ ${source} written)
    endif()
    if(NOT written STREQUAL content)
        file(WRITE ${source} "${content}")
    endif()
    list(APPEND gweni_header_check_sources ${source})
endforeach()
add_library(gweni_header_check OBJECT ${gweni_header_check_sources})
gweni_set_warnings(gweni_header_check)
# gweni brings the include dir, its compile definitions and its dependencies'
# headers; platforms/input/glfw3.h needs GLFW's, which gweni does not link (as
# for gweni_unit_tests below)
target_link_libraries(gweni_header_check PRIVATE gweni)
if(EMSCRIPTEN)
    target_compile_options(gweni_header_check PRIVATE --use-port=contrib.glfw3)
else()
    find_package(glfw3 CONFIG REQUIRED)
    target_include_directories(gweni_header_check PRIVATE
        $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

find_package(doctest CONFIG REQUIRED)
include(doctest)

# Headless C++ unit tests (doctest) under unit/: no GL context and no resources,
# so they carry no gpu label and ctest -LE gpu runs them without a lock.
# Each test case is registered as its own CTest test, so a failure names it.
# Every unit/*.cpp is a test file: a new test is a new file, no list to extend.
file(GLOB gweni_unit_tests_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp)
add_executable(gweni_unit_tests ${gweni_unit_tests_sources})
gweni_set_warnings(gweni_unit_tests)
target_link_libraries(gweni_unit_tests
    gweni
    doctest::doctest)

# utf8Typing.cpp drives a Canvas through the GLFW translator
# (gweni/platforms/input/glfw3.h), which only uses GLFW's constants: it takes
# GLFW's headers and does not link the library. On the web they come from
# Emscripten's contrib.glfw3 port (emscripten-glfw), which the web apps will
# link; vcpkg's glfw3 is not built for wasm32 (vcpkg.json).
if(EMSCRIPTEN)
    target_compile_options(gweni_unit_tests PRIVATE --use-port=contrib.glfw3)
else()
    find_package(glfw3 CONFIG REQUIRED)
    target_include_directories(gweni_unit_tests PRIVATE
        $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

# discovered at build time, so the ASAN environment below does not reach them;
# they get it here instead
set(gweni_unit_tests_properties TIMEOUT 60)
if(GWENI_SANITIZE MATCHES "address")
    list(APPEND gweni_unit_tests_properties
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
endif()
if(EMSCRIPTEN)
    # The web tree: gweni_unit_tests.js runs under emsdk's node, through
    # CMAKE_CROSSCOMPILING_EMULATOR (./generate.sh web sets it), for discovery
    # and for each case. The cases need no resources, and those that write
    # files write to the in-memory file system's /tmp.
    list(APPEND gweni_unit_tests_properties LABELS web)
    # Emscripten's heap is a fixed 16 MB unless it may grow; the native cases
    # assume no such limit.
    target_link_options(gweni_unit_tests PRIVATE -sALLOW_MEMORY_GROWTH=1)
    # Emscripten's isatty() says yes for node's piped stdout, so doctest would
    # colour its --list-test-cases output and discovery would register the
    # escape codes as test names.
    target_compile_definitions(gweni_unit_tests PRIVATE DOCTEST_CONFIG_COLORS_NONE)
endif()
doctest_discover_tests(gweni_unit_tests
    TEST_PREFIX "unit:"
    PROPERTIES ${gweni_unit_tests_properties})
# a run that matches no case fails (tests/unit/main.cpp): a ';' in a case name
# splits it into two discovered tests above, and each must fail, not pass
# without running the case; the script needs the exit status and the message
add_test(NAME unit_tests_zero_match_fails
    COMMAND ${CMAKE_COMMAND}
        -DUNIT_TESTS=$<TARGET_FILE:gweni_unit_tests>
        "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/unitZeroMatch.cmake)
set_tests_properties(unit_tests_zero_match_fails PROPERTIES ${gweni_unit_tests_properties})

# C++ unit tests (doctest) that need a GL context, one executable. They get it
# from the examples' surfaceless EGL code (gweni_capture), so they are only
# built along with the examples.
if(TARGET gweni_capture)
    # every *.cpp here is a test file (main.cpp included), as under unit/
    file(GLOB gweni_unittests_sources CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
    add_executable(gweni_unittests ${gweni_unittests_sources})
    gweni_set_warnings(gweni_unittests)
    target_link_libraries(gweni_unittests
        gweni
        gweni_capture
        doctest::doctest)

    if(EMSCRIPTEN)
        # a page (gweni_unittests.html), run in the browser as web_unittests
        # below, with resources/ preloaded where main.cpp's CurrentPath
        # ("resources") finds it (the working directory is "/")
        gweni_web_page(gweni_unittests)
        # Emscripten's isatty() says yes in the page too, and the runner passes
        # the output on to a log, not a terminal
        target_compile_definitions(gweni_unittests PRIVATE DOCTEST_CONFIG_COLORS_NONE)
    else()
        add_test(NAME unittests
            COMMAND $<TARGET_FILE:gweni_unittests>
            WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
        # every case in one process: about 45 s in the ASAN tree on its own, and
        # past 90 s on a loaded host, so 60 timed out there as soon as another
        # test ran beside it
        set_tests_properties(unittests PROPERTIES
            LABELS gpu
            TIMEOUT 300)

        if(GWENI_RENDER_VULKAN)
            # The same cases on renderer::Vulkan (main.cpp: GWENI_RENDERER=vulkan,
            # validated), but the gl_only suite: what only GL has, each file's
            # header comment saying why
            add_test(NAME unittests_vulkan
                COMMAND $<TARGET_FILE:gweni_unittests> --test-suite-exclude=gl_only
                WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
            set_tests_properties(unittests_vulkan PROPERTIES
                ENVIRONMENT GWENI_RENDERER=vulkan
                TIMEOUT 300)
            gweni_vulkan_test_environment(unittests_vulkan)
        endif()
    endif()
else()
    message(STATUS "tests: unit tests skipped, they need gweni_capture (GWENI_BUILD_EXAMPLES)")
endif()

# web_probe: the image's pinned headless Chromium gives a WebGL 2 context with
# 24 depth bits (web/probe.mjs, run on emsdk's node with Playwright from
# NODE_PATH, as the Dockerfile sets them up), and web_probe_fails_without_webgl2
# that the probe fails when the browser has no WebGL 2 (--disable-webgl2), so
# it cannot pass with no GL at all. Registered in the web tree only.
if(EMSCRIPTEN)
    # emsdk's node, which ./generate.sh web sets as the emulator
    if(NOT CMAKE_CROSSCOMPILING_EMULATOR)
        message(FATAL_ERROR "tests: the web tree needs node as CMAKE_CROSSCOMPILING_EMULATOR (./generate.sh web sets emsdk's)")
    endif()
    add_test(NAME web_probe
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_CURRENT_SOURCE_DIR}/web/probe.mjs)
    add_test(NAME web_probe_fails_without_webgl2
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_CURRENT_SOURCE_DIR}/web/probe.mjs --no-webgl2)
    set_tests_properties(web_probe web_probe_fails_without_webgl2 PROPERTIES
        LABELS web
        TIMEOUT 120)
    # each passes only on its own last line, so a probe that never ran fails
    set_tests_properties(web_probe PROPERTIES
        PASS_REGULAR_EXPRESSION "web_probe: OK, WebGL 2")
    set_tests_properties(web_probe_fails_without_webgl2 PROPERTIES
        PASS_REGULAR_EXPRESSION "FAIL: canvas.getContext\\('webgl2'\\) gave no WebGL 2 context")

    # The pages, run in the browser by web/run.mjs (no GPU lock: Chromium's
    # SwiftShader is its own, run.mjs keeps it off Mesa's lavapipe). A run fails
    # on a page error, an abort, or unless it made a WebGL 2 context on
    # SwiftShader with 24 depth bits.
    set(web_runner ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_CURRENT_SOURCE_DIR}/web/run.mjs)

    if(TARGET gweni_unittests)
        # Every gweni_unittests case but those that cannot hold on WebGL 2, each
        # with its reason (the doctest summary counts them as skipped):
        # - "Text AA:*", "LCD text:*": LCD text needs dual-source blending, which
        #   WebGL 2 does not have, so setTextAA(LcdRGB/LcdBGR) stays grayscale;
        #   noDualSource.cpp, which runs here, checks that fallback.
        # - "Text draws: a loop of glDrawElements draws what glMultiDrawElements
        #   draws": OpenGLCore has no multi-draw on the web (no
        #   GL_EXT_multi_draw_arrays), so the loop is every text draw here and
        #   there is no multi-draw to compare it with.
        # - "Primitive draws: a loop of glDrawElements draws what
        #   glMultiDrawElements draws": the same for a primitive chunk's ranges.
        set(web_unittests_excluded
            "Text AA:*"
            "LCD text:*"
            "Text draws: a loop of glDrawElements draws what glMultiDrawElements draws"
            "Primitive draws: a loop of glDrawElements draws what glMultiDrawElements draws")
        string(REPLACE ";" "," web_unittests_excluded "${web_unittests_excluded}")
        add_test(NAME web_unittests
            COMMAND ${web_runner} --timeout 900 $<TARGET_FILE:gweni_unittests>
                -- "--test-case-exclude=${web_unittests_excluded}")
        set_tests_properties(web_unittests PROPERTIES
            LABELS web
            TIMEOUT 960)
    endif()

    if(TARGET ControlTest)
        # a capture in the browser: the PNG is written, of the canvas's size
        # (ControlTest's default 640x480, OpenGLCore's 1024x768), and is not one
        # flat colour
        add_test(NAME web_controltest_shot
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_CURRENT_SOURCE_DIR}/web/shot.mjs
                --size 640x480 --png ButtonDemo.png
                --out ${CMAKE_CURRENT_BINARY_DIR}/web_controltest_shot
                $<TARGET_FILE:ControlTest>
                -- --headless --control ButtonDemo --shot /out/ButtonDemo.png)
        set(web_shot_tests web_controltest_shot)
        if(TARGET OpenGLCore)
            add_test(NAME web_openglcore_screenshot
                COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_CURRENT_SOURCE_DIR}/web/shot.mjs
                    --size 1024x768 --png OpenGLCore.png
                    --out ${CMAKE_CURRENT_BINARY_DIR}/web_openglcore_screenshot
                    $<TARGET_FILE:OpenGLCore>
                    -- --headless --screenshot /out/OpenGLCore.png)
            list(APPEND web_shot_tests web_openglcore_screenshot)
        endif()
        set_tests_properties(${web_shot_tests} PROPERTIES
            LABELS web
            TIMEOUT 300
            PASS_REGULAR_EXPRESSION "shot.mjs: OK")

        # the runner fails a page that got no WebGL 2 context, whatever its exit
        # status (ControlTest's own, once its context fails, is 1 too): passes
        # only on the runner's line saying so
        add_test(NAME web_run_fails_without_webgl2
            COMMAND ${web_runner} --no-webgl2 $<TARGET_FILE:ControlTest>
                -- --headless --control ButtonDemo --shot /out/ButtonDemo.png)
        set_tests_properties(web_run_fails_without_webgl2 PROPERTIES
            LABELS web
            TIMEOUT 120
            PASS_REGULAR_EXPRESSION "run.mjs: FAIL: ControlTest.html made no WebGL 2 context")

        # missingResources.cmake's cases in the browser: the resources/ copy
        # missing a file is mounted into the page's file system (run.mjs
        # --mount), so one preloaded resources/ serves every case
        set(web_missing_resources_tools ControlTest)
        if(TARGET OpenGLCore)
            list(APPEND web_missing_resources_tools OpenGLCore)
        endif()
        foreach(tool ${web_missing_resources_tools})
            string(TOLOWER ${tool} tool_lower)
            foreach(case nodir noskin nofont nomono complete)
                if(case STREQUAL "nodir")
                    set(test_name web_${tool_lower}_missing_resources)
                else()
                    set(test_name web_${tool_lower}_missing_resources_${case})
                endif()
                add_test(NAME ${test_name}
                    COMMAND ${CMAKE_COMMAND}
                        -DEXE=$<TARGET_FILE:${tool}>
                        -DTOOL=${tool}
                        "-DRUNNER=${web_runner}"
                        -DSOURCE_DIR=${GWENI_SOURCE_DIR}
                        -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/${test_name}
                        -DCASE=${case}
                        -P ${CMAKE_CURRENT_SOURCE_DIR}/missingResources.cmake)
                set_tests_properties(${test_name} PROPERTIES
                    LABELS web
                    TIMEOUT 300)
            endforeach()
        endforeach()

        # the browser held to native: determinism, listings, counters, pixels
        include(${CMAKE_CURRENT_SOURCE_DIR}/web/native.cmake)
    endif()

    # the interactive pages, driven with real input (web/interactive.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/web/interactive.cmake)
    # the demo's page in the URL: history, storage, a link out (web/navigation.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/web/navigation.cmake)
    # the hosted site, staged by tools/build_site.sh and loaded (web/site.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/web/site.cmake)
endif()

# The Vulkan plumbing (GWENI_RENDER_VULKAN): loader, validation layer, a 1.3
# device and SPIR-V compiled at build time. Its source is under vulkan/, out of
# the *.cpp glob above.
if(GWENI_RENDER_VULKAN)
    add_executable(vulkan_smoke vulkan/vulkanSmoke.cpp)
    gweni_set_warnings(vulkan_smoke)
    target_link_libraries(vulkan_smoke gweni_vulkan)
    gweni_add_spirv(vulkan_smoke vulkan/smoke.vert vulkan/smoke.frag)

    add_test(NAME vulkan_smoke COMMAND $<TARGET_FILE:vulkan_smoke>)
    set_tests_properties(vulkan_smoke PROPERTIES TIMEOUT 60)
    gweni_vulkan_test_environment(vulkan_smoke)
endif()

# the example window's idle loop in a real window (windowIdle.cmake, whose
# header says what it runs): ControlTest --idle-test under an Xvfb of its own,
# on GL and, with Vulkan, on renderer::Vulkan validated. Without Xvfb it fails
# rather than skips.
if(TARGET ControlTest AND NOT EMSCRIPTEN)
    add_test(NAME window_idle
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DRENDERER=gl
            -P ${CMAKE_CURRENT_SOURCE_DIR}/windowIdle.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(window_idle PROPERTIES
        LABELS gpu
        TIMEOUT 180)
    if(GWENI_RENDER_VULKAN)
        add_test(NAME window_idle_vulkan
            COMMAND ${CMAKE_COMMAND}
                -DCONTROLTEST=$<TARGET_FILE:ControlTest>
                -DRENDERER=vulkan
                -P ${CMAKE_CURRENT_SOURCE_DIR}/windowIdle.cmake
            WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
        set_tests_properties(window_idle_vulkan PROPERTIES TIMEOUT 300)
        gweni_vulkan_test_environment(window_idle_vulkan)
    endif()
    # the whole demo shell, every page built and all but one hidden: a page
    # whose animation runs while hidden keeps the loop drawing and fails it
    add_test(NAME window_idle_demo
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DRENDERER=gl
            -DCONTROL=Demo
            -P ${CMAKE_CURRENT_SOURCE_DIR}/windowIdle.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(window_idle_demo PROPERTIES
        LABELS gpu
        TIMEOUT 180)
endif()

# renderer::Vulkan in a window (windowVulkan.cmake, whose header says what it
# runs): ControlTest --window-test under an Xvfb of its own and gweni_test_wm
# (vulkan/testWindowManager.cpp), the window's presented images against headless
# captures. Without Xvfb, or without the X11 headers the window manager is built
# with, it fails rather than skips.
if(GWENI_RENDER_VULKAN AND TARGET ControlTest AND NOT EMSCRIPTEN)
    find_package(X11)
    set(test_wm "")
    if(X11_FOUND)
        add_executable(gweni_test_wm vulkan/testWindowManager.cpp)
        gweni_set_warnings(gweni_test_wm)
        target_link_libraries(gweni_test_wm X11::X11)
        set(test_wm $<TARGET_FILE:gweni_test_wm>)
    else()
        message(STATUS "tests: no X11 headers for gweni_test_wm; window_vulkan will fail")
    endif()

    add_test(NAME window_vulkan
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DTEST_WM=${test_wm}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/window_vulkan
            -P ${CMAKE_CURRENT_SOURCE_DIR}/windowVulkan.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(window_vulkan PROPERTIES TIMEOUT 300)
    gweni_vulkan_test_environment(window_vulkan)
endif()

# The system clipboard in a GL window (clipboardX11.cmake, whose header says
# what it runs): ControlTest --clipboard-test under an Xvfb of its own, with
# gweni_test_clipboard (x11/testClipboard.cpp) as the other X client, copying
# to it and pasting from it. Built in every tree with ControlTest, GL-only ones
# included. Without Xvfb, or without the X11 headers the helper is built with,
# it fails rather than skips.
if(TARGET ControlTest AND NOT EMSCRIPTEN)
    find_package(X11)
    set(test_clipboard "")
    if(X11_FOUND)
        add_executable(gweni_test_clipboard x11/testClipboard.cpp)
        gweni_set_warnings(gweni_test_clipboard)
        target_link_libraries(gweni_test_clipboard X11::X11)
        set(test_clipboard $<TARGET_FILE:gweni_test_clipboard>)
    else()
        message(STATUS "tests: no X11 headers for gweni_test_clipboard; clipboard_x11 will fail")
    endif()

    add_test(NAME clipboard_x11
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DHELPER=${test_clipboard}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/clipboardX11.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(clipboard_x11 PROPERTIES
        LABELS gpu
        TIMEOUT 300
        ENVIRONMENT GWENI_RENDERER=gl)
endif()

# renderer::Vulkan held to OpenGLCore (backends.mjs, whose header says what each
# mode runs): backend_counters, every counter of every gweni_bench scene and
# ControlTest --stats run the same on both backends; backend_pixels[_scale200],
# every --shot-all PNG and the hero within the bound below of GL's. node is
# emsdk's, which the image has (EMSDK_NODE) for the web tree; without it the
# tests fail, saying so, rather than skip. gweni_bench is added after this
# directory, in every tree that has ControlTest outside the web.
if(GWENI_RENDER_VULKAN AND TARGET ControlTest AND TARGET OpenGLCore AND NOT EMSCRIPTEN)
    find_program(GWENI_NODE node HINTS "$ENV{EMSDK_NODE}/.." /opt/emsdk/node/current/bin
        DOC "node, which runs backends.mjs (the image's is emsdk's)")
    if(GWENI_NODE)
        set(backends ${GWENI_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/backends.mjs)
    else()
        message(STATUS "tests: no node for backend_counters and backend_pixels, which will fail")
        set(backends sh -c "echo 'backends.mjs: FAIL: no node (EMSDK_NODE, PATH), which this test runs on' >&2; exit 1" backends.mjs)
    endif()
    set(backends_bin --bin $<TARGET_FILE_DIR:ControlTest>)

    add_test(NAME backend_counters
        COMMAND ${backends} counters ${backends_bin} --bench $<TARGET_FILE:gweni_bench>
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(backend_counters PROPERTIES TIMEOUT 1800)
    gweni_vulkan_test_environment(backend_counters)

    # The pixel bound, per PNG (web/pngdiff.mjs): no channel of any pixel more
    # than backend_pixels_max_delta from GL, and at most backend_pixels_max_fraction
    # percent of the pixels differing from it at all.
    # Measured 2026-11-09 over the 51 --shot-all PNGs and the hero, lavapipe
    # against llvmpipe (Mesa 23.2.1), debug tree, after style 4's gradient became
    # an exact integer level per row (#492):
    #   scale  max |delta|  pixels that differ, worst
    #   1      1            0.003 % (TextSizesDemo: 8 pixels; ShowcaseDisplayDemo 1)
    #   1.5    1            0.002 % (TextSizesDemo; 5 of the 52 differ, ThemeEditorDemo 14 pixels)
    #   2      1            0.007 % (LabelMultilineDemo; 36 of the 52 differ, in 1 to 82 pixels)
    # They are SDF glyphs' edge pixels (at 2 a glyph's horizontal stroke, up to
    # 8 pixels of one row) where a blend or a coverage lands on an 8-bit tie,
    # which the two drivers round apart; no gradient row differs. The rest is
    # exact: the shared fragment bodies spell mix() out (lerp()), which the GLSL
    # compiler and glslang otherwise hand Mesa as two different lerps, and style
    # 4's gradient level is computed in ints from the row, since a float one
    # landed on a tie on some rows and an ulp of the interpolated position decided
    # it, a level apart on whole rows: up to 0.875 % of a page before lerp(), and
    # 0.635 % of ThemeEditorDemo with #289's colours before #492
    # (docs/architecture.md -> "Vulkan's counters and pixels against GL's").
    # The worst case did not move with #492 (the gradient rows that differed
    # were in colours the theme then avoided), so 1 and 0.01 %, the worst
    # rounded up, as before.
    # The ceiling is max |delta| 2 and 0.5 %: a PNG above it is a bug to find,
    # never a reason to widen the bound.
    set(backend_pixels_max_delta 1)
    set(backend_pixels_max_fraction 0.01)
    foreach(scale_suffix "1:" "2:_scale200")
        string(REPLACE ":" ";" scale_suffix "${scale_suffix}")
        list(GET scale_suffix 0 scale)
        list(GET scale_suffix 1 suffix)
        set(scale_args)
        if(suffix)
            set(scale_args --scale ${scale})
        endif()
        add_test(NAME backend_pixels${suffix}
            COMMAND ${backends} pixels ${backends_bin} ${scale_args}
                --max-delta ${backend_pixels_max_delta}
                --max-fraction ${backend_pixels_max_fraction}
            WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
        set_tests_properties(backend_pixels${suffix} PROPERTIES TIMEOUT 900)
        gweni_vulkan_test_environment(backend_pixels${suffix})
    endforeach()
endif()

# ASAN tree (./generate.sh linux asan): overflows fail the test, leaks do not.
# Mesa/EGL leak at exit and none of that is ours, so leak checking is off; with
# abort_on_error a report aborts the process instead of exiting 1, which ctest
# and determinism.cmake (whose ControlTest children inherit this environment
# through execute_process) both count as a failure.
if(GWENI_SANITIZE MATCHES "address")
    get_property(gweni_tests DIRECTORY PROPERTY TESTS)
    # appended, so a test's own environment (the Vulkan tests') stays
    set_property(TEST ${gweni_tests} APPEND PROPERTY
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
endif()
