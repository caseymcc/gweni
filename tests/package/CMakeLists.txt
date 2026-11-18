#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The consumer of an installed gweni (tests/packageConsumer.cmake configures
#  and builds it against a scratch prefix, with the tree's vcpkg toolchain): a
#  project of its own, which takes gweni only as an app would, through
#  find_package(gweni CONFIG REQUIRED), gweni::gweni and gweni_add_resources.
#
#  Configuring fails, naming it, if gweni::gweni does not carry a requirement it
#  must export: the compile definitions (the filesystem switch, GWENI_HAS_VULKAN
#  in a package with Vulkan, GWENI_GL_ES in a GLES one) and the include dirs
#  (include/ and include/gweni/third_party/, which must exist). Every installed
#  header under include/gweni/ (the vendored third_party ones included) is
#  compiled as the only include of a translation unit, against gweni::gweni
#  alone (package_header_check), so a header the install left out, or one
#  needing an include dir the package does not export, fails the build. The
#  GLFW input headers are compiled only where vcpkg's glfw3 is found: gweni does
#  not link GLFW, an app using them adds it.
#
#  main.cpp is package_consumer, run by the test.
#
cmake_minimum_required(VERSION 3.16)
project(gweni_package_consumer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(gweni CONFIG REQUIRED)

# What gweni::gweni must carry
function(package_fail message)
    message(FATAL_ERROR "package_consumer: gweni::gweni ${message}")
endfunction()

get_target_property(gweni_definitions gweni::gweni INTERFACE_COMPILE_DEFINITIONS)
get_target_property(gweni_includes gweni::gweni INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "package_consumer: gweni ${gweni_VERSION} from ${gweni_DIR}, "
    "OpenGLCore ${gweni_RENDER_OPENGL_CORE}, Vulkan ${gweni_RENDER_VULKAN}, GLES ${gweni_GL_ES}")

foreach(definition GWENI_STD_FILESYSTEM=1 GWENI_EXPERIMENTAL_FILESYSTEM=2 GWENI_BOOST=3)
    if(NOT definition IN_LIST gweni_definitions)
        package_fail("does not define ${definition}")
    endif()
endforeach()
if(NOT gweni_definitions MATCHES "GWENI_USE_FILESYSTEM=[0-9]")
    package_fail("does not define GWENI_USE_FILESYSTEM")
endif()
if(gweni_RENDER_VULKAN AND NOT "GWENI_HAS_VULKAN" IN_LIST gweni_definitions)
    package_fail("was built with Vulkan but does not define GWENI_HAS_VULKAN")
endif()
if(gweni_GL_ES AND NOT EMSCRIPTEN AND NOT "GWENI_GL_ES" IN_LIST gweni_definitions)
    package_fail("was built for GLES but does not define GWENI_GL_ES")
endif()

if(NOT gweni_includes)
    package_fail("has no include dirs")
endif()
set(third_party_found FALSE)
foreach(dir IN LISTS gweni_includes)
    if(NOT IS_DIRECTORY "${dir}")
        package_fail("names an include dir that does not exist: ${dir}")
    endif()
    if(dir MATCHES "/include/gweni/third_party$")
        set(third_party_found TRUE)
    endif()
endforeach()
if(NOT third_party_found)
    package_fail("does not export include/gweni/third_party (the vendored freetype-gl and imglib headers)")
endif()

# Every installed header alone, against gweni::gweni
get_filename_component(gweni_include_dir "${gweni_DIR}/../../include" ABSOLUTE)
file(GLOB_RECURSE installed_headers RELATIVE "${gweni_include_dir}" "${gweni_include_dir}/gweni/*.h")
if(NOT installed_headers)
    package_fail("installs no headers under ${gweni_include_dir}/gweni")
endif()
find_package(glfw3 CONFIG QUIET)
set(header_sources)
foreach(header IN LISTS installed_headers)
    if(header MATCHES "^gweni/platforms/input/glfw" AND NOT TARGET glfw)
        continue()
    endif()
    set(source "${CMAKE_CURRENT_BINARY_DIR}/header_check/${header}.cpp")
    file(WRITE "${source}" "#include <${header}>\n")
    list(APPEND header_sources "${source}")
endforeach()
add_library(package_header_check OBJECT ${header_sources})
target_link_libraries(package_header_check PRIVATE gweni::gweni)
if(TARGET glfw)
    target_include_directories(package_header_check PRIVATE $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

# The app
add_executable(package_consumer main.cpp)
target_link_libraries(package_consumer PRIVATE gweni::gweni)
if(gweni_RENDER_OPENGL_CORE AND NOT EMSCRIPTEN)
    # its own context, as any GL app has: surfaceless EGL
    find_package(OpenGL REQUIRED COMPONENTS EGL)
    target_link_libraries(package_consumer PRIVATE OpenGL::EGL)
    target_compile_definitions(package_consumer PRIVATE PACKAGE_HAS_GL)
endif()
gweni_add_resources(package_consumer)
add_dependencies(package_consumer package_header_check)
