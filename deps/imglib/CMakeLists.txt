cmake_minimum_required(VERSION 3.12)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/Hunter/HunterGate.cmake)

HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.23.267.tar.gz"
    SHA1 "9c5c7fa6e17c2ae15dd922184bc7c51235aaae70"
)

project(imglib)

#remove min and max macro declares from windows builds, causes issues with std::min/max
add_definitions(-DNOMINMAX)

option(IMGLIB_USE_COMPILETIME_TYPE "Turn on/off support for compile time type informat" ON)
option(IMGLIB_USE_PNG "Turn on/off support for PNG" ON)
option(IMGLIB_USE_JPEG "Turn on/off support for Jpeg" ON)
option(IMGLIB_USE_EIGEN "Turn on/off support for eigen library" ON)
option(IMGLIB_USE_OPENCL "Turn on/off support for OpenCl" ON)
option(IMGLIB_USE_OPENCL_CPP "Turn on/off support for OpenCl CPP classes" ON)
option(IMGLIB_USE_CUDA "Turn on/off support for CUDA" OFF)
option(IMGLIB_BUILD_TEST "Turns on test application" OFF)

#set(IMGLIB_STD_FILESYSTEM 1)
#set(IMGLIB_EXPERIMENTAL_FILESYSTEM 2)

#build embed
include(ExternalProject)

ExternalProject_Add(embed
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/embed
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/embed
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release #just need the release version
    INSTALL_COMMAND "" #disable install, just use it from build dir
)
ExternalProject_Get_Property(embed BINARY_DIR)

set(EMBED_LOCATION ${BINARY_DIR}/embed)
add_executable(embed::embed IMPORTED)
set_target_properties(embed::embed PROPERTIES IMPORTED_LOCATION ${EMBED_LOCATION})
#complete embed project

set(imglib_headers
    include/imglib/colorConversion.h
    include/imglib/colorConversion.inl
    include/imglib/copyTo.h
    include/imglib/draw.h
    include/imglib/point_glm.h
    include/imglib/image.h
    include/imglib/imageIo.h
    include/imglib/match.h
    include/imglib/ppm.h
    include/imglib/simpleImage.h
    include/imglib/transform.h
)

set(imglib_sources
    source/colorConversion.cpp
    source/image.cpp
    source/match.cpp
    source/ppm.cpp
    source/simpleImage.cpp
    source/transform.cpp
)

set(imglib_libraries "")
set(imglib_compile_options "")

#if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
#    set(IMGLIB_USE_FILESYSTEM ${IMGLIB_EXPERIMENTAL_FILESYSTEM})
#    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
#        set(IMGLIB_USE_FILESYSTEM ${IMGLIB_STD_FILESYSTEM})
#    endif()
#
#    set(imglib_libraries ${imglib_libraries} stdc++fs)
#    set(imglib_compile_options "-lstdc++fs")
#endif()

if(IMGLIB_USE_COMPILETIME_TYPE)
    hunter_add_package(ctti)
    find_package(ctti REQUIRED CONFIG)

    set(imglib_libraries ${imglib_libraries} ctti)
endif()

if(IMGLIB_USE_EIGEN)
    hunter_add_package(Eigen)
    find_package(Eigen3 REQUIRED)

    set(imglib_libraries ${imglib_libraries} Eigen3::Eigen)
endif()

if(IMGLIB_USE_PNG)
    hunter_add_package(PNG)
    find_package(PNG CONFIG REQUIRED)

    set(imglib_headers ${imglib_headers}
        include/imglib/png.h
    )

    set(imglib_sources ${imglib_sources}
        source/png.cpp
    )

    set(imglib_libraries ${imglib_libraries} PNG::png)
endif()

if(IMGLIB_USE_JPEG)
    hunter_add_package(Jpeg)
    find_package(JPEG CONFIG REQUIRED)

    set(imglib_headers ${imglib_headers}
        include/imglib/jpeg.h
    )

    set(imglib_sources ${imglib_sources}
        source/jpeg.cpp
    )

    set(imglib_libraries ${imglib_libraries} JPEG::jpeg)
endif()

if(IMGLIB_USE_OPENCL)
    hunter_add_package(OpenCL)
    find_package(OpenCL CONFIG REQUIRED)

    hunter_add_package(OpenCL-cpp)
    find_package(OpenCL-cpp CONFIG REQUIRED)

    set(imglib_headers ${imglib_headers}
        include/imglib/colorConversion_cl.h
        include/imglib/context_cl.h
        include/imglib/kernels_cl.h
        include/imglib/openCLImage.h
        include/imglib/openCLImageBuffer.h
        include/imglib/operators_cl.h
        include/imglib/transform_cl.h
        include/imglib/utils_cl.h
    )

    set(imglib_sources ${imglib_sources}
        source/colorConversion_cl.cpp
        source/kernels_cl.cpp
        source/openCLImage.cpp
        source/openCLImageBuffer.cpp
        source/operators_cl.cpp
        source/transform_cl.cpp
        source/utils_cl.cpp
    )

    set(imglib_libraries ${imglib_libraries} 
        OpenCL::OpenCL 
        OpenCL-cpp::OpenCL-cpp
    )

    include(EmbedFile)
    set(embedFiles
        kernels/colorConversion.cl
        kernels/operators.cl
        kernels/transform.cl
        kernels/utils.cl
    )
    embed_files(embededFiles "${embedFiles}" "imglib::cl")

endif()


add_library(imglib
    ${imglib_sources}
    ${imglib_headers}
    ${embededFiles}
)

#force embed to build
add_dependencies(imglib embed)

include(GenerateExportHeader)
generate_export_header(imglib)

target_link_libraries(imglib ${imglib_libraries})

target_include_directories(imglib PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(imglib PUBLIC ${CMAKE_CURRENT_BINARY_DIR}) #export files will be here
#target_compile_definitions(imglib PRIVATE "-DIMGLIB_USE_FILESYSTEM=${IMGLIB_USE_FILESYSTEM}")
target_compile_options(imglib PUBLIC ${imglib_compile_options})

#link to file system
include(Filesystem)
set_target_filesystem(imglib)

if(IMGLIB_USE_COMPILETIME_TYPE)
    target_compile_definitions(imglib PUBLIC -DIMGLIB_USE_COMPILETIME_TYPE)

    #imglib uses ctti which defines foreach functions, if qt is ever used then its macros interfere
    target_compile_definitions(imglib PUBLIC -DQT_NO_FOREACH)
endif()

if(IMGLIB_USE_OPENCL)
    target_compile_definitions(imglib PUBLIC -DIMGLIB_OPENCL)
    if(IMGLIB_USE_OPENCL_CPP)
        target_compile_definitions(imglib PUBLIC -DIMGLIB_OPENCL_CPP)
    endif()

    get_target_property(OPENCL_BIN_DIR OpenCL::OpenCL IMPORTED_LOCATION_DEBUG)
    get_filename_component(OPENCL_BIN_DIR ${OPENCL_BIN_DIR} DIRECTORY)
endif()

if(IMGLIB_USE_EIGEN)
    target_compile_definitions(imglib PUBLIC -DIMGLIB_EIGEN)
endif()

if(IMGLIB_USE_PNG)
    target_compile_definitions(imglib PUBLIC -DIMGLIB_PNG)
endif()

if(IMGLIB_USE_JPEG)
    target_compile_definitions(imglib PUBLIC -DIMGLIB_JPG)
endif()

#build test application
#if(IMGLIB_BUILD_TEST)
#    set(imglib_benchmark_headers
#    )
#    
#    set(imglib_benchmark_sources
#        test/benchmark.cpp
#    )
#
#    if(IMGLIB_USE_OPENCL)
#        set(imglib_benchmark_headers ${imglib_benchmark_headers}
#            test/benchmarkOpenCL.h
#        )
#
#        set(imglib_benchmark_sources ${imglib_benchmark_sources}
#            test/benchmarkOpenCL.cpp
#        )
#    endif()
#
#    add_executable(imglibBenchmark WIN32
#        ${imglib_benchmark_headers}
#        ${imglib_benchmark_sources}
#    )
#
#    target_link_libraries(imglibBenchmark imglib gflags-static opencl_util)
#
#    set_target_properties(imglibBenchmark PROPERTIES FOLDER "benchmarks")
#
#    if(WIN32)
#        foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES} )
#            string( TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER )
#            set_target_properties(imglibBenchmark PROPERTIES LINK_FLAGS_${CONFIG_TYPE_UPPER} "/SUBSYSTEM:CONSOLE")
#            set_target_properties(imglibBenchmark PROPERTIES COMPILE_DEFINITIONS_${CONFIG_TYPE_UPPER} "_CONSOLE")
#        endforeach()
#    endif()
#
#    set(RUNTIME_LIBS "")
#
#    if(IMGLIB_USE_OPENCL)
#        set(RUNTIME_LIBS ${RUNTIME_LIBS} ${OPENCL_BIN_DIR})
#    endif()
#
#    #message(FATAL_ERROR "RUNTIME_LIBS: ${RUNTIME_LIBS}")
#    create_target_launcher(imglibBenchmark
#        RUNTIME_LIBRARY_DIRS ${RUNTIME_LIBS}
#    )
#endif()

