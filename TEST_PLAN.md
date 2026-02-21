# CavernAudioDriver Test Plan

## Test Environment Requirements

### Build Requirements (NOT CURRENTLY AVAILABLE)
- [ ] Windows Driver Kit (WDK) 10
- [ ] Visual Studio 2022 with C++ workload
- [ ] Windows SDK 10.0.22000.0 or later

**Status:** WDK not installed on this system. Driver cannot be built without it.

### Test Machine Requirements
- [ ] Separate test PC or VM (DO NOT test on primary machine)
- [ ] Windows 10/11 with test signing enabled
- [ ] Kernel debugger configured (WinDbg)

---

## Available Test Files

### 1. Dolby Atmos Demo (dolby-demo.mp4)
- **Location:** `cavern-project/dolby-demo.mp4`
- **Size:** 7.62 MB
- **Expected Audio:** Dolby Atmos (E-AC3 or TrueHD)
- **Test Purpose:** Verify Atmos passthrough

### 2. Raw PCM Test (test.pcm)
- **Location:** `cavern-project/test.pcm`
- **Size:** 5.49 MB
- **Format:** Raw PCM (6ch @ 48kHz, 16-bit)
- **Test Purpose:** Verify basic audio streaming

### 3. Test Tones (dolby-demo2.wav)
- **Location:** `cavern-project/dolby-demo2.wav`
- **Size:** 0 MB (appears empty/corrupted)
- **Status:** Cannot use for testing

---

## Pre-Build Tests (Code Review)

### Test 1: Syntax Check ✅
```powershell
cd CavernAudioDriver
dotnet build CavernAudioDriver.vcxproj --verbosity detailed
```
**Expected:** Compilation errors due to missing WDK libraries

### Test 2: Static Analysis
```powershell
# If WDK were available:
# inf2cat /driver:. /os:10_X64
```

---

## Post-Build Test Plan (Once WDK Installed)

### Phase 1: Driver Loading Tests

#### Test 1.1: Driver Installation
```powershell
# Enable test signing (admin)
bcdedit /set testsigning on
Restart-Computer

# Install driver (admin)
cd CavernAudioDriver\bin\x64\Debug
pnputil /add-driver CavernAudioDriver.inf /install

# Verify
Get-PnpDevice -Class "Media" | Where-Object {$_.Name -like "*Cavern*"}
```
**Expected Result:** 
- Driver installs without errors
- Device appears in Device Manager
- No BSOD

#### Test 1.2: Device Enumeration
```powershell
# Check if device appears in Sound settings
mmsys.cpl
```
**Expected Result:**
- "Cavern Atmos Virtual Audio Device" listed in Playback devices
- Device can be set as default

---

### Phase 2: Basic Audio Tests

#### Test 2.1: PCM Playback (test.pcm)
```powershell
# Stream raw PCM to driver
cd cavern-project
ffmpeg -f s16le -ar 48000 -ac 6 -i test.pcm -f wav - | 
    dotnet src\CavernPipeClient\bin\Release\net8.0\CavernPipeClient.dll pipe://stdin
```
**Expected Result:**
- Audio plays through Cavern pipeline
- No distortion
- Correct channel mapping

#### Test 2.2: Format Detection
```powershell
# Check driver logs for format detection
# View in WinDbg or Event Viewer
```
**Expected Result:**
- Driver logs show "Format: PCM detected"
- Correct sample rate (48000 Hz)
- Correct channels (6)

---

### Phase 3: Dolby Atmos Tests

#### Test 3.1: E-AC3 Detection (dolby-demo.mp4)
```powershell
# Extract and stream E-AC3
ffmpeg -i dolby-demo.mp4 -vn -acodec copy -f data - | 
    dotnet src\CavernPipeClient\bin\Release\net8.0\CavernPipeClient.dll pipe://stdin
```
**Expected Result:**
- Driver detects E-AC3 sync word (0x0B77)
- Raw bitstream forwarded to CavernPipeServer
- Cavern decodes Atmos objects
- Audio plays on wireless speakers

#### Test 3.2: Passthrough Verification
```powershell
# Check that audio is NOT decoded in driver
# Verify raw bytes are forwarded unchanged
```
**Expected Result:**
- Bytes match input exactly (passthrough)
- No PCM conversion in driver

---

### Phase 4: Application Integration Tests

#### Test 4.1: Stremio Integration
```powershell
# Configure Stremio to use Cavern Audio Device
# Play Atmos content
# Verify audio routes through driver
```
**Expected Result:**
- Stremio recognizes Cavern device
- Atmos audio streams correctly
- No audio on PC speakers (routed to wireless)

#### Test 4.2: VLC Integration
```powershell
# VLC Preferences → Audio → Output
# Select "Cavern Atmos Virtual Audio Device"
# Play Atmos file
```
**Expected Result:**
- VLC outputs to Cavern driver
- Audio processed through pipeline

#### Test 4.3: MPC-HC Integration
```powershell
# MPC-HC → Options → Playback → Output
# Audio Renderer: Cavern Audio Device
# Enable bitstream passthrough
```
**Expected Result:**
- Bitstream passthrough works
- Raw E-AC3 reaches driver

---

### Phase 5: Stress Tests

#### Test 5.1: Long Duration Playback
```powershell
# Play audio for 2+ hours
# Monitor for:
# - Memory leaks
# - Buffer overruns/underruns
# - System stability
```
**Expected Result:**
- No BSOD
- Stable memory usage
- Continuous audio playback

#### Test 5.2: Format Switching
```powershell
# Play PCM → Stop → Play E-AC3 → Stop → Play PCM
# Rapid format changes
```
**Expected Result:**
- Driver handles format switches
- No crashes
- Clean transitions

#### Test 5.3: Multiple Applications
```powershell
# Try to use driver from multiple apps simultaneously
```
**Expected Result:**
- Exclusive mode prevents conflicts
- OR: Proper mixing if shared mode

---

## Debugging Tests

### Debug Test 1: Kernel Debugging
```powershell
# Setup kernel debugging
# Break into driver code
# Step through audio processing
```

### Debug Test 2: Memory Leak Detection
```powershell
# Enable Driver Verifier
verifier /flags 0x1 /driver CavernAudioDriver.sys

# Run tests
# Check for pool leaks
!poolused
```

---

## Current Blockers

### Blocker 1: WDK Not Installed
**Impact:** Cannot build driver  
**Solution:** Install WDK from Microsoft

### Blocker 2: Audio Processing Thread Not Implemented
**Impact:** Driver won't forward audio  
**Solution:** Implement `CavernProcessAudioThread()`

### Blocker 3: Test Machine Required
**Impact:** Cannot test kernel driver safely  
**Solution:** Setup VM or secondary PC

---

## Test Execution Priority

1. **CRITICAL:** Install WDK and build driver
2. **HIGH:** Implement audio processing thread
3. **HIGH:** Test PCM playback (test.pcm)
4. **MEDIUM:** Test E-AC3 passthrough (dolby-demo.mp4)
5. **MEDIUM:** Application integration tests
6. **LOW:** Stress tests and optimization

---

## Success Criteria

✅ Driver installs without BSOD  
✅ Device appears in Sound settings  
✅ PCM audio plays correctly  
✅ E-AC3 bitstream passes through unchanged  
✅ Stremio/VLC can use the device  
✅ Audio reaches wireless speakers  
✅ Stable for 2+ hour playback  

---

## Notes

- Testing kernel drivers is inherently risky
- Always use test machine, never primary PC
- Enable kernel debugging before testing
- Have recovery plan (safe mode, system restore)

