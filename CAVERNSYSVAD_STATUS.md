# SysVAD Adaptation - Files Collected

## Status: Core files copied from Microsoft SysVAD sample

### Files Acquired:

**WaveRT Miniport (Core Audio):**
- minwavert.cpp/h - WaveRT miniport implementation
- minwavertstream.cpp - WaveRT stream handling

**Driver Framework:**
- adapter.cpp - Driver entry, device creation
- common.cpp/h - Common utilities
- basetopo.cpp/h - Base topology
- sysvad.h - Main header

**Topology Files (to be simplified):**
- hdmitopo.cpp/h - HDMI audio
- micintopo.cpp/h - Microphone input
- spdiftopo.cpp/h - SPDIF
- Various table headers (.h files)

### Next Steps:

1. **Create minimal project**
   - Remove HDMI, mic, SPDIF topologies
   - Keep only single render endpoint
   
2. **Add pipe forwarding**
   - Modify minwavertstream.cpp
   - Add WriteData hook
   - Forward to named pipe

3. **Simplify INF**
   - Remove componentized INFs
   - Create single simple INF

4. **Build and test**

### Simplification Strategy:

Instead of full SysVAD with multiple endpoints, create:
- One render miniport (minwavert)
- One simple topology
- Pipe forwarding in stream

This reduces complexity significantly.
