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
    imgui/backends/imgui_impl_sdl2.cpp
)


add_library(imgui STATIC ${IMGUI_SOURCES})
target_link_libraries(imgui SDL2-static)
target_include_directories(imgui PUBLIC ${IMGUI_INCLUDE_DIR})

add_subdirectory(D3D12MemoryAllocator)

add_subdirectory(tinyobjloader)

set(SDL_ATOMIC OFF CACHE BOOL "" FORCE)
set(SDL_CPUINFO OFF CACHE BOOL "" FORCE)
set(SDL_EVENTS ON CACHE BOOL "" FORCE)
set(SDL_FILE OFF CACHE BOOL "" FORCE)
set(SDL_FILESYSTEM OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_LOCALE OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_TIMERS ON CACHE BOOL "" FORCE)
set(SDL_DLOPEN OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_LOADSO ON CACHE BOOL "" FORCE)
set(SDL_VIDEO ON  CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_STATIC_PIC ON CACHE BOOL "" FORCE)
set(SDL_DIRECTX OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_XINPUT ON CACHE BOOL "" FORCE)
set(SDL_RENDER_D3D OFF CACHE BOOL "" FORCE)
set(SDL_OFFSCREEN OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)

add_subdirectory(SDL)

add_subdirectory(DirectX-Headers)

set(ENTT_INCLUDE_NATVIS ON CACHE BOOL "Enable natvis for EnTT" FORCE)
add_subdirectory(entt)

add_subdirectory(glm)

set(FIDELITYFX_SPD_SHADERS	
FidelityFX-SPD/ffx-spd/ffx_a.h
FidelityFX-SPD/ffx-spd/ffx_spd.h
)

add_library(FidelityFX-SPD INTERFACE ${FIDELITYFX_SPD_SHADERS})
target_compile_definitions(FidelityFX-SPD INTERFACE FIDELITYFX_SPD_SHADER_INCLUDE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/FidelityFX-SPD/ffx-spd")

target_include_directories(FidelityFX-SPD INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/FidelityFX-SPD/ffx-spd)

foreach(SHADER ${FIDELITYFX_SPD_SHADERS})
	set_source_files_properties(${SHADER} PROPERTIES VS_TOOL_OVERRIDE "Text")
endforeach()

set_folder_all_targets(${CMAKE_CURRENT_SOURCE_DIR} "External")
