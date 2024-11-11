@echo off
setlocal

REM Set the installation directory for vcpkg
set VCPKG_DIR=%USERPROFILE%\vcpkg

REM Check if Git is installed
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo "Git is not installed. Please install Git and try again."
    exit /b 1
)

REM Clone vcpkg repository if it doesn't exist
if not exist "%VCPKG_DIR%" (
    echo "Cloning vcpkg repository..."
    git clone https://github.com/microsoft/vcpkg.git %VCPKG_DIR%
    if %errorlevel% neq 0 (
        echo "Failed to clone vcpkg repository."
        exit /b 1
    )
) else (
    echo "vcpkg directory already exists. Skipping clone."
)

REM Bootstrap vcpkg
echo "Bootstrapping vcpkg..."
cd %VCPKG_DIR%
call .\bootstrap-vcpkg.bat
if %errorlevel% neq 0 (
    echo "Failed to bootstrap vcpkg."
    exit /b 1
)

REM Add vcpkg to PATH for the current session
set PATH=%PATH%;%VCPKG_DIR%

echo "vcpkg installation complete."
echo "You can now use vcpkg by running '%VCPKG_DIR%\vcpkg'."
endlocal

%VCPKG_DIR%\vcpkg install curl[openssl]:x64-windows
%VCPKG_DIR%\vcpkg install cppunit:x64-windows
%VCPKG_DIR%\vcpkg install jsoncpp:x64-windows
%VCPKG_DIR%\vcpkg install libjpeg-turbo:x64-windows
%VCPKG_DIR%\vcpkg install libdeflate:x64-windows
%VCPKG_DIR%\vcpkg install qt5:x64-windows