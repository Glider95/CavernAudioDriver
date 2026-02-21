#Requires -Version 5.1
<#
.SYNOPSIS
    Check development environment for CavernAudioDriver

.DESCRIPTION
    Verifies that all prerequisites are installed for building the driver.
#>

$ErrorActionPreference = "Continue"

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║      CavernAudioDriver Environment Check                ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$checks = @()
$allPassed = $true

# Check 1: Visual Studio
Write-Host "[CHECK] Visual Studio 2022..." -ForegroundColor Yellow
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($vsPath) {
        Write-Host "  ✓ Found at: $vsPath" -ForegroundColor Green
        $checks += @{Name="Visual Studio 2022"; Status="PASS"}
    } else {
        Write-Host "  ✗ Visual Studio 2022 with C++ tools not found" -ForegroundColor Red
        $checks += @{Name="Visual Studio 2022"; Status="FAIL"}
        $allPassed = $false
    }
} else {
    Write-Host "  ✗ vswhere.exe not found - VS may not be installed" -ForegroundColor Red
    $checks += @{Name="Visual Studio 2022"; Status="FAIL"}
    $allPassed = $false
}

# Check 2: Windows Driver Kit
Write-Host "`n[CHECK] Windows Driver Kit (WDK)..." -ForegroundColor Yellow
$wdkPath = "C:\Program Files (x86)\Windows Kits\10"
if (Test-Path $wdkPath) {
    Write-Host "  ✓ WDK found at: $wdkPath" -ForegroundColor Green
    
    # Check for specific libraries
    $ksLib = Get-ChildItem "$wdkPath\Lib" -Recurse -Filter "ks.lib" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($ksLib) {
        Write-Host "  ✓ ks.lib found: $($ksLib.FullName)" -ForegroundColor Green
        $checks += @{Name="WDK Libraries"; Status="PASS"}
    } else {
        Write-Host "  ⚠ ks.lib not found - WDK may be incomplete" -ForegroundColor Yellow
        $checks += @{Name="WDK Libraries"; Status="WARN"}
    }
} else {
    Write-Host "  ✗ WDK not found" -ForegroundColor Red
    $checks += @{Name="Windows Driver Kit"; Status="FAIL"}
    $allPassed = $false
}

# Check 3: Windows SDK
Write-Host "`n[CHECK] Windows SDK..." -ForegroundColor Yellow
$sdkPath = "C:\Program Files (x86)\Windows Kits\10\Include"
if (Test-Path $sdkPath) {
    $versions = Get-ChildItem $sdkPath -Directory | Where-Object { $_.Name -match "^10\." } | Sort-Object Name -Descending
    if ($versions) {
        Write-Host "  ✓ SDK versions found:" -ForegroundColor Green
        $versions | ForEach-Object { Write-Host "      - $($_.Name)" -ForegroundColor Gray }
        $checks += @{Name="Windows SDK"; Status="PASS"}
    }
} else {
    Write-Host "  ✗ Windows SDK not found" -ForegroundColor Red
    $checks += @{Name="Windows SDK"; Status="FAIL"}
    $allPassed = $false
}

# Check 4: .NET Framework
Write-Host "`n[CHECK] .NET Framework..." -ForegroundColor Yellow
$dotnet = Get-Command msbuild -ErrorAction SilentlyContinue
if ($dotnet) {
    Write-Host "  ✓ MSBuild found: $($dotnet.Source)" -ForegroundColor Green
    $checks += @{Name="MSBuild"; Status="PASS"}
} else {
    Write-Host "  ✗ MSBuild not found" -ForegroundColor Red
    $checks += @{Name="MSBuild"; Status="FAIL"}
    $allPassed = $false
}

# Check 5: Driver Signing (for later)
Write-Host "`n[CHECK] Driver Signing Capability..." -ForegroundColor Yellow
$testSigning = bcdedit /enum | Select-String "testsigning"
if ($testSigning -match "Yes") {
    Write-Host "  ✓ Test signing is ENABLED" -ForegroundColor Green
    $checks += @{Name="Test Signing"; Status="PASS"}
} else {
    Write-Host "  ⚠ Test signing is DISABLED" -ForegroundColor Yellow
    Write-Host "    Run as admin: bcdedit /set testsigning on" -ForegroundColor Gray
    $checks += @{Name="Test Signing"; Status="WARN"}
}

# Summary
Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "SUMMARY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan

$checks | ForEach-Object {
    $color = switch ($_.Status) {
        "PASS" { "Green" }
        "WARN" { "Yellow" }
        "FAIL" { "Red" }
    }
    $icon = switch ($_.Status) {
        "PASS" { "✓" }
        "WARN" { "⚠" }
        "FAIL" { "✗" }
    }
    Write-Host "  $icon $($_.Name): $($_.Status)" -ForegroundColor $color
}

Write-Host ""

if ($allPassed) {
    Write-Host "✓ All required components are installed!" -ForegroundColor Green
    Write-Host "`nYou can now build the driver with:" -ForegroundColor White
    Write-Host "  .\build.bat" -ForegroundColor Yellow
} else {
    Write-Host "✗ Some required components are missing." -ForegroundColor Red
    Write-Host "`nPlease install the missing components and run this check again." -ForegroundColor Yellow
}

Write-Host ""
