@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set VCPKG_DIR=%USERPROFILE%\vcpkg

REM Default MFB directory
set "DEFAULT_MFB_DIR=%USERPROFILE%\.mfb"
set "MFB_DIR=%DEFAULT_MFB_DIR%"

REM Parse command-line arguments
:parse_args
if "%~1"=="" goto end_args
if /i "%~1"=="--mfb-dir" (
    set "MFB_DIR=%~2"
    shift
    shift
    goto parse_args
) else (
    echo Unknown option: %~1
    echo Usage: %0 [--mfb-dir C:\path\to\mfb]
    exit /b 1
)

:end_args

echo MFB_DIR: %MFB_DIR%

REM Create MFB directory if it doesn't exist
if not exist "%MFB_DIR%" (
    mkdir "%MFB_DIR%"
)

cd "%MFB_DIR%"

REM Clone the repository with submodules
if exist "%MFB_DIR%\source" (
    echo "Source directory already exists. Skipping clone."
) else (
    git clone --recurse-submodules https://github.com/dreamworksanimation/openmoonray.git "%MFB_DIR%\source"
)

xcopy /e /i /y "%SCRIPT_DIR%\windows_x64" "%MFB_DIR%\source\building\windows_x64"

git init "%MFB_DIR%\dependencies"
cd "%MFB_DIR%/dependencies"

git remote add -f origin https://projects.blender.org/blender/lib-windows_x64.git
git config core.sparseCheckout true

echo boost > .git\info\sparse-checkout
echo imath >> .git\info\sparse-checkout
echo materialx >> .git\info\sparse-checkout
echo opencolorio >> .git\info\sparse-checkout
echo openexr >> .git\info\sparse-checkout
echo openimagedenoise >> .git\info\sparse-checkout
echo openimageio >> .git\info\sparse-checkout
echo opensubdiv >> .git\info\sparse-checkout
echo openvdb >> .git\info\sparse-checkout
echo python >> .git\info\sparse-checkout
echo tbb >> .git\info\sparse-checkout
echo usd >> .git\info\sparse-checkout

git fetch origin

git checkout 283d5458d373814824e5675d6fcd7a67d7c50cfb

git pull

mkdir "%MFB_DIR%\source\build"
cd "%MFB_DIR%\source\build"

call "%MFB_DIR%\source\building\windows_x64\install_packages.bat"

del /q .\*

copy /y "%SCRIPT_DIR%\CMakePresets.json" "%MFB_DIR%\source\CMakePresets.json" 
xcopy /e /i /y "%SCRIPT_DIR%\configs\windows_x64\*" "%MFB_DIR%\dependencies"

cmake %MFB_DIR%/source/building/windows_x64 -DInstallRoot="%MFB_DIR%\dependencies"
cmake --build . -- /m

del /q .\*

cmake .. --preset windows-blender-release -Wno-dev
cmake --build . -- /m

cmake --install . --prefix "%MFB_DIR%\openmoonray"

endlocal
