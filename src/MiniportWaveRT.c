/***************************************************************************
 * MiniportWaveRT.c
 * 
 * WaveRT Miniport Implementation for Cavern Audio Driver
 * 
 * This implements the IMiniportWaveRT interface to register as a 
 * proper Windows audio endpoint.
 ***************************************************************************/

#include <ntddk.h>
#include <wdf.h>

// PortCls headers - these define IMiniportWaveRT
#include <portcls.h>
#include <ks.h>
#include <ksmedia.h>

// Miniport context structure
typedef struct _CAVERN_MINIPORT {
    // PortCls interfaces
    IMiniportWaveRT Miniport;
    IPortWaveRT* Port;
    
    // Reference counting
    ULONG RefCount;
    
    // Device reference
    WDFDEVICE Device;
    
    // Audio format
    WAVEFORMATEX Format;
    BOOLEAN FormatSet;
    
    // State
    BOOLEAN Running;
    KSSTATE State;
    
    // Synchronization
    KSPIN_LOCK Lock;
    
    // Pipe connection
    HANDLE PipeHandle;
    UNICODE_STRING PipeName;
    
} CAVERN_MINIPORT, *PCAVERN_MINIPORT;

// Stream context
typedef struct _CAVERN_STREAM {
    IMiniportWaveRTStream Stream;
    PCAVERN_MINIPORT Miniport;
    IPortWaveRTStream* PortStream;
    
    ULONG RefCount;
    KSSTATE State;
    
    // Position tracking
    ULONGLONG WritePosition;
    ULONGLONG PlayPosition;
    
} CAVERN_STREAM, *PCAVERN_STREAM;

// Forward declarations for miniport
NTSTATUS CavernMiniportQueryInterface(
    _In_ PIUNKNOWN Unknown,
    _In_ REFIID InterfaceId,
    _Out_ PINTERFACE* Interface
);

ULONG CavernMiniportAddRef(_In_ PIUNKNOWN Unknown);
ULONG CavernMiniportRelease(_In_ PIUNKNOWN Unknown);
NTSTATUS CavernMiniportGetDescription(_In_ IMiniportWaveRT* Miniport, _Out_ PPCFILTER_DESCRIPTOR* Description);
NTSTATUS CavernMiniportDataRangeIntersection(_In_ IMiniportWaveRT* Miniport, _In_ ULONG PinId, _In_ PKSDATARANGE DataRange, _In_ PKSDATARANGE MatchingDataRange, _In_ ULONG OutputBufferLength, _Out_ PVOID ResultantFormat, _Out_ PULONG ResultantFormatLength);
NTSTATUS CavernMiniportSetFormat(_In_ IMiniportWaveRT* Miniport, _In_ PKSDATAFORMAT Format);
NTSTATUS CavernMiniportNewStream(_In_ IMiniportWaveRT* Miniport, _Out_ PMiniportWaveRTStream* Stream, _In_ PPORTWAVERTSTREAM PortStream, _In_ ULONG Pin, _In_ BOOLEAN Capture, _In_ PKSDATAFORMAT DataFormat);

// Forward declarations for stream
NTSTATUS CavernStreamQueryInterface(_In_ PIUNKNOWN Unknown, _In_ REFIID InterfaceId, _Out_ PINTERFACE* Interface);
ULONG CavernStreamAddRef(_In_ PIUNKNOWN Unknown);
ULONG CavernStreamRelease(_In_ PIUNKNOWN Unknown);
NTSTATUS CavernStreamSetFormat(_In_ IMiniportWaveRTStream* Stream, _In_ PKSDATAFORMAT Format);
NTSTATUS CavernStreamSetState(_In_ IMiniportWaveRTStream* Stream, _In_ KSSTATE State);
NTSTATUS CavernStreamGetPosition(_In_ IMiniportWaveRTStream* Stream, _Out_ PULONGLONG Position);
NTSTATUS CavernStreamAllocateAudioBuffer(_In_ IMiniportWaveRTStream* Stream, _In_ ULONG BufferSize, _Out_ PVOID* Buffer, _Out_ PULONG OffsetFromFirstPage, _Out_ PULONG ActualBufferSize);
VOID CavernStreamFreeAudioBuffer(_In_ IMiniportWaveRTStream* Stream, _In_ PVOID Buffer);
NTSTATUS CavernStreamRegisterNotificationEvent(_In_ IMiniportWaveRTStream* Stream, _In_ PKEVENT NotificationEvent);
NTSTATUS CavernStreamUnregisterNotificationEvent(_In_ IMiniportWaveRTStream* Stream, _In_ PKEVENT NotificationEvent);
NTSTATUS CavernStreamGetClockRegister(_In_ IMiniportWaveRTStream* Stream, _Out_ PKSRTAUDIO_HWREGISTER Register);
NTSTATUS CavernStreamGetPositionRegister(_In_ IMiniportWaveRTStream* Stream, _Out_ PKSRTAUDIO_HWREGISTER Register);
NTSTATUS CavernStreamSetWritePacket(_In_ IMiniportWaveRTStream* Stream, _In_ ULONG PacketNumber, _In_ DWORD Flags, _In_ ULONG EosPacketLength);
NTSTATUS CavernStreamGetReadPacket(_In_ IMiniportWaveRTStream* Stream, _Out_ ULONG* PacketNumber, _Out_ DWORD* Flags, _Out_ ULONG* EosPacketLength);

// Miniport vtable
IMiniportWaveRTVtbl CavernMiniportVtbl = {
    CavernMiniportQueryInterface,
    CavernMiniportAddRef,
    CavernMiniportRelease,
    CavernMiniportGetDescription,
    CavernMiniportDataRangeIntersection,
    NULL, // PowerChangeNotify - optional
    NULL, // SetWFormat - optional  
    CavernMiniportSetFormat,
    CavernMiniportNewStream
};

// Stream vtable
IMiniportWaveRTStreamVtbl CavernStreamVtbl = {
    CavernStreamQueryInterface,
    CavernStreamAddRef,
    CavernStreamRelease,
    CavernStreamSetFormat,
    CavernStreamSetState,
    CavernStreamGetPosition,
    CavernStreamAllocateAudioBuffer,
    CavernStreamFreeAudioBuffer,
    CavernStreamRegisterNotificationEvent,
    CavernStreamUnregisterNotificationEvent,
    CavernStreamGetClockRegister,
    CavernStreamGetPositionRegister,
    CavernStreamSetWritePacket,
    CavernStreamGetReadPacket
};

// Supported formats data range
KSDATAFORMAT_WAVEFORMATEX CavernDataRange = {
    {
        sizeof(KSDATAFORMAT_WAVEFORMATEX),
        0,
        0,
        0,
        {STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO)},
        {STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)},
        {STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)}
    },
    {
        WAVE_FORMAT_PCM,
        8,           // Channels (7.1)
        48000,       // Sample rate
        768000,      // Avg bytes/sec (48000 * 8 * 2)
        16,          // Block align
        16,          // Bits per sample
        0            // Extra size
    }
};

KSDATARANGE_AUDIO CavernDataRangeAudio = {
    {
        sizeof(KSDATARANGE_AUDIO),
        0,
        0,
        0,
        {STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO)},
        {STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)},
        {STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)}
    },
    48000,      // Min sample rate
    48000,      // Max sample rate  
    16,         // Min bits
    16,         // Max bits
    8,          // Min channels
    8           // Max channels
};

PKSDATARANGE CavernDataRanges[] = {
    (PKSDATARANGE)&CavernDataRangeAudio
};

// Pin descriptor
KSPIN_DESCRIPTOR_EX CavernPinDescriptor = {
    NULL,                                   // Dispatch
    NULL,                                   // Automation
    {
        DEFINE_KSPIN_DEFAULT_INTERFACES,    // Interfaces
        DEFINE_KSPIN_DEFAULT_MEDIUMS,       // Mediums
        SIZEOF_ARRAY(CavernDataRanges),     // DataRangesCount
        CavernDataRanges,                   // DataRanges
        KSPIN_DATAFLOW_IN,                  // DataFlow
        KSPIN_COMMUNICATION_SINK,           // Communication
        (GUID*)NULL,                        // Category (use default)
        NULL,                               // Name
        0                                   // Reserved
    },
    KSPIN_FLAG_RENDER_MASTER_CLOCK,         // Flags
    1,                                      // InstancesPossible
    1,                                      // InstancesNecessary
    NULL,                                   // Allocator Framing
    NULL                                    // Intersect Handler
};

// Filter descriptor
PCFILTER_DESCRIPTOR CavernFilterDescriptor = {
    NULL,                                   // AutomationTable
    NULL,                                   // PinSize
    1,                                      // PinCount
    &CavernPinDescriptor,                   // PinDescriptorSize
    0,                                      // NodeCount
    NULL,                                   // NodeDescriptor
    0,                                      // ConnectionCount
    NULL,                                   // ConnectionDescriptor
    NULL                                    // ComponentId
};

// Property set (for custom properties)
KSPROPERTY_SET CavernPropertySet = {
    NULL,                                   // Set
    0,                                      // PropertiesCount
    NULL,                                   // PropertyItem
    0,                                      // FastIoCount
    NULL                                    // FastIoTable
};

/**************************************************************************
 * Miniport Implementation
 ***************************************************************************/

NTSTATUS CavernMiniportQueryInterface(
    _In_ PIUNKNOWN Unknown,
    _In_ REFIID InterfaceId,
    _Out_ PINTERFACE* Interface
)
{
    UNREFERENCED_PARAMETER(Unknown);
    UNREFERENCED_PARAMETER(InterfaceId);
    UNREFERENCED_PARAMETER(Interface);
    
    return STATUS_INVALID_PARAMETER;
}

ULONG CavernMiniportAddRef(_In_ PIUNKNOWN Unknown)
{
    PCAVERN_MINIPORT miniport = (PCAVERN_MINIPORT)Unknown;
    return InterlockedIncrement((LONG*)&miniport->RefCount);
}

ULONG CavernMiniportRelease(_In_ PIUNKNOWN Unknown)
{
    PCAVERN_MINIPORT miniport = (PCAVERN_MINIPORT)Unknown;
    ULONG newRef = InterlockedDecrement((LONG*)&miniport->RefCount);
    
    if (newRef == 0) {
        // Close pipe if open
        if (miniport->PipeHandle) {
            ZwClose(miniport->PipeHandle);
        }
        
        ExFreePoolWithTag(miniport, 'navC');
    }
    
    return newRef;
}

NTSTATUS CavernMiniportGetDescription(
    _In_ IMiniportWaveRT* Miniport,
    _Out_ PPCFILTER_DESCRIPTOR* Description
)
{
    UNREFERENCED_PARAMETER(Miniport);
    
    *Description = &CavernFilterDescriptor;
    return STATUS_SUCCESS;
}

NTSTATUS CavernMiniportDataRangeIntersection(
    _In_ IMiniportWaveRT* Miniport,
    _In_ ULONG PinId,
    _In_ PKSDATARANGE DataRange,
    _In_ PKSDATARANGE MatchingDataRange,
    _In_ ULONG OutputBufferLength,
    _Out_ PVOID ResultantFormat,
    _Out_ PULONG ResultantFormatLength
)
{
    UNREFERENCED_PARAMETER(Miniport);
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(DataRange);
    UNREFERENCED_PARAMETER(MatchingDataRange);
    
    // Return our preferred format
    if (OutputBufferLength >= sizeof(WAVEFORMATEX)) {
        PWAVEFORMATEX format = (PWAVEFORMATEX)ResultantFormat;
        format->wFormatTag = WAVE_FORMAT_PCM;
        format->nChannels = 8;
        format->nSamplesPerSec = 48000;
        format->nAvgBytesPerSec = 768000;
        format->nBlockAlign = 16;
        format->wBitsPerSample = 16;
        format->cbSize = 0;
        
        *ResultantFormatLength = sizeof(WAVEFORMATEX);
        return STATUS_SUCCESS;
    }
    
    *ResultantFormatLength = sizeof(WAVEFORMATEX);
    return STATUS_BUFFER_TOO_SMALL;
}

NTSTATUS CavernMiniportSetFormat(
    _In_ IMiniportWaveRT* Miniport,
    _In_ PKSDATAFORMAT Format
)
{
    PCAVERN_MINIPORT miniport = (PCAVERN_MINIPORT)Miniport;
    
    if (Format->FormatSize >= sizeof(KSDATAFORMAT_WAVEFORMATEX)) {
        PKSDATAFORMAT_WAVEFORMATEX waveFormat = (PKSDATAFORMAT_WAVEFORMATEX)Format;
        RtlCopyMemory(&miniport->Format, &waveFormat->WaveFormatEx, sizeof(WAVEFORMATEX));
        miniport->FormatSet = TRUE;
        
        KdPrint(("CavernAudio: Format set - %d channels, %d Hz, %d bits\n",
            waveFormat->WaveFormatEx.nChannels,
            waveFormat->WaveFormatEx.nSamplesPerSec,
            waveFormat->WaveFormatEx.wBitsPerSample));
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS CavernMiniportNewStream(
    _In_ IMiniportWaveRT* Miniport,
    _Out_ PMiniportWaveRTStream* Stream,
    _In_ PPORTWAVERTSTREAM PortStream,
    _In_ ULONG Pin,
    _In_ BOOLEAN Capture,
    _In_ PKSDATAFORMAT DataFormat
)
{
    PCAVERN_MINIPORT miniport = (PCAVERN_MINIPORT)Miniport;
    PCAVERN_STREAM stream;
    
    UNREFERENCED_PARAMETER(Pin);
    UNREFERENCED_PARAMETER(Capture);
    UNREFERENCED_PARAMETER(DataFormat);
    
    KdPrint(("CavernAudio: NewStream\n"));
    
    // Allocate stream context
    stream = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(CAVERN_STREAM), 'navC');
    if (!stream) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    RtlZeroMemory(stream, sizeof(CAVERN_STREAM));
    
    // Initialize stream
    stream->Stream.lpVtbl = &CavernStreamVtbl;
    stream->Miniport = miniport;
    stream->PortStream = PortStream;
    stream->RefCount = 1;
    stream->State = KSSTATE_STOP;
    
    *Stream = (PMiniportWaveRTStream)stream;
    
    return STATUS_SUCCESS;
}

/**************************************************************************
 * Stream Implementation
 ***************************************************************************/

NTSTATUS CavernStreamQueryInterface(
    _In_ PIUNKNOWN Unknown,
    _In_ REFIID InterfaceId,
    _Out_ PINTERFACE* Interface
)
{
    UNREFERENCED_PARAMETER(Unknown);
    UNREFERENCED_PARAMETER(InterfaceId);
    UNREFERENCED_PARAMETER(Interface);
    
    return STATUS_INVALID_PARAMETER;
}

ULONG CavernStreamAddRef(_In_ PIUNKNOWN Unknown)
{
    PCAVERN_STREAM stream = (PCAVERN_STREAM)Unknown;
    return InterlockedIncrement((LONG*)&stream->RefCount);
}

ULONG CavernStreamRelease(_In_ PIUNKNOWN Unknown)
{
    PCAVERN_STREAM stream = (PCAVERN_STREAM)Unknown;
    ULONG newRef = InterlockedDecrement((LONG*)&stream->RefCount);
    
    if (newRef == 0) {
        ExFreePoolWithTag(stream, 'navC');
    }
    
    return newRef;
}

NTSTATUS CavernStreamSetFormat(
    _In_ IMiniportWaveRTStream* Stream,
    _In_ PKSDATAFORMAT Format
)
{
    UNREFERENCED_PARAMETER(Stream);
    UNREFERENCED_PARAMETER(Format);
    
    return STATUS_SUCCESS;
}

NTSTATUS CavernStreamSetState(
    _In_ IMiniportWaveRTStream* Stream,
    _In_ KSSTATE State
)
{
    PCAVERN_STREAM stream = (PCAVERN_STREAM)Stream;
    
    KdPrint(("CavernAudio: Stream state %d -> %d\n", stream->State, State));
    
    stream->State = State;
    stream->Miniport->State = State;
    stream->Miniport->Running = (State == KSSTATE_RUN);
    
    return STATUS_SUCCESS;
}

NTSTATUS CavernStreamGetPosition(
    _In_ IMiniportWaveRTStream* Stream,
    _Out_ PULONGLONG Position
)
{
    PCAVERN_STREAM stream = (PCAVERN_STREAM)Stream;
    
    *Position = stream->PlayPosition;
    return STATUS_SUCCESS;
}

NTSTATUS CavernStreamAllocateAudioBuffer(
    _In_ IMiniportWaveRTStream* Stream,
    _In_ ULONG BufferSize,
    _Out_ PVOID* Buffer,
    _Out_ PULONG OffsetFromFirstPage,
    _Out_ PULONG ActualBufferSize
)
{
    UNREFERENCED_PARAMETER(Stream);
    
    *Buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, BufferSize, 'navC');
    if (!*Buffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    RtlZeroMemory(*Buffer, BufferSize);
    *OffsetFromFirstPage = 0;
    *ActualBufferSize = BufferSize;
    
    return STATUS_SUCCESS;
}

VOID CavernStreamFreeAudioBuffer(
    _In_ IMiniportWaveRTStream* Stream,
    _In_ PVOID Buffer
)
{
    UNREFERENCED_PARAMETER(Stream);
    
    if (Buffer) {
        ExFreePoolWithTag(Buffer, 'navC');
    }
}

NTSTATUS CavernStreamRegisterNotificationEvent(
    _In_ IMiniportWaveRTStream* Stream,
    _In_ PKEVENT NotificationEvent
)
{
    UNREFERENCED_PARAMETER(Stream);
    UNREFERENCED_PARAMETER(NotificationEvent);
    
    return STATUS_SUCCESS;
}

NTSTATUS CavernStreamUnregisterNotificationEvent(
    _In_ IMiniportWaveRTStream* Stream,
    _In_ PKEVENT NotificationEvent
)
{
    UNREFERENCED_PARAMETER(Stream);
    UNREFERENCED_PARAMETER(NotificationEvent);
    
    return STATUS_SUCCESS;
}

NTSTATUS CavernStreamGetClockRegister(
    _In_ IMiniportWaveRTStream* Stream,
    _Out_ PKSRTAUDIO_HWREGISTER Register
)
{
    UNREFERENCED_PARAMETER(Stream);
    UNREFERENCED_PARAMETER(Register);
    
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS CavernStreamGetPositionRegister(
    _In_ IMiniportWaveRTStream* Stream,
    _Out_ PKSRTAUDIO_HWREGISTER Register
)
{
    UNREFERENCED_PARAMETER(Stream);
    UNREFERENCED_PARAMETER(Register);
    
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS CavernStreamSetWritePacket(
    _In_ IMiniportWaveRTStream* Stream,
    _In_ ULONG PacketNumber,
    _In_ DWORD Flags,
    _In_ ULONG EosPacketLength
)
{
    UNREFERENCED_PARAMETER(Stream);
    UNREFERENCED_PARAMETER(PacketNumber);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(EosPacketLength);
    
    return STATUS_SUCCESS;
}

NTSTATUS CavernStreamGetReadPacket(
    _In_ IMiniportWaveRTStream* Stream,
    _Out_ ULONG* PacketNumber,
    _Out_ DWORD* Flags,
    _Out_ ULONG* EosPacketLength
)
{
    UNREFERENCED_PARAMETER(Stream);
    UNREFERENCED_PARAMETER(PacketNumber);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(EosPacketLength);
    
    return STATUS_NOT_SUPPORTED;
}

/**************************************************************************
 * Driver Entry Point - Register with PortCls
 ***************************************************************************/

NTSTATUS CavernRegisterAudioDevice(WDFDEVICE Device)
{
    UNREFERENCED_PARAMETER(Device);
    
    KdPrint(("CavernAudio: RegisterAudioDevice called\n"));
    
    // PortCls will call us via the miniport creation
    return STATUS_SUCCESS;
}

// Export function to create miniport
NTSTATUS CavernCreateMiniport(
    _Out_ PUNKNOWN* Unknown,
    _In_ REFCLSID ClassId,
    _In_ PUNKNOWN UnknownOuter,
    _In_ POOL_TYPE PoolType
)
{
    PCAVERN_MINIPORT miniport;
    
    UNREFERENCED_PARAMETER(ClassId);
    UNREFERENCED_PARAMETER(UnknownOuter);
    UNREFERENCED_PARAMETER(PoolType);
    
    KdPrint(("CavernAudio: CreateMiniport\n"));
    
    miniport = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(CAVERN_MINIPORT), 'navC');
    if (!miniport) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    RtlZeroMemory(miniport, sizeof(CAVERN_MINIPORT));
    
    // Initialize miniport
    miniport->Miniport.lpVtbl = &CavernMiniportVtbl;
    miniport->RefCount = 1;
    miniport->State = KSSTATE_STOP;
    miniport->FormatSet = FALSE;
    KeInitializeSpinLock(&miniport->Lock);
    RtlInitUnicodeString(&miniport->PipeName, L"\\??\\pipe\\CavernAudioPipe");
    
    // Set default format
    miniport->Format.wFormatTag = WAVE_FORMAT_PCM;
    miniport->Format.nChannels = 8;
    miniport->Format.nSamplesPerSec = 48000;
    miniport->Format.nAvgBytesPerSec = 768000;
    miniport->Format.nBlockAlign = 16;
    miniport->Format.wBitsPerSample = 16;
    miniport->Format.cbSize = 0;
    
    *Unknown = (PUNKNOWN)miniport;
    
    return STATUS_SUCCESS;
}
