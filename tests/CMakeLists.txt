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
    # dragged past its parent's edges, the tooltip box and the context menu
    foreach(verify_scenario interaction resize scale clip tooltip context-menu)
        string(REPLACE "-" "_" verify_name "${verify_scenario}")
        add_test(NAME controltest_verify_layout_${verify_name}
            COMMAND $<TARGET_FILE:ControlTest> --headless --scenario ${verify_scenario}
                --verify-layout
                --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/verify_layout_${verify_name}
            WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
        set_tests_properties(controltest_verify_layout_${verify_name} PROPERTIES
            LABELS gpu
            TIMEOUT 300)
    endforeach()

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

    # PopupDemo: a click on Options opens its popup below it, changing nothing
    # outside the popup and the button; a settled frame with it open writes
    # nothing; a click outside and Escape each give the frame before opening
    # back; the button toggles it; the Corner popup opens above its button,
    # inside the canvas; one popup at a time; a ComboBox list opens over it;
    # a tooltip draws over it; hiding its button closes it; exits non-zero on
    # any failed check
    add_test(NAME controltest_popup
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario popup --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/popup
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_popup PROPERTIES
        LABELS gpu
        TIMEOUT 300)

    # VirtualListDemo at 1 000 and 100 000 items: the same pool, ids and
    # per-frame counters at both, a settled frame writing nothing, and a
    # one-row scroll and a wheel notch writing no more than the shown rows'
    # slots, entering only the list's subtree in one layout pass with no z
    # renumber; exits non-zero on any failed check
    add_test(NAME controltest_virtual_list
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario virtual-list --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/virtual-list
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_virtual_list PROPERTIES
        LABELS gpu
        TIMEOUT 120)

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

    # the same at --scale 2, where the skin's 11 px text is 22 device px and so
    # SDF text: the Window dragged around and back must still look as it did,
    # which holds only if a moved text run is exactly a fresh layout
    add_test(NAME controltest_clip_scale200
        COMMAND $<TARGET_FILE:ControlTest> --headless --scale 2 --scenario clip --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/clip_scale200
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_clip_scale200 PROPERTIES
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

    # a capture from a --resources directory that is missing, or lacks the skin
    # config or the default font, names what is missing, still writes its
    # (degraded) frame and exits 2; from a complete copy it exits 0
    set(missing_resources_tools ControlTest)
    if(TARGET OpenGLCore)
        list(APPEND missing_resources_tools OpenGLCore)
    endif()
    foreach(tool ${missing_resources_tools})
        string(TOLOWER ${tool} tool_lower)
        foreach(case nodir noskin nofont complete)
            if(case STREQUAL "nodir")
                set(test_name ${tool_lower}_missing_resources)
            else()
                set(test_name ${tool_lower}_missing_resources_${case})
            endif()
            add_test(NAME ${test_name}
                COMMAND ${CMAKE_COMMAND}
                    -DEXE=$<TARGET_FILE:${tool}>
                    -DTOOL=${tool}
                    -DSOURCE_DIR=${GWENI_SOURCE_DIR}
                    -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/${test_name}
                    -DCASE=${case}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/missingResources.cmake)
            set_tests_properties(${test_name} PROPERTIES
                LABELS gpu
                TIMEOUT 60)
        endforeach()
    endforeach()
else()
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

    add_test(NAME unittests
        COMMAND $<TARGET_FILE:gweni_unittests>
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    # every case in one process: about 57 s in the ASAN tree on its own, so 60
    # timed out there as soon as another test ran beside it
    set_tests_properties(unittests PROPERTIES
        LABELS gpu
        TIMEOUT 180)
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
