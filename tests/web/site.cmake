#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  web_site_smoke: the hosted site as a visitor gets it. Included from
#  tests/CMakeLists.txt's if(EMSCRIPTEN) block, it registers the test; run by
#  the test with cmake -P, it is the test:
#    1. tools/build_site.sh --build-dir <this tree> --out <scratch>: builds
#       ControlTest and OpenGLCore here (nothing, after the tree's own build),
#       stages the site and fails if the gallery page is over the script's
#       BUDGET_BYTES (tools/site/sizes.mjs);
#    2. web/site.mjs <scratch>: every page of it in headless Chromium (its
#       header lists the checks), its screenshots kept in <scratch>-shots.
#  Registered in the release web tree only (./generate.sh web release), the one
#  the site is built from: the debug tree's ControlTest.wasm is 31 MB, over the
#  budget by design. Like the other browser tests it needs no lock: Chromium's
#  SwiftShader is its own (probe.mjs' launchOptions keeps it off Mesa's
#  lavapipe), and building takes none.
#
if(CMAKE_SCRIPT_MODE_FILE)
    execute_process(COMMAND ${BUILD_SITE} --build-dir ${BUILD_DIR} --out ${OUT}
        RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "web_site_smoke: tools/build_site.sh failed (exit ${status})")
    endif()
    execute_process(COMMAND ${NODE} ${SITE_MJS} --out ${OUT}-shots ${OUT}
        RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "web_site_smoke: site.mjs failed (exit ${status})")
    endif()
    return()
endif()

if(TARGET ControlTest AND TARGET OpenGLCore AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME web_site_smoke
        COMMAND ${CMAKE_COMMAND}
            -DBUILD_SITE=${GWENI_SOURCE_DIR}/tools/build_site.sh
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/web_site
            -DNODE=${CMAKE_CROSSCOMPILING_EMULATOR}
            -DSITE_MJS=${CMAKE_CURRENT_LIST_DIR}/site.mjs
            -P ${CMAKE_CURRENT_LIST_FILE})
    set_tests_properties(web_site_smoke PROPERTIES
        LABELS web
        TIMEOUT 900
        PASS_REGULAR_EXPRESSION "site.mjs: OK, every page loaded")
endif()
