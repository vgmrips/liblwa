#### Audio Output Library ####
project(lwa-output LANGUAGES C CXX)

set(LWAO_DEFS)
set(LWAO_FILES
	lwao.c
)
if(MSVC)
	set(LWAO_FILES ${LWAO_FILES}
		lwa-output.def
	)
endif()
# export headers
set(LWAO_EXPORT_HDR "${PROJECT_BINARY_DIR}/lwao_export.h")
set(LWAO_HEADERS
	lwao_structs.h
	lwao_errors.h
	lwao.h
	lwao_drvfuncs.h
	${LWAO_EXPORT_HDR}
)
set(LWAO_INCLUDES)
set(LWAO_LIBS)
set(LWAO_DEPS)	# dependencies for CMake Config file

set(LWAO_PC_CFLAGS)
set(LWAO_PC_LDFLAGS)
set(LWAO_PC_REQUIRES)


if(WIN32)
	set(DRVOPT_WIN_ALL TRUE)
	if(NOT MSVC OR MSVC_VERSION LESS 1400)
		# WASAPI doesn't work with VC6 or MinGW due to a lack of Windows Vista/7 header files
		set(DRVOPT_WIN_VISTA FALSE)
	else()
		set(DRVOPT_WIN_VISTA TRUE)
	endif()
	set(DRVOPT_LINUX FALSE)
	set(DRVOPT_APPLE FALSE)
else()
	set(DRVOPT_WIN_ALL FALSE)
	set(DRVOPT_WIN_VISTA FALSE)
	if(UNIX AND NOT APPLE)
		set(DRVOPT_LINUX TRUE)
		set(DRVOPT_APPLE FALSE)
	else()
		set(DRVOPT_LINUX FALSE)
		set(DRVOPT_APPLE TRUE)
	endif()
	
	find_package(ALSA QUIET)
	find_package(PulseAudio QUIET)
endif()
find_package(LibAO QUIET)

option(LWAOD_WAVEWRITE "Audio Driver: Wave Writer" ON)

option(LWAOD_WINMM "Audio Driver: WinMM [Windows]" ${DRVOPT_WIN_ALL})
option(LWAOD_DSOUND "Audio Driver: DirectSound [Windows]" ${DRVOPT_WIN_ALL})
option(LWAOD_XAUDIO2 "Audio Driver: XAudio2 [Windows]" ${DRVOPT_WIN_ALL})
option(LWAOD_WASAPI "Audio Driver: WASAPI [Windows]" ${DRVOPT_WIN_VISTA})

option(LWAOD_OSS "Audio Driver: OSS (Open Sound System) [Linux]" OFF)
option(LWAOD_SADA "Audio Driver: SADA (Solaris Audio Device Architecture) [Solaris/BSD]" OFF)
option(LWAOD_ALSA "Audio Driver: ALSA (Advanced Linux Sound Architecture) [Linux]" ${ALSA_FOUND})
option(LWAOD_PULSE "Audio Driver: PulseAudio [Linux]" ${PULSEAUDIO_FOUND})
option(LWAOD_APPLE "Audio Driver: Core Audio [macOS]" ${DRVOPT_APPLE})
if(LINK_STATIC_LIBS)
	# linking errors when using static library in MinGW
	option(LWAOD_LIBAO "Audio Driver: libao" OFF)
else()
	option(LWAOD_LIBAO "Audio Driver: libao" ${LIBAO_FOUND})
endif()

if(LWAOD_WAVEWRITE)
	set(LWAO_DEFS ${LWAO_DEFS} " LWAO_DRIVER_WAVEWRITE")
	set(LWAO_FILES ${LWAO_FILES} AudDrv_WaveWriter.c)
endif()

if(LWAOD_WINMM)
	set(LWAO_DEFS ${LWAO_DEFS} " LWAO_DRIVER_WINMM")
	set(LWAO_FILES ${LWAO_FILES} AudDrv_WinMM.c)
	set(LWAO_LIBS ${LWAO_LIBS} winmm)
	set(LWAO_PC_LDFLAGS ${LWAO_PC_LDFLAGS} "-lwinmm")
endif()

if(LWAOD_DSOUND)
	set(LWAO_DEFS ${LWAO_DEFS} " LWAO_DRIVER_DSOUND")
	set(LWAO_FILES ${LWAO_FILES} AudDrv_DSound.cpp)
	set(LWAO_LIBS ${LWAO_LIBS} ole32 dsound uuid)
	set(LWAO_PC_LDFLAGS ${LWAO_PC_LDFLAGS} "-ldsound")
	set(LWAO_PC_LDFLAGS ${LWAO_PC_LDFLAGS} "-luuid")
endif()

if(LWAOD_XAUDIO2)
	set(LWAO_DEFS ${LWAO_DEFS} " LWAO_DRIVER_XAUD2")
	set(LWAO_FILES ${LWAO_FILES} AudDrv_XAudio2.cpp)
	set(LWAO_LIBS ${LWAO_LIBS} ole32)
	set(LWAO_PC_LDFLAGS ${LWAO_PC_LDFLAGS} "-lole32")
endif()

if(LWAOD_WASAPI)
	set(LWAO_DEFS ${LWAO_DEFS} " LWAO_DRIVER_WASAPI")
	set(LWAO_FILES ${LWAO_FILES} AudDrv_WASAPI.cpp)
	set(LWAO_LIBS ${LWAO_LIBS} ole32)
	set(LWAO_PC_LDFLAGS ${LWAO_PC_LDFLAGS} "-lole32")
endif()

if(LWAOD_OSS)
	set(LWAO_DEFS ${LWAO_DEFS} " LWAO_DRIVER_OSS")
	set(LWAO_FILES ${LWAO_FILES} AudDrv_OSS.c)
endif()

if(LWAOD_SADA)
	set(LWAO_DEFS ${LWAO_DEFS} " LWAO_DRIVER_SADA")
	set(LWAO_FILES ${LWAO_FILES} AudDrv_SADA.c)
endif()

if(LWAOD_ALSA)
	find_package(ALSA REQUIRED)
	set(LWAO_DEPS ${LWAO_DEPS} "ALSA")
	set(LWAO_DEFS ${LWAO_DEFS} " LWAO_DRIVER_ALSA")
	set(LWAO_FILES ${LWAO_FILES} AudDrv_ALSA.c)
	set(LWAO_INCLUDES ${LWAO_INCLUDES} ${ALSA_INCLUDE_DIRS})
	set(LWAO_LIBS ${LWAO_LIBS} ${ALSA_LIBRARIES})
	set(LWAO_PC_REQUIRES ${LWAO_PC_REQUIRES} "alsa")
endif()

if(LWAOD_PULSE)
	find_package(PulseAudio REQUIRED)
	set(LWAO_DEPS ${LWAO_DEPS} "PulseAudio")
	set(LWAO_DEFS ${LWAO_DEFS} " LWAO_DRIVER_PULSE")
	set(LWAO_FILES ${LWAO_FILES} AudDrv_Pulse.c)
	set(LWAO_INCLUDES ${LWAO_INCLUDES} ${PULSEAUDIO_INCLUDE_DIR})
	# There seems to be no package or variable for pulse-simple.
	set(LWAO_LIBS ${LWAO_LIBS} pulse-simple ${PULSEAUDIO_LIBRARY})
	set(LWAO_PC_REQUIRES ${LWAO_PC_REQUIRES} "libpulse-simple")
endif()

if(LWAOD_APPLE)
	set(LWAO_DEFS ${LWAO_DEFS} " LWAO_DRIVER_CA")
	set(LWAO_FILES ${LWAO_FILES} AudDrv_CoreAudio.c)
	set(LWAO_LIBS ${LWAO_LIBS} "-framework AudioToolbox")
	set(LWAO_PC_LDFLAGS ${LWAO_PC_LDFLAGS} "-framework AudioToolbox")
endif()

if(LWAOD_LIBAO)
	find_package(LibAO REQUIRED)
	set(LWAO_DEPS ${LWAO_DEPS} "LibAO")
	set(LWAO_DEFS ${LWAO_DEFS} " LWAO_DRIVER_LIBAO")
	set(LWAO_FILES ${LWAO_FILES} AudDrv_libao.c)
	set(LWAO_LIBS ${LWAO_LIBS} libao::libao)
	set(LWAO_PC_REQUIRES ${LWAO_PC_REQUIRES} "ao")
endif()


add_library(${PROJECT_NAME} ${LIBRARY_TYPE} ${LWAO_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES
	POSITION_INDEPENDENT_CODE ON
	C_VISIBILITY_PRESET hidden
	CXX_VISIBILITY_PRESET hidden
	)
target_compile_definitions(${PROJECT_NAME} PUBLIC ${LWAO_DEFS})
target_include_directories(${PROJECT_NAME}
	PUBLIC $<BUILD_INTERFACE:${LIBLWA_SOURCE_DIR}> $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> $<INSTALL_INTERFACE:${LIBLWA_INSTALL_INCLUDE_DIR}>
	PRIVATE ${LWAO_INCLUDES}
)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LWAO_LIBS} lwa-utils)
generate_export_header(${PROJECT_NAME}
	BASE_NAME lwao
	EXPORT_FILE_NAME "${LWAO_EXPORT_HDR}")

if(WIN32)
	# for some Windows Audio APIs, we need WinAPI headers from special directories
	if(NOT MSVC)
		target_include_directories(${PROJECT_NAME} PRIVATE
			${LIBLWA_SOURCE_DIR}/libs/include
			${LIBLWA_SOURCE_DIR}/libs/include_mingw
		)
	elseif(MSVC)
		target_include_directories(${PROJECT_NAME} PRIVATE
			${LIBLWA_SOURCE_DIR}/libs/include
			${LIBLWA_SOURCE_DIR}/libs/include_vc
		)
		if(MSVC_VERSION LESS 1400)
			target_include_directories(${PROJECT_NAME} PRIVATE
				${LIBLWA_SOURCE_DIR}/libs/include_vc6
			)
		endif()
	endif()
endif()


pkgcfg_configure("${LIBLWA_SOURCE_DIR}/cmake/package.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
		NAME "LibLWA Output"
		DESC "Light-Weight Audio library: audio output"
		VERSION ${LIBLWA_VERSION}
		DEFINES ${LWAO_DEFS}
		CFLAGS ${LWAO_PC_CFLAGS}
		LDFLAGS_PRIV ${LWAO_PC_LDFLAGS}
		LDFLAGS_PUB "-l${PROJECT_NAME}"
		PKGS_PRIV ${LWAO_PC_REQUIRES}
		PKGS_PUB lwa-utils
	)

install(TARGETS ${PROJECT_NAME}
		EXPORT "${LIBLWA_NAME}"
		RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
		LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	)
set(LIBLWA_INSTALL_TARGETS ${LIBLWA_INSTALL_TARGETS} "${PROJECT_NAME}" PARENT_SCOPE)
install(FILES ${LWAO_HEADERS} DESTINATION "${LIBLWA_INSTALL_INCLUDE_SUBDIR}/output")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
set(LIBLWA_DEPS ${LIBLWA_DEPS} "${LWAO_DEPS}" PARENT_SCOPE)
