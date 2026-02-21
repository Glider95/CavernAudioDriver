# Contributing to CavernAudioDriver

Thank you for your interest in contributing to the CavernAudioDriver project! This document provides guidelines and information for contributors.

## Project Overview

CavernAudioDriver is a Windows kernel-mode driver that creates a virtual audio device capable of receiving raw Dolby Atmos bitstreams (E-AC3, TrueHD) and forwarding them to CavernPipeServer for decoding and distribution to wireless speakers.

## Prerequisites

### Required Knowledge

- **C/C++ programming** (expert level)
- **Windows Driver Model (WDM)** architecture
- **Kernel-mode programming**
- **Audio processing concepts**
- **Git version control**

### Required Software

1. **Visual Studio 2022** (Community Edition or higher)
   - Workload: "Desktop development with C++"
   - Component: "Windows 11 SDK"

2. **Windows Driver Kit (WDK) 10**
   - Download: https://docs.microsoft.com/windows-hardware/drivers/download-the-wdk
   - Must match your Windows SDK version

3. **Windows SDK**
   - Included with Visual Studio or WDK

4. **Git**
   - For source control

5. **Debugging Tools**
   - WinDbg (included with WDK)
   - Or KD/NTSD from Windows SDK

## Development Environment Setup

### Step 1: Install Visual Studio 2022

```powershell
# Use Visual Studio Installer
# Select:
# - Desktop development with C++
# - MSVC v143 - VS 2022 C++ x64/x86 build tools
# - Windows 11 SDK (10.0.22000.0 or later)
```

### Step 2: Install WDK

```powershell
# Download WDK from Microsoft
# Install with default options
# Verify installation:
Test-Path "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22000.0\km\x64\ks.lib"
```

### Step 3: Clone Repository

```powershell
git clone https://github.com/Glider95/CavernAudioDriver.git
cd CavernAudioDriver
```

### Step 4: Verify Environment

```powershell
.\Check-Environment.ps1
```

### Step 5: Build Driver

```powershell
.\build.bat
# Or use Visual Studio:
# Open CavernAudioDriver.sln → Build → Build Solution
```

## Development Workflow

### 1. Test Machine Setup

**WARNING: Never test kernel drivers on your primary machine!**

Options:
- **Virtual Machine**: Use Hyper-V, VMware, or VirtualBox
- **Secondary PC**: Dedicated test machine
- **Windows To Go**: Boot from USB

### 2. Enable Test Signing

On test machine (Administrator):
```powershell
bcdedit /set testsigning on
bcdedit /set debug on
Restart-Computer
```

### 3. Install Driver

```powershell
# Navigate to build output
cd bin\x64\Debug\

# Install driver
pnputil /add-driver CavernAudioDriver.inf /install

# Or use Device Manager:
# Device Manager → Action → Add legacy hardware
```

### 4. Debug Setup

**Kernel debugging (recommended):**

On target machine:
```powershell
# Enable network debugging
bcdedit /dbgsettings net hostip:192.168.1.100 port:50000 key:1.2.3.4
```

On host machine:
```powershell
# Start WinDbg
windbg -k net:port=50000,key=1.2.3.4
```

## Code Structure

```
src/
├── CavernAudioDriver.c     # Main driver entry point
├── CavernAudioDriver.inf   # Installation file
└── MiniportWaveRT.c        # Audio miniport implementation

include/
└── CavernAudioDriver.h     # Header definitions
```

### Key Components

1. **DriverEntry**: Driver initialization
2. **AddDevice**: Device creation
3. **MiniportWaveRT**: Audio interface with Windows
4. **Data Processing**: Audio format handling and forwarding

## Coding Standards

### C Code Style

- Use **4 spaces** for indentation (no tabs)
- Maximum line length: 120 characters
- Braces: Allman style
- Comments: Use `//` for single line, `/* */` for blocks

Example:
```c
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    
    // Initialize driver
    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("Failed to create driver: 0x%08X\n", status));
        return status;
    }
    
    return STATUS_SUCCESS;
}
```

### Naming Conventions

- Functions: `Cavern{Component}{Action}` (e.g., `CavernMiniportCreate`)
- Macros: `CAVERN_{NAME}` (e.g., `CAVERN_MAX_CHANNELS`)
- Types: `CAVERN_{Name}` (e.g., `CAVERN_MINIPORT`)
- Globals: `g_Cavern{Name}` (avoid if possible)

### SAL Annotations

Always use SAL annotations for function parameters:
```c
NTSTATUS CavernProcessData(
    _In_ PDEVICE_CONTEXT Context,
    _In_reads_bytes_(DataSize) PVOID Data,
    _In_ SIZE_T DataSize,
    _Out_ PULONG BytesProcessed
);
```

## Testing

### Unit Tests

Create tests in `tests/` directory:
```c
// tests/TestFormatDetection.c
VOID TestDetectEAC3()
{
    UCHAR eac3Frame[] = { 0x0B, 0x77, ... };
    CAVERN_AUDIO_FORMAT format;
    
    NTSTATUS status = CavernDetectFormat(
        eac3Frame,
        sizeof(eac3Frame),
        &format
    );
    
    ASSERT(NT_SUCCESS(status));
    ASSERT(format.FormatTag == CAVERN_FORMAT_EAC3);
}
```

### Integration Tests

Test with real audio files:
```powershell
# Play test file
ffmpeg -i test-eac3.mkv -vn -acodec copy -f data - | 
    dotnet CavernPipeClient.dll pipe://stdin
```

### Manual Testing Checklist

- [ ] Driver installs without errors
- [ ] Device appears in Sound settings
- [ ] Media player can select device
- [ ] PCM audio plays correctly
- [ ] E-AC3 passthrough works
- [ ] TrueHD passthrough works (if supported)
- [ ] No BSOD or system crashes
- [ ] Driver uninstalls cleanly

## Common Issues

### Build Errors

**"Cannot find ks.lib"**
```powershell
# Check WDK installation
Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Lib" -Recurse -Filter "ks.lib"

# Set environment variable if needed
$env:DDK_LIB_PATH = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22000.0\km\x64"
```

**"Inf2Cat failed"**
```powershell
# This is expected for test builds
# Production builds need code signing certificate
```

### Runtime Errors

**"Driver failed to load"**
- Check test signing is enabled: `bcdedit /enum | findstr testsigning`
- Verify driver signature: `signtool verify CavernAudioDriver.sys`

**"Blue Screen (BSOD)"**
- Attach kernel debugger before loading driver
- Check `!analyze -v` output
- Review minidump at `C:\Windows\Minidump`

**"No audio output"**
- Check CavernPipeServer is running
- Verify named pipe exists
- Check Event Viewer for driver errors

## Debugging Tips

### Enable Debug Output

```c
// In code:
KdPrint(("Debug message: %d\n", value));

// View with:
// - WinDbg
// - DebugView (Sysinternals)
// - KD command line
```

### Kernel Debugging Commands

```windbg
# Set breakpoint
bp CavernAudioDriver!CavernMiniportCreate

# View call stack
k

# Inspect variables
dv

# View memory
db 0x{address}

# Continue execution
g
```

### Memory Leak Detection

```windbg
# Enable pool tagging
verifier /flags 0x1 /driver CavernAudioDriver.sys

# Check pool usage
!poolused
```

## Submitting Changes

### 1. Create Branch

```powershell
git checkout -b feature/your-feature-name
```

### 2. Make Changes

- Follow coding standards
- Add comments for complex logic
- Update documentation if needed

### 3. Test Thoroughly

- Build succeeds without warnings
- No BSOD during testing
- Audio output verified

### 4. Commit

```powershell
git add .
git commit -m "Add feature: description

- Detailed change 1
- Detailed change 2
- Breaking changes (if any)"
```

### 5. Push and Create Pull Request

```powershell
git push origin feature/your-feature-name
```

Then create PR on GitHub with:
- Clear description
- What was changed
- Testing performed
- Any breaking changes

## Code Review Process

All submissions require review before merging:

1. Automated checks (build, static analysis)
2. Manual review by maintainer
3. Testing on reference hardware
4. Merge if approved

## Documentation

- Update README.md for user-facing changes
- Update ROADMAP.md for progress tracking
- Add inline comments for complex algorithms
- Update this file for process changes

## Communication

- GitHub Issues: Bug reports, feature requests
- GitHub Discussions: Questions, ideas
- Pull Requests: Code reviews

## Resources for Learning

### Windows Driver Development
- [Windows Driver Documentation](https://docs.microsoft.com/windows-hardware/drivers/)
- [WDF Architecture](https://docs.microsoft.com/windows-hardware/drivers/wdf/)
- [PortCls Audio](https://docs.microsoft.com/windows-hardware/drivers/audio/)

### Audio Programming
- [KS Properties](https://docs.microsoft.com/windows-hardware/drivers/stream/ks-properties)
- [WaveRT Miniport](https://docs.microsoft.com/windows-hardware/drivers/audio/wavert-miniport)

### Kernel Programming
- [Windows Internals](https://docs.microsoft.com/windows-hardware/drivers/kernel/)
- [Driver Security](https://docs.microsoft.com/windows-hardware/drivers/driversecurity/)

## Code of Conduct

- Be respectful and professional
- Welcome newcomers
- Focus on constructive feedback
- Respect differing viewpoints

## Questions?

Open an issue on GitHub or start a discussion. We're here to help!

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
