# OpenClaw WDK Integration Fix

## Issue: WindowsKernelModeDriver10.0 Build Tools Not Found

### Root Cause
The Windows Driver Kit (WDK) is installed but not properly integrated with Visual Studio 2022.

### Solution Steps

#### Step 1: Verify WDK Installation

Open PowerShell as Administrator and run:

```powershell
# Check WDK installation
Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" | Where-Object { $_.Name -match "^10\." }

# Check for WDK VSIX (Visual Studio Integration)
Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Vsix" -Recurse -ErrorAction SilentlyContinue
```

#### Step 2: Install WDK Visual Studio Extension

The WDK needs to install a VSIX extension into Visual Studio 2022.

**Option A: Run WDK Installer Again**
1. Go to `C:\Installers` (or where you downloaded wdksetup.exe)
2. Run `wdksetup.exe` as Administrator
3. Select "Modify" installation
4. Ensure "Visual Studio extension" is checked
5. Complete the installation

**Option B: Manual VSIX Installation**
```powershell
# Find the VSIX file
$vsixPath = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Vsix" -Filter "*.vsix" -Recurse | Select-Object -First 1

# Install it
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\VSIXInstaller.exe" $vsixPath.FullName
```

#### Step 3: Verify Integration

After installing the VSIX, verify:

```powershell
# Check for WDK props file
Test-Path "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Microsoft\VC\v170\Platforms\x64\PlatformToolsets\WindowsKernelModeDriver10.0"

# Alternative location
Test-Path "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\MSBuild\Microsoft\VC\v170\Platforms\x64\PlatformToolsets\WindowsKernelModeDriver10.0"
```

#### Step 4: Alternative Build Approach

If integration fails, use the EWDK (Enterprise WDK):

**Download EWDK:**
```
https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk#enterprise-wdk-ewdk
```

**Build using EWDK:**
```powershell
cd C:\Users\nicol\.openclaw\workspace\workspace\CavernAudioDriver
C:\EWDK\BuildEnv\SetupBuildEnv.cmd
MSBuild CavernAudioDriver.sln /p:Configuration=Debug /p:Platform=x64
```

#### Step 5: Quick Workaround - Use Pre-built Test

Since driver development is complex, let's verify the concept works first:

```powershell
# Test if named pipe communication works
# This validates the core concept without building driver

# Start CavernPipeServer first, then:
$pipe = New-Object System.IO.Pipes.NamedPipeClientStream(".", "CavernAudioPipe", [System.IO.Pipes.PipeDirection]::Out)
$pipe.Connect(5000)
$writer = New-Object System.IO.StreamWriter($pipe)
$writer.WriteLine("Test data")
$writer.Dispose()
$pipe.Dispose()
```

### Recommended Path Forward

Given the complexity of WDK/VS integration, consider:

1. **Use EWDK** (Enterprise WDK) - Self-contained, no VS integration needed
2. **Switch to User-Mode Driver** - Use UMDF instead of KMDF (simpler)
3. **Alternative Approach** - Use existing virtual audio driver + custom app

### Documentation

- [WDK Installation Guide](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
- [EWDK Information](https://docs.microsoft.com/en-us/windows-hardware/drivers/develop/using-the-enterprise-wdk)
- [Troubleshooting WDK](https://docs.microsoft.com/en-us/windows-hardware/drivers/install/troubleshooting-installation-of-the-wdk)

---

*Generated: February 22, 2026*
