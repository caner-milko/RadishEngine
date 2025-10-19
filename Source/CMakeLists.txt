set(CMAKE_CXX_STANDARD 23)

function(rad_add_target TARGET_NAME TARGET_TYPE)
    # Usage:
    # rad_add_target(MyApp EXECUTABLE
    #     INCLUDES ${EXTERNAL_DIR}/stb
    #     DEPENDENCIES glfw glm
    # )

    set(options)
    set(oneValueArgs)
    set(multiValueArgs INCLUDES DEPENDENCIES)
    cmake_parse_arguments(RAD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Shaders")
	set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Include")

    file(GLOB_RECURSE SHADER_FILES "${SHADER_DIR}/*")
    file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "${SOURCE_DIR}/*")
    file(GLOB_RECURSE DEFAULT_INCLUDE_FILES CONFIGURE_DEPENDS "${INCLUDE_DIR}/*")

	message("INCLUDES: ${DEFAULT_INCLUDE_FILES}")

    if(TARGET_TYPE STREQUAL "EXECUTABLE")
        add_executable(${TARGET_NAME} WIN32 ${SRC_FILES} ${DEFAULT_INCLUDE_FILES} ${SHADER_FILES})
    elseif(TARGET_TYPE STREQUAL "STATIC")
        add_library(${TARGET_NAME} STATIC ${SRC_FILES} ${DEFAULT_INCLUDE_FILES} ${SHADER_FILES})
    elseif(TARGET_TYPE STREQUAL "SHARED")
        add_library(${TARGET_NAME} SHARED ${SRC_FILES} ${DEFAULT_INCLUDE_FILES} ${SHADER_FILES})
    elseif(TARGET_TYPE STREQUAL "MODULE")
        add_library(${TARGET_NAME} MODULE ${SRC_FILES} ${DEFAULT_INCLUDE_FILES} ${SHADER_FILES})
    else()
        message(FATAL_ERROR "Unknown TARGET_TYPE: ${TARGET_TYPE}")
    endif()

    # Force C++ as linker language
    set_target_properties(${TARGET_NAME} PROPERTIES LINKER_LANGUAGE CXX)

    # Shaders excluded from build but visible in VS
    foreach(SHADER ${SHADER_FILES})
        set_source_files_properties(SOURCE ${SHADER} PROPERTIES VS_SETTINGS "ExcludedFromBuild=true")
        set_source_files_properties(SOURCE ${SHADER} PROPERTIES VS_SHADER_MODEL "6.6")
    endforeach()

    # Organize sources in VS filters
    foreach(source IN LISTS SRC_FILES)
        get_filename_component(source_path "${source}" PATH)
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" source_path_compact "${source_path}")
        string(REPLACE "/" "\\" source_path_msvc "${source_path_compact}")
        source_group("${source_path_msvc}" FILES "${source}")
    endforeach()

	foreach(header IN LISTS DEFAULT_INCLUDE_FILES)
		get_filename_component(header_path "${header}" PATH)

		# Relative path to project root
		file(RELATIVE_PATH header_path_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${header_path}")

		# Remove target name immediately after Include/
		string(REGEX REPLACE "^Include/${TARGET_NAME}/?" "Include/" header_path_compact "${header_path_rel}")

		# Convert slashes to backslashes for VS filters
		string(REPLACE "/" "\\" header_path_msvc "${header_path_compact}")
		source_group("${header_path_msvc}" FILES "${header}")
	endforeach()

    foreach(shader IN LISTS SHADER_FILES)
        get_filename_component(shader_path "${shader}" PATH)
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "" shader_path_compact "${shader_path}")
        string(REPLACE "/" "\\" shader_path_msvc "${shader_path_compact}")
        source_group("${shader_path_msvc}" FILES "${shader}")
    endforeach()

    # Includes (always default + user-provided)
    target_include_directories(${TARGET_NAME} PUBLIC
        ${INCLUDE_DIR}
		${SHADER_DIR}
		${RAD_INCLUDES}
		PRIVATE
		${SOURCE_DIR}
		)

    # Dependencies
    if(RAD_DEPENDENCIES)
        target_link_libraries(${TARGET_NAME} PRIVATE ${RAD_DEPENDENCIES})
    endif()

    # Require C++23
    set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED YES
    )
endfunction()


add_subdirectory(radUtil)
add_subdirectory(radEngine)