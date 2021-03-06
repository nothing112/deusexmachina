cmake_minimum_required(VERSION 3.8.2) # source_group TREE
project(DEM-Deps)

# TODO: Detour's DebugDraw.h etc headers must be namespaced into a directory
# TODO: rename Detour's DebugUtils target!
# TODO: link only Detour, DetourCrowd & DebugUtils, don't link Recast & DetourTileCache if not used by main code
# TODO: bullet variables must be with BULLET_ prefix even if we will build it separately
# TODO: set & hide from GUI unnecessary options of third-party CMake projects

# Directory structure is copied verbatim. Relative dest is relative to CMAKE_INSTALL_PREFIX.
#install(DIRECTORY src/ DESTINATION include/myproj FILES_MATCHING PATTERN "*.h")
#install(TARGETS ... EXPORT <export_name> ...)
#install(EXPORT <export_name> ...)
# The install() command generates a file, cmake_install.cmake, inside the build directory
# cmake -DBUILD_TYPE=Debug -P cmake_install.cmake
# cmake --build . --target install

#CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD ON
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(DEM_DEPS_DISABLE_CXX_EXCEPTIONS "Disable C++ exceptions" ON)
option(DEM_DEPS_DISABLE_CXX_RTTI "Disable C++ RTTI" ON)

include(DEMCMakeUtils.cmake)

if(MSVC AND DEM_DEPS_DISABLE_CXX_EXCEPTIONS)
	if(CMAKE_CXX_FLAGS MATCHES "/EHsc")
		string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	endif()
endif()

#???run python script to update cmake src lists?

# Dependency: Lua

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/lua" "${CMAKE_CURRENT_BINARY_DIR}/lua")

# Dependency: OGG & Theora

set(OGG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ogg")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ogg" "${CMAKE_CURRENT_BINARY_DIR}/ogg")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/theora" "${CMAKE_CURRENT_BINARY_DIR}/theora")

# Dependency: TinyXML2 (all unnecessary stuff is stripped out, so we don't use subdirectory's CMakeLists.txt)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tinyxml2" "${CMAKE_CURRENT_BINARY_DIR}/tinyxml2")

# Dependency: CEGUI

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/CEGUI/dependencies/lib")
	set(DEM_CEGUI_DEPS_FOUND TRUE)
else()
	find_package(PythonInterp)
	if(PYTHONINTERP_FOUND)
		execute_process(COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_cegui_deps.py" "${CMAKE_GENERATOR}"
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			RESULT_VARIABLE DEM_CEGUI_DEPS_BUILD_RESULT
		)
		if(${DEM_CEGUI_DEPS_BUILD_RESULT} EQUAL 0 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/CEGUI/dependencies/lib")
			set(DEM_CEGUI_DEPS_FOUND TRUE)
		else()
			message(WARNING "Error occured when building CEGUI dependencies, CEGUI will not be used:
			${DEM_CEGUI_DEPS_BUILD_RESULT}")
		endif()
	else()
		message(WARNING "CEGUI dependencies are not built and no python found to run build script. You need to do it manually to use CEGUI.")
	endif()
endif()

if(DEM_CEGUI_DEPS_FOUND)
	add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/CEGUI" "${CMAKE_CURRENT_BINARY_DIR}/CEGUI")
	get_all_targets(DEM_CEGUI_TARGETS "${CMAKE_CURRENT_SOURCE_DIR}/CEGUI")
	foreach(_TARGET_NAME IN LISTS DEM_CEGUI_TARGETS)
		set_target_properties(${_TARGET_NAME} PROPERTIES FOLDER "CEGUI")
		target_include_directories(${_TARGET_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/CEGUI/cegui/include" "${CMAKE_CURRENT_BINARY_DIR}/CEGUI/cegui/include")
		if(MSVC AND DEM_DEPS_DISABLE_CXX_EXCEPTIONS)
			target_compile_options(${_TARGET_NAME} PRIVATE "/wd4530") # Don't complain about exception unwinding
		endif()
	endforeach()
endif()

# Dependency: Bullet Physics

set(BUILD_CLSOCKET OFF CACHE INTERNAL "" FORCE)
set(BUILD_ENET OFF CACHE INTERNAL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/bullet" "${CMAKE_CURRENT_BINARY_DIR}/bullet")
get_all_targets(DEM_BULLET_TARGETS "${CMAKE_CURRENT_SOURCE_DIR}/bullet")
foreach(_TARGET_NAME IN LISTS DEM_BULLET_TARGETS)
	set_target_properties(${_TARGET_NAME} PROPERTIES FOLDER "bullet")
	target_include_directories(${_TARGET_NAME} INTERFACE "${BULLET_PHYSICS_SOURCE_DIR}/src")
endforeach()

# Dependency: Recast navigation libs

set(RECASTNAVIGATION_DEMO OFF CACHE INTERNAL "" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE INTERNAL "" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set(RECASTNAVIGATION_STATIC ON CACHE INTERNAL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/recastnavigation" "${CMAKE_CURRENT_BINARY_DIR}/recastnavigation")
get_all_targets(DEM_RECASTNAV_TARGETS "${CMAKE_CURRENT_SOURCE_DIR}/recastnavigation")
foreach(_TARGET_NAME IN LISTS DEM_RECASTNAV_TARGETS)
	set_target_properties(${_TARGET_NAME} PROPERTIES FOLDER "recastnavigation")
endforeach()

# Processing common settings for all dependency targets
# NB: CEGUI ignores DEM_DEPS_DISABLE_CXX_RTTI because it uses dynamic_cast

# HACK: empty generator expressions are used for config suffix suppression
set(DEM_DEPS_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/lib/$<$<BOOL:FALSE>:>")
set(DEM_DEPS_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin/$<$<BOOL:FALSE>:>")

get_all_targets(DEM_DEPS_TARGETS "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(_TARGET_NAME IN LISTS DEM_DEPS_TARGETS)

	set_target_properties(${_TARGET_NAME} PROPERTIES
		ARCHIVE_OUTPUT_DIRECTORY ${DEM_DEPS_LIB_DIR}
		LIBRARY_OUTPUT_DIRECTORY ${DEM_DEPS_LIB_DIR}
		RUNTIME_OUTPUT_DIRECTORY ${DEM_DEPS_BIN_DIR}
		DEBUG_POSTFIX "_d"
	)

	foreach(_CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
		string(TOUPPER "${_CONFIG}" _CONFIG)
		set_target_properties(${_TARGET_NAME} PROPERTIES
			ARCHIVE_OUTPUT_DIRECTORY_${_CONFIG} ${DEM_DEPS_LIB_DIR}
			LIBRARY_OUTPUT_DIRECTORY_${_CONFIG} ${DEM_DEPS_LIB_DIR}
			RUNTIME_OUTPUT_DIRECTORY_${_CONFIG} ${DEM_DEPS_BIN_DIR}
		)
	endforeach()

	# Disable C++ RTTI if requested
	if(MSVC AND DEM_DEPS_DISABLE_CXX_RTTI AND NOT ${_TARGET_NAME} IN_LIST DEM_CEGUI_TARGETS)
		target_compile_options(${_TARGET_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/GR->)
	endif()

endforeach()

# Package

#???try installable package? try relocatable package?

#???Create interface imported target with all other targets listed? No way to obtain deps list otherwise.

export(TARGETS ${DEM_DEPS_TARGETS} NAMESPACE DEMDeps:: FILE DEMDepsConfig.cmake)

