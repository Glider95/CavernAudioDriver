# Troubleshooting: Driver Installed But Not Visible

## Check These Steps:

### 1. Check Device Manager
Open Device Manager and look in these categories:
- **"Sound, video and game controllers"** ← Should be here
- **"System devices"** ← Might be here instead

```powershell
# Find the device
Get-PnpDevice | Where-Object { $_.Name -like "*Cavern*" }
```

### 2. Check Driver Status
```powershell
# Check if driver service is running
sc query CavernAudio

# Check driver status
Get-PnpDevice -Class Media | Where-Object { $_.Name -like "*Cavern*" } | Select-Object Name, Status, InstanceId
```

### 3. Common Issues & Fixes

#### Issue: Device is disabled
```powershell
# Enable the device
Enable-PnpDevice -InstanceId (Get-PnpDevice | Where-Object { $_.Name -like "*Cavern*" }).InstanceId -Confirm:$false
```

#### Issue: Windows Audio service needs restart
```powershell
# Restart Windows Audio
Restart-Service -Name Audiosrv -Force
```

#### Issue: Device appears as "System device" instead of audio
This means the driver loaded but isn't registering as an audio endpoint. The driver needs to implement PortCls miniport interfaces properly.

**Workaround:** Try this alternative INF that uses software device registration:

```inf
; Add this section to force audio endpoint registration
[CavernAudioDevice.NTamd64.HW]
AddReg=CavernAudioDevice.HW.AddReg

[CavernAudioDevice.HW.AddReg]
HKR,,FriendlyName,,%CavernAudioDeviceDesc%
HKR,,DeviceDesc,,%CavernAudioDeviceDesc%
```

### 4. Full Reinstall Procedure

```powershell
# Run as Administrator

# 1. Remove old driver
$inf = pnputil /enum-drivers | Select-String -Pattern "CavernAudio" -Context 2 | Select-String "Published Name" | ForEach-Object { ($_ -split ":" )[1].Trim() }
if ($inf) {
    pnputil /delete-driver $inf /uninstall /force
}

# 2. Restart audio
Restart-Service -Name Audiosrv -Force

# 3. Reinstall
pnputil /add-driver C:\Path\To\CavernAudioDriver.inf /install

# 4. Restart audio again
Restart-Service -Name Audiosrv -Force

# 5. Check
Get-PnpDevice -Class Media
```

### 5. Check Windows Audio Settings
Sometimes the device exists but isn't enabled:
1. Right-click speaker icon in taskbar → **Sounds**
2. Go to **Playback** tab
3. Right-click in empty area → **Show Disabled Devices**
4. Look for "Cavern Atmos Virtual Audio Device"
5. If found, right-click → **Enable**

### 6. Debug Output
Check if driver is actually loading:
```powershell
# Check System event log
Get-WinEvent -FilterHashtable @{LogName='System'; ID=7034,7035,7036} | Where-Object { $_.Message -like "*Cavern*" }
```

### 7. Alternative: Manual Device Creation
If the driver still doesn't appear, create the device manually:

```powershell
# Create a software device (requires Windows 10 1903+)
# This is a workaround for the audio endpoint registration issue
```

## Root Cause

The driver is a WDF driver but to appear as an audio device in Sound settings, it needs to:
1. Implement PortCls miniport interfaces (IMiniportWaveRT, etc.)
2. Register with the Windows audio subsystem
3. Or use a software audio device approach

Our current driver is a **kernel-mode passthrough driver** that forwards audio to a pipe, but it doesn't implement the full audio driver interfaces needed to appear in Sound settings.

## Solution Options

### Option A: Use Virtual Audio Cable (Recommended for Testing)
Install VB-Cable or similar, then use our CavernPipeClient to capture from it.

### Option B: Complete PortCls Implementation
Implement full audio miniport interfaces (2-3 weeks work).

### Option C: Use Windows Software Audio Device
Create a user-mode audio endpoint using Windows.Media.Devices API.

---

**Quick Check - What does this show?**
```powershell
Get-PnpDevice | Where-Object { $_.Name -like "*Cavern*" } | Format-List Name, Status, Class, InstanceId
```
