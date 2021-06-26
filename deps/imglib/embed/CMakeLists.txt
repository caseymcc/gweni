cmake_minimum_required(VERSION 3.12)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/../cmake)

project(embed)

#set(EMBED_STD_FILESYSTEM 1)
#set(EMBED_EXPERIMENTAL_FILESYSTEM 2)
#
#if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
#    set(EMBED_USE_FILESYSTEM ${EMBED_EXPERIMENTAL_FILESYSTEM})
#    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
#        set(EMBED_USE_FILESYSTEM ${EMBED_STD_FILESYSTEM})
#    endif()
#endif()

set(embed_Sources
	getopt.cpp
	main.cpp
)

set(embed_Headers
	getopt.h
)

add_executable(embed
	${embed_Sources}
	${embed_Headers}
	${embed_Uis}
)

#target_compile_definitions(embed PRIVATE "-DEMBED_USE_FILESYSTEM=${EMBED_USE_FILESYSTEM}")
if(WIN32)
	target_compile_definitions(embed PRIVATE  -DNOMINMAX -DWINDOWS)
endif()

#link to file system
include(Filesystem)
set_target_filesystem(embed)

#target_compile_options(embed PUBLIC "-lstdc++fs")
#target_link_libraries(embed stdc++fs)

if(WIN32)
   set_target_properties(embed PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
   set_target_properties(embed PROPERTIES COMPILE_DEFINITIONS_DEBUG "_CONSOLE")
   set_target_properties(embed PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
   set_target_properties(embed PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO "_CONSOLE")
   set_target_properties(embed PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE")
   set_target_properties(embed PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE")
endif(WIN32)
