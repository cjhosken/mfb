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
echo jpeg
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
echo tbb
) > .git\info\sparse-checkout

git fetch origin
git checkout blender-v4.2-release

:: Install and configure vcpkg
set "VCPKG_ROOT=%MFB_DIR%\dependencies\vcpkg"

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo Installing vcpkg...
    git clone https://github.com/microsoft/vcpkg.git "%VCPKG_ROOT%"
    cd "%VCPKG_ROOT%"
    .\bootstrap-vcpkg.bat --disable-metrics
    cd ..
)

:: Install required dependencies
"%VCPKG_ROOT%\vcpkg" install ^
    jsoncpp:x64-windows ^
    curl:x64-windows ^
    openssl:x64-windows ^
    cppunit:x64-windows ^
    libdeflate:x64-windows ^
    random123:x64-windows ^
    libmicrohttpd:x64-windows


:: Setting up ISPC

set ISPC_DIR=%MFB_DIR%\dependencies\ispc
set TEMP_ZIP=%MFB_DIR%\dependencies\ispc.zip


if NOT EXIST %ISPC_DIR% (
    curl -L https://github.com/ispc/ispc/releases/download/trunk-artifacts/ispc-trunk-windows.zip -o "%TEMP_ZIP%"
    mkdir "%ISPC_DIR%" >nul 2>&1
    tar -xf "%TEMP_ZIP%" -C "%ISPC_DIR%" --strip-components=1
    del "%TEMP_ZIP%"
)


:: Copy custom CMakeConfig files
mkdir "%MFB_DIR%\dependencies\MaterialX\resources" >nul 2>&1
xcopy /E /I /Q /Y "%SCRIPT_DIR%\cmake\*" "%MFB_DIR%\dependencies"

:: Building MoonRay
mkdir "%MFB_DIR%\source\build" >nul 2>&1
cd /d "%MFB_DIR%\source\build"

:: Note: You'll need to adjust compiler paths for Windows
cmake .. ^
    -G "Ninja" ^
    -DCMAKE_TOOLCHAIN_FILE="" ^
    -DCMAKE_POLICY_DEFAULT_CMP0167=OLD ^
    -DCMAKE_POLICY_DEFAULT_CMP0128=OLD ^
    -DCMAKE_PREFIX_PATH="%MFB_DIR%\dependencies" ^
    -DBOOST_ROOT="%MFB_DIR%\dependencies" ^
    -DBoost_ARCHITECTURE="-x64" ^
    -DBoost_USE_STATIC_LIBS=ON ^
    -DBoost_USE_MULTITHREADED=ON ^
    -DPython3_ROOT="%MFB_DIR%\dependencies\python\311" ^
    -DPython_EXECUTABLE="%MFB_DIR%\dependencies\python\311\bin\python.exe" ^
    -DPython_LIBRARIES="%MFB_DIR%\dependencies\python\311\libs" ^
    -DPython_INCLUDE_DIRS="%MFB_DIR%\dependencies\python\311\include" ^
    -DBOOST_PYTHON_COMPONENT_NAME=python311 ^
    -DCppUnit_ROOT="%VCPKG_ROOT%\installed\x64-windows" ^
    -DMicrohttpd_LIBRARY="%VCPKG_ROOT%\installed\x64-windows\lib\libmicrohttpd-dll.lib" ^
    -DMicrohttpd_INCLUDE_DIRS="%VCPKG_ROOT%\installed\x64-windows\include" ^
    -Dpxr_ROOT="%MFB_DIR%\dependencies" ^
    -DImath_DIR="%MFB_DIR%\dependencies\imath\lib\cmake\Imath" ^
    -DMaterialX_DIR="%MFB_DIR%\dependencies\MaterialX\lib\cmake\MaterialX" ^
    -DCURL_LIBRARY="%VCPKG_ROOT%\installed\x64-windows\lib\libcurl.lib" ^
    -DCURL_INCLUDE_DIR="%VCPKG_ROOT%\installed\x64-windows\include" ^
    -DJsonCpp_LIBRARIES="%VCPKG_ROOT%\installed\x64-windows\lib\jsoncpp.lib" ^
    -DOPENSSL_ROOT_DIR="%VCPKG_ROOT%\installed\x64-windows" ^
    -DTBB_ROOT="%MFB_DIR%\dependencies\tbb" ^
    -DJPEG_LIBRARY="%MFB_DIR%\dependencies\jpeg\lib\jpeg.lib" ^
    -DJPEG_INCLUDE_DIR="%MFB_DIR%\dependencies\jpeg\include" ^
    -DABI_VERSION=0 ^
    -DBUILD_QT_APPS=NO ^
    -DMOONRAY_USE_OPTIX=NO ^
    -DCMAKE_DISABLE_SYMLINKS=ON ^
    -Wno-dev

cmake --build . --config Release --target ALL_BUILD -j %NUMBER_OF_PROCESSORS%
cmake --install . --prefix "%MFB_DIR%\openmoonray"

endlocal