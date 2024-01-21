set(CMAKE_CXX_STANDARD 20)

project(DX12Lib)

set(INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Include")
set(SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Source")

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "${SOURCE_DIRECTORY}/*")
file(GLOB_RECURSE INCLUDE_FILES CONFIGURE_DEPENDS "${INCLUDE_DIRECTORY}/*")

add_library(${PROJECT_NAME} STATIC ${INCLUDE_FILES} ${SRC_FILES} ${RESOURCE_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE "-D${PROJECT_NAME}_EXPORTS")

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

#source_group_files("${INCLUDE_FILES}" "Include")
#source_group_files("${SRC_FILES}" "Source")

target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${INCLUDE_DIRECTORY}>)

add_dependencies(${PROJECT_NAME} D3D12MemoryAllocator tinyobjloader)
target_link_libraries( ${PROJECT_NAME} 
	PUBLIC d3d12.lib
	dxgi.lib
	dxguid.lib
	D3D12MemoryAllocator
	tinyobjloader
)