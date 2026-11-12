#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  clipboard_x11: a GL window's clipboard is the X server's CLIPBOARD, both
#  ways. Under an X server of its own (xvfb-run -a, a free display, killed
#  when the run ends), gweni_test_clipboard (x11/testClipboard.cpp) plays the
#  other application and runs ControlTest --clipboard-test, a script in a
#  shown GL window (llvmpipe GLX) whose clipboard is the system one
#  (input::GLFW3::useSystemClipboard), on TextBoxDemo's first box:
#    1. a click focuses it, Ctrl+A and COPY typed replace its text, Ctrl+A and
#       Ctrl+C copy it; --wait-clipboard-change then polls the window, which
#       answers the helper's UTF8_STRING requests from glfwPollEvents;
#    2. the helper, once it has read exactly COPY, owns CLIPBOARD with OFFER;
#       the window sees the clipboard change and its wait ends;
#    3. Ctrl+A and Ctrl+V replace the box's text with what it pastes, and
#       --expect-text checks it is exactly OFFER.
#  Both texts hold non-ASCII codepoints (two- and three-byte UTF-8). It fails
#  unless the helper read COPY and served OFFER (its exit status), ControlTest
#  exited 0 (an --expect-text mismatch or a wait timed out fails it) and both
#  printed their lines. Without xvfb-run/Xvfb or the X11 headers the helper
#  is built with, it fails, saying so; it never skips.
#
#  -DCONTROLTEST=<exe> -DHELPER=<exe>
#
cmake_minimum_required(VERSION 3.16)

function(fail message)
    message(FATAL_ERROR "clipboard_x11: FAIL: ${message}")
endfunction()

find_program(XVFB_RUN xvfb-run)
find_program(XVFB Xvfb)
if(NOT XVFB_RUN OR NOT XVFB)
    fail("no xvfb-run or Xvfb on PATH; the clipboard test needs Xvfb (the image's xvfb package; ./run_docker.sh -r rebuilds it)")
endif()
if(NOT HELPER OR NOT EXISTS "${HELPER}")
    fail("no gweni_test_clipboard (\"${HELPER}\"): it needs the X11 headers (xorg-dev in the image)")
endif()

set(COPY "copy é €")
set(OFFER "pasted ü ✓ 日")

execute_process(
    COMMAND ${XVFB_RUN} -a -s "-screen 0 1280x1024x24 -nolisten tcp"
        ${HELPER} --expect "${COPY}" --offer "${OFFER}" --timeout 90 --
        ${CONTROLTEST} --clipboard-test --control TextBoxDemo
            --click 100,10 --frames 2
            --key ctrl+a --type "${COPY}" --expect-text "${COPY}"
            --key ctrl+a --key ctrl+c
            --wait-clipboard-change 30
            --key ctrl+a --key ctrl+v --expect-text "${OFFER}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    TIMEOUT 240)
message("${output}${errors}")
if(NOT result EQUAL 0)
    fail("the helper or ControlTest --clipboard-test exited ${result}")
endif()

foreach(line
        "gweni_test_clipboard: read \"copy \\\\xc3\\\\xa9 \\\\xe2\\\\x82\\\\xac\" from CLIPBOARD"
        "clipboard-test: the clipboard changed to \"${OFFER}\""
        "gweni_test_clipboard: served the offer")
    string(REGEX MATCH "${line}" found "${output}")
    if(NOT found)
        fail("no line \"${line}\" in the output")
    endif()
endforeach()
message("clipboard_x11: the window's copy read by another client and that client's text pasted, exactly")
