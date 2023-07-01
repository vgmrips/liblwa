#### liblwa-output ####
project(lwa-examples LANGUAGES C CXX)

add_executable(audiotest audiotest.c)
target_link_libraries(audiotest PRIVATE lwa-output)
if(USE_SANITIZERS)
	add_sanitizers(audiotest)
endif(USE_SANITIZERS)

install(TARGETS audiotest DESTINATION "${CMAKE_INSTALL_BINDIR}")
