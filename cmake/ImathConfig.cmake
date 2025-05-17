if(Imath_FOUND)
    message(STATUS "Found Imath via config file: ${Imath_DIR}")
else()
    # Fall back to manual setup if config file isn't found
    message(STATUS "Imath not found via config, trying manual setup...")
    
    set(Imath_ROOT "$ENV{HOME}/.mfb/source/deps/imath")
    set(Imath_INCLUDE_DIR "${Imath_ROOT}/include")
    set(Imath_LIB_DIR "${Imath_ROOT}/lib")

    find_library(Imath_LIBRARY 
        NAMES Imath
        PATHS ${Imath_LIB_DIR}
        NO_DEFAULT_PATH
    )

    if(Imath_LIBRARY)
        # Create the imported target that CMake expects
        add_library(Imath::Imath UNKNOWN IMPORTED)
        set_target_properties(Imath::Imath PROPERTIES
            IMPORTED_LOCATION "${Imath_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Imath_INCLUDE_DIR};${Imath_INCLUDE_DIR}/Imath" 
        )
        
        message(STATUS "Found Imath manually: ${Imath_LIBRARY}")
    else()
        message(FATAL_ERROR "Imath library not found in ${Imath_LIB_DIR}")
    endif()
endif()