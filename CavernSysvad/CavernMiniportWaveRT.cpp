/***************************************************************************
 * CavernMiniportWaveRT.cpp
 * 
 * Minimal WaveRT Miniport Implementation
 ***************************************************************************/

#include <ntddk.h>
#include <wdf.h>
#include <portcls.h>
#include <stdunk.h>
#include <ks.h>
#include <ksmedia.h>
#include "CavernMiniportWaveRT.h"

#define CAVERN_PIPE_NAME L"\\??\\pipe\\CavernAudioPipe"

//=============================================================================
// CCavernMiniportWaveRT Implementation
//=============================================================================

#pragma code_seg("PAGE")
NTSTATUS CreateCavernMiniportWaveRT(
    _Out_ PUNKNOWN *Unknown,
    _In_ REFCLSID ClassId,
    _In_opt_ PUNKNOWN UnknownOuter,
    _In_ POOL_FLAGS PoolFlags
)
{
    UNREFERENCED_PARAMETER(ClassId);
    UNREFERENCED_PARAMETER(UnknownOuter);
    
    PAGED_CODE();
    
    CCavernMiniportWaveRT *obj = new (PoolFlags, CAVERN_WAVERT_POOLTAG) 
        CCavernMiniportWaveRT(NULL);
    
    if (NULL == obj) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    obj->AddRef();
    *Unknown = (PUNKNOWN)obj;
    
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
CCavernMiniportWaveRT::CCavernMiniportWaveRT(PUNKNOWN OuterUnknown)
    : CUnknown(OuterUnknown),
      m_pPort(NULL),
      m_pStream(NULL)
{
    PAGED_CODE();
}

#pragma code_seg("PAGE")
CCavernMiniportWaveRT::~CCavernMiniportWaveRT()
{
    PAGED_CODE();
}

#pragma code_seg("PAGE")
STDMETHODIMP_(ULONG) CCavernMiniportWaveRT::AddRef()
{
    return CUnknown::AddRef();
}

#pragma code_seg("PAGE")
STDMETHODIMP_(ULONG) CCavernMiniportWaveRT::Release()
{
    return CUnknown::Release();
}

#pragma code_seg("PAGE")
STDMETHODIMP CCavernMiniportWaveRT::QueryInterface(REFIID InterfaceId, PVOID *Object)
{
    PAGED_CODE();
    
    if (IsEqualGUID(InterfaceId, IID_IUnknown)) {
        *Object = (PUNKNOWN)(IMiniportWaveRT *)this;
        AddRef();
        return STATUS_SUCCESS;
    }
    else if (IsEqualGUID(InterfaceId, IID_IMiniportWaveRT)) {
        *Object = (IMiniportWaveRT *)this;
        AddRef();
        return STATUS_SUCCESS;
    }
    
    *Object = NULL;
    return STATUS_NOINTERFACE;
}

#pragma code_seg("PAGE")
NTSTATUS CCavernMiniportWaveRT::Init(_In_ PPORTWAVERT Port)
{
    PAGED_CODE();
    
    ASSERT(Port);
    
    m_pPort = Port;
    if (m_pPort) {
        m_pPort->AddRef();
    }
    
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::GetDescription(_Out_ PPCFILTER_DESCRIPTOR *Description)
{
    static KSPIN_DESCRIPTOR_EX PinDescriptor;
    static KSDATARANGE_AUDIO DataRangeAudio;
    static PKSDATARANGE DataRanges[1];
    static PCPIN_DESCRIPTOR Pins[1];
    static PCFILTER_DESCRIPTOR FilterDescriptor;
    
    PAGED_CODE();
    
    RtlZeroMemory(&DataRangeAudio, sizeof(DataRangeAudio));
    DataRangeAudio.DataRange.FormatSize = sizeof(KSDATARANGE_AUDIO);
    DataRangeAudio.DataRange.Flags = 0;
    DataRangeAudio.DataRange.SampleSize = 0;
    DataRangeAudio.DataRange.Reserved = 0;
    DataRangeAudio.DataRange.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
    DataRangeAudio.DataRange.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    DataRangeAudio.DataRange.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
    DataRangeAudio.MaximumChannels = 8;
    DataRangeAudio.MinimumBitsPerSample = 16;
    DataRangeAudio.MaximumBitsPerSample = 16;
    DataRangeAudio.MinimumSampleFrequency = 48000;
    DataRangeAudio.MaximumSampleFrequency = 48000;
    
    DataRanges[0] = (PKSDATARANGE)&DataRangeAudio;
    
    RtlZeroMemory(&PinDescriptor, sizeof(PinDescriptor));
    PinDescriptor.PinDescriptor.DataRangesCount = 1;
    PinDescriptor.PinDescriptor.DataRanges = DataRanges;
    PinDescriptor.PinDescriptor.DataFlow = KSPIN_DATAFLOW_IN;
    PinDescriptor.PinDescriptor.Communication = KSPIN_COMMUNICATION_SINK;
    PinDescriptor.PinDescriptor.Category = &KSNODETYPE_SPEAKER;
    
    Pins[0] = (PCPIN_DESCRIPTOR)&PinDescriptor;
    
    RtlZeroMemory(&FilterDescriptor, sizeof(FilterDescriptor));
    FilterDescriptor.Version = 1;
    FilterDescriptor.PinCount = 1;
    FilterDescriptor.Pins = Pins;
    
    *Description = &FilterDescriptor;
    
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::NewStream(
    _Out_ PMiniportWaveRTStream *Stream,
    _In_ PPORTWAVERTSTREAM PortStream,
    _In_ ULONG Pin,
    _In_ BOOLEAN Capture,
    _In_ PKSDATAFORMAT DataFormat
)
{
    UNREFERENCED_PARAMETER(Pin);
    
    PAGED_CODE();
    
    NTSTATUS ntStatus;
    
    if (Capture) {
        return STATUS_NOT_SUPPORTED;
    }
    
    if (m_pStream) {
        return STATUS_DEVICE_BUSY;
    }
    
    CCavernMiniportWaveRTStream *pStream = new (NonPagedPoolNx, CAVERN_WAVERT_POOLTAG)
        CCavernMiniportWaveRTStream(NULL);
    
    if (!pStream) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    pStream->AddRef();
    
    ntStatus = pStream->Init(this, PortStream, Pin, Capture, DataFormat);
    
    if (NT_SUCCESS(ntStatus)) {
        *Stream = (PMiniportWaveRTStream)pStream;
        m_pStream = pStream;
    } else {
        pStream->Release();
    }
    
    return ntStatus;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::GetDeviceDescription(_Out_ PDEVICE_DESCRIPTION Description)
{
    UNREFERENCED_PARAMETER(Description);
    return STATUS_NOT_SUPPORTED;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::SetDeviceDescription(_In_ PDEVICE_DESCRIPTION Description)
{
    UNREFERENCED_PARAMETER(Description);
    return STATUS_NOT_SUPPORTED;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::GetDmaAdapter(_Out_ PDMA_ADAPTER *DmaAdapter)
{
    UNREFERENCED_PARAMETER(DmaAdapter);
    return STATUS_NOT_SUPPORTED;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::SetDmaAdapter(_In_ PDMA_ADAPTER DmaAdapter)
{
    UNREFERENCED_PARAMETER(DmaAdapter);
    return STATUS_NOT_SUPPORTED;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::GetPowerState(_Out_ DEVICE_POWER_STATE *PowerState)
{
    UNREFERENCED_PARAMETER(PowerState);
    return STATUS_NOT_SUPPORTED;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::SetPowerState(_In_ DEVICE_POWER_STATE PowerState)
{
    UNREFERENCED_PARAMETER(PowerState);
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::GetAudioEngineDescriptor(
    _In_ ULONG PinId,
    _Out_ PKSAUDIOENGINE_DESCRIPTOR Descriptor
)
{
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(Descriptor);
    return STATUS_NOT_SUPPORTED;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::GetAudioEngineBufferSizeRange(
    _In_ ULONG PinId,
    _Out_ PKSAUDIOENGINE_BUFFER_SIZE_RANGE BufferSizeRange
)
{
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(BufferSizeRange);
    return STATUS_NOT_SUPPORTED;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::SetOffloadPinStream(
    _In_ ULONG PinId,
    _In_ PKSOFFLOAD_PIN_OFFLOAD Stream
)
{
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(Stream);
    return STATUS_NOT_SUPPORTED;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::GetStreamCount(_Out_ PULONG StreamCount)
{
    *StreamCount = (m_pStream != NULL) ? 1 : 0;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::GetStream(_In_ ULONG StreamIndex, _Out_ PMiniportWaveRTStream *Stream)
{
    UNREFERENCED_PARAMETER(StreamIndex);
    
    if (m_pStream) {
        *Stream = (PMiniportWaveRTStream)m_pStream;
        return STATUS_SUCCESS;
    }
    return STATUS_NOT_FOUND;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::GetPerformanceCounters(_Out_ PKSAUDIOMODULE_PERFORMANCE_COUNTERS Counters)
{
    UNREFERENCED_PARAMETER(Counters);
    return STATUS_NOT_SUPPORTED;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::ValidateFormat(_In_ PKSDATAFORMAT Format, _In_ ULONG PinId)
{
    UNREFERENCED_PARAMETER(Format);
    UNREFERENCED_PARAMETER(PinId);
    return STATUS_SUCCESS;
}

NTSTATUS CCavernMiniportWaveRT::StreamCreated(_In_ PCCavernMiniportWaveRTStream Stream)
{
    m_pStream = Stream;
    return STATUS_SUCCESS;
}

NTSTATUS CCavernMiniportWaveRT::StreamClosed(_In_ PCCavernMiniportWaveRTStream Stream)
{
    UNREFERENCED_PARAMETER(Stream);
    m_pStream = NULL;
    return STATUS_SUCCESS;
}

//=============================================================================
// CCavernMiniportWaveRTStream Implementation
//=============================================================================

#pragma code_seg("PAGE")
CCavernMiniportWaveRTStream::CCavernMiniportWaveRTStream(PUNKNOWN OuterUnknown)
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
    PAGED_CODE();
    KeInitializeSpinLock(&m_PipeLock);
    RtlInitUnicodeString(&m_PipeName, CAVERN_PIPE_NAME);
}

#pragma code_seg("PAGE")
CCavernMiniportWaveRTStream::~CCavernMiniportWaveRTStream()
{
    PAGED_CODE();
    
    DisconnectPipe();
    
    if (m_pMiniport) {
        m_pMiniport->StreamClosed(this);
        m_pMiniport->Release();
    }
    
    if (m_pPortStream) {
        m_pPortStream->Release();
    }
    
    if (m_pWfExt) {
        ExFreePoolWithTag(m_pWfExt, CAVERN_WAVERT_POOLTAG);
    }
}

#pragma code_seg("PAGE")
STDMETHODIMP_(ULONG) CCavernMiniportWaveRTStream::AddRef()
{
    return CUnknown::AddRef();
}

#pragma code_seg("PAGE")
STDMETHODIMP_(ULONG) CCavernMiniportWaveRTStream::Release()
{
    return CUnknown::Release();
}

#pragma code_seg("PAGE")
STDMETHODIMP CCavernMiniportWaveRTStream::QueryInterface(REFIID InterfaceId, PVOID *Object)
{
    PAGED_CODE();
    
    if (IsEqualGUID(InterfaceId, IID_IUnknown)) {
        *Object = (PUNKNOWN)this;
        AddRef();
        return STATUS_SUCCESS;
    }
    
    *Object = NULL;
    return STATUS_NOINTERFACE;
}

#pragma code_seg("PAGE")
NTSTATUS CCavernMiniportWaveRTStream::Init(
    _In_ PCCavernMiniportWaveRT Miniport,
    _In_ PPORTWAVERTSTREAM PortStream,
    _In_ ULONG Pin,
    _In_ BOOLEAN Capture,
    _In_ PKSDATAFORMAT DataFormat
)
{
    UNREFERENCED_PARAMETER(Pin);
    UNREFERENCED_PARAMETER(Capture);
    
    PAGED_CODE();
    
    ASSERT(Miniport);
    ASSERT(PortStream);
    ASSERT(DataFormat);
    
    m_pMiniport = Miniport;
    m_pMiniport->AddRef();
    
    m_pPortStream = PortStream;
    m_pPortStream->AddRef();
    
    if (DataFormat->FormatSize >= sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE)) {
        m_pWfExt = (PWAVEFORMATEXTENSIBLE)ExAllocatePool2(
            POOL_FLAG_NON_PAGED, 
            DataFormat->FormatSize, 
            CAVERN_WAVERT_POOLTAG
        );
        
        if (!m_pWfExt) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        
        RtlCopyMemory(m_pWfExt, DataFormat, DataFormat->FormatSize);
    }
    
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::SetState(_In_ KSSTATE State)
{
    m_State = State;
    m_Running = (State == KSSTATE_RUN);
    
    if (m_Running) {
        ConnectPipe();
    } else {
        DisconnectPipe();
    }
    
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::GetPosition(_Out_ PKSAUDIO_POSITION Position)
{
    Position->PlayOffset = m_ullLinearPosition;
    Position->WriteOffset = m_ullLinearPosition;
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::AllocateAudioBuffer(
    _In_ ULONG BufferSize,
    _Out_ PVOID *Buffer,
    _Out_ PULONG OffsetFromFirstPage,
    _Out_ PULONG ActualBufferSize
)
{
    PAGED_CODE();
    
    *Buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, BufferSize, CAVERN_WAVERT_POOLTAG);
    if (!*Buffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    RtlZeroMemory(*Buffer, BufferSize);
    *OffsetFromFirstPage = 0;
    *ActualBufferSize = BufferSize;
    
    m_pDmaBuffer = *Buffer;
    m_ulDmaBufferSize = BufferSize;
    
    return STATUS_SUCCESS;
}

STDMETHODIMP_(VOID) CCavernMiniportWaveRTStream::FreeAudioBuffer(_In_opt_ PVOID Buffer)
{
    if (Buffer) {
        ExFreePoolWithTag(Buffer, CAVERN_WAVERT_POOLTAG);
        if (m_pDmaBuffer == Buffer) {
            m_pDmaBuffer = NULL;
            m_ulDmaBufferSize = 0;
        }
    }
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::GetClockRegister(_Out_ PKSRTAUDIO_HWREGISTER Register)
{
    UNREFERENCED_PARAMETER(Register);
    return STATUS_NOT_SUPPORTED;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::GetPositionRegister(_Out_ PKSRTAUDIO_HWREGISTER Register)
{
    UNREFERENCED_PARAMETER(Register);
    return STATUS_NOT_SUPPORTED;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::SetWritePacket(_In_ ULONG PacketNumber, _In_ DWORD Flags, _In_ ULONG EosPacketLength)
{
    UNREFERENCED_PARAMETER(PacketNumber);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(EosPacketLength);
    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::GetReadPacket(_Out_ ULONG *PacketNumber, _Out_ DWORD *Flags, _Out_ ULONG *EosPacketLength)
{
    UNREFERENCED_PARAMETER(PacketNumber);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(EosPacketLength);
    return STATUS_NOT_SUPPORTED;
}

VOID CCavernMiniportWaveRTStream::WriteBytes(_In_ ULONG ByteDisplacement)
{
    if (!m_pDmaBuffer || !ByteDisplacement) {
        return;
    }
    
    ULONG bufferOffset = (ULONG)(m_ullLinearPosition % m_ulDmaBufferSize);
    
    while (ByteDisplacement > 0) {
        ULONG runWrite = min(ByteDisplacement, m_ulDmaBufferSize - bufferOffset);
        
        ForwardToPipe((PBYTE)m_pDmaBuffer + bufferOffset, runWrite);
        
        bufferOffset = (bufferOffset + runWrite) % m_ulDmaBufferSize;
        ByteDisplacement -= runWrite;
        m_ullLinearPosition += runWrite;
    }
}

NTSTATUS CCavernMiniportWaveRTStream::ConnectPipe()
{
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_PipeLock, &oldIrql);
    
    if (m_hPipe) {
        KeReleaseSpinLock(&m_PipeLock, oldIrql);
        return STATUS_SUCCESS;
    }
    
    OBJECT_ATTRIBUTES objAttr;
    IO_STATUS_BLOCK ioStatus;
    
    InitializeObjectAttributes(
        &objAttr,
        &m_PipeName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL
    );
    
    NTSTATUS status = ZwCreateFile(
        &m_hPipe,
        GENERIC_WRITE | SYNCHRONIZE,
        &objAttr,
        &ioStatus,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        0,
        FILE_OPEN,
        FILE_NON_DIRECTORY_FILE,
        NULL,
        0
    );
    
    if (NT_SUCCESS(status)) {
        m_PipeConnected = TRUE;
        KdPrint(("CavernAudio: Pipe connected\n"));
    } else {
        KdPrint(("CavernAudio: Pipe connect failed 0x%08X\n", status));
    }
    
    KeReleaseSpinLock(&m_PipeLock, oldIrql);
    return status;
}

VOID CCavernMiniportWaveRTStream::DisconnectPipe()
{
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_PipeLock, &oldIrql);
    
    if (m_hPipe) {
        ZwClose(m_hPipe);
        m_hPipe = NULL;
        m_PipeConnected = FALSE;
        KdPrint(("CavernAudio: Pipe disconnected\n"));
    }
    
    KeReleaseSpinLock(&m_PipeLock, oldIrql);
}

NTSTATUS CCavernMiniportWaveRTStream::ForwardToPipe(_In_reads_bytes_(Length) PVOID Buffer, _In_ ULONG Length)
{
    if (!m_PipeConnected || !m_hPipe) {
        if (!NT_SUCCESS(ConnectPipe())) {
            return STATUS_DEVICE_NOT_CONNECTED;
        }
    }
    
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_PipeLock, &oldIrql);
    
    if (!m_hPipe) {
        KeReleaseSpinLock(&m_PipeLock, oldIrql);
        return STATUS_DEVICE_NOT_CONNECTED;
    }
    
    IO_STATUS_BLOCK ioStatus;
    NTSTATUS status = ZwWriteFile(
        m_hPipe,
        NULL,
        NULL,
        NULL,
        &ioStatus,
        Buffer,
        Length,
        NULL,
        NULL
    );
    
    KeReleaseSpinLock(&m_PipeLock, oldIrql);
    
    return status;
}
