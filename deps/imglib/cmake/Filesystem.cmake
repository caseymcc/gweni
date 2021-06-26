#get location of macro file
set(filesystem_macro_dir ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")

macro(set_target_filesystem target)
    set(target_name "${target}")

    set(${target}_BOOST_FILESYSTEM 0)
    set(${target}_STD_FILESYSTEM 1)
    set(${target}_EXPERIMENTAL_FILESYSTEM 2)
    set(${target}_FILESYSTEM ${${target}_BOOST_FILESYSTEM})

    #find what filesystem to use
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        if(MSVC_VERSION GREATER_EQUAL 1900)
            set(${target}_FILESYSTEM ${${target}_EXPERIMENTAL_FILESYSTEM})
            if(MSVC_VERSION GREATER 1900)
                include(CheckCXXCompilerFlag)

                check_cxx_compiler_flag("/std:c++17" _cpp_17_flag_supported)
                if(_cpp_17_flag_supported)
#                    add_compile_options("/std:c++17")
                    target_compile_options(${target} PRIVATE "/std:c++17")
                    set(${target}_FILESYSTEM ${${target}_STD_FILESYSTEM})
                endif()
            endif()
        endif()
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") 
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0)
            set(${target}_FILESYSTEM ${${target}_STD_FILESYSTEM})
        endif()
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
        set(CMAKE_CXX_STANDARD 14)
        set(${target}_FILESYSTEM ${${target}_BOOST_FILESYSTEM})
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(${target}_FILESYSTEM ${${target}_EXPERIMENTAL_FILESYSTEM})
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
            set(${target}_FILESYSTEM ${${target}_STD_FILESYSTEM})
        endif()
    endif()

    target_compile_definitions(${target} PUBLIC "-D${target}_FILESYSTEM=${${target}_FILESYSTEM}")
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
        if(${target}_FILESYSTEM GREATER ${${target}_BOOST_FILESYSTEM})
            target_compile_options(voxigen PUBLIC "-lstdc++fs")
            target_link_libraries(voxigen stdc++fs)
        endif()
    endif()

    configure_file(
        ${filesystem_macro_dir}/target_filesystem.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/${target}/${target}_filesystem.h
    )
    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/${target})

endmacro(set_target_filesystem target)