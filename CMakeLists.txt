cmake_minimum_required(VERSION 3.19)
set(CMAKE_POLICY_DEFAULT_CMP0110 NEW) # requires CMake 3.19

project(SlideController)

include(deps/buildcore/base.cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${QTDIR})

if(NOT MSVC)
	# enable warnings
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-conversion")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_INSTALL_RPATH "$ORIGIN" "$ORIGIN/../")
if(QTDIR)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH} "${QTDIR}/lib")
endif()
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

add_subdirectory(src)

install(
	PROGRAMS
		install.sh
	DESTINATION
		.
)

