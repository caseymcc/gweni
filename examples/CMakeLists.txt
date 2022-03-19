## Gweni examples
#
#macro(gweni_add_example example_name)
#    set(${example_name}_sources
#        "${example_name}/${example_name}.cpp")
#
#    set(${example_name}_resources
#        "${GWENI_SOURCE_DIR}/source/resource/DefaultSkin.png"
#        "${GWENI_SOURCE_DIR}/source/resource/OpenSans.ttf"
#        "${GWENI_SOURCE_DIR}/source/test/resource/logo.png"
#        "${GWENI_SOURCE_DIR}/source/test/resource/test16.png")
#
#    if(APPLE AND APP_BUNDLE)
#        set(${example_name}_build_type MACOSX_BUNDLE)
#        set(${example_name}_resource_dir "resources")
#        set(${example_name}_runtime_resource_dir "../resources/") # relative to MacOS/
#    else()
#        set(${example_name}_build_type )
#        set(${example_name}_resource_dir "$<TARGET_FILE_DIR:${example_name}>")
#        set(${example_name}_runtime_resource_dir "")
#    endif()
#
#    #add_definitions(-DGWENI_EXAMPLE -DGWENI_EXAMPLE_RESOURCE_DIR="${APP_RUNTIME_RESOURCE_DIR}")
#
#    add_executable(${example_name} ${${example_name}_build_type} ${${example_name}_sources})
#
#    target_link_libraries(${example_name}
#                          gweni
##                          Gweni${GWENI_RENDER_NAME}
##                          ${GWENI_RENDER_LIBRARIES}
##                          ${GWENI_REFLECT_LIBRARIES}
#    )
#
#    if(APPLE AND APP_BUNDLE)
#        # make MacOS bundle
#        set_target_properties(${example_name} PROPERTIES
#            MACOSX_BUNDLE_COPYRIGHT "${APP_COPYRIGHT}"
#            MACOSX_BUNDLE_INFO_STRING "${APP_VERSION}, ${APP_COPYRIGHT}"
#            MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
#            MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_NAME} example v${APP_VERSION}"
#            MACOSX_BUNDLE_GUI_IDENTIFIER "org.gweni.${PROJECT_NAME}.${example_name}"
#            )
#        set_source_files_properties(${${example_name}_resources} PROPERTIES
#            MACOSX_PACKAGE_LOCATION "${${example_name}_resource_dir}")
#    else()
#        # Non-bundled app
#        macro(CopyAppResource RES_FROM)
#            get_filename_component(FNANE "${RES_FROM}" NAME)
#            add_custom_command(
#                TARGET ${example_name} POST_BUILD
#                COMMAND ${CMAKE_COMMAND} -E copy "${RES_FROM}" "${APP_RESOURCE_DIR}/${FNANE}"
#                COMMENT "Copying sample resource")
#        endmacro(CopyAppResource)
#
#        foreach(resource_file ${${example_name}_resources})
#            CopyAppResource(${resource_file})
#        endforeach()
#    endif()
#endmacro()
#
#macro(subdirectorylist result curdir)
#  file(GLOB children RELATIVE ${curdir} ${curdir}/*)
#  set(dirlist "")
#
#  foreach(child ${children})
#    if(IS_DIRECTORY ${curdir}/${child})
#      list(APPEND dirlist ${child})
#    endif()
#  endforeach()
#  set(${result} ${dirlist})
#endmacro()
#
#subdirectorylist(examples ${CMAKE_CURRENT_LIST_DIR})
#
#foreach(example_name ${examples})
#    gweni_add_example(${example_name})
#endforeach()

set(GWENI_DEMO_SOURCES
    demo/button.cpp
    demo/button.h
    demo/checkBox.cpp
    demo/checkBox.h
    demo/collapsibleList.cpp
    demo/collapsibleList.h
    demo/colorPicker.cpp
    demo/colorPicker.h
    demo/comboBox.cpp
    demo/comboBox.h
    demo/crossSplitter.cpp
    demo/crossSplitter.h
    demo/demo.cpp
    demo/demo.h
    demo/groupBox.cpp
    demo/groupBox.h
    demo/imagePanel.cpp
    demo/imagePanel.h
    demo/label.cpp
    demo/label.h
    demo/labelMultiline.cpp
    demo/labelMultiline.h
    demo/layoutDemo.cpp
    demo/layoutDemo.h
    demo/listBox.cpp
    demo/listBox.h
    demo/menuStrip.cpp
    demo/menuStrip.h
    demo/numeric.cpp
    demo/numeric.h
    demo/pageControl.cpp
    demo/pageControl.h
    demo/progressBar.cpp
    demo/progressBar.h
    demo/properties.cpp
    demo/properties.h
    demo/radioButton.cpp
    demo/radioButton.h
    demo/scrollControl.cpp
    demo/scrollControl.h
    demo/slider.cpp
    demo/slider.h
    demo/statusBar.cpp
    demo/statusBar.h
    demo/tabControl.cpp
    demo/tabControl.h
    demo/textBox.cpp
    demo/textBox.h
    demo/treeControl.cpp
    demo/treeControl.h
    demo/window.cpp
    demo/window.h
)

add_library(gweni_demo STATIC
    ${GWENI_DEMO_SOURCES}
)

source_group("" FILES ${GWENI_DEMO_SOURCES})

target_include_directories(gweni_demo PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:examples>
)
target_link_libraries(gweni_demo gweni)

if(GWENI_RENDER_OPENGL_CORE)
    add_subdirectory(openglcore)
endif()
