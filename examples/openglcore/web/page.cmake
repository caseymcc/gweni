#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  gweni_web_page(<target>): makes an Emscripten executable a page of the web
#  tree, <target>.html beside its .js, .wasm and .data:
#  - shell.html, a bare page with the #canvas the WebGL 2 context is made on
#    (capture::initEglContext) and a panel showing the output;
#  - pre.js, which turns the page's URL query into argv and hands the run's
#    exit code, output and files to the browser runner (tests/web/run.mjs);
#  - resources/ preloaded at /resources, where RelativeToExecutablePaths
#    ("resources") looks on the web (platform::getExecutableDir() is "/"), and
#    relinked when a file under resources/ changes, since the .data is made at
#    link time; the fonts' license texts are left out (nothing loads them, and
#    the hosted site, tools/build_site.sh, ships them as files of their own);
#  - WebGL 2 (from gweni), a heap that grows, a 4 MB stack (Emscripten's default
#    is 64 KB; the layout and render walks recurse once per tree level) and the
#    runtime exiting when main returns, so a capture run ends as natively (an
#    interactive window's main loop, capture::runWindowLoop, keeps it alive);
#  - Module._malloc, Module._free and Module.HEAPU8, for a script handing a
#    page bytes (a fetched image, ControlTest's gweni_web_set_image);
#  - Module.ccall, for a script reading what a page exports with
#    EMSCRIPTEN_KEEPALIVE (ControlTest's gweni_web_* functions).
#  An interactive window fills the shell's #canvas-container (capture.h's
#  initGlfwContext), and runs one frame per animation frame.
#  Defined once, for the examples here and gweni_unittests (tests/).
#
function(gweni_web_page target)
    set(page_dir ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
    file(GLOB_RECURSE resources CONFIGURE_DEPENDS ${GWENI_SOURCE_DIR}/resources/*)
    set_target_properties(${target} PROPERTIES SUFFIX ".html")
    target_link_options(${target} PRIVATE
        "SHELL:--shell-file ${page_dir}/shell.html"
        "SHELL:--pre-js ${page_dir}/pre.js"
        "SHELL:--preload-file ${GWENI_SOURCE_DIR}/resources@/resources"
        "SHELL:--exclude-file *-LICENSE.txt"
        -sALLOW_MEMORY_GROWTH=1
        -sSTACK_SIZE=4MB
        -sEXIT_RUNTIME=1
        -sEXPORTED_RUNTIME_METHODS=ccall,HEAPU8
        -sEXPORTED_FUNCTIONS=_main,_malloc,_free)
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
        ${page_dir}/shell.html
        ${page_dir}/pre.js
        ${resources})
endfunction()
