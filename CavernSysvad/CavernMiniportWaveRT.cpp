/***************************************************************************
 * CavernMiniportWaveRT.cpp
 * 
 * Simplified WaveRT Miniport Implementation
 * Based on Microsoft SysVAD sample
 ***************************************************************************/

#include <ntddk.h>
#include <portcls.h>
#include <ks.h>
#include <ksmedia.h>
#include "CavernMiniportWaveRT.h"

#pragma code_seg("PAGE")
NTSTATUS CreateCavernMiniportWaveRT(
    _Out_ PUNKNOWN *Unknown,
    _In_ REFCLSID ClassId,
    _In_ PUNKNOWN UnknownOuter,
    _In_ POOL_TYPE PoolType
)
{
    UNREFERENCED_PARAMETER(ClassId);
    UNREFERENCED_PARAMETER(PoolType);
    
    PAGED_CODE();
    
    CCavernMiniportWaveRT *pMiniport = new(UnknownOuter, CAVERN_WAVERT_POOLTAG) 
        CCavernMiniportWaveRT(UnknownOuter);
    
    if (!pMiniport) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    *Unknown = PUNKNOWN(pMiniport);
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
CCavernMiniportWaveRT::~CCavernMiniportWaveRT()
{
    PAGED_CODE();
}

#pragma code_seg("PAGE")
NTSTATUS CCavernMiniportWaveRT::Init(
    _In_ PUNKNOWN UnknownAdapter,
    _In_ PPORTWAVERT Port
)
{
    PAGED_CODE();
    
    ASSERT(UnknownAdapter);
    ASSERT(Port);
    
    m_pAdapterCommon = (PADAPTERCOMMON)UnknownAdapter;
    m_pPort = Port;
    
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRT::GetDescription(
    _Out_ PPCFILTER_DESCRIPTOR *Description
)
{
    // Static filter descriptor for a simple render device
    static KSPIN_DESCRIPTOR_EX PinDescriptor;
    static PKSDATARANGE DataRanges[1];
    static KSDATARANGE_AUDIO DataRangeAudio;
    static PCFILTER_DESCRIPTOR FilterDescriptor;
    
    PAGED_CODE();
    
    // Initialize data range for 8-channel 48kHz PCM
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
    
    // Initialize pin descriptor
    RtlZeroMemory(&PinDescriptor, sizeof(PinDescriptor));
    PinDescriptor.PinDescriptor.DataRangesCount = 1;
    PinDescriptor.PinDescriptor.DataRanges = DataRanges;
    PinDescriptor.PinDescriptor.DataFlow = KSPIN_DATAFLOW_IN;
    PinDescriptor.PinDescriptor.Communication = KSPIN_COMMUNICATION_SINK;
    PinDescriptor.PinDescriptor.Category = &KSNODETYPE_SPEAKER;
    PinDescriptor.PinDescriptor.Name = NULL;
    
    // Initialize filter descriptor
    RtlZeroMemory(&FilterDescriptor, sizeof(FilterDescriptor));
    FilterDescriptor.Version = kVersion;
    FilterDescriptor.PinCount = 1;
    FilterDescriptor.PinDescriptorSize = sizeof(KSPIN_DESCRIPTOR_EX);
    FilterDescriptor.Pins = &PinDescriptor;
    
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
    
    // Only support render (not capture)
    if (Capture) {
        return STATUS_NOT_SUPPORTED;
    }
    
    // Only one stream at a time
    if (m_pStream) {
        return STATUS_DEVICE_BUSY;
    }
    
    // Create stream
    CCavernMiniportWaveRTStream *pStream = new(PortStream, CAVERN_WAVERT_POOLTAG)
        CCavernMiniportWaveRTStream(PortStream);
    
    if (!pStream) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    // Initialize stream
    ntStatus = pStream->Init(this, PortStream, Pin, Capture, DataFormat);
    
    if (NT_SUCCESS(ntStatus)) {
        *Stream = PMiniportWaveRTStream(pStream);
        m_pStream = pStream;
    } else {
        delete pStream;
    }
    
    return ntStatus;
}

#pragma code_seg()
NTSTATUS CCavernMiniportWaveRT::StreamCreated(
    _In_ PCCavernMiniportWaveRTStream Stream
)
{
    m_pStream = Stream;
    return STATUS_SUCCESS;
}

#pragma code_seg()
NTSTATUS CCavernMiniportWaveRT::StreamClosed(
    _In_ PCCavernMiniportWaveRTStream Stream
)
{
    UNREFERENCED_PARAMETER(Stream);
    
    m_pStream = NULL;
    return STATUS_SUCCESS;
}

//=============================================================================
// Stream Implementation
//=============================================================================

#pragma code_seg("PAGE")
CCavernMiniportWaveRTStream::~CCavernMiniportWaveRTStream()
{
    PAGED_CODE();
    
    DisconnectPipe();
    
    if (m_pMiniport) {
        m_pMiniport->StreamClosed(this);
        m_pMiniport->Release();
    }
    
    if (m_pWfExt) {
        ExFreePoolWithTag(m_pWfExt, CAVERN_WAVERT_POOLTAG);
    }
}

#pragma code_seg("PAGE")
NTSTATUS CCavernMiniportWaveRTStream::Init(
    _In_ PCMiniportWaveRT Miniport,
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
    
    m_pMiniport = (PCCavernMiniportWaveRT)Miniport;
    m_pMiniport->AddRef();
    
    m_pPortStream = PortStream;
    
    // Store format
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

#pragma code_seg()
STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::SetState(
    _In_ KSSTATE State
)
{
    m_State = State;
    m_Running = (State == KSSTATE_RUN);
    
    if (m_Running) {
        // Connect pipe when starting
        ConnectPipe();
    } else {
        // Disconnect when stopping
        DisconnectPipe();
    }
    
    return STATUS_SUCCESS;
}

#pragma code_seg()
STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::GetPosition(
    _Out_ PULONGLONG Position
)
{
    *Position = m_ullLinearPosition;
    return STATUS_SUCCESS;
}

#pragma code_seg()
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

#pragma code_seg()
STDMETHODIMP_(VOID) CCavernMiniportWaveRTStream::FreeAudioBuffer(
    _In_ PVOID Buffer
)
{
    if (Buffer) {
        ExFreePoolWithTag(Buffer, CAVERN_WAVERT_POOLTAG);
        m_pDmaBuffer = NULL;
        m_ulDmaBufferSize = 0;
    }
}

#pragma code_seg()
STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::SetFormat(
    _In_ PKSDATAFORMAT Format
)
{
    UNREFERENCED_PARAMETER(Format);
    return STATUS_SUCCESS;
}

#pragma code_seg()
STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::RegisterNotificationEvent(
    _In_ PKEVENT NotificationEvent
)
{
    UNREFERENCED_PARAMETER(NotificationEvent);
    return STATUS_SUCCESS;
}

#pragma code_seg()
STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::UnregisterNotificationEvent(
    _In_ PKEVENT NotificationEvent
)
{
    UNREFERENCED_PARAMETER(NotificationEvent);
    return STATUS_SUCCESS;
}

#pragma code_seg()
STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::GetClockRegister(
    _Out_ PKSRTAUDIO_HWREGISTER Register
)
{
    UNREFERENCED_PARAMETER(Register);
    return STATUS_NOT_SUPPORTED;
}

#pragma code_seg()
STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::GetPositionRegister(
    _Out_ PKSRTAUDIO_HWREGISTER Register
)
{
    UNREFERENCED_PARAMETER(Register);
    return STATUS_NOT_SUPPORTED;
}

#pragma code_seg()
STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::SetWritePacket(
    _In_ ULONG PacketNumber,
    _In_ DWORD Flags,
    _In_ ULONG EosPacketLength
)
{
    UNREFERENCED_PARAMETER(PacketNumber);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(EosPacketLength);
    return STATUS_SUCCESS;
}

#pragma code_seg()
STDMETHODIMP_(NTSTATUS) CCavernMiniportWaveRTStream::GetReadPacket(
    _Out_ ULONG *PacketNumber,
    _Out_ DWORD *Flags,
    _Out_ ULONG *EosPacketLength
)
{
    UNREFERENCED_PARAMETER(PacketNumber);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(EosPacketLength);
    return STATUS_NOT_SUPPORTED;
}

#pragma code_seg()
VOID CCavernMiniportWaveRTStream::WriteBytes(_In_ ULONG ByteDisplacement)
{
    if (!m_pDmaBuffer || !ByteDisplacement) {
        return;
    }
    
    ULONG bufferOffset = (ULONG)(m_ullLinearPosition % m_ulDmaBufferSize);
    
    while (ByteDisplacement > 0) {
        ULONG runWrite = min(ByteDisplacement, m_ulDmaBufferSize - bufferOffset);
        
        // Forward to pipe
        ForwardToPipe((PBYTE)m_pDmaBuffer + bufferOffset, runWrite);
        
        bufferOffset = (bufferOffset + runWrite) % m_ulDmaBufferSize;
        ByteDisplacement -= runWrite;
        m_ullLinearPosition += runWrite;
    }
}

#pragma code_seg()
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

#pragma code_seg()
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

#pragma code_seg()
NTSTATUS CCavernMiniportWaveRTStream::ForwardToPipe(
    _In_reads_bytes_(Length) PVOID Buffer,
    _In_ ULONG Length
)
{
    if (!m_PipeConnected || !m_hPipe) {
        // Try to connect
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
