# Gweni examples
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(example_name "OpenGLCore")

hunter_add_package(glfw)
find_package(glfw3 REQUIRED)
hunter_add_package(CreateLaunchers)
find_package(CreateLaunchers CONFIG REQUIRED)

include(CreateLaunchers)

set(${example_name}_sources
    openglcore.cpp)

set(${example_name}_resources
    "${GWENI_SOURCE_DIR}/resource/DefaultSkin.png"
    "${GWENI_SOURCE_DIR}/resource/OpenSans.ttf"
    "${GWENI_SOURCE_DIR}/test/resource/logo.png"
    "${GWENI_SOURCE_DIR}/test/resource/test16.png")

if(APPLE AND APP_BUNDLE)
    set(${example_name}_build_type MACOSX_BUNDLE)
    set(${example_name}_resource_dir "resources")
    set(${example_name}_runtime_resource_dir "../resources/") # relative to MacOS/
else()
    set(${example_name}_build_type )
    set(${example_name}_resource_dir "$<TARGET_FILE_DIR:${example_name}>")
    set(${example_name}_runtime_resource_dir "")
endif()

add_executable(${example_name} ${${example_name}_build_type} ${${example_name}_sources})

target_link_libraries(${example_name}
    gweni
    gweni_demo
    glfw
)

if(APPLE AND APP_BUNDLE)
    # make MacOS bundle
    set_target_properties(${example_name} PROPERTIES
        MACOSX_BUNDLE_COPYRIGHT "${APP_COPYRIGHT}"
        MACOSX_BUNDLE_INFO_STRING "${APP_VERSION}, ${APP_COPYRIGHT}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_NAME} example v${APP_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.gweni.${PROJECT_NAME}.${example_name}"
        )
    set_source_files_properties(${${example_name}_resources} PROPERTIES
        MACOSX_PACKAGE_LOCATION "${${example_name}_resource_dir}")
else()
    # Non-bundled app
#    macro(CopyAppResource RES_FROM)
#        get_filename_component(FNANE "${RES_FROM}" NAME)
#        add_custom_command(
#            TARGET ${example_name} POST_BUILD
#            COMMAND ${CMAKE_COMMAND} -E copy "${RES_FROM}" "${APP_RESOURCE_DIR}/${FNANE}"
#            COMMENT "Copying sample resource")
#    endmacro(CopyAppResource)
#
#    foreach(resource_file ${${example_name}_resources})
#        CopyAppResource(${resource_file})
#    endforeach()
endif()

create_target_launcher(${example_name}
    WORKING_DIRECTORY "${GWENI_SOURCE_DIR}"
)

