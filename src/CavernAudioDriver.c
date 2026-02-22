/***************************************************************************
 * CavernAudioDriver.c
 * 
 * Cavern Dolby Atmos Virtual Audio Driver
 * Forwards raw bitstream to CavernPipeServer via named pipe
 ***************************************************************************/

#include <ntddk.h>
#include <wdf.h>
#include "..\include\FormatDetection.h"

#define DRIVER_TAG 'nvarC'  // "Cavn" in little-endian
#define PIPE_NAME L"\\??\\pipe\\CavernAudioPipe"
#define CAVERN_POOL_TAG 'navC'

// Driver context structure
typedef struct _DRIVER_CONTEXT {
    WDFDEVICE Device;
    HANDLE PipeHandle;
    BOOLEAN PassthroughEnabled;
    BOOLEAN PipeConnected;
    UNICODE_STRING PipeName;
    CAVERN_FORMAT_INFO CurrentFormat;
    KSPIN_LOCK PipeLock;
} DRIVER_CONTEXT, *PDRIVER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DRIVER_CONTEXT, GetDriverContext)

// Function prototypes
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD CavernAudioEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_WRITE CavernAudioEvtIoWrite;
EVT_WDF_DEVICE_PREPARE_HARDWARE CavernAudioEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE CavernAudioEvtReleaseHardware;

NTSTATUS CavernAudioCreatePipe(PDRIVER_CONTEXT Context);
VOID CavernAudioClosePipe(PDRIVER_CONTEXT Context);
NTSTATUS CavernAudioForwardToPipe(
    PDRIVER_CONTEXT Context,
    PVOID Buffer,
    SIZE_T Length
);

/**************************************************************************
 * DriverEntry
 ***************************************************************************/
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    WDF_DRIVER_CONFIG config;

    KdPrint(("CavernAudioDriver: DriverEntry - v1.0 Dolby Atmos Passthrough\n"));

    WDF_DRIVER_CONFIG_INIT(&config, CavernAudioEvtDeviceAdd);

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );

    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: WdfDriverCreate failed 0x%08X\n", status));
        return status;
    }

    KdPrint(("CavernAudioDriver: Driver loaded successfully\n"));
    return STATUS_SUCCESS;
}

/**************************************************************************
 * CavernAudioEvtDeviceAdd
 ***************************************************************************/
NTSTATUS CavernAudioEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    WDFDEVICE device;
    PDRIVER_CONTEXT context;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    WDF_IO_QUEUE_CONFIG queueConfig;

    UNREFERENCED_PARAMETER(Driver);

    KdPrint(("CavernAudioDriver: DeviceAdd\n"));

    // Initialize PnP callbacks
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
    pnpCallbacks.EvtDevicePrepareHardware = CavernAudioEvtPrepareHardware;
    pnpCallbacks.EvtDeviceReleaseHardware = CavernAudioEvtReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);

    // Set device type
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_UNKNOWN);

    // Initialize context attributes
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DRIVER_CONTEXT);

    // Create device
    status = WdfDeviceCreate(&DeviceInit, 
        &attributes, 
        &device
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: WdfDeviceCreate failed 0x%08X\n", status));
        return status;
    }

    // Get driver context
    context = GetDriverContext(device);
    RtlZeroMemory(context, sizeof(DRIVER_CONTEXT));
    context->Device = device;
    context->PassthroughEnabled = TRUE;
    context->PipeConnected = FALSE;
    RtlInitUnicodeString(&context->PipeName, PIPE_NAME);
    KeInitializeSpinLock(&context->PipeLock);

    // Create I/O queue
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoWrite = CavernAudioEvtIoWrite;

    status = WdfIoQueueCreate(
        device, 
        &queueConfig, 
        WDF_NO_OBJECT_ATTRIBUTES, 
        WDF_NO_HANDLE
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: WdfIoQueueCreate failed 0x%08X\n", status));
        return status;
    }

    KdPrint(("CavernAudioDriver: Device created successfully\n"));
    return STATUS_SUCCESS;
}

/**************************************************************************
 * CavernAudioEvtPrepareHardware
 ***************************************************************************/
NTSTATUS CavernAudioEvtPrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
)
{
    PDRIVER_CONTEXT context;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    KdPrint(("CavernAudioDriver: PrepareHardware\n"));

    context = GetDriverContext(Device);
    status = CavernAudioCreatePipe(context);
    
    if (NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: Pipe connected on PrepareHardware\n"));
    } else {
        KdPrint(("CavernAudioDriver: Pipe not available (will retry on write)\n"));
        // Don't fail - we'll retry when data arrives
    }

    return STATUS_SUCCESS;
}

/**************************************************************************
 * CavernAudioEvtReleaseHardware
 ***************************************************************************/
NTSTATUS CavernAudioEvtReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
)
{
    PDRIVER_CONTEXT context;

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    KdPrint(("CavernAudioDriver: ReleaseHardware\n"));

    context = GetDriverContext(Device);
    CavernAudioClosePipe(context);

    return STATUS_SUCCESS;
}

/**************************************************************************
 * CavernAudioCreatePipe
 ***************************************************************************/
NTSTATUS CavernAudioCreatePipe(PDRIVER_CONTEXT Context)
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objAttr;
    IO_STATUS_BLOCK ioStatus;
    KIRQL oldIrql;

    KeAcquireSpinLock(&Context->PipeLock, &oldIrql);

    if (Context->PipeHandle != NULL) {
        KeReleaseSpinLock(&Context->PipeLock, oldIrql);
        return STATUS_SUCCESS;
    }

    KdPrint(("CavernAudioDriver: Creating pipe connection to %wZ\n", &Context->PipeName));

    InitializeObjectAttributes(
        &objAttr,
        &Context->PipeName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL
    );

    status = ZwCreateFile(
        &Context->PipeHandle,
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

    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: ZwCreateFile failed 0x%08X\n", status));
        Context->PipeHandle = NULL;
        Context->PipeConnected = FALSE;
    } else {
        KdPrint(("CavernAudioDriver: Pipe connected successfully\n"));
        Context->PipeConnected = TRUE;
    }

    KeReleaseSpinLock(&Context->PipeLock, oldIrql);
    return status;
}

/**************************************************************************
 * CavernAudioClosePipe
 ***************************************************************************/
VOID CavernAudioClosePipe(PDRIVER_CONTEXT Context)
{
    KIRQL oldIrql;

    KeAcquireSpinLock(&Context->PipeLock, &oldIrql);

    if (Context->PipeHandle != NULL) {
        KdPrint(("CavernAudioDriver: Closing pipe\n"));
        ZwClose(Context->PipeHandle);
        Context->PipeHandle = NULL;
        Context->PipeConnected = FALSE;
    }

    KeReleaseSpinLock(&Context->PipeLock, oldIrql);
}

/**************************************************************************
 * CavernAudioForwardToPipe
 ***************************************************************************/
NTSTATUS CavernAudioForwardToPipe(
    PDRIVER_CONTEXT Context,
    PVOID Buffer,
    SIZE_T Length
)
{
    NTSTATUS status;
    IO_STATUS_BLOCK ioStatus;
    KIRQL oldIrql;

    // Ensure pipe is connected
    if (!Context->PipeConnected || Context->PipeHandle == NULL) {
        status = CavernAudioCreatePipe(Context);
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    KeAcquireSpinLock(&Context->PipeLock, &oldIrql);

    if (Context->PipeHandle == NULL) {
        KeReleaseSpinLock(&Context->PipeLock, oldIrql);
        return STATUS_DEVICE_NOT_CONNECTED;
    }

    status = ZwWriteFile(
        Context->PipeHandle,
        NULL,
        NULL,
        NULL,
        &ioStatus,
        Buffer,
        (ULONG)Length,
        NULL,
        NULL
    );

    KeReleaseSpinLock(&Context->PipeLock, oldIrql);

    if (NT_SUCCESS(status)) {
        status = ioStatus.Status;
    }

    return status;
}

/**************************************************************************
 * CavernAudioEvtIoWrite - Handles audio data from applications
 ***************************************************************************/
VOID CavernAudioEvtIoWrite(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
)
{
    PDRIVER_CONTEXT context;
    PVOID buffer;
    NTSTATUS status;
    WDFDEVICE device;
    CAVERN_FORMAT_TYPE formatType;

    device = WdfIoQueueGetDevice(Queue);
    context = GetDriverContext(device);

    // Get the write buffer
    status = WdfRequestRetrieveInputBuffer(Request, 0, &buffer, NULL);
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: WdfRequestRetrieveInputBuffer failed 0x%08X\n", status));
        WdfRequestComplete(Request, status);
        return;
    }

    // Detect format if we don't know it yet
    if (context->CurrentFormat.Format == CavernFormatUnknown) {
        formatType = CavernDetectFormatInline((PUCHAR)buffer, Length);
        
        if (formatType != CavernFormatUnknown) {
            CavernGetFormatInfo(formatType, &context->CurrentFormat);
            
            switch (formatType) {
                case CavernFormatEAC3:
                    KdPrint(("CavernAudioDriver: Detected E-AC3 (Dolby Digital Plus)\n"));
                    break;
                case CavernFormatTrueHD:
                    KdPrint(("CavernAudioDriver: Detected Dolby TrueHD\n"));
                    break;
                case CavernFormatAC3:
                    KdPrint(("CavernAudioDriver: Detected AC3 (Dolby Digital)\n"));
                    break;
                default:
                    KdPrint(("CavernAudioDriver: Detected format type %d\n", formatType));
                    break;
            }
        }
    }

    // Forward to pipe if passthrough is enabled
    if (context->PassthroughEnabled) {
        status = CavernAudioForwardToPipe(context, buffer, Length);
        
        if (!NT_SUCCESS(status)) {
            // Log error but still complete the request
            // The audio subsystem should continue working even if pipe fails
            KdPrint(("CavernAudioDriver: ForwardToPipe failed 0x%08X\n", status));
        }
    }

    // Complete the request
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
}
