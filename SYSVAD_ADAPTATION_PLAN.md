# SysVAD Adaptation Plan

## Overview
Adapt Microsoft's SysVAD (Virtual Audio Device) sample driver to add pipe forwarding for Dolby Atmos passthrough.

## SysVAD Source
- Microsoft Windows Driver Samples (GitHub)
- Location: `https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad`
- License: MIT License

## Adaptation Steps

### 1. Download SysVAD Sample
```
- WaveRT miniport implementation (working)
- Table-driven filter structure
- Simple audio processing
```

### 2. Modifications Needed

#### A. Remove Unneeded Components
- Remove phone jack/Bluetooth features
- Remove offload processing
- Keep only basic WaveRT render

#### B. Add Pipe Forwarding
- Add named pipe client (`\\.\pipe\CavernAudioPipe`)
- Hook into `WriteSilence` or `WriteData` function
- Forward raw audio buffers to pipe

#### C. Configure for Passthrough
- Set format to support packed PCM (for bitstream)
- Disable audio processing (volume, mute)
- Pass through raw data unchanged

### 3. File Structure
```
CavernAudioDriver/
├── src/
│   ├── CavernAudioDriver.c      # Driver entry (from SysVAD)
│   ├── MiniportWaveRT.c         # WaveRT miniport (from SysVAD)
│   ├── Common.cpp               # Common utilities (from SysVAD)
│   └── PipeForward.c            # NEW: Pipe forwarding
├── include/
│   ├── CavernAudioDriver.h      # Main header
│   ├── MiniportWaveRT.h         # Miniport header
│   └── SysVADhelpers.h          # SysVAD utilities
└── CavernAudioDriver.inf        # Updated INF
```

### 4. Key Functions to Modify

#### In MiniportWaveRT.c:
```c
// Existing SysVAD function:
NTSTATUS WriteData(...)
{
    // Copy audio to DMA buffer
    // TODO: Add pipe write here
}
```

#### New Function - PipeForward.c:
```c
NTSTATUS CavernForwardToPipe(
    PVOID Buffer,
    SIZE_T Length
)
{
    // Open pipe to CavernPipeServer
    // Write audio data
    // Handle disconnects
}
```

### 5. Build Configuration
- Use SysVAD project structure
- Keep portcls.lib linkage
- Add our pipe code

### 6. Testing
1. Build SysVAD unmodified first (verify it works)
2. Add pipe forwarding
3. Test with CavernPipeServer

## Timeline
- Download & setup: 1 hour
- Strip unneeded features: 2 hours
- Add pipe forwarding: 3 hours
- Testing: 2 hours
- **Total: 1 day**

## Risks
- SysVAD is complex - need to understand structure
- May need to preserve certain interfaces for Windows audio
- Licensing: Must include Microsoft copyright notices

## Next Action
Download SysVAD sample and begin adaptation.
