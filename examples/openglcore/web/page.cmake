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
#    link time;
#  - WebGL 2 (from gweni), a heap that grows, a 4 MB stack (Emscripten's default
#    is 64 KB; the layout and render walks recurse once per tree level) and the
#    runtime exiting when main returns, so a capture run ends as natively.
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
        -sALLOW_MEMORY_GROWTH=1
        -sSTACK_SIZE=4MB
        -sEXIT_RUNTIME=1)
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
        ${page_dir}/shell.html
        ${page_dir}/pre.js
        ${resources})
endfunction()
