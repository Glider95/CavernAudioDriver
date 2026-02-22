# CavernAudioDriver Testing Guide

## Prerequisites

1. **Windows 10/11 with Test Signing Enabled**
2. **Visual C++ Redistributables** (for .NET 8.0 apps)
3. **Snapserver** running on localhost:1705 (optional, for forwarding)

---

## Step 1: Enable Test Signing

**Important:** This allows unsigned drivers to load. Required for development.

```powershell
# Run as Administrator
bcdedit /set testsigning on
```

**Reboot required!**

To verify:
```powershell
bcdedit /enum | findstr test
```

You should see: `testsigning Yes`

---

## Step 2: Install the Driver

### Option A: Manual Installation

1. Copy driver files to a folder (e.g., `C:\CavernDriver\`):
   - `CavernAudioDriver.sys`
   - `CavernAudioDriver.inf`
   - `cavernaudiodriver.cat`

2. Install using pnputil:
```powershell
# Run as Administrator
pnputil /add-driver C:\CavernDriver\CavernAudioDriver.inf /install
```

3. Verify installation:
```powershell
# List audio devices
Get-PnpDevice -Class Media | Where-Object { $_.Name -like "*Cavern*" }

# Or check Device Manager
# Look for: "Cavern Atmos Virtual Audio Device"
```

### Option B: Using Install Script

```powershell
# Run Install-Driver.ps1 as Administrator
.\Install-Driver.ps1
```

---

## Step 3: Start CavernPipeServer

The pipe server receives audio from the driver and forwards it to snapserver.

```powershell
# Run the pipe server
.\tools\CavernPipeServer\bin\Release\net8.0\win-x64\publish\CavernPipeServer.exe
```

Expected output:
```
╔══════════════════════════════════════════════════════════════╗
║          Cavern Pipe Server v1.0 - Dolby Atmos Bridge        ║
╚══════════════════════════════════════════════════════════════╝

Named Pipe:  \\.\pipe\CavernAudioPipe
Snapserver:  localhost:1705
Buffer Size: 65536 bytes

Press Ctrl+C to exit

[Waiting for driver connection...]
```

---

## Step 4: Test with Audio Playback

### Test 1: Verify Driver Loads

```powershell
# Check if driver is loaded
Get-Process | Where-Object { $_.ProcessName -like "*Cavern*" }

# Check driver status
sc query CavernAudio
```

### Test 2: Play Dolby Atmos Content

1. Open **Windows Settings** > **System** > **Sound**
2. Set **"Cavern Atmos Virtual Audio Device"** as default playback device
3. Play Dolby Atmos content using:
   - VLC with passthrough enabled
   - Movies & TV app
   - Dolby Access app test content

### Test 3: Check PipeServer Output

When audio plays, you should see:
```
[Driver connected!]
[Connected to Snapserver]
[Capturing to: cavern_capture_20250222_080100.raw]
[Format Detected: E-AC3 / Dolby Digital Plus]
[Atmos Support: YES]
[Stats] Packets: 1,234 | Bytes: 4,567,890 | Rate: 384,000 B/s
```

---

## Step 5: Verify End-to-End

### Check Raw Capture File

```powershell
# The raw capture file contains the bitstream
# You can inspect it with a hex editor

# Check file size (should grow while playing)
Get-ChildItem cavern_capture_*.raw | Select-Object Name, Length, LastWriteTime
```

### Check Snapserver

If snapserver is running:
```bash
# Check snapserver status
curl http://localhost:1780/jsonrpc

# Or check logs
tail -f /var/log/snapserver.log
```

---

## Troubleshooting

### Driver Won't Load

**Symptom:** `sc query CavernAudio` shows STOPPED

**Solutions:**
1. Verify test signing is enabled (requires reboot)
2. Check Event Viewer > Windows Logs > System for driver errors
3. Try manual driver update in Device Manager

### PipeServer Can't Connect

**Symptom:** "Waiting for driver connection..." forever

**Solutions:**
1. Ensure driver is loaded: `Get-PnpDevice -Class Media`
2. Check pipe name matches: `\\.\pipe\CavernAudioPipe`
3. Run PipeServer as Administrator

### No Audio Detected

**Symptom:** Format not detected, no packets received

**Solutions:**
1. Verify Cavern device is default playback device
2. Check app audio settings (passthrough/encode settings)
3. Try different content (known Dolby Atmos file)

### Snapserver Connection Failed

**Symptom:** "Snapserver connection failed"

**Solutions:**
1. Ensure snapserver is running: `netstat -an | findstr 1705`
2. Check firewall rules
3. Verify snapserver IP/port settings

---

## Uninstall

```powershell
# Run as Administrator

# Uninstall driver
pnputil /delete-driver oem##.inf /uninstall /force

# Or use Device Manager
# 1. Open Device Manager
# 2. Find "Cavern Atmos Virtual Audio Device"
# 3. Right-click > Uninstall device

# Disable test signing (optional)
bcdedit /set testsigning off
```

---

## Debug Output

To see driver debug messages:

1. Install **DebugView** from Sysinternals
2. Run as Administrator
3. Enable **Capture Kernel** (Ctrl+K)
4. Look for messages starting with `CavernAudioDriver:`

Or use WinDbg:
```
!dbgprint
```

---

## Test Files

Sample Dolby Atmos test files:
- `dolby-demo.mp4` (in workspace/cavern-project/)
- Dolby Access app (Microsoft Store)
- Netflix/Disney+ Atmos content

---

*Last Updated: 2026-02-22*
