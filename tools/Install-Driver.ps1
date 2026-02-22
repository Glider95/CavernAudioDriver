#Requires -RunAsAdministrator

<#
.SYNOPSIS
    Installs CavernAudioDriver on Windows

.DESCRIPTION
    Installs the kernel driver for Cavern Dolby Atmos virtual audio device

.PARAMETER DriverPath
    Path to driver files (default: ..\bin\Debug\CavernAudioDriver\)
#>

param(
    [string]$DriverPath = "$PSScriptRoot\..\bin\Debug\CavernAudioDriver\"
)

Write-Host "==============================================================" -ForegroundColor Cyan
Write-Host "          CavernAudioDriver Installation Script               " -ForegroundColor Cyan
Write-Host "==============================================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as admin
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "This script must be run as Administrator!"
    exit 1
}

# Check test signing
Write-Host "Checking test signing status..." -ForegroundColor Yellow
$testSigning = bcdedit /enum | Select-String "testsigning"
if ($testSigning -match "Yes") {
    Write-Host "[OK] Test signing enabled" -ForegroundColor Green
} else {
    Write-Warning "Test signing is NOT enabled!"
    Write-Host "Run: bcdedit /set testsigning on" -ForegroundColor Red
    Write-Host "Then reboot and try again." -ForegroundColor Red
    exit 1
}

# Resolve driver path
$DriverPath = Resolve-Path $DriverPath -ErrorAction SilentlyContinue
if (-not $DriverPath) {
    Write-Error "Driver path not found: $DriverPath"
    Write-Host "Please build the driver first (build-vs2022.bat)" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "Driver path: $DriverPath" -ForegroundColor Gray

# Verify driver files exist
Write-Host ""
Write-Host "Checking driver files..." -ForegroundColor Yellow
$requiredFiles = @(
    "CavernAudioDriver.sys",
    "CavernAudioDriver.inf",
    "cavernaudiodriver.cat"
)

$missing = $false
foreach ($file in $requiredFiles) {
    $path = Join-Path $DriverPath $file
    if (Test-Path $path) {
        Write-Host "  [OK] $file" -ForegroundColor Green
    } else {
        Write-Host "  [MISSING] $file" -ForegroundColor Red
        $missing = $true
    }
}

if ($missing) {
    Write-Error "Driver files missing! Build the driver first."
    exit 1
}

# Check if driver already installed
Write-Host ""
Write-Host "Checking existing installation..." -ForegroundColor Yellow
$existing = Get-PnpDevice -Class Media -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "*Cavern*" }
if ($existing) {
    Write-Host "Driver already installed: $($existing.Name)" -ForegroundColor Yellow
    $reinstall = Read-Host "Reinstall? (y/N)"
    if ($reinstall -ne 'y') {
        Write-Host "Installation cancelled." -ForegroundColor Yellow
        exit 0
    }
    
    # Uninstall existing
    Write-Host "Uninstalling existing driver..." -ForegroundColor Yellow
    $infName = (Get-ChildItem "$env:SystemRoot\inf\oem*.inf" -ErrorAction SilentlyContinue | 
        Select-String -Pattern "CavernAudio" -List | 
        Select-Object -First 1).Path
    
    if ($infName) {
        pnputil /delete-driver $infName /uninstall /force 2>$null | Out-Null
    }
}

# Install driver
Write-Host ""
Write-Host "Installing driver..." -ForegroundColor Yellow
$infPath = Join-Path $DriverPath "CavernAudioDriver.inf"

if (-not (Test-Path $infPath)) {
    Write-Error "INF file not found: $infPath"
    exit 1
}

Write-Host "INF: $infPath" -ForegroundColor Gray
Write-Host "Running: pnputil /add-driver /install ..." -ForegroundColor Gray

$process = Start-Process -FilePath "pnputil" -ArgumentList "/add-driver", "$infPath", "/install" -Wait -PassThru -NoNewWindow

if ($process.ExitCode -eq 0) {
    Write-Host ""
    Write-Host "[OK] Driver installed successfully!" -ForegroundColor Green
} else {
    Write-Error "Driver installation failed (Exit code: $($process.ExitCode))"
    exit 1
}

# Verify installation
Write-Host ""
Write-Host "Verifying installation..." -ForegroundColor Yellow
Start-Sleep -Seconds 2

$device = Get-PnpDevice -Class Media -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "*Cavern*" }
if ($device) {
    Write-Host "[OK] Device found: $($device.Name)" -ForegroundColor Green
    Write-Host "     Status: $($device.Status)" -ForegroundColor $(if($device.Status -eq "OK"){"Green"}else{"Red"})
    Write-Host "     InstanceId: $($device.InstanceId)" -ForegroundColor Gray
} else {
    Write-Warning "Device not found in PnP list. Check Device Manager."
}

Write-Host ""
Write-Host "==============================================================" -ForegroundColor Cyan
Write-Host "                  Installation Complete!                      " -ForegroundColor Cyan
Write-Host "==============================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Start CavernPipeServer:" -ForegroundColor White
Write-Host "     .\tools\CavernPipeServer\bin\Release\net8.0\win-x64\publish\CavernPipeServer.exe"
Write-Host ""
Write-Host "  2. Set 'Cavern Atmos Virtual Audio Device' as default playback device"
Write-Host "     Settings > System > Sound > Output"
Write-Host ""
Write-Host "  3. Play Dolby Atmos content"
Write-Host ""
Write-Host "See TESTING.md for detailed instructions." -ForegroundColor Gray
