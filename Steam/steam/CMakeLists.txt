cmake_minimum_required(VERSION 3.10)
project(Steam)

include(CTest)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

set(COMMON_MACROS STEAM_EXPORTS GNUC POSIX _POSIX _FILE_OFFSET_BITS=64 Z_HAVE_UNISTD_H)

if(CMAKE_SYSTEM_NAME MATCHES Linux)
	set(PLATFORM_MACROS LINUX _LINUX)
elseif(CMAKE_SYSTEM_NAME MATCHES Darwin)
	set(PLATFORM_MACROS OSX _OSX)
endif()

add_library(steam SHARED)
target_compile_definitions(steam PRIVATE ${COMMON_MACROS} ${PLATFORM_MACROS})
target_link_libraries(steam PRIVATE dl pthread)
if(CMAKE_SYSTEM_NAME MATCHES Darwin)
	set_target_properties(steam PROPERTIES VERSION 1.0.0 SOVERSION 1)
endif()

file(GLOB MAIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB ZLIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/zlib/*.c")
list(REMOVE_ITEM MAIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/registry.cpp")
list(REMOVE_ITEM MAIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Win32MiniDump.cpp")
target_sources(steam PRIVATE
${MAIN_SOURCES}
${ZLIB_SOURCES}
../steamclient_legacy/auth/ClassicRevEmuTicket.cpp
)

if(CMAKE_SYSTEM_NAME MATCHES Linux)
	add_library(steamvalidateuseridtickets SHARED
		Steam.cpp
		logFile.cpp
		strtools.cpp
		strtools_unicode.cpp
		../steamclient_legacy/auth/ClassicRevEmuTicket.cpp
	)
	target_compile_definitions(steamvalidateuseridtickets PRIVATE VALIDATOR_DLL ${COMMON_MACROS} ${PLATFORM_MACROS})
	target_link_libraries(steamvalidateuseridtickets PRIVATE dl pthread)

	# Old Source/Steam API compatibility backend used by CS:S Build 4100.
	# This is deliberately a small standalone target: it implements the exact
	# SteamClient006 / SteamGameServer002 era ABI instead of pulling in modern
	# Steamworks interfaces.
	add_library(steamclient SHARED
		../steamclient_legacy/LegacySteamClient.cpp
		../steamclient_legacy/LegacySteamRuntime.cpp
		../steamclient_legacy/LegacySteamCallbacks.cpp
		../steamclient_legacy/LegacySteamAuth.cpp
		../steamclient_legacy/LegacySteamInterfaces.cpp
		../steamclient_legacy/LegacySteamTrace.cpp
		../steamclient_legacy/auth/ClassicRevEmuTicket.cpp
		../steamclient_legacy/auth/LegacyAuthTicket.cpp
	)
	target_include_directories(steamclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_compile_definitions(steamclient PRIVATE ${COMMON_MACROS} ${PLATFORM_MACROS})
	target_compile_options(steamclient PRIVATE -Wall -Wextra -Werror)
	target_link_libraries(steamclient PRIVATE dl pthread)
	set_target_properties(steamclient PROPERTIES
		OUTPUT_NAME steamclient
		CXX_VISIBILITY_PRESET hidden
		VISIBILITY_INLINES_HIDDEN YES
	)

	if(BUILD_TESTING)
		add_executable(steamclient_abi_smoke ../steamclient_legacy/tests/LegacySteamAbiSmoke.cpp)
		target_link_libraries(steamclient_abi_smoke PRIVATE dl pthread)
		add_test(NAME steamclient_legacy_abi
			COMMAND steamclient_abi_smoke $<TARGET_FILE:steamclient>)

		add_executable(steamclient_auth_ticket_smoke
			../steamclient_legacy/tests/LegacyAuthTicketSmoke.cpp
			../steamclient_legacy/auth/ClassicRevEmuTicket.cpp
			../steamclient_legacy/auth/LegacyAuthTicket.cpp
		)
		target_include_directories(steamclient_auth_ticket_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
		target_compile_definitions(steamclient_auth_ticket_smoke PRIVATE ${COMMON_MACROS} ${PLATFORM_MACROS})
		target_compile_options(steamclient_auth_ticket_smoke PRIVATE -Wall -Wextra -Werror)
		add_test(NAME steamclient_auth_ticket
			COMMAND steamclient_auth_ticket_smoke)
	endif()
endif()
