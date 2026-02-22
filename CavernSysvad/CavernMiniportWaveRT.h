/***************************************************************************
 * CavernMiniportWaveRT.h
 * 
 * Simplified WaveRT Miniport for Cavern Audio Driver
 * Based on Microsoft SysVAD sample
 ***************************************************************************/

#ifndef _CAVERN_MINWAVERT_H_
#define _CAVERN_MINWAVERT_H_

#include <portcls.h>
#include <ks.h>
#include <ksmedia.h>

// Forward declaration
class CCavernMiniportWaveRTStream;
typedef CCavernMiniportWaveRTStream *PCCavernMiniportWaveRTStream;

// Pool tag
#define CAVERN_WAVERT_POOLTAG 'navC'
#define CAVERN_PIPE_NAME L"\\??\\pipe\\CavernAudioPipe"

//=============================================================================
// CCavernMiniportWaveRTStream
// Simplified WaveRT stream with pipe forwarding
//=============================================================================
#pragma code_seg()
class CCavernMiniportWaveRTStream : public IMiniportWaveRTStream, public CUnknown
{
private:
    PCMiniportWaveRT            m_pMiniport;        // Parent miniport
    PPORTWAVERTSTREAM           m_pPortStream;      // Port stream interface
    
    // State
    KSSTATE                     m_State;
    BOOLEAN                     m_Running;
    
    // DMA buffer
    PVOID                       m_pDmaBuffer;
    ULONG                       m_ulDmaBufferSize;
    ULONGLONG                   m_ullLinearPosition;
    
    // Format
    PWAVEFORMATEXTENSIBLE       m_pWfExt;
    
    // Pipe connection
    HANDLE                      m_hPipe;
    UNICODE_STRING              m_PipeName;
    KSPIN_LOCK                  m_PipeLock;
    BOOLEAN                     m_PipeConnected;
    
public:
    DECLARE_STD_UNKNOWN();
    
    // Constructor/Destructor
    CCavernMiniportWaveRTStream(_In_ PUNKNOWN OuterUnknown)
        : CUnknown(OuterUnknown),
          m_pMiniport(NULL),
          m_pPortStream(NULL),
          m_State(KSSTATE_STOP),
          m_Running(FALSE),
          m_pDmaBuffer(NULL),
          m_ulDmaBufferSize(0),
          m_ullLinearPosition(0),
          m_pWfExt(NULL),
          m_hPipe(NULL),
          m_PipeConnected(FALSE)
    {
        KeInitializeSpinLock(&m_PipeLock);
        RtlInitUnicodeString(&m_PipeName, CAVERN_PIPE_NAME);
    }
    
    ~CCavernMiniportWaveRTStream();
    
    // Initialization
    NTSTATUS Init(
        _In_ PCMiniportWaveRT Miniport,
        _In_ PPORTWAVERTSTREAM PortStream,
        _In_ ULONG Pin,
        _In_ BOOLEAN Capture,
        _In_ PKSDATAFORMAT DataFormat
    );
    
    // IMiniportWaveRTStream
    IMP_IMiniportWaveRTStream;
    
    // Pipe forwarding
    NTSTATUS ConnectPipe();
    VOID DisconnectPipe();
    NTSTATUS ForwardToPipe(_In_reads_bytes_(Length) PVOID Buffer, _In_ ULONG Length);
    
    // Called from miniport to process audio
    VOID WriteBytes(_In_ ULONG ByteDisplacement);
};

typedef CCavernMiniportWaveRTStream *PCCavernMiniportWaveRTStream;

//=============================================================================
// CCavernMiniportWaveRT
// Simplified WaveRT miniport
//=============================================================================
#pragma code_seg()
class CCavernMiniportWaveRT : public IMiniportWaveRT, public CUnknown
{
private:
    PADAPTERCOMMON              m_pAdapterCommon;
    PPORTWAVERT                 m_pPort;
    
    // Stream tracking
    PCCavernMiniportWaveRTStream m_pStream;
    
public:
    DECLARE_STD_UNKNOWN();
    
    CCavernMiniportWaveRT(_In_ PUNKNOWN OuterUnknown)
        : CUnknown(OuterUnknown),
          m_pAdapterCommon(NULL),
          m_pPort(NULL),
          m_pStream(NULL)
    {
    }
    
    ~CCavernMiniportWaveRT();
    
    // Initialization
    NTSTATUS Init(
        _In_ PUNKNOWN UnknownAdapter,
        _In_ PPORTWAVERT Port
    );
    
    // IMiniportWaveRT
    IMP_IMiniportWaveRT;
    
    // Stream management
    NTSTATUS StreamCreated(_In_ PCCavernMiniportWaveRTStream Stream);
    NTSTATUS StreamClosed(_In_ PCCavernMiniportWaveRTStream Stream);
};

typedef CCavernMiniportWaveRT *PCCavernMiniportWaveRT;

// Newminiport creation function
NTSTATUS CreateCavernMiniportWaveRT(
    _Out_ PUNKNOWN *Unknown,
    _In_ REFCLSID ClassId,
    _In_ PUNKNOWN UnknownOuter,
    _In_ POOL_TYPE PoolType
);

#endif // _CAVERN_MINWAVERT_H_
