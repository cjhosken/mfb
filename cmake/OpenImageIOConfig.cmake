
find_package(Imath CONFIG REQUIRED)
find_package(Boost COMPONENTS
    thread chrono atomic date_time filesystem program_options system iostreams
)

if(OpenImageIO_FOUND)
    message(STATUS "Found OpenImageIO via config file: ${OpenImageIO_DIR}")
else()
    # Fall back to manual setup if config file isn't found
    message(STATUS "OpenImageIO not found via config, trying manual setup...")
    
    set(OpenImageIO_ROOT "$ENV{HOME}/.mfb/source/deps/openimageio")
    set(OpenImageIO_INCLUDE_DIR "${OpenImageIO_ROOT}/include")
    set(OpenImageIO_LIB_DIR "${OpenImageIO_ROOT}/lib")

    find_library(OpenImageIO_LIBRARY
        NAMES OpenImageIO
        PATHS ${OpenImageIO_LIB_DIR}
        NO_DEFAULT_PATH
    )
    find_library(OpenImageIO_UTIL_LIBRARY
        NAMES OpenImageIO_Util
        PATHS ${OpenImageIO_LIB_DIR}
        NO_DEFAULT_PATH
    )

    if(OpenImageIO_LIBRARY AND OpenImageIO_UTIL_LIBRARY)
        # Create the imported targets that CMake expects
        add_library(OpenImageIO::OpenImageIO_Util UNKNOWN IMPORTED)
        set_target_properties(OpenImageIO::OpenImageIO_Util PROPERTIES
            IMPORTED_LOCATION "${OpenImageIO_UTIL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpenImageIO_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "OpenEXR::OpenEXR;OpenEXR::OpenEXRCore;Imath::Imath;Boost::thread;Boost::chrono;Boost::atomic"
        )

        add_library(OpenImageIO::OpenImageIO UNKNOWN IMPORTED)
        set_target_properties(OpenImageIO::OpenImageIO PROPERTIES
            IMPORTED_LOCATION "${OpenImageIO_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OpenImageIO_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${OpenImageIO_UTIL_LIBRARY};OpenEXR::OpenEXR;OpenEXR::OpenEXRCore;Imath::Imath;Boost::thread;Boost::chrono;Boost::atomic"
        )

        message(STATUS "Found OpenImageIO manually: ${OpenImageIO_LIBRARY}")
    else()
        message(FATAL_ERROR "OpenImageIO libraries not found in ${OpenImageIO_LIB_DIR}")
    endif()
endif()
