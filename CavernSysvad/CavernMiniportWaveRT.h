/***************************************************************************
 * CavernMiniportWaveRT.h
 * 
 * Minimal WaveRT Miniport for Cavern Audio Driver
 * Using basic C++ with PortCls
 ***************************************************************************/

#ifndef _CAVERN_MINWAVERT_H_
#define _CAVERN_MINWAVERT_H_

#include <ntddk.h>
#include <wdf.h>
#include <portcls.h>
#include <stdunk.h>
#include <ks.h>
#include <ksmedia.h>

// Pool tag
#define CAVERN_WAVERT_POOLTAG 'navC'

// Forward declarations
class CCavernMiniportWaveRT;
class CCavernMiniportWaveRTStream;

typedef CCavernMiniportWaveRT *PCCavernMiniportWaveRT;
typedef CCavernMiniportWaveRTStream *PCCavernMiniportWaveRTStream;

//=============================================================================
// CCavernMiniportWaveRTStream - Stream class
//=============================================================================
class CCavernMiniportWaveRTStream : public CUnknown
{
public:
    // IUnknown
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    STDMETHODIMP QueryInterface(REFIID InterfaceId, PVOID *Object);
    
    // Constructor/Destructor
    CCavernMiniportWaveRTStream(PUNKNOWN OuterUnknown);
    ~CCavernMiniportWaveRTStream();
    
    // Initialization
    NTSTATUS Init(
        _In_ PCCavernMiniportWaveRT Miniport,
        _In_ PPORTWAVERTSTREAM PortStream,
        _In_ ULONG Pin,
        _In_ BOOLEAN Capture,
        _In_ PKSDATAFORMAT DataFormat
    );
    
    // WaveRT Stream methods
    STDMETHODIMP_(NTSTATUS) SetState(_In_ KSSTATE State);
    STDMETHODIMP_(NTSTATUS) GetPosition(_Out_ PKSAUDIO_POSITION Position);
    STDMETHODIMP_(NTSTATUS) AllocateAudioBuffer(
        _In_ ULONG BufferSize,
        _Out_ PVOID *Buffer,
        _Out_ PULONG OffsetFromFirstPage,
        _Out_ PULONG ActualBufferSize
    );
    STDMETHODIMP_(VOID) FreeAudioBuffer(_In_opt_ PVOID Buffer);
    STDMETHODIMP_(NTSTATUS) GetClockRegister(_Out_ PKSRTAUDIO_HWREGISTER Register);
    STDMETHODIMP_(NTSTATUS) GetPositionRegister(_Out_ PKSRTAUDIO_HWREGISTER Register);
    STDMETHODIMP_(NTSTATUS) SetWritePacket(_In_ ULONG PacketNumber, _In_ DWORD Flags, _In_ ULONG EosPacketLength);
    STDMETHODIMP_(NTSTATUS) GetReadPacket(_Out_ ULONG *PacketNumber, _Out_ DWORD *Flags, _Out_ ULONG *EosPacketLength);
    
    // Pipe forwarding
    NTSTATUS ConnectPipe();
    VOID DisconnectPipe();
    NTSTATUS ForwardToPipe(_In_reads_bytes_(Length) PVOID Buffer, _In_ ULONG Length);
    VOID WriteBytes(_In_ ULONG ByteDisplacement);

private:
    PCCavernMiniportWaveRT    m_pMiniport;
    PPORTWAVERTSTREAM         m_pPortStream;
    KSSTATE                   m_State;
    BOOLEAN                   m_Running;
    PVOID                     m_pDmaBuffer;
    ULONG                     m_ulDmaBufferSize;
    ULONGLONG                 m_ullLinearPosition;
    PWAVEFORMATEXTENSIBLE     m_pWfExt;
    HANDLE                    m_hPipe;
    UNICODE_STRING            m_PipeName;
    KSPIN_LOCK                m_PipeLock;
    BOOLEAN                   m_PipeConnected;
};

//=============================================================================
// CCavernMiniportWaveRT - Miniport class  
//=============================================================================
class CCavernMiniportWaveRT : public IMiniportWaveRT, public CUnknown
{
public:
    // IUnknown
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    STDMETHODIMP QueryInterface(REFIID InterfaceId, PVOID *Object);
    
    // IMiniportWaveRT
    STDMETHODIMP_(NTSTATUS) GetDescription(_Out_ PPCFILTER_DESCRIPTOR *Description);
    STDMETHODIMP_(NTSTATUS) NewStream(
        _Out_ PMiniportWaveRTStream *Stream,
        _In_ PPORTWAVERTSTREAM PortStream,
        _In_ ULONG Pin,
        _In_ BOOLEAN Capture,
        _In_ PKSDATAFORMAT DataFormat
    );
    STDMETHODIMP_(NTSTATUS) GetDeviceDescription(_Out_ PDEVICE_DESCRIPTION Description);
    STDMETHODIMP_(NTSTATUS) SetDeviceDescription(_In_ PDEVICE_DESCRIPTION Description);
    STDMETHODIMP_(NTSTATUS) GetDmaAdapter(_Out_ PDMA_ADAPTER *DmaAdapter);
    STDMETHODIMP_(NTSTATUS) SetDmaAdapter(_In_ PDMA_ADAPTER DmaAdapter);
    STDMETHODIMP_(NTSTATUS) GetPowerState(_Out_ DEVICE_POWER_STATE *PowerState);
    STDMETHODIMP_(NTSTATUS) SetPowerState(_In_ DEVICE_POWER_STATE PowerState);
    STDMETHODIMP_(NTSTATUS) GetAudioEngineDescriptor(
        _In_ ULONG PinId,
        _Out_ PKSAUDIOENGINE_DESCRIPTOR Descriptor
    );
    STDMETHODIMP_(NTSTATUS) GetAudioEngineBufferSizeRange(
        _In_ ULONG PinId,
        _Out_ PKSAUDIOENGINE_BUFFER_SIZE_RANGE BufferSizeRange
    );
    STDMETHODIMP_(NTSTATUS) SetOffloadPinStream(
        _In_ ULONG PinId,
        _In_ PKSOFFLOAD_PIN_OFFLOAD Stream
    );
    STDMETHODIMP_(NTSTATUS) GetStreamCount(_Out_ PULONG StreamCount);
    STDMETHODIMP_(NTSTATUS) GetStream(_In_ ULONG StreamIndex, _Out_ PMiniportWaveRTStream *Stream);
    STDMETHODIMP_(NTSTATUS) GetPerformanceCounters(_Out_ PKSAUDIOMODULE_PERFORMANCE_COUNTERS Counters);
    STDMETHODIMP_(NTSTATUS) ValidateFormat(_In_ PKSDATAFORMAT Format, _In_ ULONG PinId);
    
    // Constructor/Destructor
    CCavernMiniportWaveRT(PUNKNOWN OuterUnknown);
    ~CCavernMiniportWaveRT();
    
    // Initialization
    NTSTATUS Init(_In_ PPORTWAVERT Port);
    
    // Stream management
    NTSTATUS StreamCreated(_In_ PCCavernMiniportWaveRTStream Stream);
    NTSTATUS StreamClosed(_In_ PCCavernMiniportWaveRTStream Stream);

private:
    PPORTWAVERT                  m_pPort;
    PCCavernMiniportWaveRTStream m_pStream;
};

// Create function
NTSTATUS CreateCavernMiniportWaveRT(
    _Out_ PUNKNOWN *Unknown,
    _In_ REFCLSID ClassId,
    _In_opt_ PUNKNOWN UnknownOuter,
    _In_ POOL_FLAGS PoolFlags
);

#endif // _CAVERN_MINWAVERT_H_
