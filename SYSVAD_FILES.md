# SysVAD Files Located

Found in: `sysvad-src/audio/sysvad/`

## Key Files to Adapt:

### Core Files (Required):
- **adapter.cpp/h** - Main adapter, driver entry
- **common.cpp/h** - Common utilities, helper functions
- **basetopo.cpp/h** - Base topology implementation
- **sysvad.h** - Main header file
- **kshelper.cpp/h** - KS (kernel streaming) helpers
- **hw.cpp/h** - Hardware abstraction

### Sample Implementations:
- **TabletAudioSample/** - Complete working example
  - minwavert.cpp - WaveRT miniport
  - minwavert.h - WaveRT header
  - minwavertstream.cpp - Stream implementation

### To Remove:
- APO/ - Audio processing objects
- A2dpHpDevice.cpp - Bluetooth headset
- BthhfpDevice.cpp - Bluetooth hands-free
- UsbHsDevice.cpp - USB headset
- KeywordDetectorAdapter/ - Keyword detection

## Adaptation Strategy:

1. Copy TabletAudioSample as base
2. Remove Bluetooth/USB features
3. Add pipe forwarding to WaveRT stream
4. Simplify topology to single render endpoint
5. Update INF file

## Next Steps:
1. Copy TabletAudioSample folder
2. Rename to CavernAudioDriver
3. Modify stream WriteData to forward to pipe
4. Build and test
