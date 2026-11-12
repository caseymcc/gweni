#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  web_navigation (web tree only, included from tests/CMakeLists.txt's
#  if(EMSCRIPTEN) block): the OpenGLCore demo's page in the URL, driven in
#  headless Chromium by web/navigation.mjs (its header lists the checks): a
#  pick pushing ?demo=<Name>, back and forward, a deep-link reload, the shell's
#  link opening a tab, the last page kept in localStorage, on OpenGLCore.html
#  and the hosted site's demo.html (tools/site/, served beside the build's
#  files), and the page served at every path under /app/. Like the other
#  browser tests it needs no lock.
#
if(TARGET OpenGLCore)
    add_test(NAME web_navigation
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_CURRENT_LIST_DIR}/navigation.mjs
            $<TARGET_FILE_DIR:OpenGLCore>)
    set_tests_properties(web_navigation PROPERTIES
        LABELS web
        TIMEOUT 900
        PASS_REGULAR_EXPRESSION "navigation.mjs: OK, every check passed")
endif()
