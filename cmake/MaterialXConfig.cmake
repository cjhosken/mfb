# MaterialX v1.38.8 Configuration
set(MaterialX_ROOT "$ENV{HOME}/.mfb/source/deps/materialx")
set(MaterialX_INCLUDE_DIR "${MaterialX_ROOT}/include")
set(MaterialX_LIB_DIR "${MaterialX_ROOT}/lib")

if(MaterialX_FOUND)
    message(STATUS "Found MaterialX v1.38.8 via config file: ${MaterialX_DIR}")
else()
    # Manual setup
    message(STATUS "MaterialX config not found, setting up manually for v1.38.8")

    set(MaterialX_LIBRARIES
        MaterialXCore
        MaterialXFormat
        MaterialXGenShader
        MaterialXRender
        MaterialXRenderGlsl
    )

    foreach(lib ${MaterialX_LIBRARIES})
        find_library(MaterialX_${lib}_LIBRARY
            NAMES ${lib}
            PATHS ${MaterialX_LIB_DIR}
            NO_DEFAULT_PATH
        )
        
        if(MaterialX_${lib}_LIBRARY)
            add_library(MaterialX::${lib} UNKNOWN IMPORTED)
            set_target_properties(MaterialX::${lib} PROPERTIES
                IMPORTED_LOCATION "${MaterialX_${lib}_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${MaterialX_INCLUDE_DIR}"
                INTERFACE_LINK_LIBRARIES ""
            )
            list(APPEND MaterialX_ALL_LIBRARIES MaterialX::${lib})
        endif()
    endforeach()

    if(NOT MaterialX_ALL_LIBRARIES)
        message(FATAL_ERROR "MaterialX v1.38.8 libraries not found in ${MaterialX_LIB_DIR}")
    endif()
endif()