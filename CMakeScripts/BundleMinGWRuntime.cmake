# SPDX-License-Identifier: GPL-2.0-or-later
#
# Install-time script (Windows/MSYS2): copy every MinGW DLL that the installed executables and
# libraries load, directly or indirectly, into bin/.
#
# The fixed DLL list in InstallMSYS2.cmake can miss a dependency that only shows up on a clean
# Windows machine (a validation run failed with STATUS_DLL_NOT_FOUND). This script resolves the
# real dependency graph with objdump and bundles whatever is missing, logging each DLL it adds.
#
# Expects INKSVECTOR_MINGW_BIN to be set by a preceding install(CODE) rule.

if(NOT INKSVECTOR_MINGW_BIN)
    message(FATAL_ERROR "BundleMinGWRuntime: INKSVECTOR_MINGW_BIN is not set")
endif()

set(_prefix "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
set(_bin "${_prefix}/bin")
if(NOT EXISTS "${_bin}")
    # Component install without bin/ (e.g. translations only): nothing to do.
    return()
endif()

file(GLOB _executables "${_bin}/*.exe")
file(GLOB _libraries "${_bin}/*.dll")
if(NOT _executables AND NOT _libraries)
    return()
endif()

if(CMAKE_HOST_WIN32)
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "objdump")
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${INKSVECTOR_MINGW_BIN}/objdump.exe")
endif()

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES ${_executables}
    LIBRARIES ${_libraries}
    DIRECTORIES "${_bin}" "${INKSVECTOR_MINGW_BIN}"
    # API sets are provided by Windows itself.
    PRE_EXCLUDE_REGEXES "^[Aa][Pp][Ii]-[Mm][Ss]-" "^[Ee][Xx][Tt]-[Mm][Ss]-"
    RESOLVED_DEPENDENCIES_VAR _resolved
    UNRESOLVED_DEPENDENCIES_VAR _unresolved
    CONFLICTING_DEPENDENCIES_PREFIX _conflict
)

file(TO_CMAKE_PATH "${INKSVECTOR_MINGW_BIN}" _mingw_bin)
string(TOLOWER "${_mingw_bin}/" _mingw_bin)
set(_added 0)
foreach(_dependency IN LISTS _resolved)
    file(TO_CMAKE_PATH "${_dependency}" _path)
    string(TOLOWER "${_path}" _lower)
    string(FIND "${_lower}" "${_mingw_bin}" _position)
    if(NOT _position EQUAL 0)
        continue() # system DLL or a file already inside the package
    endif()
    get_filename_component(_name "${_path}" NAME)
    if(NOT EXISTS "${_bin}/${_name}")
        message(STATUS "Inksvector: bundling missing runtime DLL ${_name}")
        file(COPY "${_path}" DESTINATION "${_bin}")
        math(EXPR _added "${_added} + 1")
    endif()
endforeach()
message(STATUS "Inksvector: ${_added} missing runtime DLL(s) bundled")

foreach(_name IN LISTS _conflict_FILENAMES)
    message(STATUS "Inksvector: dependency ${_name} found in several places: ${_conflict_${_name}}")
endforeach()
if(_unresolved)
    message(STATUS "Inksvector: unresolved dependencies (expected to be provided by Windows): ${_unresolved}")
endif()
