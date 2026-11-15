# HudExample: gweni as a HUD over an app's own GL scene (hud.cpp), on the
# examples' capture plumbing (gweni_capture). It draws its scene with GL, so it
# is built with the GL renderer only (examples/CMakeLists.txt), and not in the
# web tree.
add_executable(HudExample hud.cpp)
gweni_set_warnings(HudExample)
target_link_libraries(HudExample
    gweni
    gweni_capture
    glfw
)

# It loads the resources/ next to it through capture::resourcePaths, as the
# other examples do, from a mirror of its own (cmake/mirrorDirectory.cmake),
# with the lazy CJK face's resources-lazy/ beside it.
get_property(hud_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(hud_multi_config)
    set(hud_resource_dir ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/resources)
else()
    set(hud_resource_dir ${CMAKE_CURRENT_BINARY_DIR}/resources)
endif()

add_custom_target(gweni_hud_resources
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${GWENI_SOURCE_DIR}/resources -DDST=${hud_resource_dir}
        -P ${GWENI_SOURCE_DIR}/cmake/mirrorDirectory.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${GWENI_LAZY_RESOURCE_DIR} -DDST=${hud_resource_dir}/../resources-lazy
        -P ${GWENI_SOURCE_DIR}/cmake/mirrorDirectory.cmake
    COMMENT "Mirroring resources/ next to HudExample"
    VERBATIM)
add_dependencies(HudExample gweni_hud_resources)
