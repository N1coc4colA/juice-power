cmake_minimum_required(VERSION 3.20)
project(imgui)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

set(CMAKE_DEBUG_POSTFIX d)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED)

set(ROOT ${IMGUI_DIR})

add_library(
imgui STATIC
	${ROOT}/imgui.cpp
	${ROOT}/imgui.h
	${ROOT}/imstb_rectpack.h
	${ROOT}/imstb_textedit.h
	${ROOT}/imstb_truetype.h
	${ROOT}/imgui_demo.cpp
	${ROOT}/imgui_draw.cpp
	${ROOT}/imgui_internal.h
	${ROOT}/imgui_tables.cpp
	${ROOT}/imgui_widgets.cpp
)

target_include_directories(
imgui
	PUBLIC
		$<BUILD_INTERFACE:${ROOT}>
		$<INSTALL_INTERFACE:include>
)

set(INSTALL_TARGETS imgui)
set(INSTALL_HEADERS ${ROOT}/imgui.h ${ROOT}/imconfig.h)

foreach(BACKEND sdl3 vulkan)
	set(NAME imgui_impl_${BACKEND})
	set(HEADER ${ROOT}/backends/${NAME}.h)
	add_library(${NAME} STATIC ${ROOT}/backends/${NAME}.cpp ${HEADER})
	target_link_libraries(${NAME} PUBLIC imgui)
	target_include_directories(${NAME} PUBLIC
		$<BUILD_INTERFACE:${ROOT}/backends>
		$<INSTALL_INTERFACE:include>)
	LIST(APPEND INSTALL_TARGETS ${NAME})
	LIST(APPEND INSTALL_HEADERS ${HEADER})
endforeach()


if(SDL3_FOUND)
	target_link_libraries(imgui_impl_sdl3 PRIVATE SDL3::SDL3)
endif()
if(Vulkan_FOUND)
	target_link_libraries(imgui_impl_vulkan PRIVATE Vulkan::Vulkan)
endif()


install(
	TARGETS ${INSTALL_TARGETS}
	EXPORT imgui-targets
	DESTINATION lib
)
install(
	EXPORT imgui-targets
	FILE imgui-config.cmake
	NAMESPACE imgui::
	DESTINATION lib/cmake/imgui
)
install(
	FILES ${INSTALL_HEADERS}
	DESTINATION include
)

