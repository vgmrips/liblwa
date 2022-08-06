#### miscellaneous utility functions/classes ####
project(vgm-utils)

option(UTIL_THREADING "build threading utilities (thread/mutex/signal)" ON)


set(UTIL_DEFS)
set(UTIL_FILES)
set(UTIL_HEADERS)	# export headers
set(UTIL_INCLUDES)
set(UTIL_LIBS)
set(UTIL_DEPS)	# dependencies for CMake Config file

set(UTIL_PC_CFLAGS)
set(UTIL_PC_LDFLAGS)
set(UTIL_PC_PKGS)



# Threads and Synchronization
# ---------------------------
if(UTIL_THREADING)
find_package(Threads REQUIRED)
set(UTIL_DEPS ${UTIL_DEPS} "Threads")

set(UTIL_HEADERS ${UTIL_HEADERS} OSMutex.h OSSignal.h OSThread.h)
if(CMAKE_USE_WIN32_THREADS_INIT)
	set(UTIL_FILES ${UTIL_FILES}
		OSMutex_Win.c
		OSSignal_Win.c
		OSThread_Win.c
	)
elseif(CMAKE_USE_PTHREADS_INIT)
	set(UTIL_FILES ${UTIL_FILES}
		OSMutex_POSIX.c
		OSSignal_POSIX.c
		OSThread_POSIX.c
	)
endif()
set(UTIL_LIBS ${UTIL_LIBS} Threads::Threads)
set(UTIL_PC_LDFLAGS ${UTIL_PC_LDFLAGS} ${CMAKE_THREAD_LIBS_INIT})
endif(UTIL_THREADING)



add_library(${PROJECT_NAME} ${LIBRARY_TYPE} ${UTIL_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(${PROJECT_NAME} PUBLIC ${UTIL_DEFS})
target_include_directories(${PROJECT_NAME}
	PUBLIC $<BUILD_INTERFACE:${LIBVGM_SOURCE_DIR}> $<INSTALL_INTERFACE:${LIBVGM_INSTALL_INCLUDE_DIR}>
	PRIVATE ${UTIL_INCLUDES}
)
target_link_libraries(${PROJECT_NAME} PUBLIC ${UTIL_LIBS})


pkgcfg_configure("${LIBVGM_SOURCE_DIR}/cmake/libvgm.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/vgm-utils.pc"
		NAME "LibVGM Utils"
		DESC "LibVGM utility library"
		VERSION ${LIBVGM_VERSION}
		DEFINES ${UTIL_DEFS}
		CFLAGS ${UTIL_PC_CFLAGS}
		LDFLAGS_PRIV ${UTIL_PC_LDFLAGS}
		LDFLAGS_PUB "-l${PROJECT_NAME}"
		PKGS_PRIV ${UTIL_PC_PKGS}
	)

install(TARGETS ${PROJECT_NAME}
		EXPORT "${LIBVGM_NAME}"
		RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
		LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	)
set(LIBVGM_INSTALL_TARGETS ${LIBVGM_INSTALL_TARGETS} "${PROJECT_NAME}" PARENT_SCOPE)
install(FILES ${UTIL_HEADERS} DESTINATION "${LIBVGM_INSTALL_INCLUDE_DIR}/utils")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vgm-utils.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
set(LIBVGM_DEPS ${LIBVGM_DEPS} "${UTIL_DEPS}" PARENT_SCOPE)
