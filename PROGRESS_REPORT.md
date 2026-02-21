# CavernAudioDriver - Development Progress Report

**Date:** February 21, 2026  
**Repository:** https://github.com/Glider95/CavernAudioDriver

---

## Executive Summary

Created a Windows kernel-mode driver framework for capturing raw Dolby Atmos bitstreams from applications and forwarding them to CavernPipeServer.

**Current Status:** Core infrastructure complete, audio streaming implementation in progress.

---

## Completed Components

### 1. Driver Framework ✅

| Component | File | Status | Description |
|-----------|------|--------|-------------|
| Main Driver | `src/CavernAudioDriver.c` | ✅ Skeleton Complete | WDF driver entry, device creation, pipe connection stubs |
| INF File | `src/CavernAudioDriver.inf` | ✅ Complete | Driver installation and registry configuration |
| Header | `include/CavernAudioDriver.h` | ✅ Complete | Format definitions, structures, function prototypes |
| WaveRT Miniport | `src/MiniportWaveRT.c` | ✅ Core Implemented | Audio interface with Windows audio system |

### 2. WaveRT Miniport Implementation ✅

The miniport (`MiniportWaveRT.c`) implements:

- **IMiniportWaveRT Interface**: Core audio miniport functions
- **Stream Management**: Create/destroy audio streams
- **Format Support**: 
  - PCM (uncompressed audio)
  - E-AC3 (Dolby Digital Plus)
  - Infrastructure for TrueHD (needs implementation)
- **DMA Buffer Management**: Allocate contiguous memory for audio data
- **State Management**: Play, pause, stop handling
- **Position Tracking**: Playback position reporting

**Key Features:**
- Reports as audio render device (sink)
- Supports 6-channel (5.1) configuration at 48kHz/16-bit
- Connects to `\\??\\pipe\\CavernAudioPipe` named pipe

### 3. Build System ✅

| Component | Purpose |
|-----------|---------|
| `CavernAudioDriver.sln` | Visual Studio 2022 solution |
| `CavernAudioDriver.vcxproj` | MSBuild project configuration |
| `build.bat` | Command-line build script |
| `Check-Environment.ps1` | Prerequisites checker |

### 4. Documentation ✅

| Document | Purpose |
|----------|---------|
| `README.md` | User documentation, build instructions |
| `CONTRIBUTING.md` | Developer guide, coding standards |
| `ROADMAP.md` | Development phases and timeline |
| This report | Progress summary |

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Windows Audio System                      │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │   Stremio    │    │  VLC/Kodi    │    │   MPC-HC     │  │
│  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘  │
│         │                   │                   │          │
│         └───────────────────┼───────────────────┘          │
│                             ↓                              │
│              KS (Kernel Streaming)                          │
│                             ↓                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │           CavernAudioDriver.sys (Kernel Mode)         │  │
│  │  ┌─────────────────────────────────────────────────┐ │  │
│  │  │           IMiniportWaveRT Interface              │ │  │
│  │  │  - Format negotiation (PCM, E-AC3, TrueHD)      │ │  │
│  │  │  - DMA buffer management                         │ │  │
│  │  │  - Stream state control                          │ │  │
│  │  └─────────────────────────────────────────────────┘ │  │
│  │                         ↓                            │  │
│  │  ┌─────────────────────────────────────────────────┐ │  │
│  │  │         Audio Processing Thread                  │ │  │
│  │  │  - Read from DMA buffer                         │ │  │
│  │  │  - Detect format (PCM vs encoded)               │ │  │
│  │  │  - Forward to named pipe                        │ │  │
│  │  └─────────────────────────────────────────────────┘ │  │
│  └──────────────────────────────────────────────────────┘  │
│                             ↓                              │
│              Named Pipe: \\??\\pipe\\CavernAudioPipe         │
└─────────────────────────────────────────────────────────────┘
                             ↓
┌─────────────────────────────────────────────────────────────┐
│              CavernPipeServer.exe (User Mode)               │
│  - Decode Dolby Atmos objects                               │
│  - Render to multi-channel PCM                              │
└─────────────────────────────────────────────────────────────┘
                             ↓
┌─────────────────────────────────────────────────────────────┐
│              Snapserver (User Mode)                         │
│  - Stream audio over network                                │
└─────────────────────────────────────────────────────────────┘
                             ↓
┌─────────────────────────────────────────────────────────────┐
│              ESP32-C5 Speakers (Network)                    │
│  - Receive audio stream                                     │
│  - Play synchronized audio                                  │
└─────────────────────────────────────────────────────────────┘
```

---

## What's Working

1. ✅ **Driver Loads**: Basic WDF driver structure compiles and loads
2. ✅ **Device Appears**: Virtual audio device registers with Windows
3. ✅ **Format Support**: Infrastructure for PCM and E-AC3 formats
4. ✅ **Named Pipe**: Connection code to CavernAudioPipe implemented
5. ✅ **Build System**: Complete Visual Studio project with build scripts
6. ✅ **Documentation**: Comprehensive README and contributor guide

---

## What's NOT Working (TODO)

### Critical Missing Components

| Feature | Status | Priority | Effort |
|---------|--------|----------|--------|
| **Audio Processing Thread** | ❌ Not implemented | **CRITICAL** | 1-2 weeks |
| **Format Detection** | ❌ Stub only | **CRITICAL** | 1 week |
| **Data Forwarding** | ❌ Not implemented | **CRITICAL** | 1 week |
| **Exclusive Mode** | ❌ Not implemented | High | 2 weeks |
| **TrueHD Support** | ❌ Not implemented | Medium | 1 week |

### Audio Processing Thread (MOST IMPORTANT)

The driver currently lacks the core audio processing loop:

```c
// TODO: Implement in CavernProcessAudioThread()

VOID CavernProcessAudioThread(_In_ PVOID Context)
{
    while (running) {
        // 1. Read audio data from DMA buffer
        // 2. Detect if it's PCM or E-AC3/TrueHD
        // 3. If encoded, forward raw bytes to named pipe
        // 4. Update playback position
        // 5. Handle buffer wrap-around
    }
}
```

This thread needs to:
- Run at real-time priority
- Read from DMA buffer without glitches
- Detect format by examining sync words
- Forward data to CavernPipeServer
- Handle errors gracefully

---

## Next Development Steps

### Phase 2A: Audio Processing (Current Priority)

**Week 1-2: Implement Audio Thread**

1. Create kernel thread in `DriverEntry` or `PrepareHardware`
2. Implement DMA buffer reading
3. Add format detection:
   - E-AC3 sync word: `0x0B77`
   - TrueHD sync word: `0xF8726FBA`
   - PCM: No sync word, continuous samples
4. Forward data to named pipe
5. Test with actual audio playback

**Files to modify:**
- `src/CavernAudioDriver.c`: Add thread creation
- `src/MiniportWaveRT.c`: Implement processing thread

### Phase 2B: Format Support

**Week 3: Format Negotiation**

1. Implement `KSPROPERTY` handlers
2. Advertise E-AC3 and TrueHD support properly
3. Handle format switching at runtime
4. Test with media players

**Files to modify:**
- `src/MiniportWaveRT.c`: Add property handlers

### Phase 2C: Exclusive Mode

**Week 4-5: Passthrough Mode**

1. Support `AUDCLNT_SHAREMODE_EXCLUSIVE`
2. Disable Windows audio mixing
3. Ensure raw bitstream reaches driver unmolested
4. Test with Stremio, VLC, etc.

---

## Testing Plan

### Unit Tests (TODO)

```c
// tests/TestFormatDetection.c
Test_EAC3_Detection();
Test_TrueHD_Detection();
Test_PCM_Detection();
Test_InvalidData();
```

### Integration Tests

1. **Driver Loading**
   ```powershell
   pnputil /add-driver CavernAudioDriver.inf /install
   # Verify no BSOD, device appears in Device Manager
   ```

2. **Audio Playback**
   ```powershell
   # Play test file
   ffmpeg -i test-eac3.mkv -vn -acodec copy -f data - | \
       dotnet CavernPipeClient.dll pipe://stdin
   # Verify audio reaches speakers
   ```

3. **Application Integration**
   - Test with Stremio (primary goal)
   - Test with VLC
   - Test with MPC-HC
   - Test with Kodi

---

## Build Instructions

### Prerequisites Check

```powershell
.\Check-Environment.ps1
```

Expected output:
```
✓ Visual Studio 2022: PASS
✓ WDK Libraries: PASS
✓ Windows SDK: PASS
✓ MSBuild: PASS
⚠ Test Signing: WARN (must enable for testing)
```

### Build Driver

```powershell
.\build.bat
# Or specify configuration:
.\build.bat Release
```

Output:
```
bin\x64\Debug\
├── CavernAudioDriver.sys    # Driver binary
├── CavernAudioDriver.inf    # Installation file
└── CavernAudioDriver.cat    # Security catalog
```

### Install for Testing

**WARNING: Only on test machine!**

```powershell
# Enable test signing (admin required)
bcdedit /set testsigning on
Restart-Computer

# Install driver (admin required)
cd bin\x64\Debug
pnputil /add-driver CavernAudioDriver.inf /install

# Verify
Get-PnpDevice -Class "Media" | Where-Object {$_.Name -like "*Cavern*"}
```

---

## Known Limitations

1. **Not Production Ready**: Requires extensive testing
2. **Test Signing Required**: Production needs EV code signing cert ($400/year)
3. **Format Support Limited**: E-AC3 detection stubbed, TrueHD not implemented
4. **No Audio Yet**: Processing thread not implemented
5. **Windows Only**: No Linux/macOS support planned

---

## Estimated Completion

| Phase | Duration | Status |
|-------|----------|--------|
| 1. Skeleton | 1 day | ✅ Complete |
| 2. Audio Processing | 2 weeks | 🚧 In Progress |
| 3. Format Support | 1 week | ⏳ Not Started |
| 4. Passthrough Mode | 2 weeks | ⏳ Not Started |
| 5. Testing | 2 weeks | ⏳ Not Started |
| 6. Code Signing | 1 week | ⏳ Not Started |
| **Total** | **~8-9 weeks** | **~20% Complete** |

---

## Resources for Continuing Development

### Key Documentation
- [MSDN: WaveRT Miniport](https://docs.microsoft.com/windows-hardware/drivers/audio/wavert-miniport)
- [MSDN: IMiniportWaveRT](https://docs.microsoft.com/windows-hardware/drivers/ddi/portcls/nn-portcls-iminiportwavert)
- [SysVAD Sample](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad)

### Reference Projects
- [Scream](https://github.com/duncanthrax/scream) - Virtual audio driver
- [VB-Cable](https://vb-audio.com/Cable/) - Commercial virtual audio

### Dolby Formats
- [E-AC3 Specification](https://www.atsc.org/wp-content/uploads/2015/03/A52-2010-Standard.pdf)
- [TrueHD Whitepaper](https://www.dolby.com/us/en/technologies/dolby-truehd.html)

---

## Summary

The CavernAudioDriver project has progressed from concept to a **functional skeleton** with:
- ✅ Complete driver framework
- ✅ WaveRT miniport implementation
- ✅ Build system and documentation
- ✅ GitHub repository with contributor guide

**Next immediate step:** Implement the audio processing thread to forward data from DMA buffer to CavernPipeServer.

**Estimated time to working audio:** 2-3 weeks of focused development.

**Repository:** https://github.com/Glider95/CavernAudioDriver

---

*Report generated on February 21, 2026*
