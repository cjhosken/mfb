if(OpenEXR_FOUND)
    message(STATUS "Found OpenEXR via config file: ${OpenEXR_DIR}")
else()
    # Fall back to manual setup if config file isn't found
    message(STATUS "OpenEXR not found via config, trying manual setup...")
    
    set(OpenEXR_ROOT "$ENV{HOME}/.mfb/source/deps/openexr")
    set(OpenEXR_INCLUDE_DIR "${OpenEXR_ROOT}/include")
    set(OpenEXR_LIB_DIR "${OpenEXR_ROOT}/lib")

    # Find all required OpenEXR libraries
    find_library(OpenEXR_LIB OpenEXR PATHS ${OpenEXR_LIB_DIR} NO_DEFAULT_PATH)
    find_library(OpenEXR_CORE_LIB OpenEXRCore PATHS ${OpenEXR_LIB_DIR} NO_DEFAULT_PATH)
    find_library(OpenEXR_UTIL_LIB OpenEXRUtil PATHS ${OpenEXR_LIB_DIR} NO_DEFAULT_PATH)
    find_library(OpenEXR_ILMTHREAD_LIB IlmThread PATHS ${OpenEXR_LIB_DIR} NO_DEFAULT_PATH)
    find_library(OpenEXR_IEX_LIB Iex PATHS ${OpenEXR_LIB_DIR} NO_DEFAULT_PATH)

    if(OpenEXR_LIB AND OpenEXR_CORE_LIB AND OpenEXR_UTIL_LIB AND 
       OpenEXR_ILMTHREAD_LIB AND OpenEXR_IEX_LIB)
        # Create the imported targets that CMake expects
        add_library(OpenEXR::OpenEXR UNKNOWN IMPORTED)
        set_target_properties(OpenEXR::OpenEXR PROPERTIES
            IMPORTED_LOCATION "${OpenEXR_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpenEXR_INCLUDE_DIR}"
        )

        add_library(OpenEXR::OpenEXRCore UNKNOWN IMPORTED)
        set_target_properties(OpenEXR::OpenEXRCore PROPERTIES
            IMPORTED_LOCATION "${OpenEXR_CORE_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpenEXR_INCLUDE_DIR}"
        )

        add_library(OpenEXR::OpenEXRUtil UNKNOWN IMPORTED)
        set_target_properties(OpenEXR::OpenEXRUtil PROPERTIES
            IMPORTED_LOCATION "${OpenEXR_UTIL_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpenEXR_INCLUDE_DIR}"
        )

        add_library(OpenEXR::IlmThread UNKNOWN IMPORTED)
        set_target_properties(OpenEXR::IlmThread PROPERTIES
            IMPORTED_LOCATION "${OpenEXR_ILMTHREAD_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpenEXR_INCLUDE_DIR}"
        )

        add_library(OpenEXR::Iex UNKNOWN IMPORTED)
        set_target_properties(OpenEXR::Iex PROPERTIES
            IMPORTED_LOCATION "${OpenEXR_IEX_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpenEXR_INCLUDE_DIR}"
        )

        message(STATUS "Found OpenEXR manually")
    else()
        message(FATAL_ERROR "One or more OpenEXR libraries not found in ${OpenEXR_LIB_DIR}")
    endif()
endif()