
#### gweni Project Configuration

# Version
set(GWENI_VERSION_MAJOR 0)
set(GWENI_VERSION_MINOR 1)
set(GWENI_VERSION_PATCH 0)
set(GWENI_BRANCH "Dev")       # Dev or Release
set(GWENI_VERSION_STR "${GWENI_VERSION_MAJOR}.${GWENI_VERSION_MINOR}.${GWENI_VERSION_PATCH} ${GWENI_BRANCH}")
message("Project version: ${GWENI_VERSION_STR}")

#-----------------------------------------------------------
# User options

# Cross-platform
#option(GWENI_RENDER_ALLEGRO5      "Renderer: Allegro5"            OFF)
#option(GWENI_RENDER_IRRLICHT      "Renderer: Irrlicht"            OFF)
option(GWENI_RENDER_OPENGL        "Renderer: OPENGL"              OFF)
option(GWENI_RENDER_OPENGL_CORE   "Renderer: OPENGL Core Profile" OFF)
option(GWENI_RENDER_SDL2          "Renderer: SDL2"                OFF)
#option(GWENI_RENDER_SFML2         "Renderer: SFML2"               OFF)
#option(GWENI_RENDER_SW            "Renderer: Software"            OFF)
#option(GWENI_RENDER_NULL          "Renderer: Null"                OFF) # Used for testing

option(GWENI_USE_GLFW             "Use GLFW for OpenGL renderer." ON)

#option(GWENI_WITH_TESTS           "Include unittests" ON)
option(GWENI_WITH_SAMPLE          "Include sample" ON)

#option(GWENI_WITH_REFLECTION      "Use reflection (requires external dependencies)" OFF)

# This is for development but can be used by the user.
option(GWENI_WITH_ALLOC_STATS     "Track memory allocations" OFF)

# Find SFML by SFMLConfig.cmake instead of local CMake module FindSFML.cmake.
option(GWENI_USE_SFML_CONFIG "Use SFMLConfig.cmake to find SFML (>=2.5)" OFF)

#-----------------------------------------------------------
# Configure once options known

option(GWENI_WANT_SHARED_LIBS OFF) # TODO: Shared libs not implemented yet.

#option(GWENI_WITH_REFLECTION_LOCAL "Use local libaries" ON) # TODO: OFF not tested.
#set(GWENI_PONDER_INCLUDE "${GWENI_SOURCE_DIR}/deps/ponder/include"
#    CACHE STRING "Ponder includes directory")
#set(GWENI_LUA_INCLUDE "${GWENI_SOURCE_DIR}/deps/lua-5.3/src"
#    CACHE STRING "Lua include directory")

set(GWENI_TARGET_ARCH "Unknown")      # default architecture e.g. x86, x64

# Set the default build type to release with debug info
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo
        CACHE STRING "Type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
    )
endif()

# define install directory for miscelleneous files
if(WIN32 AND NOT UNIX)
    set(INSTALL_MISC_DIR .)

    # Determine the target architecture, which is useful for linking.
    if (CMAKE_GENERATOR MATCHES "Win64")
        set(GWENI_TARGET_ARCH "x64")
    else()
        set(GWENI_TARGET_ARCH "x86")
    endif()
    message(STATUS "Generator: ${CMAKE_GENERATOR}. Architecture: ${GWENI_TARGET_ARCH}")
elseif(UNIX)
    set(INSTALL_MISC_DIR share/gweni)
endif()

if(WITH_TESTS)
    message("Including tests")
endif(WITH_TESTS)

if(WITH_SAMPLE)
    message(STATUS "Including gweni sample")

    if (NOT USE_GLFW AND (RENDER_OPENGL OR RENDER_OPENGL_CORE))
        message(FATAL_ERROR "Samples with OpenGL or OpenGLCore require GLFW")
    endif()
endif(WITH_SAMPLE)

if(WITH_REFLECTION)
    message("Using reflection")
endif(WITH_REFLECTION)

#-----------------------------------------------------------
# Renderer config
#   - Order alphabetic.
if(GWENI_RENDER_NULL)
    set(GWENI_RENDER_NAME "Null")
    set(GWENI_INPUT_NAME "Null")
    set(GWENI_PLATFORM_NAME "Null")
    set(GWENI_RENDER_INCLUDES "")
    set(GWENI_RENDER_LIBRARIES "")
endif()

if(GWENI_RENDER_OPENGL)
    set(GWENI_RENDER_NAME "OpenGL")
    set(GWENI_INPUT_NAME "GLFW3")
    set(GWENI_PLATFORM_NAME "Cross")

    if (GWENI_USE_GLFW)
        message(STATUS "Configuring GLFW3...")
        hunter_add_package(glfw)
        find_package(glfw3 REQUIRED)

        set(GWENI_RENDER_LIBRARIES glfw)
    endif()
endif()

if(GWENI_RENDER_OPENGL_CORE)
    set(GWENI_RENDER_NAME "OpenGLCore")
    set(GWENI_INPUT_NAME "GLFW3")
    set(GWENI_PLATFORM_NAME "Cross")

    hunter_add_package(glm)
    find_package(glm CONFIG REQUIRED)

    hunter_add_package(glew)
    find_package(glew CONFIG REQUIRED)

    set(GWENI_RENDER_LIBRARIES glm glew::glew)
    
    if(GWENI_USE_GLFW)
        hunter_add_package(glfw)
        find_package(glfw3 REQUIRED)

        set(GWENI_RENDER_LIBRARIES ${GWENI_RENDER_LIBRARIES} glfw)
    endif()
endif()

if(GWENI_RENDER_SDL2)
    set(GWENI_RENDER_NAME "SDL2")
    set(GWENI_INPUT_NAME "SDL2")
    set(GWENI_PLATFORM_NAME "Cross")

    hunter_add_package(SDL2)
    find_package(SDL2 CONFIG REQUIRED)
    hunter_add_package(SDL_ttf)
    find_package(SDL_ttf CONFIG REQUIRED)
    hunter_add_package(SDL_image)
    find_package(SDL_image CONFIG REQUIRED)

    set(GWENI_RENDER_LIBRARIES SDL2::SDL2 SDL_ttf::SDL_ttf SDL_image::SDL_image)
endif()

if(GWENI_RENDER_SW)
    set(GWENI_SW_VIEWER 1)
    set(GWENI_RENDER_NAME "Software")
    set(GWENI_PLATFORM_NAME "Cross")
    if(GWENI_SW_VIEWER)
        set(GWENI_INPUT_NAME "SDL2")
        hunter_add_package(SDL2)
        find_package(SDL2 CONFIG REQUIRED)
        set(GWENI_RENDER_LIBRARIES SDL2::SDL2)
    else()
        set(GWENI_INPUT_NAME "Null")
        set(GWENI_RENDER_INCLUDES "")
        set(GWENI_RENDER_LIBRARIES "")
    endif()
endif()

#-----------------------------------------------------------
# Sanity checks

if(GWENI_WANT_SHARED_LIBS)
    message(FATAL_ERROR "Shared libraries not currently tested/supported")
endif()

if(NOT GWENI_RENDER_NAME)
    message(FATAL_ERROR "No renderer was specified. See RENDER_<name> options.")
endif(NOT GWENI_RENDER_NAME)

if(NOT GWENI_INPUT_NAME)
    message(FATAL_ERROR "No GWENI_INPUT_NAME specified")
endif()

if(NOT GWENI_PLATFORM_NAME)
    message(FATAL_ERROR "No GWENI_PLATFORM_NAME specified")
endif()

#-----------------------------------------------------------

# MinGW problems
if (WIN32)
    set(GWENI_RENDER_LIBRARIES ${GWENI_RENDER_LIBRARIES} -liconv)
endif()

set(GWENI_LIB_DEFINES "-DGWENI_PLATFORM_${GWENI_PLATFORM_NAME}=1 -DGWENI_RENDER_${GWENI_RENDER_NAME}=1")

if(GWENI_RENDER_INCLUDES)
    list(REMOVE_DUPLICATES GWENI_RENDER_INCLUDES)
endif()
if(GWENI_RENDER_LIBRARIES)
    list(REMOVE_DUPLICATES GWENI_RENDER_LIBRARIES)
endif()

# Summary
message(STATUS "Using renderer '${GWENI_RENDER_NAME}' with input '${GWENI_INPUT_NAME}' on platform '${GWENI_PLATFORM_NAME}'")
message(STATUS "${GWENI_RENDER_NAME} includes: ${GWENI_RENDER_INCLUDES}")
message(STATUS "${GWENI_RENDER_NAME} libs: ${GWENI_RENDER_LIBRARIES}")
