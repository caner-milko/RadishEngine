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

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} UPPERCONFIG )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${UPPERCONFIG} ${CMAKE_SOURCE_DIR}/Builds/${OUTPUTCONFIG}/lib )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${UPPERCONFIG} ${CMAKE_SOURCE_DIR}/Builds/${OUTPUTCONFIG}/lib )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${UPPERCONFIG} ${CMAKE_SOURCE_DIR}/Builds/${OUTPUTCONFIG}/bin )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )
#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "PredefinedTargets")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Source)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT DeferredRenderer)