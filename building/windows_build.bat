@echo off
setlocal enabledelayedexpansion

:: Get the script directory
set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

:: Set default MFB directory
set "DEFAULT_MFB_DIR=%SCRIPT_DIR%\..\mfb"

:: Initialize variables with default values
set "MFB_DIR=%DEFAULT_MFB_DIR%"

:: Parse command-line arguments
:parse_args
if "%~1"=="" goto end_parse
if "%~1"=="--mfb-dir" (
    set "MFB_DIR=%~2"
    shift
    shift
    goto parse_args
)
echo Unknown option: %~1
echo Usage: %~nx0 [--mfb-dir \path\to\mfb]
exit /b 1
:end_parse

echo MFB_DIR: %MFB_DIR%

:: Create directories and clone repositories
mkdir "%MFB_DIR%" >nul 2>&1
cd /d "%MFB_DIR%"

:: Clone openmoonray repository
git clone --recurse-submodules https://github.com/dreamworksanimation/openmoonray.git "%MFB_DIR%\source"

:: Initialize dependencies repository
git init "%MFB_DIR%\dependencies"
cd /d "%MFB_DIR%\dependencies"

git remote add -f origin https://projects.blender.org/blender/lib-windows_x64.git
git config core.sparseCheckout true

(
echo imath
echo boost
echo materialx
echo opencolorio
echo openexr
echo openimagedenoise
echo openimageio
echo opensubdiv
echo openvdb
echo python
echo usd
) > .git\info\sparse-checkout

git fetch origin
git checkout blender-v4.2-release

:: Clear any existing toolchain file
set "CMAKE_TOOLCHAIN_FILE="

:: Building dependencies
mkdir "%MFB_DIR%\dependencies\build" >nul 2>&1
cd /d "%MFB_DIR%\dependencies\build"

:: Clone and build jsoncpp
git clone https://github.com/open-source-parsers/jsoncpp.git
cd jsoncpp
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX="%MFB_DIR%\dependencies"
cmake --build . --config Release --target ALL_BUILD -j %NUMBER_OF_PROCESSORS%
cmake --build . --config Release --target INSTALL

:: Building MoonRay
mkdir "%MFB_DIR%\source\build" >nul 2>&1
cd /d "%MFB_DIR%\source\build"

:: Note: You'll need to adjust compiler paths for Windows
cmake .. ^
    -DCMAKE_PREFIX_PATH="%MFB_DIR%\dependencies\*;%MFB_DIR%\dependencies\" ^
    -DPYTHON_EXECUTABLE="%MFB_DIR%\dependencies\python\bin\python3.exe" ^
    -DBOOST_PYTHON_COMPONENT_NAME=python311 ^
    -DBoost_INCLUDE_DIR="%MFB_DIR%\dependencies\boost\include" ^
    -DBoost_LIBRARYDIR="%MFB_DIR%\dependencies\boost\lib" ^
    -DABI_VERSION=0 ^
    -DBUILD_QT_APPS=NO ^
    -DMOONRAY_USE_OPTIX=NO ^
    -Wno-dev

cmake --build . --config Release --target ALL_BUILD -j %NUMBER_OF_PROCESSORS%
cmake --install . --prefix "%MFB_DIR%\openmoonray"

endlocal