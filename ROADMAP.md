# CavernAudioDriver Development Roadmap

## Current Status: Phase 1 - Skeleton Complete ✓

The basic driver structure is in place. Now implementing actual audio functionality.

---

## Phase 2: Core Driver Implementation (Current)

### Week 1-2: Miniport Audio Support
- [ ] Implement WaveRT miniport interface
- [ ] Add audio format descriptors
- [ ] Create DMA buffer management
- [ ] Implement basic audio streaming

**Key Files:**
- `src/MiniportWaveRT.c` - WaveRT miniport implementation
- `include/MiniportWaveRT.h` - Miniport header

**Learning Resources:**
- Microsoft SysVAD sample
- PortCls documentation

### Week 3-4: Format Negotiation
- [ ] Implement KSPROPERTY support
- [ ] Add format property sets
- [ ] Support E-AC3 format descriptor
- [ ] Support TrueHD format descriptor
- [ ] Handle format changes

**Challenge:** Windows apps need to recognize this as a "Dolby capable" device

### Week 5-6: Data Streaming
- [ ] Implement audio data flow
- [ ] Create kernel-mode threads
- [ ] Buffer management (circular buffers)
- [ ] Timestamp handling
- [ ] Synchronization

---

## Phase 3: Cavern Integration

### Week 7-8: Named Pipe Communication
- [ ] Create robust pipe client
- [ ] Handle pipe connection/disconnection
- [ ] Error recovery
- [ ] Buffering between driver and user-mode

### Week 9-10: Format Detection
- [ ] Detect E-AC3 sync words
- [ ] Detect TrueHD sync words
- [ ] Parse metadata
- [ ] Handle format switching

---

## Phase 4: Passthrough Support

### Week 11-12: Exclusive Mode
- [ ] Support AUDCLNT_SHAREMODE_EXCLUSIVE
- [ ] Disable mixing in exclusive mode
- [ ] Pass raw bitstream without modification

### Week 13-14: Dolby Certification
- [ ] Implement required Dolby interfaces
- [ ] Test with Dolby test suite
- [ ] Handle edge cases

---

## Phase 5: Testing & Validation

### Week 15-16: Testing
- [ ] Unit tests for each component
- [ ] Integration tests
- [ ] Test with real Atmos content
- [ ] Test with Stremio
- [ ] Test with VLC
- [ ] Test with MPC-HC

### Week 17-18: Debugging
- [ ] Kernel debugging setup
- [ ] Performance profiling
- [ ] Memory leak detection
- [ ] Stability testing

---

## Phase 6: Distribution

### Week 19-20: Signing & Packaging
- [ ] Code signing certificate
- [ ] WHQL submission (optional)
- [ ] Installer creation
- [ ] Documentation

---

## Technical Challenges

### Challenge 1: Windows Audio Engine
**Problem:** Windows decodes audio before passing to drivers

**Approach:**
- Implement as "HDMI-like" device
- Support exclusive mode
- Advertise as Dolby Digital Plus capable

### Challenge 2: Format Detection
**Problem:** Need to distinguish PCM from E-AC3/TrueHD

**Approach:**
- Parse first few bytes for sync words
- E-AC3: 0x0B77 syncword
- TrueHD: 0xF8726FBA syncword

### Challenge 3: Timing
**Problem:** Kernel streaming requires precise timing

**Approach:**
- Use KeQueryPerformanceCounter
- Implement proper ISR/DPC
- Buffer management

---

## Resources Needed

### Software
- Windows Driver Kit (installed ✓)
- Visual Studio 2022 (installed ✓)
- WinDbg for kernel debugging

### Hardware
- Test machine (can use current PC in test mode)
- Kernel debugging cable (or use network debugging)

### Knowledge
- Windows driver architecture
- Kernel programming
- Audio processing
- Dolby formats

---

## Next Immediate Steps

1. **Setup Kernel Debugging**
   - Configure target machine
   - Setup host debugger
   - Test with simple driver

2. **Implement Miniport**
   - Study SysVAD sample
   - Create basic WaveRT miniport
   - Get "device appears in Sound settings"

3. **Test Format Support**
   - Query supported formats
   - Advertise E-AC3 support
   - Test with media player

---

## Success Criteria

✓ Driver loads without crashing  
✓ Device appears in Windows Sound settings  
✓ Media player can select device  
✓ Device accepts E-AC3 format  
✓ Raw bitstream forwarded to Cavern  
✓ Audio plays on wireless speakers  
✓ Stable for extended playback  

---

## Notes

- This is a significant undertaking (~3-4 months for experienced driver dev)
- Test signing required during development
- Production requires EV code signing cert (~$400/year)
- Consider partnering with existing driver developer

