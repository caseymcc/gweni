#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  gweni_add_resources(<target>): the textured skin's files of an installed
#  gweni (GWENI_RESOURCE_DIR, gweniConfig.cmake) where <target>, an executable,
#  finds them through RelativeToExecutablePaths("resources"):
#
#  - natively, mirrored to resources/ in the target's output directory on every
#    build, by mirrorDirectory.cmake's rules (installed beside this file): a
#    file the package no longer has is removed from the copy, an unchanged one
#    is not rewritten. The directory is the target's RUNTIME_OUTPUT_DIRECTORY,
#    else CMAKE_RUNTIME_OUTPUT_DIRECTORY, else its binary directory (with the
#    configuration's under a multi-config generator), read when this is called,
#    so set those first. $<TARGET_FILE_DIR> cannot be used: the copy is a
#    target the executable depends on, so it would depend on itself;
#  - on the web (Emscripten), preloaded into the target's .data at /resources,
#    where platform::getExecutableDir() ("/") puts RelativeToExecutablePaths,
#    the license texts left out (nothing loads them; a site ships them as
#    files), with the files as link dependencies, so an update relinks.
#
#  Included by gweniConfig.cmake. Named add, not copy, because on the web it
#  preloads.
#

# Global properties, not variables: the function may be called from a directory
# that does not see the variables of the one find_package(gweni) ran in.
set_property(GLOBAL PROPERTY gweni_mirror_script "${CMAKE_CURRENT_LIST_DIR}/mirrorDirectory.cmake")
set_property(GLOBAL PROPERTY gweni_resource_dir "${GWENI_RESOURCE_DIR}")

function(gweni_add_resources target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "gweni_add_resources: ${target} is not a target")
    endif()
    get_property(resource_dir GLOBAL PROPERTY gweni_resource_dir)
    get_property(mirror_script GLOBAL PROPERTY gweni_mirror_script)
    if(NOT resource_dir OR NOT IS_DIRECTORY "${resource_dir}")
        message(FATAL_ERROR
            "gweni_add_resources: the package's resources (${resource_dir}) are not a "
            "directory")
    endif()

    if(EMSCRIPTEN)
        file(GLOB_RECURSE resources CONFIGURE_DEPENDS "${resource_dir}/*")
        target_link_options(${target} PRIVATE
            "SHELL:--preload-file ${resource_dir}@/resources"
            "SHELL:--exclude-file *-LICENSE.txt")
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${resources})
        return()
    endif()

    get_target_property(output_dir ${target} RUNTIME_OUTPUT_DIRECTORY)
    if(NOT output_dir)
        if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
            set(output_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        else()
            get_target_property(output_dir ${target} BINARY_DIR)
        endif()
        get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
        if(multi_config)
            string(APPEND output_dir "/$<CONFIG>")
        endif()
    endif()

    add_custom_target(${target}_gweni_resources
        COMMAND ${CMAKE_COMMAND}
            "-DSRC=${resource_dir}" "-DDST=${output_dir}/resources"
            -P "${mirror_script}"
        COMMENT "Mirroring gweni's resources next to ${target}"
        VERBATIM)
    add_dependencies(${target} ${target}_gweni_resources)
endfunction()
