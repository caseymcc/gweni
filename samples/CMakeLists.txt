# Gweni samples

set(APP_VERSION "Sample 1.0 - Gweni ${GWENI_VERSION_STR}")

set(APP_BUNDLE ON)

include_directories(
    ${GWENI_SOURCE_DIR}/source/platform/include
    ${GWENI_SOURCE_DIR}/source/gweni/include
    ${GWENI_RENDER_INCLUDES}
    ${GWENI_SOURCE_DIR}/source/test/include
    ${GWENI_REFLECT_INCLUDE}
)

if(WITH_SAMPLE)

    set(SAMPLE_NAME Gweni${GWENI_RENDER_NAME}Sample)

    set(SAMPLE_SOURCES
        "${GWENI_RENDER_NAME}/${GWENI_RENDER_NAME}Sample.cpp")

    set(SAMPLE_RESOURCES
        "${GWENI_SOURCE_DIR}/source/gweni/resource/DefaultSkin.png"
        "${GWENI_SOURCE_DIR}/source/gweni/resource/OpenSans.ttf"
        "${GWENI_SOURCE_DIR}/source/test/resource/logo.png"
        "${GWENI_SOURCE_DIR}/source/test/resource/test16.png")

    if(APPLE AND APP_BUNDLE)
        set(APP_BUILD_TYPE MACOSX_BUNDLE)
        set(APP_RESOURCE_DIR "Resources")
        set(APP_RUNTIME_RESOURCE_DIR "../Resources/") # relative to MacOS/
    else()
        set(APP_BUILD_TYPE )
        set(APP_RESOURCE_DIR "$<TARGET_FILE_DIR:${SAMPLE_NAME}>")
        set(APP_RUNTIME_RESOURCE_DIR "")
    endif()

    add_definitions(-DGWENI_SAMPLE -DGWENI_SAMPLE_RESOURCE_DIR="${APP_RUNTIME_RESOURCE_DIR}")

    add_executable(${SAMPLE_NAME} ${APP_BUILD_TYPE} ${SAMPLE_SOURCES} ${SAMPLE_RESOURCES})

    target_link_libraries(${SAMPLE_NAME}
                          Gweni
                          Gweni${GWENI_RENDER_NAME}
                          ${GWENI_RENDER_LIBRARIES}
                          ${GWENI_REFLECT_LIBRARIES})

    if(APPLE AND APP_BUNDLE)
        # make MacOS bundle
        set_target_properties(${SAMPLE_NAME} PROPERTIES
            MACOSX_BUNDLE_COPYRIGHT "${APP_COPYRIGHT}"
            MACOSX_BUNDLE_INFO_STRING "${APP_VERSION}, ${APP_COPYRIGHT}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
            MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_NAME} sample v${APP_VERSION}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "org.gweni.${PROJECT_NAME}.${SAMPLE_NAME}"
            )
        set_source_files_properties(${SAMPLE_RESOURCES} PROPERTIES
            MACOSX_PACKAGE_LOCATION "${APP_RESOURCE_DIR}")
    else()
        # Non-bundled app
        macro(CopyAppResource RES_FROM)
            get_filename_component(FNANE "${RES_FROM}" NAME)
            add_custom_command(
                TARGET ${SAMPLE_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "${RES_FROM}" "${APP_RESOURCE_DIR}/${FNANE}"
                COMMENT "Copying sample resource")
        endmacro(CopyAppResource)

        foreach(RESFILE ${SAMPLE_RESOURCES})
            CopyAppResource(${RESFILE})
        endforeach()
    endif()

endif(WITH_SAMPLE)
