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

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║          CavernAudioDriver Installation Script               ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Check if running as admin
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "This script must be run as Administrator!"
    exit 1
}

# Check test signing
Write-Host "Checking test signing status..." -ForegroundColor Yellow
$testSigning = bcdedit /enum | Select-String "testsigning"
if ($testSigning -notmatch "Yes") {
    Write-Warning "Test signing is NOT enabled!"
    Write-Host "Run: bcdedit /set testsigning on" -ForegroundColor Red
    Write-Host "Then reboot and try again." -ForegroundColor Red
    exit 1
}
Write-Host "✓ Test signing enabled" -ForegroundColor Green

# Verify driver files exist
Write-Host "`nChecking driver files..." -ForegroundColor Yellow
$requiredFiles = @(
    "CavernAudioDriver.sys",
    "CavernAudioDriver.inf",
    "cavernaudiodriver.cat"
)

$missing = $false
foreach ($file in $requiredFiles) {
    $path = Join-Path $DriverPath $file
    if (Test-Path $path) {
        Write-Host "  ✓ $file" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $file (NOT FOUND)" -ForegroundColor Red
        $missing = $true
    }
}

if ($missing) {
    Write-Error "`nDriver files missing! Build the driver first."
    exit 1
}

# Check if driver already installed
Write-Host "`nChecking existing installation..." -ForegroundColor Yellow
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
        pnputil /delete-driver $infName /uninstall /force 2>&1 | Out-Null
    }
}

# Install driver
Write-Host "`nInstalling driver..." -ForegroundColor Yellow
$infPath = Join-Path $DriverPath "CavernAudioDriver.inf"

$process = Start-Process -FilePath "pnputil" -ArgumentList "/add-driver", "`"$infPath`"", "/install" -Wait -PassThru -NoNewWindow

if ($process.ExitCode -eq 0) {
    Write-Host "✓ Driver installed successfully!" -ForegroundColor Green
} else {
    Write-Error "Driver installation failed (Exit code: $($process.ExitCode))"
    exit 1
}

# Verify installation
Write-Host "`nVerifying installation..." -ForegroundColor Yellow
Start-Sleep -Seconds 2

$device = Get-PnpDevice -Class Media -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "*Cavern*" }
if ($device) {
    Write-Host "✓ Device found: $($device.Name)" -ForegroundColor Green
    Write-Host "  Status: $($device.Status)" -ForegroundColor $(if($device.Status -eq "OK"){"Green"}else{"Red"})
    Write-Host "  InstanceId: $($device.InstanceId)" -ForegroundColor Gray
} else {
    Write-Warning "Device not found in PnP list. Check Device Manager."
}

# Check driver service
Write-Host "`nChecking driver service..." -ForegroundColor Yellow
$service = Get-Service -Name "CavernAudio" -ErrorAction SilentlyContinue
if ($service) {
    Write-Host "✓ Service found: $($service.Status)" -ForegroundColor Green
} else {
    Write-Host "  Service not found (may be normal for some driver types)" -ForegroundColor Yellow
}

Write-Host "`n╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                  Installation Complete!                      ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Start CavernPipeServer: .\tools\CavernPipeServer\bin\Release\net8.0\win-x64\publish\CavernPipeServer.exe"
Write-Host "  2. Set 'Cavern Atmos Virtual Audio Device' as default playback device"
Write-Host "  3. Play Dolby Atmos content"
Write-Host ""
Write-Host "See TESTING.md for detailed instructions." -ForegroundColor Gray
