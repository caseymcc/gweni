# Gweni examples
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(example_name "OpenGLCore")

if(EMSCRIPTEN)
    # The web tree: GLFW is Emscripten's contrib.glfw3 port (emscripten-glfw),
    # compiled and linked with --use-port (vcpkg's glfw3 is not built for wasm32,
    # vcpkg.json), behind an INTERFACE target named as vcpkg's, so the targets
    # below link "glfw" as natively.
    add_library(glfw INTERFACE)
    target_compile_options(glfw INTERFACE --use-port=contrib.glfw3)
    target_link_options(glfw INTERFACE --use-port=contrib.glfw3)
else()
    find_package(glfw3 CONFIG REQUIRED)
endif()

set(${example_name}_sources
    openglcore.cpp)

if(APPLE AND APP_BUNDLE)
    set(${example_name}_build_type MACOSX_BUNDLE)
else()
    set(${example_name}_build_type )
endif()

# Offscreen capture plumbing (GL context with or without a display, offscreen
# target, PNG readback), shared by both examples and the unit tests (tests/) so
# capture.cpp is compiled once. EGL stays here, out of the gweni library.
add_library(gweni_capture STATIC capture.cpp)
gweni_set_warnings(gweni_capture)
target_include_directories(gweni_capture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gweni_capture PUBLIC
    gweni
    glfw
)

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    # Surfaceless EGL is the offscreen path used when there is no display at all.
    find_package(OpenGL REQUIRED COMPONENTS EGL)
    target_link_libraries(gweni_capture PUBLIC OpenGL::EGL)
endif()

add_executable(${example_name} ${${example_name}_build_type} ${${example_name}_sources})

gweni_set_warnings(${example_name})

target_link_libraries(${example_name}
    gweni
    gweni_demo
    gweni_capture
    glfw
)

# Single-control test harness: drops one control on the canvas by name.
add_executable(ControlTest controltest.cpp)
gweni_set_warnings(ControlTest)
target_link_libraries(ControlTest
    gweni
    gweni_demo
    gweni_capture
    glfw
)

# The web tree: each executable is a page (<exe>.html, .js, .wasm and the
# preloaded .data) that the browser runner (tests/web/run.mjs) or a person
# loads; gweni_web_page() (web/page.cmake) gives it the shell, the URL query as
# argv and the run's hand-off to the runner (web/pre.js), resources/ preloaded at
# /resources (platform::getExecutableDir() is "/" there), WebGL 2 and a heap
# that grows.
if(EMSCRIPTEN)
    include(${CMAKE_CURRENT_SOURCE_DIR}/web/page.cmake)
    gweni_web_page(${example_name})
    gweni_web_page(ControlTest)
endif()

# Both examples default to RelativeToExecutablePaths("resources"), so the
# resources/ they load sits next to them and they run from any working
# directory. One copy serves both, and each depends on it, so building either
# alone makes it. A custom target runs on every build, unlike a POST_BUILD
# step, so an edit under resources/ is copied even when nothing relinks. The
# copy is a mirror (cmake/mirrorDirectory.cmake): a file deleted or renamed in
# resources/ is removed from it too, so an old tree has what a fresh one has,
# and an unchanged file is not rewritten. It names the exes' default output
# directory: $<TARGET_FILE_DIR> here would make the copy depend on the exes
# that depend on it.
get_property(example_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(example_multi_config)
    set(example_resource_dir ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/resources)
else()
    set(example_resource_dir ${CMAKE_CURRENT_BINARY_DIR}/resources)
endif()

add_custom_target(gweni_example_resources
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${GWENI_SOURCE_DIR}/resources -DDST=${example_resource_dir}
        -P ${GWENI_SOURCE_DIR}/cmake/mirrorDirectory.cmake
    COMMENT "Mirroring resources/ next to the examples"
    VERBATIM)
add_dependencies(${example_name} gweni_example_resources)
add_dependencies(ControlTest gweni_example_resources)

if(APPLE AND APP_BUNDLE)
    # make MacOS bundle
    set_target_properties(${example_name} PROPERTIES
        MACOSX_BUNDLE_COPYRIGHT "${APP_COPYRIGHT}"
        MACOSX_BUNDLE_INFO_STRING "${APP_VERSION}, ${APP_COPYRIGHT}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_NAME} example v${APP_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.gweni.${PROJECT_NAME}.${example_name}"
        )
endif()

