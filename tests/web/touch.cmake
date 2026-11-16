#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  web_touch (web tree only, included from tests/CMakeLists.txt's if(EMSCRIPTEN)
#  block): ControlTest's window driven as a phone drives it, a touch context's
#  fingers and an input method through CDP, by web/touch.mjs (its header lists
#  the checks). No lock, like the other browser tests.
#
if(TARGET ControlTest)
    add_test(NAME web_touch
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_CURRENT_LIST_DIR}/touch.mjs
            --out ${CMAKE_CURRENT_BINARY_DIR}/web_touch
            $<TARGET_FILE_DIR:ControlTest>)
    set_tests_properties(web_touch PROPERTIES
        LABELS web
        TIMEOUT 900
        PASS_REGULAR_EXPRESSION "touch.mjs: OK, every check passed")
endif()
