# Set default paths
$DEFAULT_MFB_DIR = "$env:USERPROFILE\.mfb"
$MFB_DIR = $DEFAULT_MFB_DIR

# Parse command-line arguments
param (
    [string]$mfbDir
)

if ($mfbDir) {
    $MFB_DIR = $mfbDir
}

Write-Output "MFB_DIR: $MFB_DIR"

# Create necessary directories
if (-not (Test-Path $MFB_DIR)) {
    New-Item -ItemType Directory -Path $MFB_DIR
}
Set-Location -Path $MFB_DIR

# Clone the repository if not already cloned
if (-not (Test-Path "$MFB_DIR\source")) {
    git clone --recurse-submodules https://github.com/dreamworksanimation/openmoonray.git "$MFB_DIR\source"
}

# Set up dependencies directory
if (Test-Path "$MFB_DIR\dependencies") {
    Remove-Item -Recurse -Force "$MFB_DIR\dependencies"
}
New-Item -ItemType Directory -Path "$MFB_DIR\dependencies"
New-Item -ItemType Directory -Path "$MFB_DIR\dependencies\bin"

# Copy RHEL9 folder
if (Test-Path "$MFB_DIR\source\building\Win") {
    Remove-Item -Recurse -Force "$MFB_DIR\source\building\Win"
}
Copy-Item -Recurse -Path "$PSScriptRoot\Win" -Destination "$MFB_DIR\source\building\Win"

# Run install_packages.ps1
Start-Process powershell -ArgumentList "-ExecutionPolicy Bypass -File `"$MFB_DIR\source\building\Win\install_packages.ps1`"" -Wait

# Build process
if (Test-Path "$MFB_DIR\build") {
    Remove-Item -Recurse -Force "$MFB_DIR\build"
}
New-Item -ItemType Directory -Path "$MFB_DIR\build"
Set-Location -Path "$MFB_DIR\build"

cmake -S "$MFB_DIR\source\building\Win" -B "$MFB_DIR\build" -DInstallRoot="$MFB_DIR\dependencies"
cmake --build "$MFB_DIR\build" -- /maxcpucount

# Clean up build directory
Remove-Item -Recurse -Force "$MFB_DIR\build\*"

# Define the source and destination paths
$SOURCE_EXE = "$PSScriptRoot\optix.exe"
$DEST_DIR = "$MFB_DIR\dependencies"
$DEST_EXE = "$DEST_DIR\optix.exe"

# Ensure the destination directory exists
if (-not (Test-Path $DEST_DIR)) {
    New-Item -ItemType Directory -Path $DEST_DIR
}
# Copy the executable to the target directory
Copy-Item -Path $SOURCE_EXE -Destination $DEST_EXE -Force

# Run the executable
& $DEST_EXE --skip-license --exclude-subdir

# Copy additional configuration files
Copy-Item -Recurse -Path "$PSScriptRoot\CMakePresets.json" -Destination "$MFB_DIR\source\CMakePresets.json" -Force
Copy-Item -Recurse -Path "$PSScriptRoot\configs\windows_x64\*" -Destination "$MFB_DIR\dependencies" -Force
New-Item -ItemType Directory -Path "$MFB_DIR\dependencies\bl_deps"
Copy-Item -Recurse -Path "$PSScriptRoot\bl_deps\windows_x64\*" -Destination "$MFB_DIR\dependencies\bl_deps" -Force

# Set environment variables
[System.Environment]::SetEnvironmentVariable("PATH", "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.2\bin;$env:PATH", [System.EnvironmentVariableTarget]::Machine)
[System.Environment]::SetEnvironmentVariable("PATH", "$MFB_DIR\dependencies\bl_deps\python\lib;$env:PATH", [System.EnvironmentVariableTarget]::Machine)
[System.Environment]::SetEnvironmentVariable("PATH", "$MFB_DIR\dependencies\bl_deps\boost\lib;$env:PATH", [System.EnvironmentVariableTarget]::Machine)
[System.Environment]::SetEnvironmentVariable("PATH", "$MFB_DIR\dependencies\bl_deps\materialx\lib;$env:PATH", [System.EnvironmentVariableTarget]::Machine)
[System.Environment]::SetEnvironmentVariable("PATH", "$MFB_DIR\dependencies\bl_deps\opensubdiv\lib;$env:PATH", [System.EnvironmentVariableTarget]::Machine)
[System.Environment]::SetEnvironmentVariable("PATH", "$MFB_DIR\dependencies\bl_deps\openimageio\lib;$env:PATH", [System.EnvironmentVariableTarget]::Machine)
[System.Environment]::SetEnvironmentVariable("PATH", "$MFB_DIR\dependencies\bl_deps\openvdb\lib;$env:PATH", [System.EnvironmentVariableTarget]::Machine)
[System.Environment]::SetEnvironmentVariable("PATH", "$MFB_DIR\dependencies\bl_deps\openexr\lib;$env:PATH", [System.EnvironmentVariableTarget]::Machine)
[System.Environment]::SetEnvironmentVariable("PATH", "$MFB_DIR\dependencies\bl_deps\imath\lib;$env:PATH", [System.EnvironmentVariableTarget]::Machine)

# Build with presets
cmake --preset linux-blender-release
cmake --build "$MFB_DIR\build" -- /maxcpucount

# Install
if (Test-Path "$MFB_DIR\installs") {
    Remove-Item -Recurse -Force "$MFB_DIR\installs"
}
New-Item -ItemType Directory -Path "$MFB_DIR\installs"
New-Item -ItemType Directory -Path "$MFB_DIR\installs\openmoonray"
cmake --install "$MFB_DIR\build" --prefix "$MFB_DIR\installs\openmoonray"

# Source setup script
& "$MFB_DIR\installs\openmoonray\scripts\setup.bat"

# Add to environment variable
$SOURCE_LINE = "export MFB_DIR=$DEFAULT_MFB_DIR; call $MFB_DIR\installs\openmoonray\scripts\setup.bat; set PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.2\lib\x64;$MFB_DIR\dependencies\bl_deps\python\lib;$MFB_DIR\dependencies\bl_deps\boost\lib;$MFB_DIR\dependencies\bl_deps\materialx\lib;$MFB_DIR\dependencies\bl_deps\opensubdiv\lib;$MFB_DIR\dependencies\bl_deps\openimageio\lib;$MFB_DIR\dependencies\bl_deps\openvdb\lib;$MFB_DIR\dependencies\bl_deps\openexr\lib;$MFB_DIR\dependencies\bl_deps\imath\lib;$env:PATH"

# Check if line already exists in environment setup file
$BASHRC_PATH = "$env:USERPROFILE\.bashrc"
if (-not (Select-String -Path $BASHRC_PATH -Pattern $SOURCE_LINE)) {
    Add-Content -Path $BASHRC_PATH -Value $SOURCE_LINE
    Write-Output "The line has been added to .bashrc."
} else {
    Write-Output "The line already exists in .bashrc."
}

# Return to script directory
Set-Location -Path $PSScriptRoot

# Clean up background process if any
if ($LAST_PID) {
    Stop-Process -Id $LAST_PID -Force -ErrorAction SilentlyContinue
}

# Wait for the user to hit Enter before exiting
Write-Host "Press Enter to exit..."
[void][System.Console]::ReadLine()

