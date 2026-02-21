/***************************************************************************
 * CavernAudioDriver.h
 * 
 * Header file for Cavern Dolby Atmos Virtual Audio Driver
 ***************************************************************************/

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <ks.h>
#include <ksmedia.h>

// Driver version
#define CAVERN_AUDIO_VERSION_MAJOR 1
#define CAVERN_AUDIO_VERSION_MINOR 0
#define CAVERN_AUDIO_VERSION_BUILD 0

// Maximum supported channels
#define CAVERN_MAX_CHANNELS 16

// Buffer sizes
#define CAVERN_BUFFER_SIZE (48 * 1024)      // 48KB buffers
#define CAVERN_MAX_PACKET_SIZE (4096)       // 4KB max packet

// Supported formats
#define CAVERN_FORMAT_PCM       0x0001
#define CAVERN_FORMAT_AC3       0x2000
#define CAVERN_FORMAT_EAC3      0x2001  // Dolby Digital Plus
#define CAVERN_FORMAT_TRUEHD    0x2002  // Dolby TrueHD
#define CAVERN_FORMAT_MAT       0x2003  // Dolby MAT (Atmos)
#define CAVERN_FORMAT_DTS       0x2004
#define CAVERN_FORMAT_DTSHD     0x2005

// Custom properties
#define CAVERN_PROP_ENABLE_PASSTHROUGH  1
#define CAVERN_PROP_SET_FORMAT          2
#define CAVERN_PROP_GET_FORMAT          3

// Structure for format information
typedef struct _CAVERN_AUDIO_FORMAT {
    ULONG FormatTag;        // CAVERN_FORMAT_* 
    ULONG Channels;
    ULONG SampleRate;
    ULONG BitsPerSample;
    ULONG BitRate;          // For compressed formats
    BOOLEAN IsAtmos;        // TRUE if Atmos content
} CAVERN_AUDIO_FORMAT, *PCAVERN_AUDIO_FORMAT;

// Device context extension
typedef struct _CAVERN_DEVICE_EXTENSION {
    // Audio format info
    CAVERN_AUDIO_FORMAT CurrentFormat;
    
    // State
    BOOLEAN StreamingActive;
    BOOLEAN PassthroughEnabled;
    
    // Statistics
    ULONGLONG BytesProcessed;
    ULONGLONG PacketsProcessed;
    
    // Synchronization
    KSPIN_LOCK Lock;
    
    // Pipe handle to CavernPipeServer
    HANDLE PipeHandle;
    UNICODE_STRING PipeName;
    
} CAVERN_DEVICE_EXTENSION, *PCAVERN_DEVICE_EXTENSION;

// Function prototypes for miniport interface

// Wave RT miniport functions
NTSTATUS CavernWaveRTCreate(
    _Out_ PUNKNOWN *Unknown,
    _In_ REFCLSID ClassId,
    _In_opt_ PUNKNOWN UnknownOuter
);

NTSTATUS CavernWaveRTInit(
    _In_ PUNKNOWN Unknown,
    _In_ PRESOURCELIST ResourceList,
    _In_ PPORTWAVERT Port
);

// Format detection
NTSTATUS CavernDetectFormat(
    _In_reads_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Out_ PCAVERN_AUDIO_FORMAT Format
);

// Data processing
NTSTATUS CavernProcessAudioData(
    _In_ PCAVERN_DEVICE_EXTENSION Extension,
    _In_reads_bytes_(DataSize) PVOID Data,
    _In_ SIZE_T DataSize
);

// Pipe communication
NTSTATUS CavernOpenPipe(
    _In_ PCAVERN_DEVICE_EXTENSION Extension
);

VOID CavernClosePipe(
    _In_ PCAVERN_DEVICE_EXTENSION Extension
);

NTSTATUS CavernSendToPipe(
    _In_ PCAVERN_DEVICE_EXTENSION Extension,
    _In_reads_bytes_(DataSize) PVOID Data,
    _In_ SIZE_T DataSize
);

// Debug helpers
#if DBG
    #define CavernTrace(Format, ...) \
        DbgPrint("CavernAudio: " Format "\n", ##__VA_ARGS__)
#else
    #define CavernTrace(Format, ...) ((void)0)
#endif
