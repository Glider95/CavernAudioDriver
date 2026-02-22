/***************************************************************************
 * CavernAudioDriver.c
 * 
 * Cavern Dolby Atmos Virtual Audio Driver
 * Simplified PortCls-based implementation
 ***************************************************************************/

#include <ntddk.h>
#include <wdf.h>

// PortCls headers must be included after ntddk/wdf
#include <portcls.h>
#include <ks.h>
#include <ksmedia.h>

// Global driver object for PortCls
PDRIVER_OBJECT g_DriverObject = NULL;

// Driver entry point - this is the main entry for kernel drivers
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    
    KdPrint(("CavernAudioDriver: DriverEntry\n"));
    
    // Save driver object
    g_DriverObject = DriverObject;
    
    // Register with PortCls
    // This sets up the audio driver infrastructure
    status = PcInitializeAdapterDriver(
        DriverObject,
        RegistryPath,
        (PDRIVER_ADD_DEVICE)AddDevice
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: PcInitializeAdapterDriver failed 0x%08X\n", status));
        return status;
    }
    
    KdPrint(("CavernAudioDriver: Driver loaded\n"));
    return STATUS_SUCCESS;
}

// AddDevice - Called by PortCls when device is detected
NTSTATUS AddDevice(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PDEVICE_OBJECT PhysicalDeviceObject
)
{
    NTSTATUS status;
    PDEVICE_OBJECT functionalDeviceObject = NULL;
    
    UNREFERENCED_PARAMETER(DriverObject);
    
    KdPrint(("CavernAudioDriver: AddDevice\n"));
    
    // Create the functional device object
    status = IoCreateDevice(
        g_DriverObject,
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
    
    // Attach to the device stack
    PDEVICE_OBJECT lowerDeviceObject = IoAttachDeviceToDeviceStack(
        functionalDeviceObject,
        PhysicalDeviceObject
    );
    
    if (!lowerDeviceObject) {
        KdPrint(("CavernAudioDriver: IoAttachDeviceToDeviceStack failed\n"));
        IoDeleteDevice(functionalDeviceObject);
        return STATUS_UNSUCCESSFUL;
    }
    
    // Create a simple miniport that just registers the device
    // PortCls will handle the audio subsystem integration
    
    // Set device flags
    functionalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    functionalDeviceObject->Flags |= DO_DIRECT_IO;
    
    KdPrint(("CavernAudioDriver: Device created successfully\n"));
    
    return STATUS_SUCCESS;
}

// Unload - Cleanup when driver is unloaded
VOID DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);
    KdPrint(("CavernAudioDriver: Unload\n"));
}
