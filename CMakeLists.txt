# CMake build instructions for gweni

# configure cmake
cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 17)

if(WIN32)
    add_definitions(-DNOMINMAX)
endif()

# Dependencies are provided by vcpkg (see vcpkg.json). The vcpkg toolchain file
# is supplied via CMAKE_TOOLCHAIN_FILE by generate.sh; all builds run inside the
# Docker container (see run_docker.sh).
cmake_policy(SET CMP0074 NEW)

option(GWENI_RENDER_OPENGL_CORE "Opengl 3.0+ Renderer" ON)
# OpenGLCore, the examples, the tests and the bench against <GLES3/gl3.h> and
# libGLESv2 instead of glew, on a GLES 3.0 context (./generate.sh linux gles):
# a call newer than GLES 3.0 does not compile. Emscripten is GLES without it.
option(GWENI_GL_ES "Build the GL renderer against GLES 3.0 instead of desktop GL" OFF)
# Off by default, so a build without the Vulkan headers and tools works;
# ./generate.sh turns it on in every tree it configures (with the vcpkg
# manifest feature "vulkan").
option(GWENI_RENDER_VULKAN "Vulkan 1.3 Renderer" OFF)
option(GWENI_BUILD_EXAMPLES "Build Examples" ON)
option(GWENI_BUILD_TESTS "Build tests" ON)
option(GWENI_DEBUG_LOGGING "Verbose per-control update/layout debug logging" OFF)
set(GWENI_SANITIZE "" CACHE STRING "Sanitizers to build with, e.g. address")
option(GWENI_FRAME_POINTERS "Keep frame pointers (-fno-omit-frame-pointer), for profiling" OFF)

project(gweni VERSION 0.1.0 LANGUAGES CXX)

set(GWENI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(GWENI_HEADER_FOLDER "Headers")
set(GWENI_SOURCE_FOLDER "Source")

# some build options
set(CMAKE_FIND_FRAMEWORK LAST)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${GWENI_SOURCE_DIR}/cmake/Modules/")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# output directory
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${GWENI_SOURCE_DIR}/bin) # exe
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${GWENI_SOURCE_DIR}/bin) # .dylib
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${GWENI_SOURCE_DIR}/lib) # .a
#make_directory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
#make_directory(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

# configure projects
include(${GWENI_SOURCE_DIR}/cmake/Config.cmake)
include(${GWENI_SOURCE_DIR}/cmake/Warnings.cmake)

# write the version info
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gweni_config.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/source/platform/include/gwen/Version.h
    @ONLY)

# Sanitizers apply to everything built here, so set them before deps: the
# vendored freetype-gl and imglib must be instrumented too (text_buffer_add_text
# is where an over-read of a gweni string executes). vcpkg's prebuilt
# dependencies stay uninstrumented.
if(GWENI_SANITIZE)
    add_compile_options(-fsanitize=${GWENI_SANITIZE} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${GWENI_SANITIZE})
endif()

# The profile tree (./generate.sh linux profile) keeps frame pointers so a
# profiler can walk the stack. Set here, before deps, for the same reason as the
# sanitizers: the vendored freetype-gl and imglib are on the hot paths too.
if(GWENI_FRAME_POINTERS)
    add_compile_options(-fno-omit-frame-pointer)
endif()

# The web tree: C++ exceptions as natively. Emscripten builds with catching
# off by default, so a throw (std::filesystem's, say) aborts the page.
# -fexceptions is Emscripten's JavaScript-based exception handling, the one
# that links with the vcpkg ports as they are built (their C setjmp/longjmp,
# libpng's and FreeType's, is Emscripten's JavaScript one, which native wasm
# exceptions, -fwasm-exceptions, refuse to link with). Set before deps, like the
# sanitizers, so everything built here agrees.
if(EMSCRIPTEN)
    add_compile_options(-fexceptions)
    add_link_options(-fexceptions)
    # The release web tree is the hosted site (tools/build_site.sh), which is
    # sent over the network on every visit: -Oz, not CMake's -O3. Measured on
    # ControlTest.wasm: 2566 kB (975 kB gzip) against 3041 kB (1080 kB gzip),
    # with no difference in run time that a browser run shows (its
    # --check-teardown and the resize scenario within 3 %). CMake puts these on
    # the link line too, where emcc runs wasm-opt at the same level.
    set(CMAKE_C_FLAGS_RELEASE "-Oz -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-Oz -DNDEBUG")
endif()

# add our projects
#add_subdirectory(docs)
add_subdirectory(deps)

#gweni library
include_directories(
    ${GWENI_SOURCE_DIR}/source/platform/include
    ${GWENI_SOURCE_DIR}/source/gweni/include
    ${GWENI_RENDER_INCLUDES}
    ${GWENI_REFLECT_INCLUDE}
)

set(GWENI_UTILS_SOURCE
    include/gweni/utils/json.h
)

set(GWENI_SOURCES
    include/gweni/align.h
    include/gweni/anim.h
    source/anim.cpp
    include/gweni/config.h
    include/gweni/controlList.h
    source/controlList.cpp
    include/gweni/controls.h
    include/gweni/dragAndDrop.h
    source/dragAndDrop.cpp
    include/gweni/events.h
    source/events.cpp
    include/gweni/export.h
    include/gweni/gweni.h
    source/gweni.cpp
    include/gweni/hook.h
    source/hook.cpp
    include/gweni/inputHandler.h
    source/inputHandler.cpp
    include/gweni/utils/json.h
    include/gweni/objectIdBank.h
    include/gweni/primitiveIds.h
    source/primitiveIds.cpp
    include/gweni/skin.h
    source/skin.cpp
    include/gweni/uiTypes.h
    include/gweni/userData.h
    include/gweni/utility.h
    source/utility.cpp
)

set(GWENI_CONTROL_SOURCES
    include/gweni/controls/base.h
    source/controls/base.cpp
    include/gweni/controls/button.h
    source/controls/button.cpp
    include/gweni/controls/canvas.h
    source/controls/canvas.cpp
    include/gweni/controls/checkBox.h
    source/controls/checkBox.cpp
    include/gweni/controls/collapsibleCategory.h
    source/controls/collapsibleCategory.cpp
    include/gweni/controls/collapsibleList.h
    include/gweni/controls/colorButton.h
    source/controls/colorButton.cpp
    include/gweni/controls/dragNumber.h
    source/controls/dragNumber.cpp
    include/gweni/controls/colorControls.h
    source/controls/colorControls.cpp
    include/gweni/controls/colorEdit.h
    source/controls/colorEdit.cpp
    include/gweni/controls/colorPicker.h
    source/controls/colorPicker.cpp
    include/gweni/controls/comboBox.h
    source/controls/comboBox.cpp
    include/gweni/controls/crossSplitter.h
    source/controls/crossSplitter.cpp
    include/gweni/controls/dataTable.h
    source/controls/dataTable.cpp
    include/gweni/controls/dockBase.h
    source/controls/dockBase.cpp
    include/gweni/controls/dockedTabControl.h
    source/controls/dockedTabControl.cpp
    include/gweni/controls/dragger.h
    source/controls/dragger.cpp
    include/gweni/controls/fieldLabel.h
    include/gweni/controls/filePicker.h
    include/gweni/controls/folderPicker.h
    include/gweni/controls/groupBox.h
    source/controls/groupBox.cpp
    include/gweni/controls/highlight.h
    include/gweni/controls/hsvColorPicker.h
    source/controls/hsvColorPicker.cpp
    include/gweni/controls/horizontalScrollBar.h
    source/controls/horizontalScrollBar.cpp
    include/gweni/controls/horizontalSlider.h
    source/controls/horizontalSlider.cpp
    include/gweni/controls/imagePanel.h
    source/controls/imagePanel.cpp
    include/gweni/controls/label.h
    source/controls/label.cpp
    include/gweni/controls/bullet.h
    source/controls/bullet.cpp
    include/gweni/controls/labelClickable.h
    source/controls/labelClickable.cpp
    include/gweni/controls/toolTip.h
    source/controls/toolTip.cpp
    include/gweni/controls/listBox.h
    source/controls/listBox.cpp
    include/gweni/controls/menu.h
    source/controls/menu.cpp
    include/gweni/controls/menuItem.h
    source/controls/menuItem.cpp
    include/gweni/controls/menuStrip.h
    source/controls/menuStrip.cpp
    include/gweni/controls/modal.h
    include/gweni/controls/numericUpDown.h
    source/controls/numericUpDown.cpp
    include/gweni/controls/pageControl.h
    source/controls/pageControl.cpp
    include/gweni/controls/plotHistogram.h
    source/controls/plotHistogram.cpp
    include/gweni/controls/popup.h
    source/controls/popup.cpp
    include/gweni/controls/progressBar.h
    source/controls/progressBar.cpp
    include/gweni/controls/properties.h
    source/controls/properties.cpp
    include/gweni/controls/propertyTree.h
    source/controls/propertyTree.cpp
    include/gweni/controls/radioButton.h
    source/controls/radioButton.cpp
    include/gweni/controls/radioButtonController.h
    source/controls/radioButtonController.cpp
    include/gweni/controls/rectangle.h
    source/controls/rectangle.cpp
    include/gweni/controls/resizableControl.h
    source/controls/resizableControl.cpp
    include/gweni/controls/resizer.h
    source/controls/resizer.cpp
    include/gweni/controls/richLabel.h
    source/controls/richLabel.cpp
    include/gweni/controls/scrollBar.h
    source/controls/scrollBar.cpp
    include/gweni/controls/scrollBarBar.h
    source/controls/scrollBarBar.cpp
    include/gweni/controls/scrollBarButton.h
    source/controls/scrollBarButton.cpp
    include/gweni/controls/scrollControl.h
    source/controls/scrollControl.cpp
    include/gweni/controls/selectable.h
    source/controls/selectable.cpp
    include/gweni/controls/separator.h
    source/controls/separator.cpp
    include/gweni/controls/slider.h
    source/controls/slider.cpp
    include/gweni/controls/splitterBar.h
    source/controls/splitterBar.cpp
    include/gweni/controls/splitters.h
    include/gweni/controls/statusBar.h
    include/gweni/controls/tabButton.h
    source/controls/tabButton.cpp
    include/gweni/controls/tabControl.h
    source/controls/tabControl.cpp
    include/gweni/controls/tabStrip.h
    source/controls/tabStrip.cpp
    include/gweni/controls/tabTitleBar.h
    include/gweni/controls/text.h
    source/controls/text.cpp
    include/gweni/controls/textBox.h
    source/controls/textBox.cpp
    source/controls/textBoxNumeric.cpp
    include/gweni/controls/toolBar.h
    include/gweni/controls/treeControl.h
    source/controls/treeControl.cpp
    include/gweni/controls/treeNode.h
    source/controls/treeNode.cpp
    include/gweni/controls/verticalScrollBar.h
    source/controls/verticalScrollBar.cpp
    include/gweni/controls/verticalSlider.h
    source/controls/verticalSlider.cpp
    include/gweni/controls/virtualList.h
    source/controls/virtualList.cpp
    include/gweni/controls/windowButtons.h
    include/gweni/controls/windowCanvas.h
    source/controls/windowCanvas.cpp
    include/gweni/controls/windowControl.h
    source/controls/windowControl.cpp
)

set(GWENI_LAYOUT_SOURCES
    # layout
    include/gweni/controls/layout/layout.h
    source/controls/layout/layout.cpp
    include/gweni/controls/layout/position.h
    include/gweni/controls/layout/table.h
    source/controls/layout/table.cpp
    include/gweni/controls/layout/tile.h
)

set(GWENI_DIALOG_SOURCES
    # dialog
    include/gweni/controls/dialogs/fileOpen.h
    source/controls/dialog/fileOpen.cpp
    include/gweni/controls/dialogs/fileSave.h
    source/controls/dialog/fileSave.cpp
    include/gweni/controls/dialogs/folderOpen.h
    source/controls/dialog/folderOpen.cpp
    include/gweni/controls/dialogs/query.h
    source/controls/dialog/query.cpp
)

set(GWENI_SKIN_SIMPLE_SOURCES
    #simple skin
    include/gweni/skins/simple/simple.h
)
set(GWENI_SKIN_TEXTURED_SOURCES
    #textured skin
    include/gweni/skins/textured/texturedControls.h
    source/skins/textured/texturedControls.cpp
    include/gweni/skins/textured/texturedPrimitives.h
    source/skins/textured/texturedPrimitives.cpp
    include/gweni/skins/textured/texturedSkin.h
    source/skins/textured/texturedSkin.cpp
)

set(GWENI_PLATFORM_SOURCES
#    source/platforms/allegro5Platform.cpp
    include/gweni/platforms/baseRender.h
    source/platforms/baseRender.cpp
    include/gweni/platforms/commonPlatform.h
    source/platforms/commonPlatform.cpp
    source/platforms/crossPlatform.cpp
    include/gweni/platforms/inputEvent.h
    include/gweni/platforms/platform.h
    include/gweni/platforms/platformTypes.h
    include/gweni/platforms/windowProvider.h
)

set(GWENI_PROPERTY_SOURCES
    # property
    include/gweni/controls/property/baseProperty.h
    include/gweni/controls/property/checkBox.h
    include/gweni/controls/property/colorSelector.h
    include/gweni/controls/property/comboBox.h
    include/gweni/controls/property/file.h
    include/gweni/controls/property/folder.h
    include/gweni/controls/property/text.h
)

set(GWENI_RENDER_SOURCES "")
# imglib with every renderer or none: the textured skin loads its atlas with it
set(GWENI_ADDITIONAL_LIBRARIES imglib)
set(GWENI_COMPILE_DEFINITIONS "")

# RapidJSON is header-only; the vcpkg config exposes RAPIDJSON_INCLUDE_DIRS
# rather than an imported target.
find_package(RapidJSON CONFIG REQUIRED)
include_directories(${RAPIDJSON_INCLUDE_DIRS})

if(GWENI_RENDER_OPENGL_CORE)
    find_package(glm CONFIG REQUIRED)

    find_package(freetype CONFIG REQUIRED)

    # The built-in fallback font OpenGLCore::loadFont draws a missing or
    # unreadable face in: resources/OpenSans.ttf (Apache-2.0, see
    # resources/OpenSans-LICENSE.txt) as a byte array, generated into the build
    # tree whenever the font or the script changes.
    set(GWENI_FALLBACK_FONT ${GWENI_SOURCE_DIR}/resources/OpenSans.ttf)
    set(GWENI_FALLBACK_FONT_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/generated/fallbackFont.cpp)
    add_custom_command(
        OUTPUT ${GWENI_FALLBACK_FONT_SOURCE}
        COMMAND ${CMAKE_COMMAND} -DINPUT=${GWENI_FALLBACK_FONT}
            -DOUTPUT=${GWENI_FALLBACK_FONT_SOURCE} -DSYMBOL=fallbackFontData
            -P ${GWENI_SOURCE_DIR}/cmake/embedFile.cmake
        DEPENDS ${GWENI_FALLBACK_FONT} ${GWENI_SOURCE_DIR}/cmake/embedFile.cmake
        COMMENT "Embedding the fallback font ${GWENI_FALLBACK_FONT}"
        VERBATIM)

    # The programs' shader bodies, shared by OpenGLCore (embedded here as text,
    # behind GL's prologue) and the Vulkan backend (compiled to SPIR-V from
    # wrappers that #include them, source/platforms/renderers/Vulkan/)
    set(GWENI_SHADER_DIR ${GWENI_SOURCE_DIR}/source/platforms/renderers/shaders)
    set(GWENI_SHADER_SOURCES "")
    foreach(shader primitive.vert:primitiveVertexGlsl primitive.frag:primitiveFragmentGlsl
            text.vert:textVertexGlsl text.frag:textFragmentGlsl)
        string(REPLACE ":" ";" shader "${shader}")
        list(GET shader 0 shaderName)
        list(GET shader 1 shaderSymbol)
        set(shaderOutput ${CMAKE_CURRENT_BINARY_DIR}/generated/${shaderSymbol}.cpp)
        add_custom_command(
            OUTPUT ${shaderOutput}
            COMMAND ${CMAKE_COMMAND} -DINPUT=${GWENI_SHADER_DIR}/${shaderName}.glsl
                -DOUTPUT=${shaderOutput} -DSYMBOL=${shaderSymbol}
                -P ${GWENI_SOURCE_DIR}/cmake/embedFile.cmake
            DEPENDS ${GWENI_SHADER_DIR}/${shaderName}.glsl ${GWENI_SOURCE_DIR}/cmake/embedFile.cmake
            COMMENT "Embedding the shader body ${shaderName}.glsl"
            VERBATIM)
        list(APPEND GWENI_SHADER_SOURCES ${shaderOutput})
    endforeach()

    # SlotRenderer, the API-independent part of a retained renderer (the mirrors,
    # the vertex packing, the text runs and fonts), and OpenGLCore, its GL backend
    list(APPEND GWENI_RENDER_SOURCES
        include/gweni/platforms/renderers/slotRenderer.h
        source/platforms/renderers/SlotRenderer/slotRenderer.cpp
        include/gweni/platforms/renderers/glApi.h
        include/gweni/platforms/renderers/openglCore.h
        source/platforms/renderers/OpenGLCore/openglCore.cpp
        ${GWENI_FALLBACK_FONT_SOURCE}
        ${GWENI_SHADER_SOURCES}
    )
    set(GWENI_ADDITIONAL_LIBRARIES ${GWENI_ADDITIONAL_LIBRARIES}
        glm::glm
        freetype
        freetype-gl
    )

    set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS}
        GLM_HAS_TEMPLATE_ALIASES
    )

    if(GWENI_GL_ES)
        # glApi.h's switch to the GLES headers, for the library and everything
        # linking it; EGL resolves the GLES extensions' entry points
        # (glMultiDrawElementsEXT), which libGLESv2 does not export
        find_package(OpenGL REQUIRED COMPONENTS GLES3 EGL)
        set(GWENI_ADDITIONAL_LIBRARIES ${GWENI_ADDITIONAL_LIBRARIES}
            OpenGL::GLES3
            OpenGL::EGL
        )
        # GLFW_INCLUDE_NONE: <GLFW/glfw3.h> would include the desktop <GL/gl.h>,
        # whose prototypes would let a call newer than GLES 3.0 compile
        set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS} GWENI_GL_ES GLFW_INCLUDE_NONE)
    elseif(EMSCRIPTEN)
        # the web tree: Emscripten's <GLES3/gl3.h> (glApi.h defines GWENI_GL_ES
        # under __EMSCRIPTEN__ too) and its WebGL 2 library, which everything
        # linking OpenGLCore must link at version 2: without it a GLES 3.0 entry
        # point such as glTexSubImage3D's family has no implementation
        set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS} GWENI_GL_ES GLFW_INCLUDE_NONE)
        set(GWENI_WEB_LINK_OPTIONS -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2)
    endif()
endif()

if(GWENI_RENDER_VULKAN)
    # gweni_vulkan, gweni_add_spirv and gweni_vulkan_test_environment
    include(${GWENI_SOURCE_DIR}/cmake/Vulkan.cmake)

    # so code (the library, the examples, the tests) can #if on it
    set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS} GWENI_HAS_VULKAN)

    # renderer::Vulkan, the Vulkan backend of SlotRenderer, which (with
    # freetype-gl and the shader bodies) is built with the GL renderer for now
    if(NOT GWENI_RENDER_OPENGL_CORE)
        message(FATAL_ERROR "GWENI_RENDER_VULKAN needs GWENI_RENDER_OPENGL_CORE: "
            "renderer::Vulkan derives from SlotRenderer, built with it")
    endif()
    list(APPEND GWENI_RENDER_SOURCES
        include/gweni/platforms/renderers/vulkan.h
        source/platforms/renderers/Vulkan/vulkan.cpp
    )
    set(GWENI_ADDITIONAL_LIBRARIES ${GWENI_ADDITIONAL_LIBRARIES} gweni_vulkan)
    set(GWENI_VULKAN_SHADERS
        source/platforms/renderers/Vulkan/primitive.vert
        source/platforms/renderers/Vulkan/primitive.frag
        source/platforms/renderers/Vulkan/text.vert
        source/platforms/renderers/Vulkan/text.frag
        source/platforms/renderers/Vulkan/textSingle.frag
    )
endif()

set(GWENI_STD_FILESYSTEM 1)
set(GWENI_EXPERIMENTAL_FILESYSTEM 2)
set(GWENI_BOOST 3)

set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS} "GWENI_STD_FILESYSTEM=${GWENI_STD_FILESYSTEM}")
set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS} "GWENI_EXPERIMENTAL_FILESYSTEM=${GWENI_EXPERIMENTAL_FILESYSTEM}")
set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS} "GWENI_BOOST=${GWENI_BOOST}")

if(GWENI_DEBUG_LOGGING)
    set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS} "GWENI_DEBUG_LOGGING")
endif()

set(GWENI_USE_FILESYSTEM GWENI_STD_FILESYSTEM)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    #filesystem seems to be missing from NDK
    set(GWENI_USE_FILESYSTEM GWENI_BOOST)

    find_package(Boost CONFIG REQUIRED system filesystem)

    set(GWENI_ADDITIONAL_LIBRARIES ${GWENI_ADDITIONAL_LIBRARIES}
        "Boost::system"
        "Boost::filesystem"
    )
else()
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0)
            set(GWENI_USE_FILESYSTEM ${GWENI_STD_FILESYSTEM})
        endif()
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(GWENI_USE_FILESYSTEM ${GWENI_EXPERIMENTAL_FILESYSTEM})
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
            set(GWENI_USE_FILESYSTEM ${GWENI_STD_FILESYSTEM})
            # Append, don't overwrite: this list already holds glm/freetype/
            # freetype-gl/imglib and their usage requirements (include dirs).
            set(GWENI_ADDITIONAL_LIBRARIES ${GWENI_ADDITIONAL_LIBRARIES} "stdc++fs")
        endif()
    endif()
endif()

set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS} "GWENI_USE_FILESYSTEM=${GWENI_USE_FILESYSTEM}")


#set(GWENI_RENDER_SOURCES
#    include/gweni/Renderers/${GWENI_RENDERER_NAME}.h
#    renderers/${GWENI_RENDERER_NAME}/${GWENI_RENDERER_NAME}.cpp
#    source/Platforms/${GWENI_PLATFORM_NAME}.cpp
#)

# header folder groups
source_group("" FILES ${GWENI_HEADERS} ${GWENI_SOURCES})
source_group("controls" FILES ${GWENI_CONTROL_SOURCES})
source_group("dialogs" FILES ${GWENI_DIALOG_SOURCES})
source_group("layouts" FILES ${GWENI_LAYOUT_SOURCES})
source_group("skins\\simple" FILES ${GWENI_SKIN_SIMPLE_SOURCES})
source_group("skins\\textured" FILES ${GWENI_SKIN_TEXTURED_SOURCES})
source_group("platforms" FILES ${GWENI_PLATFORM_SOURCES})
source_group("platforms\\renderers" FILES ${GWENI_RENDER_SOURCES})
source_group("properties" FILES ${GWENI_PROPERTY_SOURCES})

# gweni library
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

add_library(gweni STATIC
    ${GWENI_SOURCES}
    ${GWENI_CONTROL_SOURCES}
    ${GWENI_LAYOUT_SOURCES}
    ${GWENI_DIALOG_SOURCES}
    ${GWENI_PROPERTY_SOURCES}
    ${GWENI_SKIN_SIMPLE_SOURCES}
    ${GWENI_SKIN_TEXTURED_SOURCES}
    ${GWENI_PLATFORM_SOURCES}
    ${GWENI_RENDER_SOURCES}
)
target_link_options(gweni PUBLIC "/WHOLEARCHIVE:$<TARGET_FILE:gweni>")

else()

add_library(gweni OBJECT
    ${GWENI_SOURCES}
    ${GWENI_CONTROL_SOURCES}
    ${GWENI_LAYOUT_SOURCES}
    ${GWENI_DIALOG_SOURCES}
    ${GWENI_PROPERTY_SOURCES}
    ${GWENI_SKIN_SIMPLE_SOURCES}
    ${GWENI_SKIN_TEXTURED_SOURCES}
    ${GWENI_PLATFORM_SOURCES}
    ${GWENI_RENDER_SOURCES}
)

endif()

gweni_set_warnings(gweni)

target_include_directories(gweni PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_libraries(gweni
    ${GWENI_ADDITIONAL_LIBRARIES}
#    gweni${GWENI_RENDER_NAME}
)
#if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
#    target_link_options(gweni PUBLIC "/WHOLEARCHIVE:$<TARGET_FILE:gweni>")
#else()
#    target_link_options(gweni PUBLIC -W1,--whole-archive $<TARGET_FILE:gweni> -W1,--no-whole-archive)
#endif()

target_compile_definitions(gweni PUBLIC ${GWENI_COMPILE_DEFINITIONS})
if(GWENI_VULKAN_SHADERS)
    # renderer::Vulkan's SPIR-V, each wrapper #including its body from shaders/
    gweni_add_spirv(gweni ${GWENI_VULKAN_SHADERS})
endif()
if(GWENI_WEB_LINK_OPTIONS)
    target_link_options(gweni INTERFACE ${GWENI_WEB_LINK_OPTIONS})
endif()

if(GWENI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif(GWENI_BUILD_EXAMPLES)

# after examples: the process-level tests drive the ControlTest target
if(GWENI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif(GWENI_BUILD_TESTS)

# gweni_bench (bench/): needs gweni_capture from the examples, and registers its
# smoke test with the tests. Not in the web tree: the browser runs captures, not
# timed scenes.
if(GWENI_BUILD_TESTS AND GWENI_BUILD_EXAMPLES AND NOT EMSCRIPTEN)
    add_subdirectory(bench)
endif()

#target_include_directories(gweni PUBLIC
#        ${GWENI_SOURCE_DIR}/source/platform/include
#        ${GWENI_SOURCE_DIR}/source/gweni/include
#        ${GWENI_RENDER_INCLUDES}
#)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/gweni
        DESTINATION include
)

install(TARGETS gweni
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)
