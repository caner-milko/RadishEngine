set(CMAKE_CXX_STANDARD 20)

project(DXPG)

set(INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Include")
set(SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(SHADER_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Shaders")

file(GLOB_RECURSE SHADER_FILES "${SHADER_DIRECTORY}/*")
file(GLOB_RECURSE VERTEX_SHADER_FILES "${SHADER_DIRECTORY}/Vertex/*")
file(GLOB_RECURSE PIXEL_SHADER_FILES "${SHADER_DIRECTORY}/Pixel/*")
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "${SOURCE_DIRECTORY}/*")
file(GLOB_RECURSE INCLUDE_FILES CONFIGURE_DEPENDS "${INCLUDE_DIRECTORY}/*")

add_executable(${PROJECT_NAME} WIN32 ${INCLUDE_FILES} ${SRC_FILES} ${SHADER_FILES})

foreach(VERTEX_SHADER ${VERTEX_SHADER_FILES})
	set_source_files_properties(${VERTEX_SHADER} PROPERTIES VS_SHADER_TYPE Vertex)
endforeach()

foreach(PIXEL_SHADER ${PIXEL_SHADER_FILES})
	set_source_files_properties(${PIXEL_SHADER} PROPERTIES VS_SHADER_TYPE Pixel)
endforeach()

foreach(SHADER ${SHADER_FILES})
	set_source_files_properties(${SHADER} PROPERTIES VS_SHADER_MODEL 5.1)
endforeach()

foreach(source IN LISTS SRC_FILES)
	get_filename_component(source_path "${source}" PATH)
	string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" source_path_compact "${source_path}")
	string(REPLACE "/" "\\" source_path_msvc "${source_path_compact}")
	source_group("${source_path_msvc}" FILES "${source}")
endforeach()

foreach(header IN LISTS INCLUDE_FILES)
	get_filename_component(header_path "${header}" PATH)
	string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" header_path_compact "${header_path}")
	string(REPLACE "/" "\\" header_path_msvc "${header_path_compact}")
	source_group("${header_path_msvc}" FILES "${header}")
endforeach()

foreach(shader IN LISTS SHADER_FILES)
	get_filename_component(shader_path "${shader}" PATH)
	string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" shader_path_compact "${shader_path}")
	string(REPLACE "/" "\\" shader_path_msvc "${shader_path_compact}")
	source_group("${shader_path_msvc}" FILES "${shader}")
endforeach()

target_compile_definitions(${PROJECT_NAME} PRIVATE DXPG_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Assets")

target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${INCLUDE_DIRECTORY}>)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    d3d12 dxgi dxcompiler SDL2-static SDL2::SDL2main imgui DirectX-Headers
)