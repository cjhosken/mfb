if(embree_FOUND)
    message(STATUS "Found Embree via config file: ${embree_DIR}")
else()
    # Fall back to manual setup if config file isn't found
    message(STATUS "Embree not found via config, trying manual setup...")
    
    set(Embree_ROOT "$ENV{HOME}/.mfb/source/deps/embree")
    set(Embree_INCLUDE_DIR "${Embree_ROOT}/include")
    set(Embree_LIB_DIR "${Embree_ROOT}/lib")

    find_library(Embree_LIBRARY
        NAMES embree4 embree
        PATHS ${Embree_LIB_DIR}
        NO_DEFAULT_PATH
    )

    find_package(sycl CONFIG REQUIRED)

    include_directories(${Embree_INCLUDE_DIR})

    if(Embree_LIBRARY)
        # Create the imported target that CMake expects
        add_library(embree UNKNOWN IMPORTED)
        set_target_properties(embree PROPERTIES
            IMPORTED_LOCATION "${Embree_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES 
                "${Embree_INCLUDE_DIR};${Embree_INCLUDE_DIR}/embree4"
            INTERFACE_LINK_LIBRARIES
                "sycl"
            VERSION "${Embree_VERSION}"
        )

        message(STATUS "Found Embree manually: ${Embree_LIBRARY}")
    else()
        message(FATAL_ERROR "Embree library not found in ${Embree_LIB_DIR}")
    endif()
endif()