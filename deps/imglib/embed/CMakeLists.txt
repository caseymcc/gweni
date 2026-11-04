cmake_minimum_required(VERSION 3.12)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/../cmake)

project(embed)

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
)

if(WIN32)
	target_compile_definitions(embed PRIVATE -DNOMINMAX -DWINDOWS)
endif()

#link to file system
include(Filesystem)
set_target_filesystem(embed)

if(WIN32)
   set_target_properties(embed PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
   set_target_properties(embed PROPERTIES COMPILE_DEFINITIONS_DEBUG "_CONSOLE")
   set_target_properties(embed PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
   set_target_properties(embed PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO "_CONSOLE")
   set_target_properties(embed PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE")
   set_target_properties(embed PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE")
endif(WIN32)
