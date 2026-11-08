#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  web_input (web tree only, included from tests/CMakeLists.txt's if(EMSCRIPTEN)
#  block): the interactive pages, ControlTest's window and the OpenGLCore demo,
#  driven in headless Chromium with real input events by web/interactive.mjs
#  (its header lists the checks). Both pages are in ControlTest's directory.
#  Like the other browser tests it needs no lock: Chromium's SwiftShader is its
#  own (probe.mjs' launchOptions keeps it off Mesa's lavapipe).
#
if(TARGET ControlTest AND TARGET OpenGLCore)
    add_test(NAME web_input
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_CURRENT_LIST_DIR}/interactive.mjs
            --out ${CMAKE_CURRENT_BINARY_DIR}/web_input
            $<TARGET_FILE_DIR:ControlTest>)
    set_tests_properties(web_input PROPERTIES
        LABELS web
        TIMEOUT 900
        PASS_REGULAR_EXPRESSION "interactive.mjs: OK, every check passed")
endif()
