# CavernAudioDriver

**Windows Virtual Audio Driver for Cavern Dolby Atmos Pipeline**

## Project Status

⚠️ **EARLY DEVELOPMENT - WORK IN PROGRESS**

This is a kernel-mode Windows driver that creates a virtual audio device capable of receiving raw Dolby Atmos bitstreams (E-AC3, TrueHD) from applications and forwarding them to CavernPipeServer.

## Architecture

```
Application (Stremio, etc.)
    ↓ [Raw E-AC3/TrueHD bitstream]
Windows Audio Stack
    ↓ [Exclusive mode passthrough]
CavernAudioDriver (Virtual Audio Device)
    ↓ [Named pipe]
CavernPipeServer
    ↓ [Decode Atmos objects]
Multi-channel PCM
    ↓ [Named pipe/Socket]
Snapserver
    ↓ [Network stream]
ESP32-C5 Speakers
```

## Project Structure

```
CavernAudioDriver/
├── src/
│   ├── CavernAudioDriver.c      # Main driver implementation
│   └── CavernAudioDriver.inf    # Driver installation file
├── include/
│   └── CavernAudioDriver.h      # Header file
├── bin/                         # Build output (generated)
├── obj/                         # Object files (generated)
├── CavernAudioDriver.sln        # Visual Studio solution
├── CavernAudioDriver.vcxproj    # MSBuild project file
└── README.md                    # This file
```

## Prerequisites

### Required Software

1. **Windows Driver Kit (WDK) 10**
   - Download: https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
   - Install with Windows SDK

2. **Visual Studio 2022** (or 2019)
   - Workload: "Desktop development with C++"
   - Individual components:
     - Windows 10/11 SDK
     - MSVC v143 (or v142) build tools

3. **Windows SDK**
   - Included with WDK installation

### Development Environment Setup

```powershell
# Verify WDK installation
Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\" | Where-Object { $_.Name -like "*Include*" }

# Check for required libraries
Test-Path "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22000.0\km\x64\ks.lib"
Test-Path "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22000.0\km\x64\portcls.lib"
```

## Building

### Using Visual Studio

1. Open `CavernAudioDriver.sln` in Visual Studio
2. Select configuration: `Debug` or `Release`
3. Select platform: `x64`
4. Build → Build Solution (Ctrl+Shift+B)

### Using MSBuild Command Line

```powershell
# Open Developer Command Prompt for VS 2022
cd C:\Users\nicol\.openclaw\workspace\workspace\CavernAudioDriver

# Build Debug version
msbuild CavernAudioDriver.sln /p:Configuration=Debug /p:Platform=x64

# Build Release version
msbuild CavernAudioDriver.sln /p:Configuration=Release /p:Platform=x64
```

### Build Output

After successful build:
- Driver binary: `bin\x64\Debug\CavernAudioDriver.sys`
- Driver package: `bin\x64\Debug\CavernAudioDriver\`

## Installation

### Test Mode (Development Only)

⚠️ **WARNING: Disables driver signature enforcement**

```powershell
# Enable test signing (requires admin)
bcdedit /set testsigning on

# Reboot required
Restart-Computer
```

### Install Driver

```powershell
# Navigate to driver package
cd "C:\Users\nicol\.openclaw\workspace\workspace\CavernAudioDriver\bin\x64\Debug\CavernAudioDriver"

# Install using pnputil (admin required)
pnputil /add-driver CavernAudioDriver.inf /install

# Or use Device Manager:
# 1. Device Manager → Action → Add legacy hardware
# 2. Install from INF file
```

### Uninstall

```powershell
# Find and remove driver
pnputil /delete-driver oemXX.inf /uninstall /force

# Disable test signing (when done testing)
bcdedit /set testsigning off
```

## Configuration

### Registry Settings

```powershell
# Enable passthrough mode
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Services\CavernAudio" -Name "EnablePassthrough" -Value 1

# Set pipe name
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Services\CavernAudio" -Name "PipeName" -Value "\\??\\pipe\\CavernAudioPipe"
```

## Debugging

### Kernel Debugging Setup

1. **Enable kernel debugging on target machine:**
```powershell
# Enable debugging
bcdedit /debug on
bcdedit /dbgsettings serial debugport:1 baudrate:115200

# Or use network debugging (recommended for VMs)
bcdedit /dbgsettings net hostip:192.168.1.100 port:50000
```

2. **Connect WinDbg from host:**
   - Install Windows Debugging Tools
   - File → Kernel Debug → Net/Serial
   - Enter connection parameters

### Debug Output

Driver uses `KdPrint` for debug output. View with:
- WinDbg
- DebugView (Sysinternals)
- KD/NTSD command line tools

## Current Implementation Status

### ✅ Completed
- [x] Basic driver skeleton
- [x] WDF integration
- [x] INF file structure
- [x] Named pipe connection stubs
- [x] Project build configuration

### 🚧 In Progress
- [ ] PortCls miniport implementation
- [ ] Audio format negotiation
- [ ] Data streaming path
- [ ] Format detection (E-AC3, TrueHD)

### ⏳ TODO
- [ ] Proper audio miniport (WaveRT)
- [ ] Exclusive mode passthrough
- [ ] Format property sets
- [ ] Buffer management
- [ ] Error handling
- [ ] WHQL compliance
- [ ] Code signing certificate

## Known Limitations

1. **Not Production Ready**: This is a development prototype
2. **Test Signing Required**: Windows won't load unsigned driver in normal mode
3. **No Audio Yet**: Miniport implementation is incomplete
4. **Format Support**: Only skeleton for format detection

## References

### Microsoft Documentation
- [WDF Driver Development Guide](https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/)
- [Audio Drivers Overview](https://docs.microsoft.com/en-us/windows-hardware/drivers/audio/)
- [SysVAD Sample](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad)

### Related Projects
- [Scream](https://github.com/duncanthrax/scream) - Virtual audio driver reference
- [VB-Cable](https://vb-audio.com/Cable/) - Commercial virtual audio driver
- [Snapcast](https://github.com/badaix/snapcast) - Multiroom audio streaming

## License

MIT License - See LICENSE file

## Contributing

This is an experimental project. Contributions welcome but expect significant development effort required.

## Troubleshooting

### Build Errors

**"Cannot find ks.lib"**
- Ensure WDK is properly installed
- Check `$(DDK_LIB_PATH)` environment variable

**"Inf2Cat error"**
- Disable driver signing enforcement for test builds
- Or use proper code signing certificate

### Runtime Errors

**"Driver failed to load"**
- Check `bcdedit /enum` for testsigning status
- Verify driver signature with `signtool verify`

**"No audio output"**
- Check CavernPipeServer is running
- Verify named pipe exists: `[System.IO.Directory]::GetFiles("\\.\pipe\")`
- Check Event Viewer for driver errors

## Contact

For issues related to this driver project, open an issue on the repository.

For Cavern-specific questions, see: https://github.com/VoidXH/Cavern
