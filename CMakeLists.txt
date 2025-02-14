cmake_minimum_required(VERSION 3.8)

add_subdirectory("submodules/vk-bootstrap")

project(
juice-power
    VERSION
		# Major, Minor, Revision, [Alpha, Beta, Release]
		0.0.0.0
	LANGUAGES
	    C
		CXX
)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

message(STATUS "[BT] ${PROJECT_NAME}: ${CMAKE_BUILD_TYPE}")


file(GLOB_RECURSE CXX_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
file(GLOB_RECURSE CXX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")


add_executable(
juice-power
    ${CXX_HEADERS}
	${CXX_SOURCES}
	${C_SOURCES}
)

target_include_directories(
juice-power
    PRIVATE
	    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
juice-power
    PUBLIC
	    vk-bootstrap::vk-bootstrap
)


install(
TARGETS
    juice-power
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
