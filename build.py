#!/usr/bin/env python3
import os
import sys
import shutil
import subprocess
import argparse

def log(msg):
    print(f"==> {msg}", flush=True)

def check_requirements():
    tools = ["git", "cmake"]
    missing = []
    for tool in tools:
        if not shutil.which(tool):
            missing.append(tool)
    if missing:
        log(f"Error: Missing required system tools: {', '.join(missing)}")
        sys.exit(1)

def install_system_deps():
    packages = [
        "libmicrohttpd-dev",
        "libcurl4-openssl-dev",
        "libcppunit-dev",
        "libjpeg-dev",
        "bison",
        "flex",
        "liblua5.4-dev",
        "lua5.4",
        "liblog4cplus-dev",
    ]
    log("Installing system dependencies...")
    run_cmd(["sudo", "apt-get", "update", "-qq"])
    run_cmd(["sudo", "apt-get", "install", "-y", "-qq"] + packages)

def run_cmd(cmd, cwd=None):
    log(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if result.returncode != 0:
        print(result.stdout)
        log(f"Command failed with exit code {result.returncode}")
        sys.exit(result.returncode)
    return result.stdout


DEPS_CMAKELists = """\
cmake_minimum_required (VERSION 3.23.1)
project(mfb_third_party)

include(ExternalProject)
include(ProcessorCount)
ProcessorCount(NPROC)
if(NPROC EQUAL 0)
    set(NPROC 2)
endif()

set(InstallRoot ./deps/ CACHE FILEPATH "Install root for dependencies")

ExternalProject_Add(JsonCpp
    GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
    GIT_TAG 5defb4ed1a4293b8e2bf641e16b156fb9de498cc # 1.9.5
    INSTALL_DIR ${InstallRoot}
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        -DBUILD_SHARED_LIBS=1
        -DPYTHON_EXECUTABLE=/usr/bin/python3
        -DJSONCPP_LIB_BUILD_SHARED:BOOL=ON
        -DJSONCPP_WITH_PKGCONFIG_SUPPORT=OFF
)
set(CHAIN JsonCpp)

ExternalProject_Add(Random123
    GIT_REPOSITORY https://github.com/DEShawResearch/random123
    GIT_TAG 726a093cd9a73f3ec3c8d7a70ff10ed8efec8d13 # v1.14.0 (internally we use 1.08.3)
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND make install-include prefix=${InstallRoot}
    DEPENDS ${CHAIN}
)
set(CHAIN Random123)

ExternalProject_Add(ISPC
    URL https://github.com/ispc/ispc/releases/download/v1.21.0/ispc-v1.21.0-linux.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND cp bin/ispc ${InstallRoot}/bin
    DEPENDS ${CHAIN}
)
set(CHAIN ISPC)

ExternalProject_Add(Boost
    URL https://archives.boost.io/release/1.82.0/source/boost_1_82_0.tar.gz
    INSTALL_DIR ${InstallRoot}
    CONFIGURE_COMMAND env CXXFLAGS=-I/usr/include/python3.14 ./bootstrap.sh --prefix=<INSTALL_DIR> --with-python=/usr/bin/python3 --with-libraries=chrono,date_time,filesystem,program_options,system,python,regex,thread
    BUILD_COMMAND sh -c "./b2 -j${NPROC} link=shared runtime-link=shared --keep-going || true"
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND sh -c "cp -f stage/lib/*.so* <INSTALL_DIR>/lib/ && mkdir -p <INSTALL_DIR>/lib/cmake && cp -rf stage/lib/cmake/* <INSTALL_DIR>/lib/cmake/ && cp -rf boost <INSTALL_DIR>/ && sed -i 's|/../../../|/../../|g' <INSTALL_DIR>/lib/cmake/boost_*/*.cmake"
    DEPENDS ${CHAIN}
)
set(CHAIN Boost)
"""

PXR_CONFIG = """\
set(PXR_MAJOR_VERSION "0")
set(PXR_MINOR_VERSION "25")
set(PXR_PATCH_VERSION "08")
set(PXR_VERSION "2508")

get_filename_component(_PXR_CONFIG_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(USD_ROOT "${_PXR_CONFIG_DIR}/../source/blender/usd" ABSOLUTE)
set(PXR_usd_ms_LIBRARY "${USD_ROOT}/lib/libusd_ms.so")
set(PXR_INCLUDE_DIRS "${USD_ROOT}/include" CACHE PATH "Path to the pxr include directory")

add_library(arch INTERFACE IMPORTED)
set_target_properties(arch PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(tf INTERFACE IMPORTED)
set_target_properties(tf PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(gf INTERFACE IMPORTED)
set_target_properties(gf PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(vt INTERFACE IMPORTED)
set_target_properties(vt PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(ndr INTERFACE IMPORTED)
set_target_properties(ndr PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(sdr INTERFACE IMPORTED)
set_target_properties(sdr PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(sdf INTERFACE IMPORTED)
set_target_properties(sdf PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(usd INTERFACE IMPORTED)
set_target_properties(usd PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(ar INTERFACE IMPORTED)
set_target_properties(ar PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(plug INTERFACE IMPORTED)
set_target_properties(plug PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(trace INTERFACE IMPORTED)
set_target_properties(trace PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(work INTERFACE IMPORTED)
set_target_properties(work PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(hf INTERFACE IMPORTED)
set_target_properties(hf PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(hd INTERFACE IMPORTED)
set_target_properties(hd PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(usdGeom INTERFACE IMPORTED)
set_target_properties(usdGeom PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(usdImaging INTERFACE IMPORTED)
set_target_properties(usdImaging PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(usdLux INTERFACE IMPORTED)
set_target_properties(usdLux PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(usdShade INTERFACE IMPORTED)
set_target_properties(usdShade PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(pxOsd INTERFACE IMPORTED)
set_target_properties(pxOsd PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(cameraUtil INTERFACE IMPORTED)
set_target_properties(cameraUtil PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(pcp INTERFACE IMPORTED)
set_target_properties(pcp PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(usdUtils INTERFACE IMPORTED)
set_target_properties(usdUtils PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(usdVol INTERFACE IMPORTED)
set_target_properties(usdVol PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(usdSkel INTERFACE IMPORTED)
set_target_properties(usdSkel PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

add_library(usdRender INTERFACE IMPORTED)
set_target_properties(usdRender PROPERTIES IMPORTED_LOCATION "${PXR_usd_ms_LIBRARY}")

set(PXR_LIBRARIES "${PXR_usd_ms_LIBRARY}")

message(STATUS "PXR_INCLUDE_DIRS: ${PXR_INCLUDE_DIRS}")
message(STATUS "PXR_LIBRARIES: ${PXR_LIBRARIES}")

include_directories(${PXR_INCLUDE_DIRS})
link_libraries(${PXR_LIBRARIES})
"""

PXR_CONFIG_VERSION = """\
set(PACKAGE_VERSION "0.25.8")

if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)

    if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()
"""

EMBREE_CONFIG = """\
cmake_minimum_required(VERSION 3.15)

if(NOT TARGET Embree::Embree)
    add_library(Embree::Embree SHARED IMPORTED)

    set_target_properties(Embree::Embree PROPERTIES
        IMPORTED_LOCATION
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/embree/lib/libembree4.so"
        INTERFACE_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/embree/include"
        INTERFACE_LINK_LIBRARIES
            Threads::Threads
    )
endif()

set(Embree_FOUND TRUE)
"""

EMBREE_CONFIG_VERSION = """\
set(PACKAGE_VERSION "4.4.0")

if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()
"""

OPENEXR_CONFIG = """\
cmake_minimum_required(VERSION 3.15)

include(CMakeFindDependencyMacro)

if(NOT TARGET Imath::Imath)
    add_library(Imath::Imath SHARED IMPORTED)
    set_target_properties(Imath::Imath PROPERTIES
        IMPORTED_LOCATION
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/imath/lib/libImath.so"
        INTERFACE_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/imath/include;${CMAKE_CURRENT_LIST_DIR}/../source/blender/imath/include/Imath"
    )
endif()

if(NOT TARGET OpenEXR::Iex)
    add_library(OpenEXR::Iex SHARED IMPORTED)
    set_target_properties(OpenEXR::Iex PROPERTIES
        IMPORTED_LOCATION
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/openexr/lib/libIex.so"
        INTERFACE_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/openexr/include"
    )
endif()

if(NOT TARGET OpenEXR::IlmThread)
    add_library(OpenEXR::IlmThread SHARED IMPORTED)
    set_target_properties(OpenEXR::IlmThread PROPERTIES
        IMPORTED_LOCATION
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/openexr/lib/libIlmThread.so"
        INTERFACE_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/openexr/include"
    )
endif()

if(NOT TARGET OpenEXR::OpenEXRCore)
    add_library(OpenEXR::OpenEXRCore SHARED IMPORTED)
    set_target_properties(OpenEXR::OpenEXRCore PROPERTIES
        IMPORTED_LOCATION
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/openexr/lib/libOpenEXRCore.so"
        INTERFACE_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/openexr/include"
        INTERFACE_LINK_LIBRARIES
            "OpenEXR::Iex;OpenEXR::IlmThread"
    )
endif()

if(NOT TARGET OpenEXR::OpenEXRUtil)
    add_library(OpenEXR::OpenEXRUtil SHARED IMPORTED)
    set_target_properties(OpenEXR::OpenEXRUtil PROPERTIES
        IMPORTED_LOCATION
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/openexr/lib/libOpenEXRUtil.so"
        INTERFACE_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/openexr/include"
        INTERFACE_LINK_LIBRARIES
            OpenEXR::OpenEXRCore
    )
endif()

if(NOT TARGET OpenEXR::OpenEXR)
    add_library(OpenEXR::OpenEXR SHARED IMPORTED)
    set_target_properties(OpenEXR::OpenEXR PROPERTIES
        IMPORTED_LOCATION
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/openexr/lib/libOpenEXR.so"
        INTERFACE_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/openexr/include"
    )
endif()
"""

OPENEXR_CONFIG_VERSION = """\
set(PACKAGE_VERSION "3.3.5")

if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()
"""

OPENIMAGEIO_CONFIG = """\
cmake_minimum_required(VERSION 3.15)

find_package(OpenEXR REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

if(NOT TARGET OpenImageIO::OpenImageIO)
    add_library(OpenImageIO::OpenImageIO SHARED IMPORTED)
    set_target_properties(OpenImageIO::OpenImageIO PROPERTIES
        IMPORTED_LOCATION
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/openimageio/lib/libOpenImageIO.so"
        INTERFACE_INCLUDE_DIRECTORIES
            "${CMAKE_CURRENT_LIST_DIR}/../source/blender/openimageio/include"
        INTERFACE_LINK_LIBRARIES
            "OpenEXR::OpenEXR;ZLIB::ZLIB;Threads::Threads;dl"
    )
endif()

set(OpenImageIO_FOUND TRUE)
"""

OPENIMAGEIO_CONFIG_VERSION = """\
set(PACKAGE_VERSION "3.0.9.1")

if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()
"""


def write_cmake_configs(script_dir):
    cmake_dir = os.path.join(script_dir, "cmake")
    os.makedirs(cmake_dir, exist_ok=True)

    configs = {
        "pxrConfig.cmake": PXR_CONFIG,
        "pxrConfigVersion.cmake": PXR_CONFIG_VERSION,
        "EmbreeConfig.cmake": EMBREE_CONFIG,
        "EmbreeConfigVersion.cmake": EMBREE_CONFIG_VERSION,
        "OpenEXRConfig.cmake": OPENEXR_CONFIG,
        "OpenEXRConfigVersion.cmake": OPENEXR_CONFIG_VERSION,
        "OpenImageIOConfig.cmake": OPENIMAGEIO_CONFIG,
        "OpenImageIOConfigVersion.cmake": OPENIMAGEIO_CONFIG_VERSION,
    }

    for name, content in configs.items():
        path = os.path.join(cmake_dir, name)
        with open(path, "w") as f:
            f.write(content)
        log(f"Wrote {name}")


def write_deps_cmake(script_dir):
    path = os.path.join(script_dir, "CMakeLists.txt")
    with open(path, "w") as f:
        f.write(DEPS_CMAKELists)
    log("Wrote deps CMakeLists.txt")


def _get_boost_python_config():
    pyver = f"{sys.version_info.major}{sys.version_info.minor}"
    return f"""\
set(boost_python_FOUND TRUE)
set(boost_python_VERSION "1.82.0")
set(boost_python_VERSION_STRING "1.82.0")
if(NOT TARGET Boost::python)
  add_library(Boost::python SHARED IMPORTED)
  set_target_properties(Boost::python PROPERTIES
    IMPORTED_LOCATION "${{_BOOST_LIBDIR}}/libboost_python{pyver}.so.1.82.0"
    INTERFACE_INCLUDE_DIRECTORIES "${{_BOOST_INCLUDEDIR}}"
  )
endif()
"""

BOOST_PYTHON_CONFIG_VERSION = """\
set(PACKAGE_VERSION "1.82.0")
if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()
"""


def write_boost_python_config(deps_dir):
    cmake_dir = os.path.join(deps_dir, "lib", "cmake", "boost_python-1.82.0")
    os.makedirs(cmake_dir, exist_ok=True)
    with open(os.path.join(cmake_dir, "boost_pythonConfig.cmake"), "w") as f:
        f.write(_get_boost_python_config())
    with open(os.path.join(cmake_dir, "boost_pythonConfigVersion.cmake"), "w") as f:
        f.write(BOOST_PYTHON_CONFIG_VERSION)
    log("Wrote boost_python cmake config")


NDR_HEADERS = {
    "api.h": '''\
//
// Copyright 2018 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//

#ifndef PXR_USD_NDR_API_H
#define PXR_USD_NDR_API_H

#include "pxr/base/arch/export.h"

#if defined(PXR_STATIC)
#   define NDR_API
#   define NDR_API_TEMPLATE_CLASS(...)
#   define NDR_API_TEMPLATE_STRUCT(...)
#   define NDR_LOCAL
#else
#   if defined(NDR_EXPORTS)
#       define NDR_API ARCH_EXPORT
#       define NDR_API_TEMPLATE_CLASS(...) ARCH_EXPORT_TEMPLATE(class, __VA_ARGS__)
#       define NDR_API_TEMPLATE_STRUCT(...) ARCH_EXPORT_TEMPLATE(struct, __VA_ARGS__)
#   else
#       define NDR_API ARCH_IMPORT
#       define NDR_API_TEMPLATE_CLASS(...) ARCH_IMPORT_TEMPLATE(class, __VA_ARGS__)
#       define NDR_API_TEMPLATE_STRUCT(...) ARCH_IMPORT_TEMPLATE(struct, __VA_ARGS__)
#   endif
#   define NDR_LOCAL ARCH_HIDDEN
#endif

#endif
''',
    "declare.h": '''\
//
// Copyright 2018 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//

#ifndef PXR_USD_NDR_DECLARE_H
#define PXR_USD_NDR_DECLARE_H

#include "pxr/pxr.h"
#include "pxr/usd/ndr/api.h"
#include "pxr/base/tf/token.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

class NdrNode;
class NdrProperty;
class SdfValueTypeName;

typedef TfToken NdrIdentifier;
typedef TfToken::HashFunctor NdrIdentifierHashFunctor;
inline const std::string& NdrGetIdentifierString(const NdrIdentifier& id) { return id.GetString(); }
typedef std::vector<NdrIdentifier> NdrIdentifierVec;
typedef std::unordered_set<NdrIdentifier, NdrIdentifierHashFunctor> NdrIdentifierSet;
typedef std::vector<TfToken> NdrTokenVec;
typedef std::unordered_map<TfToken, std::string, TfToken::HashFunctor> NdrTokenMap;
typedef NdrProperty* NdrPropertyPtr;
typedef NdrProperty const* NdrPropertyConstPtr;
typedef std::unique_ptr<NdrProperty> NdrPropertyUniquePtr;
typedef std::vector<NdrPropertyUniquePtr> NdrPropertyUniquePtrVec;
typedef std::unordered_map<TfToken, NdrPropertyConstPtr, TfToken::HashFunctor> NdrPropertyPtrMap;
typedef NdrNode* NdrNodePtr;
typedef NdrNode const* NdrNodeConstPtr;
typedef std::unique_ptr<NdrNode> NdrNodeUniquePtr;
typedef std::vector<NdrNodeConstPtr> NdrNodeConstPtrVec;
typedef std::vector<NdrNodeUniquePtr> NdrNodeUniquePtrVec;
typedef std::vector<std::string> NdrStringVec;
typedef std::pair<TfToken, TfToken> NdrOption;
typedef std::vector<NdrOption> NdrOptionVec;
typedef std::unordered_set<std::string> NdrStringSet;

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_NDR_DECLARE_H
''',
    "discoveryPlugin.h": '''\
//
// Copyright 2018 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//

#ifndef PXR_USD_NDR_DISCOVERY_PLUGIN_H
#define PXR_USD_NDR_DISCOVERY_PLUGIN_H

#include "pxr/pxr.h"
#include "pxr/usd/ndr/api.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/tf/weakBase.h"
#include "pxr/usd/ndr/declare.h"
#include "pxr/usd/ndr/nodeDiscoveryResult.h"

PXR_NAMESPACE_OPEN_SCOPE

#define NDR_REGISTER_DISCOVERY_PLUGIN(DiscoveryPluginClass)                   \\
TF_REGISTRY_FUNCTION(TfType)                                                  \\
{                                                                             \\
    TfType::Define<DiscoveryPluginClass, TfType::Bases<NdrDiscoveryPlugin>>() \\
        .SetFactory<NdrDiscoveryPluginFactory<DiscoveryPluginClass>>();       \\
}

TF_DECLARE_WEAK_AND_REF_PTRS(NdrDiscoveryPluginContext);

class NdrDiscoveryPluginContext : public TfRefBase, public TfWeakBase
{
public:
    NDR_API
    virtual ~NdrDiscoveryPluginContext() = default;
    NDR_API
    virtual TfToken GetSourceType(const TfToken& discoveryType) const = 0;
};

TF_DECLARE_WEAK_AND_REF_PTRS(NdrDiscoveryPlugin);

class NdrDiscoveryPlugin : public TfRefBase, public TfWeakBase
{
public:
    using Context = NdrDiscoveryPluginContext;
    NDR_API
    NdrDiscoveryPlugin();
    NDR_API
    virtual ~NdrDiscoveryPlugin();
    NDR_API
    virtual NdrNodeDiscoveryResultVec DiscoverNodes(const Context&) = 0;
    NDR_API
    virtual const NdrStringVec& GetSearchURIs() const = 0;
};

class NdrDiscoveryPluginFactoryBase : public TfType::FactoryBase
{
public:
    NDR_API
    virtual NdrDiscoveryPluginRefPtr New() const = 0;
};

template <class T>
class NdrDiscoveryPluginFactory : public NdrDiscoveryPluginFactoryBase
{
public:
    NdrDiscoveryPluginRefPtr New() const override
    {
        return TfCreateRefPtr(new T);
    }
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_NDR_DISCOVERY_PLUGIN_H
''',
    "parserPlugin.h": '''\
//
// Copyright 2018 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//

#ifndef PXR_USD_NDR_PARSER_PLUGIN_H
#define PXR_USD_NDR_PARSER_PLUGIN_H

#include "pxr/pxr.h"
#include "pxr/usd/ndr/api.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/tf/weakBase.h"
#include "pxr/base/tf/weakPtr.h"
#include "pxr/usd/ndr/declare.h"

PXR_NAMESPACE_OPEN_SCOPE

struct NdrNodeDiscoveryResult;

#define NDR_REGISTER_PARSER_PLUGIN(ParserPluginClass)                   \\
TF_REGISTRY_FUNCTION(TfType)                                            \\
{                                                                       \\
    TfType::Define<ParserPluginClass, TfType::Bases<NdrParserPlugin>>() \\
        .SetFactory<NdrParserPluginFactory<ParserPluginClass>>();       \\
}

class NdrParserPlugin : public TfWeakBase
{
public:
    NDR_API
    NdrParserPlugin();
    NDR_API
    virtual ~NdrParserPlugin();
    NDR_API
    virtual NdrNodeUniquePtr Parse(const NdrNodeDiscoveryResult& discoveryResult) = 0;
    NDR_API
    virtual const NdrTokenVec& GetDiscoveryTypes() const = 0;
    NDR_API
    virtual const TfToken& GetSourceType() const = 0;
    NDR_API
    static NdrNodeUniquePtr GetInvalidNode(const NdrNodeDiscoveryResult& dr);
};

class NdrParserPluginFactoryBase : public TfType::FactoryBase
{
public:
    virtual NdrParserPlugin* New() const = 0;
};

template <class T>
class NdrParserPluginFactory : public NdrParserPluginFactoryBase
{
public:
    virtual NdrParserPlugin* New() const
    {
        return new T;
    }
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_NDR_PARSER_PLUGIN_H
''',
    "nodeDiscoveryResult.h": '''\
//
// Copyright 2018 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//

#ifndef PXR_USD_NDR_NODE_DISCOVERY_RESULT_H
#define PXR_USD_NDR_NODE_DISCOVERY_RESULT_H

#include "pxr/usd/ndr/declare.h"

PXR_NAMESPACE_OPEN_SCOPE

struct NdrNodeDiscoveryResult {
    NdrNodeDiscoveryResult(
        const NdrIdentifier& identifier,
        const NdrVersion& version,
        const std::string& name,
        const TfToken& family,
        const TfToken& discoveryType,
        const TfToken& sourceType,
        const std::string& uri,
        const std::string& resolvedUri,
        const std::string &sourceCode=std::string(),
        const NdrTokenMap &metadata=NdrTokenMap(),
        const std::string& blindData=std::string(),
        const TfToken& subIdentifier=TfToken()
    ) : identifier(identifier),
        version(version),
        name(name),
        family(family),
        discoveryType(discoveryType),
        sourceType(sourceType),
        uri(uri),
        resolvedUri(resolvedUri),
        sourceCode(sourceCode),
        metadata(metadata),
        blindData(blindData),
        subIdentifier(subIdentifier)
    { }

    NdrIdentifier identifier;
    NdrVersion version;
    std::string name;
    TfToken family;
    TfToken discoveryType;
    TfToken sourceType;
    std::string uri;
    std::string resolvedUri;
    std::string sourceCode;
    NdrTokenMap metadata;
    std::string blindData;
    TfToken subIdentifier;
};

typedef std::vector<NdrNodeDiscoveryResult> NdrNodeDiscoveryResultVec;

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_NDR_NODE_DISCOVERY_RESULT_H
''',
    "debugCodes.h": '''\
//
// Copyright 2018 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//

#ifndef PXR_USD_NDR_DEBUG_CODES_H
#define PXR_USD_NDR_DEBUG_CODES_H

#include "pxr/pxr.h"
#include "pxr/base/tf/debug.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEBUG_CODES(
    NDR_DISCOVERY,
    NDR_PARSING,
    NDR_INFO,
    NDR_STATS,
    NDR_DEBUG
);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_NDR_DEBUG_CODES_H
''',
}


def install_ndr_headers(blender_libs_dir):
    ndr_dir = os.path.join(blender_libs_dir, "usd", "include", "pxr", "usd", "ndr")
    if os.path.exists(ndr_dir):
        log("NDR headers already present, skipping.")
        return
    os.makedirs(ndr_dir, exist_ok=True)
    for name, content in NDR_HEADERS.items():
        with open(os.path.join(ndr_dir, name), "w") as f:
            f.write(content)
    log(f"Installed {len(NDR_HEADERS)} NDR header stubs to {ndr_dir}")


def main():
    parser = argparse.ArgumentParser(description="Build MoonRay Hydra Delegate for mfb")
    parser.add_argument("--blender-version", default="5.2", help="Blender version (default: 5.2)")
    parser.add_argument("--moonray-version", default="3.6.0.1", help="OpenMoonray version (default: 3.6.0.1)")
    parser.add_argument("--build-dir", help="Override build workspace directory")
    parser.add_argument("--install-dir", help="Override install destination directory")

    args = parser.parse_args()

    check_requirements()
    install_system_deps()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    workspace_dir = args.build_dir or script_dir

    source_dir = os.path.join(workspace_dir, "source")
    build_dir = os.path.join(workspace_dir, "build")
    build_deps_dir = os.path.join(workspace_dir, "build-deps")
    deps_dir = os.path.join(workspace_dir, "deps")

    blender_version = args.blender_version
    moonray_version = args.moonray_version

    install_root = args.install_dir or os.path.expanduser(f"~/.mfb/mfb-{blender_version}-{moonray_version}")

    blender_lib_repo = "https://projects.blender.org/blender/lib-linux_x64.git"
    moonray_repo = "https://github.com/dreamworksanimation/openmoonray.git"

    os.makedirs(source_dir, exist_ok=True)
    os.makedirs(build_dir, exist_ok=True)
    os.makedirs(build_deps_dir, exist_ok=True)
    os.makedirs(deps_dir, exist_ok=True)

    # 1. Write cmake configs and deps CMakeLists
    write_cmake_configs(script_dir)
    write_deps_cmake(script_dir)

    # 2. Clone OpenMoonray
    moonray_dir = os.path.join(source_dir, "openmoonray")
    if not os.path.exists(moonray_dir):
        log("Cloning OpenMoonray...")
        run_cmd([
            "git", "clone", "--recurse-submodules",
            "-b", f"openmoonray-{moonray_version}",
            moonray_repo, moonray_dir
        ])
    else:
        log("OpenMoonray already exists, skipping clone.")

    # 3. Clone Blender libraries
    blender_libs_dir = os.path.join(source_dir, "blender")
    if not os.path.exists(blender_libs_dir):
        log("Cloning Blender libraries...")
        run_cmd([
            "git", "clone", "--depth", "1",
            "-b", f"blender-v{blender_version}-release",
            blender_lib_repo, blender_libs_dir
        ])
    else:
        log("Blender libraries already exist, skipping clone.")

    # 3b. Install NDR headers (removed in USD 26.x but needed by OpenMoonray)
    install_ndr_headers(blender_libs_dir)

    # 4. Build dependencies
    os.makedirs(os.path.join(deps_dir, "bin"), exist_ok=True)

    log("Building dependencies...")
    run_cmd([
        "cmake", script_dir,
        f"-DInstallRoot={deps_dir}",
    ], cwd=build_deps_dir)
    run_cmd([
        "cmake", "--build", ".",
        "--", f"-j{os.cpu_count() or 2}"
    ], cwd=build_deps_dir)

    write_boost_python_config(deps_dir)

    # 5. Configure OpenMoonray
    if os.path.exists(os.path.join(build_dir, "CMakeCache.txt")):
        log("Cleaning stale build directory...")
        shutil.rmtree(build_dir)
        os.makedirs(build_dir, exist_ok=True)
    log("Running CMake configure...")
    cmake_args = [
        "cmake", moonray_dir,
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        f"-DCMAKE_PREFIX_PATH={deps_dir}/lib/cmake;{blender_libs_dir}/openimageio/bin",
        f"-DMOONRAY_USE_OPTIX=NO",
        f"-DBUILD_QT_APPS=NO",
        f"-DCMAKE_INSTALL_PREFIX={install_root}",
        f"-DDEPS_ROOT={deps_dir}",
        f"-DJsonCpp_ROOT={deps_dir}",
        f"-DISPC={deps_dir}/bin/ispc",
        f"-DOpenEXR_DIR={script_dir}/cmake",
        f"-DOpenImageIO_DIR={script_dir}/cmake",
        f"-Dpxr_DIR={script_dir}/cmake",
        f"-DPXR_USD_LOCATION={blender_libs_dir}/usd",
        f"-DPXR_INCLUDE_DIRS={blender_libs_dir}/usd/include",
        f"-DOpenSubDiv_ROOT={blender_libs_dir}/opensubdiv",
        f"-DOpenVDB_ROOT={blender_libs_dir}/openvdb",
        f"-DRandom123_ROOT={deps_dir}",
        f"-DBoost_ROOT={deps_dir}",
        f"-DBoost_INCLUDE_DIR={deps_dir}/include",
        f"-DTBB_ROOT={blender_libs_dir}/tbb",
        f"-DTBB_LIBRARY={blender_libs_dir}/tbb/lib",
        f"-DTBB_tbb_LIBRARY={blender_libs_dir}/tbb/lib/libtbb.so",
        f"-DTBB_tbbmalloc_LIBRARY={blender_libs_dir}/tbb/lib/libtbbmalloc.so",
        f"-DOIIO_PYTHON={blender_libs_dir}/openimageio/lib/python3.11/site-packages",
        f"-DOpenImageDenoise_DIR={blender_libs_dir}/openimagedenoise/lib/cmake/OpenImageDenoise",
        f"-DCMAKE_ISPC_COMPILER={deps_dir}/bin/ispc",
        f"-DEmbree_DIR={script_dir}/cmake",
        f"-DCMAKE_CXX_FLAGS=-I/usr/include/python3.{sys.version_info.minor}",
        "-Wno-dev"
    ]
    run_cmd(cmake_args, cwd=build_dir)

    # 6. Build and install
    log("Building OpenMoonray...")
    run_cmd([
        "cmake", "--build", ".", "--target", "install",
        "--", f"-j{os.cpu_count() or 2}"
    ], cwd=build_dir)

    log("Build completed successfully!")
    log(f"Installed to: {install_root}")


if __name__ == "__main__":
    main()
