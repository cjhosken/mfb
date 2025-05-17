# OpenVDB v11.0 Configuration
set(OpenVDB_ROOT "$ENV{HOME}/.mfb/source/deps/openvdb")
set(OpenVDB_INCLUDE_DIR "${OpenVDB_ROOT}/include")
set(OpenVDB_LIB_DIR "${OpenVDB_ROOT}/lib")

find_package(TBB REQUIRED)
#find_package(Blosc REQUIRED)
find_package(OpenEXR REQUIRED)
find_package(Boost COMPONENTS
    thread chrono atomic date_time filesystem program_options system iostreams
)

if(OpenVDB_FOUND)
    message(STATUS "Found OpenVDB v11.0 via config file: ${OpenVDB_DIR}")
else()
    # Manual setup
    message(STATUS "OpenVDB config not found, setting up manually for v11.0")

    set(OpenVDB_LIBRARIES
        openvdb
        openvdb_static
    )

    foreach(lib ${OpenVDB_LIBRARIES})
        find_library(OpenVDB_${lib}_LIBRARY
            NAMES ${lib}
            PATHS ${OpenVDB_LIB_DIR}
            NO_DEFAULT_PATH
        )
        
        if(OpenVDB_${lib}_LIBRARY)
            add_library(OpenVDB::${lib} UNKNOWN IMPORTED)
            set_target_properties(OpenVDB::${lib} PROPERTIES
                IMPORTED_LOCATION "${OpenVDB_${lib}_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OpenVDB_INCLUDE_DIR}"
                INTERFACE_LINK_LIBRARIES 
                    "TBB::tbb;Boost::iostreams;Boost::system;Boost::thread;Boost::chrono"
            )
            list(APPEND OpenVDB_ALL_LIBRARIES OpenVDB::${lib})
        endif()
    endforeach()

    if(NOT OpenVDB_ALL_LIBRARIES)
        message(FATAL_ERROR "OpenVDB v11.0 libraries not found in ${OpenVDB_LIB_DIR}")
    endif()
endif()