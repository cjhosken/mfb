# Set directory for installation
$InstallDir = 'C:\ProgramData\chocoportable'
$env:ChocolateyInstall = $InstallDir

# Check if Chocolatey is already installed
if (Test-Path $env:ChocolateyInstall) {
    Write-Host "Chocolatey is already installed at $env:ChocolateyInstall"
} else {
    # If your PowerShell Execution policy is restrictive, set session to Bypass
    Set-ExecutionPolicy Bypass -Scope Process -Force
    
    # Install Chocolatey
    Write-Host "Installing Chocolatey..."
    iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
}

# Add Chocolatey to PATH
$env:PATH += ";$env:ChocolateyInstall\bin"

# Verify Chocolatey installation
if (Get-Command choco -ErrorAction SilentlyContinue) {
    Write-Host "Chocolatey is installed and accessible."
} else {
    Write-Error "Chocolatey installation failed or is not accessible."
}

choco feature enable -n=allowGlobalConfirmation
# Install make using Chocolatey
Write-Host "Installing make..."
choco install make -y

# Verify make installation
if (Get-Command make -ErrorAction SilentlyContinue) {
    Write-Host "Make has been installed successfully."
    
    # Add make to PATH
    $makePath = (Get-Command make).Source
    $makeDir = [System.IO.Path]::GetDirectoryName($makePath)
    
    if ($env:PATH -notlike "*$makeDir*") {
        Write-Host "Adding make directory to PATH..."
        [System.Environment]::SetEnvironmentVariable('PATH', "$env:PATH;$makeDir", [System.EnvironmentVariableTarget]::Machine)
    } else {
        Write-Host "Make directory is already in PATH."
    }
} else {
    Write-Error "Make installation failed or is not accessible."
}

# Wait for the user to hit Enter before exiting
Write-Host "Press Enter to exit..."
[void][System.Console]::ReadLine()