cmake_minimum_required(VERSION 3.20.0)

function (get_all_cmake_targets out_var current_dir)
    get_property(targets DIRECTORY ${current_dir} PROPERTY BUILDSYSTEM_TARGETS)
    get_property(subdirs DIRECTORY ${current_dir} PROPERTY SUBDIRECTORIES)

    foreach(subdir ${subdirs})
        get_all_cmake_targets(subdir_targets ${subdir})
        list(APPEND targets ${subdir_targets})
    endforeach()

    set(${out_var} ${targets} PARENT_SCOPE)
endfunction()

macro(set_folder_all_targets dir folder_name)
	get_all_cmake_targets(TARGETS ${dir})
	foreach(TARGET ${TARGETS})
		get_target_property(FOLD ${TARGET} FOLDER)
		if(${FOLD} STREQUAL "FOLD-NOTFOUND")
			set(FOLD_NAME ${folder_name})
		else()
			set(FOLD_NAME "${folder_name}/${FOLD}")
		endif()
		set_target_properties(${TARGET} PROPERTIES FOLDER ${FOLD_NAME})
	endforeach()
endmacro()

macro(source_group_files FILES GROUP_NAME)
	foreach(file ${FILES})
		get_filename_component(directory "${file}" DIRECTORY)
		string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" directory_compact "${directory}")
		source_group("${GROUP_NAME}" FILES "${file}")
	endforeach()
endmacro()

project(DX12Playground LANGUAGES CXX)


set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) # Output directory for static lib (.LIB)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for shared lib (.DLL)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for executables (.EXE)
#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
#set(CMAKE_CXX_STANDARD 20)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "PredefinedTargets")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Source)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT DeferredRenderer)