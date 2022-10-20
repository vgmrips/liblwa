#### miscellaneous utility functions/classes ####
project(lwa-utils)

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
find_package(Threads REQUIRED)
set(UTIL_DEPS ${UTIL_DEPS} "Threads")

set(UTIL_HEADERS ${UTIL_HEADERS}
	lwauMutex.h
	lwauSignal.h
	lwauThread.h
)
if(CMAKE_USE_WIN32_THREADS_INIT)
	set(UTIL_FILES ${UTIL_FILES}
		lwauMutex_Win.c
		lwauSignal_Win.c
		lwauThread_Win.c
	)
elseif(CMAKE_USE_PTHREADS_INIT)
	set(UTIL_FILES ${UTIL_FILES}
		lwauMutex_POSIX.c
		lwauSignal_POSIX.c
		lwauThread_POSIX.c
	)
endif()
set(UTIL_LIBS ${UTIL_LIBS} Threads::Threads)
set(UTIL_PC_LDFLAGS ${UTIL_PC_LDFLAGS} ${CMAKE_THREAD_LIBS_INIT})



add_library(${PROJECT_NAME} ${LIBRARY_TYPE} ${UTIL_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(${PROJECT_NAME} PUBLIC ${UTIL_DEFS})
target_include_directories(${PROJECT_NAME}
	PUBLIC $<BUILD_INTERFACE:${LIBLWA_SOURCE_DIR}> $<INSTALL_INTERFACE:${LIBLWA_INSTALL_INCLUDE_DIR}>
	PRIVATE ${UTIL_INCLUDES}
)
target_link_libraries(${PROJECT_NAME} PUBLIC ${UTIL_LIBS})


pkgcfg_configure("${LIBLWA_SOURCE_DIR}/cmake/package.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
		NAME "LibLWA Utils"
		DESC "Light-Weight Audio library: utilities"
		VERSION ${LIBLWA_VERSION}
		DEFINES ${UTIL_DEFS}
		CFLAGS ${UTIL_PC_CFLAGS}
		LDFLAGS_PRIV ${UTIL_PC_LDFLAGS}
		LDFLAGS_PUB "-l${PROJECT_NAME}"
		PKGS_PRIV ${UTIL_PC_PKGS}
	)

install(TARGETS ${PROJECT_NAME}
		EXPORT "${LIBLWA_NAME}"
		RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
		LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	)
set(LIBLWA_INSTALL_TARGETS ${LIBLWA_INSTALL_TARGETS} "${PROJECT_NAME}" PARENT_SCOPE)
install(FILES ${UTIL_HEADERS} DESTINATION "${LIBLWA_INSTALL_INCLUDE_DIR}/utils")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
set(LIBLWA_DEPS ${LIBLWA_DEPS} "${UTIL_DEPS}" PARENT_SCOPE)
