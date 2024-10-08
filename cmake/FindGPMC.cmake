include(FindPackageHandleStandardArgs)

# Keep track of the original library suffixes to reset them later.
set(_gpmc_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})

# Look for .a or .lib libraries in case of a static library.
if(GPMC_USE_STATIC_LIBS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a .lib)
endif()

# Find libraries and headers.
find_library(GPMC_LIBRARY NAMES gpmc)
find_path(GPMC_INCLUDE_DIR NAMES gpmc.hpp)

# Windows (dynamic): Also find import libraries.
if(WIN32 AND NOT GPMC_USE_STATIC_LIBS)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .dll.a .lib)
    find_library(GPMC_IMPORT_LIBRARY NAMES gpmc)
endif()

# Reset library suffixes.
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_gpmc_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})

# Register the found package.
if(WIN32 AND NOT GPMC_USE_STATIC_LIBS)
    # Windows (dynamic): also require import libraries.
    find_package_handle_standard_args(GPMC REQUIRED_VARS GPMC_LIBRARY GPMC_IMPORT_LIBRARY GPMC_INCLUDE_DIR)
else()
    find_package_handle_standard_args(GPMC REQUIRED_VARS GPMC_LIBRARY GPMC_INCLUDE_DIR)
endif()

if(GPMC_FOUND)
    mark_as_advanced(GPMC_LIBRARY)
    mark_as_advanced(GPMC_IMPORT_LIBRARY)
    mark_as_advanced(GPMC_INCLUDE_DIR)

    # Create targets in case not already done.
    if(NOT TARGET GPMC::GPMC)
        if(GPMC_USE_STATIC_LIBS)
            add_library(GPMC::GPMC STATIC IMPORTED)
        else()
            add_library(GPMC::GPMC SHARED IMPORTED)
        endif()

        # Set library and include paths.
        set_target_properties(GPMC::GPMC PROPERTIES IMPORTED_LOCATION ${GPMC_LIBRARY})
        target_include_directories(GPMC::GPMC INTERFACE ${GPMC_INCLUDE_DIR})

        # Windows (dynamic): Also set import library.
        if(WIN32 AND NOT GPMC_USE_STATIC_LIBS)
            set_target_properties(GPMC::GPMC PROPERTIES IMPORTED_IMPLIB ${GPMC_IMPORT_LIBRARY})
        endif()
    endif()
endif()
