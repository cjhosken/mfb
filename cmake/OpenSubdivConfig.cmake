# OpenSubdiv v3.6.0 Configuration
set(OpenSubdiv_ROOT "$ENV{HOME}/.mfb/source/deps/opensubdiv")
set(OpenSubdiv_INCLUDE_DIR "${OpenSubdiv_ROOT}/include")
set(OpenSubdiv_LIB_DIR "${OpenSubdiv_ROOT}/lib")

if(OpenSubdiv_FOUND)
    message(STATUS "Found OpenSubdiv v3.6.0 via config file: ${OpenSubdiv_DIR}")
else()
    # Manual setup
    message(STATUS "OpenSubdiv config not found, setting up manually for v3.6.0")

    set(OpenSubdiv_LIBRARIES
        osdCPU
        osdGPU
    )

    foreach(lib ${OpenSubdiv_LIBRARIES})
        find_library(OpenSubdiv_${lib}_LIBRARY
            NAMES ${lib}
            PATHS ${OpenSubdiv_LIB_DIR}
            NO_DEFAULT_PATH
        )
        
        if(OpenSubdiv_${lib}_LIBRARY)
            add_library(OpenSubdiv::${lib} UNKNOWN IMPORTED)
            set_target_properties(OpenSubdiv::${lib} PROPERTIES
                IMPORTED_LOCATION "${OpenSubdiv_${lib}_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OpenSubdiv_INCLUDE_DIR}"
            )
            list(APPEND OpenSubdiv_ALL_LIBRARIES OpenSubdiv::${lib})
        endif()
    endforeach()

    if(NOT OpenSubdiv_ALL_LIBRARIES)
        message(FATAL_ERROR "OpenSubdiv v3.6.0 libraries not found in ${OpenSubdiv_LIB_DIR}")
    endif()
endif()