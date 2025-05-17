if(embree_FOUND)
    message(STATUS "Found sycl via config file: ${embree_DIR}")
else()
    # Fall back to manual setup if config file isn't found
    message(STATUS "sycl not found via config, trying manual setup...")
    
    set(sycl_ROOT "$ENV{HOME}/.mfb/source/deps/dpcpp")
    set(sycl_INCLUDE_DIR "${sycl_ROOT}/include")
    set(sycl_LIB_DIR "${sycl_ROOT}/lib")

    find_library(sycl_LIBRARY
        NAMES sycl
        PATHS ${sycl_LIB_DIR}
        NO_DEFAULT_PATH
    )

    if(sycl_LIBRARY)
        # Create the imported target that CMake expects
        add_library(sycl UNKNOWN IMPORTED)
        set_target_properties(sycl PROPERTIES
            IMPORTED_LOCATION "${sycl_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES 
                "${sycl_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES
                "${sycl_LIBRARY}"
        )

        message(STATUS "Found sycl manually: ${sycl_LIBRARY}")
    else()
        message(FATAL_ERROR "sycl library not found in ${sycl_LIB_DIR}")
    endif()
endif()

include_directories("$ENV{HOME}/.mfb/source/deps/random123/include")