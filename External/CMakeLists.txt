#add_subdirectory(glm)

message("Processing imgui")
set(IMGUI_INCLUDE_DIR imgui imgui/backends)

set(IMGUI_SOURCES 
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_demo.cpp
    imgui/backends/imgui_impl_dx12.cpp
    imgui/backends/imgui_impl_win32.cpp
)


add_library(imgui STATIC ${IMGUI_SOURCES})
target_link_libraries(imgui PUBLIC glfw)
target_include_directories(imgui PUBLIC ${IMGUI_INCLUDE_DIR} ${GLFW_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS})

add_subdirectory(D3D12MemoryAllocator)

set_folder_all_targets(${CMAKE_CURRENT_SOURCE_DIR} "External")
