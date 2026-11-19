#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  gweni_app (gweni::app): the app shell, a static library over gweni that
#  opens a window or renders headless (include/gweni/app/, source/app/): the GL
#  context (a GLFW window, surfaceless EGL, or WebGL 2 in the browser), the
#  offscreen framebuffer and PNG readback, the Vulkan device and window with
#  GWENI_RENDER_VULKAN, the window loop and the scale a window draws at
#  (device.h); the parts an app draws with (shell.h); app::Window, those
#  parts in a window with every GLFW callback wired (window.h); app::Headless,
#  those parts offscreen on a stepped clock, driven by script steps
#  (headless.h); the key names scripts take (keys.h); and a frame's counters
#  as ControlTest --stats prints them (frameStats.h).
#
#  Built with the GL renderer (the desktop, GLES and web trees; not the
#  Vulkan-only one) unless GWENI_BUILD_APP is off, so an app taking gweni with
#  add_subdirectory that wants no GLFW can leave it out. The core gweni target
#  links no GLFW either way. Included from the top-level CMakeLists.txt after
#  gweni is defined; sets GWENI_APP_HEADERS (relative to include/).
#

option(GWENI_BUILD_APP "Build gweni_app, the window and headless app shell (GLFW, EGL)" ON)

file(GLOB GWENI_APP_HEADERS CONFIGURE_DEPENDS
    RELATIVE ${GWENI_SOURCE_DIR}/include
    ${GWENI_SOURCE_DIR}/include/gweni/app/*.h)

if(NOT GWENI_BUILD_APP OR NOT GWENI_RENDER_OPENGL_CORE)
    return()
endif()

if(EMSCRIPTEN)
    # The web tree: GLFW is Emscripten's contrib.glfw3 port (emscripten-glfw),
    # compiled and linked with --use-port (vcpkg's glfw3 is not built for
    # wasm32, vcpkg.json), behind an INTERFACE target named as vcpkg's, so
    # gweni_app and the apps link "glfw" as natively.
    if(NOT TARGET glfw)
        add_library(glfw INTERFACE)
        target_compile_options(glfw INTERFACE --use-port=contrib.glfw3)
        target_link_options(glfw INTERFACE --use-port=contrib.glfw3)
    endif()
else()
    find_package(glfw3 CONFIG REQUIRED)
endif()

add_library(gweni_app STATIC
    include/gweni/app/device.h
    source/app/device.cpp
    include/gweni/app/shell.h
    source/app/shell.cpp
    include/gweni/app/window.h
    source/app/window.cpp
    include/gweni/app/headless.h
    source/app/headless.cpp
    include/gweni/app/keys.h
    source/app/keys.cpp
    include/gweni/app/frameStats.h
    source/app/frameStats.cpp
    source/app/instance.h
    # the Vulkan window's swapchain; empty without GWENI_RENDER_VULKAN
    source/app/vulkanWindow.h
    source/app/vulkanWindow.cpp
)
add_library(gweni::app ALIAS gweni_app)
gweni_set_warnings(gweni_app)
target_link_libraries(gweni_app PUBLIC
    gweni
    glfw
)

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    # Surfaceless EGL is the headless path, the one taken when there is no
    # display at all (the GLES tree's gweni links it already)
    find_package(OpenGL REQUIRED COMPONENTS EGL)
    target_link_libraries(gweni_app PUBLIC OpenGL::EGL)
endif()
