cmake_minimum_required(VERSION 3.8)

add_subdirectory("submodules/vk-bootstrap")
add_subdirectory("submodules")

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


# We need GLSLC to compile our shaders.
find_program(GLSLC_EXECUTABLE glslc PATHS) # ... PATHS <some_path_to_my_glslc>)
message(STATUS "GLSLC Path: ${GLSLC_EXECUTABLE}")


# Object sources
file(GLOB_RECURSE CXX_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
file(GLOB_RECURSE CXX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

# Shader sources
# Compute shaders
file(GLOB_RECURSE COMP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp")
# Fragment shaders
file(GLOB_RECURSE FRAG_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag")
# Vertex shaders
file(GLOB_RECURSE VERT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert")
set(SHADER_SOURCES ${FRAG_SOURCES} ${VERT_SOURCES} ${COMP_SOURCES})
# Used to ease access of the shaders when testing
add_compile_definitions(COMPILED_SHADERS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders")


find_package(Vulkan REQUIRED)
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
find_package(fmt CONFIG REQUIRED)			# Convenience dependency
find_package(magic_enum CONFIG REQUIRED)	# Convenience dependency

set(MAGIC_ENUM_INCL_DIR "")


# Shader compilation steps
foreach(shader_source ${SHADER_SOURCES})
	get_filename_component(file_c ${shader_source} NAME)
	get_filename_component(file_no_ext ${shader_source} NAME_WE)
	get_filename_component(file_dir ${shader_source} DIRECTORY)
	add_custom_command(
		OUTPUT ${file_dir}/${file_c}.spv
		COMMAND ${GLSLC_EXECUTABLE} ${shader_source} -o ${file_dir}/${file_c}.spv
		DEPENDS ${shader_source}
		COMMENT "Compiling shader ${shader_source}"
		VERBATIM
	)
	list(APPEND SHADER_FILES ${file_dir}/${file_c}.spv)
endforeach()


# Check if the include directory is flat or nested
get_target_property(magic_enum_include_dirs magic_enum::magic_enum INTERFACE_INCLUDE_DIRECTORIES)

foreach(dir ${magic_enum_include_dirs})
	if (EXISTS "${dir}/magic_enum.hpp")
		set(MAGIC_ENUM_INCL_DIR "${dir}")
		break()
	elseif (EXISTS "${dir}/magic_enum/magic_enum.hpp")
		set(MAGIC_ENUM_INCL_DIR "${dir}/magic_enum")
		break()
	endif()
endforeach()


# Used to compile the shaders
add_custom_target(
shaders
	ALL
	DEPENDS
		${SHADER_FILES}
)

# Main executable
add_executable(
juice-power
	${CXX_HEADERS}
	${CXX_SOURCES}
	${C_SOURCES}
)
target_include_directories(juice-power PRIVATE "${MAGIC_ENUM_INCL_DIR}")


# Check that we have AMD's VMA available
get_target_property(JP_INCLUDE_DIRS juice-power INCLUDE_DIRECTORIES)

set(VMA_HEADER_FOUND FALSE)

foreach(dir ${JP_INCLUDE_DIRS})
	if (EXISTS "${dir}/vk_mem_alloc.h")
		set(VMA_HEADER_FOUND TRUE)
		message(STATUS "Found vk_mem_alloc.h in ${dir}")
		break()
	endif()
endforeach()

if (NOT VMA_HEADER_FOUND)
	message(FATAL_ERROR "AMD's VMA's vk_mem_alloc.h not found in juice-power include directories!")
endif()


# Compile our shaders when the main exec is.
add_dependencies(juice-power shaders)

target_include_directories(
juice-power
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${IMGUI_DIR}
)

target_include_directories(
juice-power
	PUBLIC
		magic_enum::
)

target_link_libraries(
juice-power
	PRIVATE
		Vulkan::Vulkan
		vk-bootstrap::vk-bootstrap
		SDL3::SDL3
		fmt::fmt
		magic_enum::magic_enum
		imgui
		imgui_impl_sdl3
		imgui_impl_vulkan
)


install(
TARGETS
	juice-power
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
