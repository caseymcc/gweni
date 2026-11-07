#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  gweni tests, run with: ctest --test-dir build_linux_x64_debug
#
#  Tests labelled "gpu" open a GL context (the examples' surfaceless EGL path,
#  --headless), so skip them with ctest -LE gpu. The examples load the
#  resources/ the build copies next to them, so they run from any working
#  directory (controltest_any_cwd checks it); their tests still run from the
#  repo root, where relative --shot paths are meant. gweni_unittests loads
#  resources/ relative to the working directory, so it must run there.
#

if(TARGET ControlTest)
    # every --shot-all PNG and --dump-bounds listing, byte-identical across two
    # separate ControlTest processes
    add_test(NAME controltest_determinism
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/determinism
            -P ${CMAKE_CURRENT_SOURCE_DIR}/determinism.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_determinism PROPERTIES
        LABELS gpu
        TIMEOUT 600)

    # the same at --scale 1.5, where every edge is rounded to the device-pixel
    # grid, so a rounding that depends on anything but the input shows here
    add_test(NAME controltest_determinism_scale150
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/determinism_scale150
            -DSCALE=1.5
            -P ${CMAKE_CURRENT_SOURCE_DIR}/determinism.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_determinism_scale150 PROPERTIES
        LABELS gpu
        TIMEOUT 600)

    # --shot-all --dump-bounds at --scale 1 and 2: the listings (logical units)
    # are identical and every PNG (device pixels) is exactly twice the size
    add_test(NAME controltest_scale_bounds
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/scale_bounds
            -P ${CMAKE_CURRENT_SOURCE_DIR}/scaleBounds.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_scale_bounds PROPERTIES
        LABELS gpu
        TIMEOUT 600)

    # every --shot-all PNG (and OpenGLCore's --screenshot) byte-identical with the
    # draws split into chunks of 64 slots (GWENI_INDEX_CHUNK_SLOTS) and without
    set(index_chunks_opengl)
    if(TARGET OpenGLCore)
        set(index_chunks_opengl -DOPENGLCORE=$<TARGET_FILE:OpenGLCore>)
    endif()
    add_test(NAME controltest_index_chunks
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            ${index_chunks_opengl}
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/index_chunks
            -DCHUNK_SLOTS=64
            -P ${CMAKE_CURRENT_SOURCE_DIR}/indexChunks.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_index_chunks PROPERTIES
        LABELS gpu
        TIMEOUT 600)

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
    add_test(NAME controltest_teardown
        COMMAND $<TARGET_FILE:ControlTest> --headless --check-teardown
            --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/teardown
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_teardown PROPERTIES
        LABELS gpu
        TIMEOUT 300)

    # the whole Demo at 640x480, 1024x768 and 640x480 again: the listing, the
    # renderer mirror (per id) and the pixels of the last shot match the first
    # and a canvas built directly at 640x480, and every frame takes one layout
    # pass (issue #58)
    add_test(NAME controltest_resize
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario resize
            --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/resize
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_resize PROPERTIES
        LABELS gpu
        TIMEOUT 120)

    # the same round trip for every other control in --list, one process each
    add_test(NAME controltest_resize_each
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/resize_each
            -DMODE=resize
            -DSKIP=Demo
            -P ${CMAKE_CURRENT_SOURCE_DIR}/eachControl.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_resize_each PROPERTIES
        LABELS gpu
        TIMEOUT 600)

    # the plain run of every control in --list (Demo included): every frame
    # takes one layout pass and the render phase lays out nothing
    add_test(NAME controltest_one_pass_each
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/one_pass_each
            -DMODE=one-pass
            -P ${CMAKE_CURRENT_SOURCE_DIR}/eachControl.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_one_pass_each PROPERTIES
        LABELS gpu
        TIMEOUT 600)

    # Canvas::setScale 1 -> 2 -> 1 for every control in --list (Demo included),
    # one process each: the last 1x shot, its listing and the renderer mirror
    # are the first one's, the 2x shot is a canvas built at scale 2, and every
    # frame takes one layout pass
    add_test(NAME controltest_scale_roundtrip
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/scale_roundtrip
            -DMODE=scale
            -P ${CMAKE_CURRENT_SOURCE_DIR}/eachControl.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_scale_roundtrip PROPERTIES
        LABELS gpu
        TIMEOUT 600)

    # hover, press, disabled and focus repaint the control (and only it), a
    # settled frame does nothing, and collapsing a tree node still moves the
    # nodes below; exits non-zero on any failed check
    add_test(NAME controltest_interaction
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario interaction
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/interaction
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_interaction PROPERTIES
        LABELS gpu
        TIMEOUT 300)

    # the canvas's tooltip on ButtonDemo, on the stepped clock: it waits for
    # its delay, shows below the button above every other control (an open
    # Menu included), writes nothing once settled or for a move inside the
    # button, moves the same box straight to the next button, hides on leaving
    # (the frame as before hovering) and on a press, and goes above a button in
    # the bottom-right corner; exits non-zero on any failed check
    add_test(NAME controltest_tooltip
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario tooltip --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/tooltip
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_tooltip PROPERTIES
        LABELS gpu
        TIMEOUT 300)

    # right-clicks on ContextMenuDemo (Base::setContextMenu): the menu opens with
    # its top-left at the cursor changing nothing visible outside it and its box,
    # a settled frame writes nothing, a click outside and Escape give the frame
    # before opening back, it flips inside the canvas in the bottom-right
    # corner, a right-click on the other box moves to its menu, a disabled item
    # does nothing and an enabled one is chosen, the Help tooltip draws over an
    # open menu, and hiding or deleting the box closes it; exits non-zero on any
    # failed check
    add_test(NAME controltest_context_menu
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario context-menu --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/context-menu
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_context_menu PROPERTIES
        LABELS gpu
        TIMEOUT 300)

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
        add_test(NAME controltest_multiline_keys_${variant}
            COMMAND $<TARGET_FILE:ControlTest> --headless --control TextBoxDemo
                --frames 3 --click 5,314 --frames 2
                --key home --type X ${between} --key up --type Y ${between}
                --key up --type Z ${between} --expect-text "${SAME_FRAME_TEXT}"
            WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
        set_tests_properties(controltest_multiline_keys_${variant} PROPERTIES
            LABELS gpu
            TIMEOUT 60)
    endforeach()

    # children are cut at their clipping parent's edges: content overflowing a
    # ScrollControl and a Window dragged past each edge of its parent leave the
    # pixels outside unchanged, and moved right out they leave nothing behind;
    # exits non-zero on any failed check
    add_test(NAME controltest_clip
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario clip --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/clip
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_clip PROPERTIES
        LABELS gpu
        TIMEOUT 300)

    # a Label's font changed from 24 to 36 px, both drawn from the face's SDF
    # atlas, uploads nothing to an atlas and writes only that Label's glyph slots
    add_test(NAME controltest_text_size
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario text-size
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/text_size
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_text_size PROPERTIES
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

    # a --resources directory that does not exist is one line naming it, before
    # any GL context is made, so this needs no GPU
    add_test(NAME controltest_missing_resources
        COMMAND $<TARGET_FILE:ControlTest> --headless --control ButtonDemo
            --shot ${CMAKE_CURRENT_BINARY_DIR}/missing_resources/ButtonDemo.png
            --resources ${CMAKE_CURRENT_BINARY_DIR}/no_such_resources)
    set_tests_properties(controltest_missing_resources PROPERTIES
        PASS_REGULAR_EXPRESSION "^ControlTest: resource directory not found: [^\n]*/no_such_resources\n*$"
        TIMEOUT 30)
else()
    message(STATUS "tests: ControlTest tests skipped, GWENI_BUILD_EXAMPLES is off")
endif()

find_package(doctest CONFIG REQUIRED)
include(doctest)

# Headless C++ unit tests (doctest) under unit/: no GL context and no resources,
# so they carry no gpu label and ctest -LE gpu runs them without the GPU lock.
# Each test case is registered as its own CTest test, so a failure names it.
add_executable(gweni_unit_tests
    unit/main.cpp
    unit/imglibEnums.cpp
    unit/skinSwap.cpp
    unit/platformSleep.cpp
    unit/layoutPasses.cpp
    unit/renderWalk.cpp
    unit/clipRegion.cpp
    unit/contextMenu.cpp
    unit/selectable.cpp
    unit/separator.cpp
    unit/sliderNotches.cpp
    unit/colorControls.cpp
    unit/colorDisplay.cpp
    unit/scrollControl.cpp
    unit/resourcePaths.cpp
    unit/requestedSize.cpp
    unit/layoutSizes.cpp
    unit/tabOrder.cpp
    unit/depthLayers.cpp
    unit/overlayDepth.cpp
    unit/updateWalk.cpp
    unit/utf8Typing.cpp
    unit/placeholder.cpp
    unit/progressBarIndeterminate.cpp
    unit/cursorScale.cpp
    unit/crossSplitter.cpp
    unit/placement.cpp
    unit/windowSkin.cpp
    unit/tabStrip.cpp
    unit/menuItem.cpp
    unit/textBoxCaret.cpp
    unit/dockNoneAlignment.cpp
    unit/textAlignment.cpp
    unit/toolTip.cpp
    unit/textBoxColor.cpp
    unit/drawLine.cpp
    unit/mouseCapture.cpp)
gweni_set_warnings(gweni_unit_tests)
target_link_libraries(gweni_unit_tests
    gweni
    doctest::doctest)

# utf8Typing.cpp drives a Canvas through the GLFW translator
# (gweni/platforms/input/glfw3.h), which only uses GLFW's constants: it takes
# GLFW's headers and does not link the library.
find_package(glfw3 CONFIG REQUIRED)
target_include_directories(gweni_unit_tests PRIVATE
    $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)

# discovered at build time, so the ASAN environment below does not reach them;
# they get it here instead
set(gweni_unit_tests_properties TIMEOUT 60)
if(GWENI_SANITIZE MATCHES "address")
    list(APPEND gweni_unit_tests_properties
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
endif()
doctest_discover_tests(gweni_unit_tests
    TEST_PREFIX "unit:"
    PROPERTIES ${gweni_unit_tests_properties})

# C++ unit tests (doctest) that need a GL context, one executable. They get it
# from the examples' surfaceless EGL code (gweni_capture), so they are only
# built along with the examples.
if(TARGET gweni_capture)
    add_executable(gweni_unittests
        main.cpp
        textLength.cpp
        textRunCache.cpp
        textWrap.cpp
        textAtlases.cpp
        textAA.cpp
        textSubpixel.cpp
        textGrayscale.cpp
        sdfText.cpp
        fontLoad.cpp
        fontRelease.cpp
        color.cpp
        fillStyles.cpp
        iconGlyphs.cpp
        clipBoxes.cpp
        indexChunks.cpp
        scaledEdges.cpp
        scaledText.cpp
        textureReload.cpp
        imageRects.cpp
        progressBarFill.cpp
        comboBoxSize.cpp
        emptyText.cpp
        scrollControlBackdrop.cpp
        menuRows.cpp
        lines.cpp)
    gweni_set_warnings(gweni_unittests)
    target_link_libraries(gweni_unittests
        gweni
        gweni_capture
        doctest::doctest)

    add_test(NAME unittests
        COMMAND $<TARGET_FILE:gweni_unittests>
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(unittests PROPERTIES
        LABELS gpu
        TIMEOUT 60)
else()
    message(STATUS "tests: unit tests skipped, they need gweni_capture (GWENI_BUILD_EXAMPLES)")
endif()

# ASAN tree (./generate.sh linux asan): overflows fail the test, leaks do not.
# Mesa/EGL leak at exit and none of that is ours, so leak checking is off; with
# abort_on_error a report aborts the process instead of exiting 1, which ctest
# and determinism.cmake (whose ControlTest children inherit this environment
# through execute_process) both count as a failure.
if(GWENI_SANITIZE MATCHES "address")
    get_property(gweni_tests DIRECTORY PROPERTY TESTS)
    set_tests_properties(${gweni_tests} PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
endif()
