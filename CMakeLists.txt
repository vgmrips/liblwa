#### liblwa-output ####
cmake_minimum_required(VERSION 3.1)
if(POLICY CMP0042)
	cmake_policy(SET CMP0042 NEW)
endif()
if(POLICY CMP0048)
	cmake_policy(SET CMP0048 NEW)
endif()
if(POLICY CMP0074)
	cmake_policy(SET CMP0074 NEW)
endif()

project(lwa VERSION 0.1 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/libs/cmake_modules/")

include(GNUInstallDirs)
include(GenerateExportHeader)
include(CheckIncludeFile)
include("${PROJECT_SOURCE_DIR}/cmake/utils.cmake")

set(LIBLWA_NAME ${PROJECT_NAME})	# for use in subdirectories
set(LIBLWA_VERSION ${PROJECT_VERSION})	# for use in subdirectories
set(LIBLWA_SOURCE_DIR ${PROJECT_SOURCE_DIR})	# for use in subdirectories
set(LIBLWA_INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}/lwa")
set(LIBLWA_INSTALL_TARGETS)	# will be filled in by sub-projects
set(LIBLWA_DEPS)	# will be filled in by sub-projects

option(BUILD_EXAMPLES "build example programs" OFF)
set(LIBRARY_TYPE "STATIC" CACHE STRING "library build type (static/shared)")
set_property(CACHE LIBRARY_TYPE PROPERTY STRINGS "SHARED;STATIC")
option(USE_SANITIZERS "use sanitizers" ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

check_include_file("stdint.h" HAVE_STDINT)
if(HAVE_STDINT)
	add_definitions("-D HAVE_STDINT_H")
endif()
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)


if(MSVC)
	set(CMAKE_DEBUG_POSTFIX "d")
	if("${LIBRARY_TYPE}" STREQUAL "STATIC")
		if(MSVC_VERSION LESS 1400)
			set(MSVC_POSTFIX "VC6")	# there are a few limitations for MSVC versions before 2005
		elseif(CMAKE_CL_64)
			set(MSVC_POSTFIX "Win64")
		else()
			set(MSVC_POSTFIX "Win32")
		endif()
	endif()
	if (NOT "${MSVC_POSTFIX}" STREQUAL "")
		set(CMAKE_RELEASE_POSTFIX "_${MSVC_POSTFIX}${CMAKE_RELEASE_POSTFIX}")
		set(CMAKE_DEBUG_POSTFIX "_${MSVC_POSTFIX}${CMAKE_DEBUG_POSTFIX}")
		set(CMAKE_MINSIZEREL_POSTFIX "_${MSVC_POSTFIX}${CMAKE_MINSIZEREL_POSTFIX}")
		set(CMAKE_RELWITHDEBINFO_POSTFIX "_${MSVC_POSTFIX}${CMAKE_RELWITHDEBINFO_POSTFIX}")
	endif()
	
	if(NOT MSVC_VERSION LESS 1400)
		# Note: CMake 3.12 and up have add_compile_options()
		add_definitions("-D _CRT_SECURE_NO_WARNINGS")
	endif()
endif()

if(MINGW)
	option(LINK_STATIC_LIBS "link with static runtime libraries (MinGW only)" ON)
	if(LINK_STATIC_LIBS)
		set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
		set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ ${CMAKE_CXX_STANDARD_LIBRARIES}")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
	endif()
else()
	option(LINK_STATIC_LIBS "link with static runtime libraries (MinGW only)" OFF)
endif()

if(CMAKE_COMPILER_IS_GNUCC)

# assume Windows 2000 and later for GetConsoleWindow API call
if(WIN32)
	add_definitions("-D _WIN32_WINNT=0x500")
endif()
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-long-long -Wno-unused-value")

# silence typical sound core warnings
#set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unknown-pragmas")
#set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-sign-compare")
#set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unused-variable -Wno-unused-const-variable -Wno-unused-function")

# additional warnings from http://blog.httrack.com/blog/2014/03/09/what-are-your-gcc-flags/
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpointer-arith -Winit-self -Wstrict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat -Wformat-security -Wformat-nonliteral")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fstack-protector")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_C_FLAGS_RELEASE}")

endif(CMAKE_COMPILER_IS_GNUCC)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/sanitizers")
find_package(Sanitizers)


# --- main library ---
add_subdirectory(utils)
if(USE_SANITIZERS)
	add_sanitizers(utils)
endif(USE_SANITIZERS)

add_subdirectory(audio)
if(USE_SANITIZERS)
	add_sanitizers(lwa-output)
endif(USE_SANITIZERS)


# --- additional stuff ---
if(BUILD_EXAMPLES)

add_executable(audiotest audiotest.c)
target_include_directories(audiotest PRIVATE ${LIBLWA_SOURCE_DIR})
target_link_libraries(audiotest PRIVATE lwa-output)
if(USE_SANITIZERS)
	add_sanitizers(audiotest)
endif(USE_SANITIZERS)

install(TARGETS audiotest DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif(BUILD_EXAMPLES)

set(COMMON_HEADERS
	common_def.h
	lwa_api.h
	stdtype.h
)

install(FILES ${COMMON_HEADERS} DESTINATION "${LIBLWA_INSTALL_INCLUDE_DIR}")
cmake_cfg_install("${LIBLWA_SOURCE_DIR}/cmake/config.cmake.in"
	NAME "${LIBLWA_NAME}"
	VERSION "${LIBLWA_VERSION}"
	TARGETS "${LIBLWA_INSTALL_TARGETS}"
	DEPS "${LIBLWA_DEPS}"
	)
