#### Audio Output Library ####
project(libVgmTest)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# assume Windows 2000 and later for GetConsoleWindow API call
add_definitions("-D _WIN32_WINNT=0x500")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-long-long")

# silence typical sound core warnings
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unknown-pragmas")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unused-value -Wno-sign-compare")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unused-variable -Wno-unused-const-variable -Wno-unused-function")

# additional warnings from http://blog.httrack.com/blog/2014/03/09/what-are-your-gcc-flags/
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector -Wpointer-arith -Winit-self")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fstack-protector")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat -Wformat-security -Wformat-nonliteral")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstrict-aliasing")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")


add_subdirectory(audio)
add_subdirectory(emu)


add_executable(audiotest audiotest.c)
target_include_directories(audiotest PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(audiotest PUBLIC libaudio)

add_executable(emutest emutest.c)
target_include_directories(emutest PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(emutest PUBLIC m libemu)

add_executable(audemutest audemutest.c)
target_include_directories(audemutest PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(audemutest PUBLIC m libaudio libemu)

add_executable(vgmtest vgmtest.c)
target_include_directories(vgmtest PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(vgmtest PUBLIC m z libaudio libemu)