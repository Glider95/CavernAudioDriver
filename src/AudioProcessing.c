/***************************************************************************
 * AudioProcessing.c
 * 
 * Audio Processing Thread Implementation
 * 
 * THIS IS THE CRITICAL MISSING PIECE
 * 
 * This module implements the kernel thread that reads audio data from the
 * DMA buffer and forwards it to CavernPipeServer via named pipe.
 ***************************************************************************/

#include "CavernAudioDriver.h"

// Thread priority for real-time audio
#define CAVERN_THREAD_PRIORITY LOW_REALTIME_PRIORITY

// Buffer reading parameters
#define CAVERN_READ_INTERVAL_MS     10      // 10ms read interval
#define CAVERN_BUFFER_THRESHOLD     4096    // Minimum bytes to process
#define CAVERN_MAX_FORWARD_SIZE     8192    // Max bytes per forward

// E-AC3 sync word
#define EAC3_SYNC_WORD              0x0B77

// TrueHD sync word (4 bytes: 0xF8726FBA)
#define TRUEHD_SYNC_WORD_0          0xF8
#define TRUEHD_SYNC_WORD_1          0x72
#define TRUEHD_SYNC_WORD_2          0x6F
#define TRUEHD_SYNC_WORD_3          0xBA

// Context for audio processing thread
typedef struct _CAVERN_AUDIO_CONTEXT {
    PCAVERN_MINIPORT Miniport;
    PKTHREAD Thread;
    KEVENT StopEvent;
    BOOLEAN Running;
} CAVERN_AUDIO_CONTEXT, *PCAVERN_AUDIO_CONTEXT;

// Function prototypes
VOID CavernAudioProcessingThread(_In_ PVOID Context);
NTSTATUS CavernForwardToPipe(
    _In_ PCAVERN_MINIPORT Miniport,
    _In_reads_bytes_(DataSize) PVOID Data,
    _In_ SIZE_T DataSize
);
CAVERN_AUDIO_FORMAT CavernDetectFormat(
    _In_reads_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferSize
);

/***************************************************************************
 * CavernStartAudioProcessing
 * Starts the audio processing thread
 ***************************************************************************/
NTSTATUS CavernStartAudioProcessing(_In_ PCAVERN_MINIPORT Miniport)
{
    NTSTATUS status;
    PCAVERN_AUDIO_CONTEXT context;
    OBJECT_ATTRIBUTES objAttr;
    HANDLE threadHandle;
    
    CavernTrace("StartAudioProcessing");
    
    // Allocate context
    context = (PCAVERN_AUDIO_CONTEXT)ExAllocatePoolWithTag(
        NonPagedPoolNx,
        sizeof(CAVERN_AUDIO_CONTEXT),
        DRIVER_TAG
    );
    
    if (!context) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    RtlZeroMemory(context, sizeof(CAVERN_AUDIO_CONTEXT));
    context->Miniport = Miniport;
    context->Running = TRUE;
    KeInitializeEvent(&context->StopEvent, NotificationEvent, FALSE);
    
    // Initialize object attributes
    InitializeObjectAttributes(&objAttr, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
    
    // Create processing thread
    status = PsCreateSystemThread(
        &threadHandle,
        THREAD_ALL_ACCESS,
        &objAttr,
        NULL,
        NULL,
        CavernAudioProcessingThread,
        context
    );
    
    if (!NT_SUCCESS(status)) {
        CavernTrace("Failed to create thread: 0x%08X", status);
        ExFreePoolWithTag(context, DRIVER_TAG);
        return status;
    }
    
    // Get thread pointer and set priority
    status = ObReferenceObjectByHandle(
        threadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        KernelMode,
        (PVOID*)&context->Thread,
        NULL
    );
    
    ZwClose(threadHandle);
    
    if (!NT_SUCCESS(status)) {
        CavernTrace("Failed to reference thread: 0x%08X", status);
        context->Running = FALSE;
        KeSetEvent(&context->StopEvent, 0, FALSE);
        ExFreePoolWithTag(context, DRIVER_TAG);
        return status;
    }
    
    // Set real-time priority for audio
    KeSetPriorityThread(context->Thread, CAVERN_THREAD_PRIORITY);
    
    // Store context in miniport
    Miniport->AudioContext = context;
    
    CavernTrace("Audio processing thread started");
    return STATUS_SUCCESS;
}

/***************************************************************************
 * CavernStopAudioProcessing
 * Stops the audio processing thread
 ***************************************************************************/
VOID CavernStopAudioProcessing(_In_ PCAVERN_MINIPORT Miniport)
{
    PCAVERN_AUDIO_CONTEXT context;
    
    CavernTrace("StopAudioProcessing");
    
    context = (PCAVERN_AUDIO_CONTEXT)Miniport->AudioContext;
    if (!context) {
        return;
    }
    
    // Signal thread to stop
    context->Running = FALSE;
    KeSetEvent(&context->StopEvent, 0, FALSE);
    
    // Wait for thread to exit (max 5 seconds)
    LARGE_INTEGER timeout;
    timeout.QuadPart = -50000000LL; // 5 seconds in 100ns units
    
    KeWaitForSingleObject(
        context->Thread,
        Executive,
        KernelMode,
        FALSE,
        &timeout
    );
    
    // Dereference thread object
    ObDereferenceObject(context->Thread);
    
    // Free context
    ExFreePoolWithTag(context, DRIVER_TAG);
    Miniport->AudioContext = NULL;
    
    CavernTrace("Audio processing thread stopped");
}

/***************************************************************************
 * CavernAudioProcessingThread
 * Main audio processing loop
 ***************************************************************************/
VOID CavernAudioProcessingThread(_In_ PVOID Context)
{
    PCAVERN_AUDIO_CONTEXT context;
    PCAVERN_MINIPORT miniport;
    PVOID dmaBuffer;
    ULONG dmaSize;
    ULONG readPosition;
    ULONG writePosition;
    LARGE_INTEGER interval;
    
    context = (PCAVERN_AUDIO_CONTEXT)Context;
    miniport = context->Miniport;
    
    CavernTrace("Audio thread started");
    
    // Set read interval (10ms)
    interval.QuadPart = -100000LL; // 10ms in 100ns units
    
    while (context->Running) {
        // Check if we have valid DMA buffer
        dmaBuffer = miniport->DmaBuffer;
        dmaSize = miniport->DmaBufferSize;
        
        if (!dmaBuffer || dmaSize == 0) {
            // No buffer yet, wait
            KeDelayExecutionThread(KernelMode, FALSE, &interval);
            continue;
        }
        
        // Get current positions
        readPosition = miniport->DmaPosition;
        writePosition = miniport->WritePosition;
        
        // Calculate available data
        ULONG availableData;
        if (writePosition >= readPosition) {
            availableData = writePosition - readPosition;
        } else {
            // Wrap-around
            availableData = (dmaSize - readPosition) + writePosition;
        }
        
        // Process if we have enough data
        if (availableData >= CAVERN_BUFFER_THRESHOLD) {
            // Determine how much to process (handle wrap-around)
            ULONG processSize = min(availableData, CAVERN_MAX_FORWARD_SIZE);
            
            if (readPosition + processSize > dmaSize) {
                // Need to handle wrap-around in two parts
                ULONG firstPart = dmaSize - readPosition;
                ULONG secondPart = processSize - firstPart;
                
                // Process first part
                CavernProcessAudioData(miniport, 
                    (PUCHAR)dmaBuffer + readPosition, firstPart);
                
                // Process second part (wrapped)
                CavernProcessAudioData(miniport, dmaBuffer, secondPart);
                
                readPosition = secondPart;
            } else {
                // Single contiguous read
                CavernProcessAudioData(miniport,
                    (PUCHAR)dmaBuffer + readPosition, processSize);
                
                readPosition += processSize;
            }
            
            // Update position
            miniport->DmaPosition = readPosition;
        }
        
        // Wait before next iteration
        KeDelayExecutionThread(KernelMode, FALSE, &interval);
    }
    
    CavernTrace("Audio thread exiting");
    PsTerminateSystemThread(STATUS_SUCCESS);
}

/***************************************************************************
 * CavernProcessAudioData
 * Process audio data chunk - detect format and forward
 ***************************************************************************/
NTSTATUS CavernProcessAudioData(
    _In_ PCAVERN_MINIPORT Miniport,
    _In_reads_bytes_(DataSize) PVOID Data,
    _In_ SIZE_T DataSize
)
{
    CAVERN_AUDIO_FORMAT format;
    NTSTATUS status = STATUS_SUCCESS;
    
    // Detect format
    format = CavernDetectFormat(Data, DataSize);
    
    switch (format.FormatTag) {
        case CAVERN_FORMAT_PCM:
            // PCM data - forward as-is (for testing)
            // In production, might want to handle differently
            CavernTrace("Processing PCM: %zu bytes", DataSize);
            status = CavernForwardToPipe(Miniport, Data, DataSize);
            break;
            
        case CAVERN_FORMAT_EAC3:
            // E-AC3 bitstream - forward raw
            CavernTrace("Processing E-AC3: %zu bytes", DataSize);
            status = CavernForwardToPipe(Miniport, Data, DataSize);
            break;
            
        case CAVERN_FORMAT_TRUEHD:
            // TrueHD bitstream - forward raw
            CavernTrace("Processing TrueHD: %zu bytes", DataSize);
            status = CavernForwardToPipe(Miniport, Data, DataSize);
            break;
            
        default:
            // Unknown format - try to forward anyway
            CavernTrace("Unknown format, forwarding raw: %zu bytes", DataSize);
            status = CavernForwardToPipe(Miniport, Data, DataSize);
            break;
    }
    
    return status;
}

/***************************************************************************
 * CavernDetectFormat
 * Detect audio format by examining sync words
 ***************************************************************************/
CAVERN_AUDIO_FORMAT CavernDetectFormat(
    _In_reads_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferSize
)
{
    CAVERN_AUDIO_FORMAT format = {0};
    PUCHAR data = (PUCHAR)Buffer;
    
    format.FormatTag = CAVERN_FORMAT_PCM; // Default
    
    if (BufferSize < 4) {
        return format; // Not enough data
    }
    
    // Check for E-AC3 sync word (0x0B77)
    // Can appear at offset 0 or 1 (byte-aligned)
    for (SIZE_T i = 0; i < min(BufferSize - 2, 2); i++) {
        if (data[i] == 0x0B && data[i+1] == 0x77) {
            format.FormatTag = CAVERN_FORMAT_EAC3;
            format.IsAtmos = TRUE; // Assume Atmos for E-AC3
            return format;
        }
    }
    
    // Check for TrueHD sync word (0xF8726FBA)
    if (BufferSize >= 4) {
        if (data[0] == TRUEHD_SYNC_WORD_0 &&
            data[1] == TRUEHD_SYNC_WORD_1 &&
            data[2] == TRUEHD_SYNC_WORD_2 &&
            data[3] == TRUEHD_SYNC_WORD_3) {
            format.FormatTag = CAVERN_FORMAT_TRUEHD;
            format.IsAtmos = TRUE;
            return format;
        }
    }
    
    // Check for DTS sync word (0x7FFE8001)
    if (BufferSize >= 4) {
        if ((data[0] == 0x7F && data[1] == 0xFE &&
             data[2] == 0x80 && data[3] == 0x01) ||
            (data[0] == 0xFE && data[1] == 0x7F &&
             data[2] == 0x01 && data[3] == 0x80)) {
            format.FormatTag = CAVERN_FORMAT_DTS;
            return format;
        }
    }
    
    // Default to PCM if no sync words found
    return format;
}

/***************************************************************************
 * CavernForwardToPipe
 * Forward audio data to named pipe
 ***************************************************************************/
NTSTATUS CavernForwardToPipe(
    _In_ PCAVERN_MINIPORT Miniport,
    _In_reads_bytes_(DataSize) PVOID Data,
    _In_ SIZE_T DataSize
)
{
    NTSTATUS status;
    IO_STATUS_BLOCK ioStatus;
    
    // Ensure pipe is open
    if (!Miniport->PipeHandle) {
        status = CavernOpenPipeConnection(Miniport);
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }
    
    // Write to pipe
    status = ZwWriteFile(
        Miniport->PipeHandle,
        NULL,
        NULL,
        NULL,
        &ioStatus,
        Data,
        (ULONG)DataSize,
        NULL,
        NULL
    );
    
    if (status == STATUS_PIPE_BROKEN ||
        status == STATUS_PIPE_DISCONNECTED) {
        // Try to reconnect
        CavernTrace("Pipe disconnected, attempting reconnect");
        CavernClosePipeConnection(Miniport);
        status = CavernOpenPipeConnection(Miniport);
        
        if (NT_SUCCESS(status)) {
            // Retry write
            status = ZwWriteFile(
                Miniport->PipeHandle,
                NULL,
                NULL,
                NULL,
                &ioStatus,
                Data,
                (ULONG)DataSize,
                NULL,
                NULL
            );
        }
    }
    
    if (!NT_SUCCESS(status)) {
        CavernTrace("Failed to write to pipe: 0x%08X", status);
    }
    
    return status;
}

/***************************************************************************
 * CavernOpenPipeConnection
 * Open named pipe to CavernPipeServer
 ***************************************************************************/
NTSTATUS CavernOpenPipeConnection(_In_ PCAVERN_MINIPORT Miniport)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING pipeName;
    IO_STATUS_BLOCK ioStatus;
    
    if (Miniport->PipeHandle) {
        return STATUS_SUCCESS; // Already open
    }
    
    RtlInitUnicodeString(&pipeName, L"\\??\\pipe\\CavernAudioPipe");
    InitializeObjectAttributes(
        &objAttr,
        &pipeName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL
    );
    
    status = ZwCreateFile(
        &Miniport->PipeHandle,
        GENERIC_WRITE | SYNCHRONIZE,
        &objAttr,
        &ioStatus,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        0,
        FILE_OPEN,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONIZE_IO_NONALERT,
        NULL,
        0
    );
    
    if (NT_SUCCESS(status)) {
        CavernTrace("Pipe connection opened");
    } else {
        CavernTrace("Failed to open pipe: 0x%08X", status);
    }
    
    return status;
}

/***************************************************************************
 * CavernClosePipeConnection
 * Close named pipe connection
 ***************************************************************************/
VOID CavernClosePipeConnection(_In_ PCAVERN_MINIPORT Miniport)
{
    if (Miniport->PipeHandle) {
        ZwClose(Miniport->PipeHandle);
        Miniport->PipeHandle = NULL;
        CavernTrace("Pipe connection closed");
    }
}
