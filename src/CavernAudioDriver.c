/***************************************************************************
 * CavernAudioDriver.c
 * 
 * Cavern Dolby Atmos Virtual Audio Driver
 * PortCls-based implementation for proper audio endpoint registration
 ***************************************************************************/

#include <ntddk.h>
#include <wdf.h>
#include <portcls.h>
#include <ks.h>
#include <ksmedia.h>

#define DRIVER_TAG 'nvarC'
#define PIPE_NAME L"\\??\\pipe\\CavernAudioPipe"

// Driver context
typedef struct _DRIVER_CONTEXT {
    WDFDEVICE Device;
    PPORTCLSETWHELPER PortClsEtwHelper;
} DRIVER_CONTEXT, *PDRIVER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DRIVER_CONTEXT, GetDriverContext)

// Forward declarations
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD CavernAudioEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE CavernAudioEvtPrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE CavernAudioEvtReleaseHardware;

// PortCls miniport creation (exported from MiniportWaveRT.c)
extern NTSTATUS CavernCreateMiniport(
    _Out_ PUNKNOWN* Unknown,
    _In_ REFCLSID ClassId,
    _In_ PUNKNOWN UnknownOuter,
    _In_ POOL_TYPE PoolType
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
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFDRIVER driver;
    
    KdPrint(("CavernAudioDriver: DriverEntry - PortCls Audio Driver v1.0\n"));
    
    // Initialize WDF
    WDF_DRIVER_CONFIG_INIT(&config, CavernAudioEvtDeviceAdd);
    
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = NULL;
    
    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &attributes,
        &config,
        &driver
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: WdfDriverCreate failed 0x%08X\n", status));
        return status;
    }
    
    // Register with PortCls
    // This makes us an audio driver
    status = PcInitializeAdapterDriver(
        DriverObject,
        RegistryPath,
        NULL  // Use default AddDevice handler
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: PcInitializeAdapterDriver failed 0x%08X\n", status));
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
    
    UNREFERENCED_PARAMETER(Driver);
    
    KdPrint(("CavernAudioDriver: DeviceAdd\n"));
    
    // Set device type to audio
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_KS);
    
    // Initialize PnP callbacks
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
    pnpCallbacks.EvtDevicePrepareHardware = CavernAudioEvtPrepareHardware;
    pnpCallbacks.EvtDeviceReleaseHardware = CavernAudioEvtReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);
    
    // Initialize context
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
    
    // Get context
    context = GetDriverContext(device);
    RtlZeroMemory(context, sizeof(DRIVER_CONTEXT));
    context->Device = device;
    
    // Register with PortCls for audio processing
    // This creates the audio endpoint
    status = PcNewAdapterPowerManagement(
        (PPOWER_NOTIFY*)&context->PortClsEtwHelper,
        device
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: PcNewAdapterPowerManagement warning 0x%08X\n", status));
        // Continue anyway - not critical
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
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    
    KdPrint(("CavernAudioDriver: PrepareHardware\n"));
    
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
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    
    KdPrint(("CavernAudioDriver: ReleaseHardware\n"));
    
    return STATUS_SUCCESS;
}

/**************************************************************************
 * AddDevice - PortCls entry point
 * This is called by PortCls to create the audio device
 ***************************************************************************/
NTSTATUS AddDevice(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PDEVICE_OBJECT PhysicalDeviceObject
)
{
    NTSTATUS status;
    PDEVICE_OBJECT functionalDeviceObject = NULL;
    PUNKNOWN unknownMiniport = NULL;
    PUNKNOWN unknownTopology = NULL;
    IResourceList* resourceList = NULL;
    
    KdPrint(("CavernAudioDriver: AddDevice (PortCls)\n"));
    
    // Create the functional device object
    status = IoCreateDevice(
        DriverObject,
        0,
        NULL,
        FILE_DEVICE_KS,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &functionalDeviceObject
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: IoCreateDevice failed 0x%08X\n", status));
        return status;
    }
    
    // Attach to device stack
    PDEVICE_OBJECT lowerDeviceObject = IoAttachDeviceToDeviceStack(
        functionalDeviceObject,
        PhysicalDeviceObject
    );
    
    if (!lowerDeviceObject) {
        KdPrint(("CavernAudioDriver: IoAttachDeviceToDeviceStack failed\n"));
        IoDeleteDevice(functionalDeviceObject);
        return STATUS_UNSUCCESSFUL;
    }
    
    // Create resource list
    status = PcNewResourceList(
        &resourceList,
        NULL,
        NonPagedPoolNx,
        PhysicalDeviceObject,
        NULL,
        0
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: PcNewResourceList warning 0x%08X\n", status));
    }
    
    // Create our miniport
    status = CavernCreateMiniport(
        &unknownMiniport,
        NULL,
        NULL,
        NonPagedPoolNx
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: CavernCreateMiniport failed 0x%08X\n", status));
        IoDetachDevice(lowerDeviceObject);
        IoDeleteDevice(functionalDeviceObject);
        if (resourceList) {
            resourceList->lpVtbl->Release(resourceList);
        }
        return status;
    }
    
    // Register with PortCls - this creates the audio endpoint
    status = PcRegisterAdapterPowerManagement(
        unknownMiniport,
        functionalDeviceObject
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: PcRegisterAdapterPowerManagement warning 0x%08X\n", status));
        // Continue anyway
    }
    
    // Create the PortCls port for WaveRT
    IPortWaveRT* portWaveRT = NULL;
    status = PcNewPort(
        &portWaveRT,
        &CLSID_PortWaveRT
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: PcNewPort failed 0x%08X\n", status));
        unknownMiniport->lpVtbl->Release(unknownMiniport);
        IoDetachDevice(lowerDeviceObject);
        IoDeleteDevice(functionalDeviceObject);
        if (resourceList) {
            resourceList->lpVtbl->Release(resourceList);
        }
        return status;
    }
    
    // Initialize the port with our miniport
    status = portWaveRT->lpVtbl->Init(
        portWaveRT,
        unknownMiniport,
        functionalDeviceObject,
        lowerDeviceObject,
        NonPagedPoolNx,
        resourceList,
        NULL,  // Device name
        NULL,  // DRM rights
        0,     # DRM right count
        NULL   // Evt Interrupt service routine
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: PortWaveRT Init failed 0x%08X\n", status));
        portWaveRT->lpVtbl->Release(portWaveRT);
        unknownMiniport->lpVtbl->Release(unknownMiniport);
        IoDetachDevice(lowerDeviceObject);
        IoDeleteDevice(functionalDeviceObject);
        if (resourceList) {
            resourceList->lpVtbl->Release(resourceList);
        }
        return status;
    }
    
    // Device is ready
    functionalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    
    KdPrint(("CavernAudioDriver: AddDevice completed successfully\n"));
    
    // Clean up - PortCls now owns these
    portWaveRT->lpVtbl->Release(portWaveRT);
    unknownMiniport->lpVtbl->Release(unknownMiniport);
    if (resourceList) {
        resourceList->lpVtbl->Release(resourceList);
    }
    
    return STATUS_SUCCESS;
}
