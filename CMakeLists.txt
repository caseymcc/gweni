# CMake build instructions for gweni

# configure cmake
cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 17)

if(WIN32)
    add_definitions(-DNOMINMAX)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmake/Hunter/HunterGate.cmake)
HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.23.254.tar.gz"
    SHA1 "017e688b9848c1f695303e8cfe9f35d40e154112"
)
cmake_policy(SET CMP0074 NEW)

option(GWENI_RENDER_SDL2 "Opengl Renderer" ON)
option(GWENI_RENDER_OPENGL "Opengl Renderer" ON)
option(GWENI_RENDER_OPENGL_CORE "Opengl 3.0+ Renderer" ON)
option(GWENI_BUILD_EXAMPLES "Build Examples" ON)

project(gweni)

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

# write the version info
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gweni_config.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/source/platform/include/gwen/Version.h
    @ONLY)

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
    include/gweni/toolTip.h
    source/tooltip.cpp
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
    include/gweni/controls/colorControls.h
    source/controls/colorControls.cpp
    include/gweni/controls/colorPicker.h
    source/controls/colorPicker.cpp
    include/gweni/controls/comboBox.h
    source/controls/comboBox.cpp
    include/gweni/controls/crossSplitter.h
    source/controls/crossSplitter.cpp
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
    include/gweni/controls/labelClickable.h
    source/controls/labelClickable.cpp
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
    include/gweni/controls/layout/tile.h
)

set(GWENI_DIALOG_SOURCES
    # dialog
    include/gweni/controls/dialogs/fileOpen.h
    source/controls/Dialog/fileOpen.cpp
    include/gweni/controls/dialogs/fileSave.h
    source/controls/Dialog/fileSave.cpp
    include/gweni/controls/dialogs/folderOpen.h
    source/controls/Dialog/folderOpen.cpp
    include/gweni/controls/dialogs/query.h
    source/controls/Dialog/query.cpp
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
    source/platforms/platformTypes.cpp
    include/gweni/platforms/windowProvider.h
    #source/platforms/nullPlatform.cpp
    #source/platforms/windowsPlatform.cpp
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

set(GWENI_RENDER_SOURCES
)

if(GWENI_RENDER_SDL2)
    set(GWENI_RENDER_SOURCES
        include/gweni/platforms/renderers/sdl2.h
        source/platforms/renderers/OpenGL/sdl2.cpp
    )
endif()

if(GWENI_RENDER_OPENGL)
    set(GWENI_RENDER_SOURCES
        include/gweni/platforms/renderers/opengl.h
        source/platforms/renderers/OpenGL/opengl.cpp
    )
endif()

set(GWENI_ADDITIONAL_LIBRARIES "")
set(GWENI_COMPILE_DEFINITIONS "")

hunter_add_package(RapidJSON)
find_package(RapidJSON CONFIG REQUIRED)
set(GWENI_ADDITIONAL_LIBRARIES ${GWENI_ADDITIONAL_LIBRARIES} RapidJSON::rapidjson)

if(GWENI_RENDER_OPENGL_CORE)
    hunter_add_package(glm)
    find_package(glm CONFIG REQUIRED)

    hunter_add_package(freetype)
    find_package(freetype CONFIG REQUIRED)

    set(GWENI_RENDER_SOURCES
        include/gweni/platforms/renderers/openglCore.h
        source/platforms/renderers/OpenGLCore/openglCore.cpp
    )
    set(GWENI_ADDITIONAL_LIBRARIES ${GWENI_ADDITIONAL_LIBRARIES}
        glm
        freetype::freetype
        freetype-gl
        imglib
    )

    set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS}
        GLM_HAS_TEMPLATE_ALIASES
    )
endif()

set(GWENI_STD_FILESYSTEM 1)
set(GWENI_EXPERIMENTAL_FILESYSTEM 2)
set(GWENI_BOOST 3)

set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS} "GWENI_STD_FILESYSTEM=${GWENI_STD_FILESYSTEM}")
set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS} "GWENI_EXPERIMENTAL_FILESYSTEM=${GWENI_EXPERIMENTAL_FILESYSTEM}")
set(GWENI_COMPILE_DEFINITIONS ${GWENI_COMPILE_DEFINITIONS} "GWENI_BOOST=${GWENI_BOOST}")

set(GWENI_USE_FILESYSTEM GWENI_STD_FILESYSTEM)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    #filesystem seems to be missing from NDK
    set(GWENI_USE_FILESYSTEM GWENI_BOOST)

    hunter_add_package(Boost COMPONENTS system filesystem)
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
            set(GWENI_ADDITIONAL_LIBRARIES "stdc++fs")
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

target_include_directories(gweni PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_libraries(gweni
    ${GWENI_ADDITIONAL_LIBRARIES}
#    gweni${GWENI_RENDER_NAME}
)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_link_options(gweni PUBLIC "/WHOLEARCHIVE:$<TARGET_FILE:gweni>")
else()
    target_link_options(gweni PUBLIC --whole-archive)
endif()

target_compile_definitions(gweni PUBLIC ${GWENI_COMPILE_DEFINITIONS})

if(GWENI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif(GWENI_BUILD_EXAMPLES)

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
