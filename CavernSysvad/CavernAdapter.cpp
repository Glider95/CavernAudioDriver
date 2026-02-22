/***************************************************************************
 * CavernAdapter.cpp
 * 
 * Adapter for Cavern Audio Driver
 * Based on Microsoft SysVAD sample
 ***************************************************************************/

#include <ntddk.h>
#include <wdf.h>
#include <portcls.h>
#include <ks.h>
#include <ksmedia.h>

#include "CavernMiniportWaveRT.h"

// Driver entry point
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    
    KdPrint(("CavernAudioDriver: DriverEntry\n"));
    
    // Initialize PortCls
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

// AddDevice - Called by PortCls to create the audio device
NTSTATUS AddDevice(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PDEVICE_OBJECT PhysicalDeviceObject
)
{
    NTSTATUS status;
    PDEVICE_OBJECT functionalDeviceObject = NULL;
    PUNKNOWN unknownMiniport = NULL;
    IResourceList* resourceList = NULL;
    
    UNREFERENCED_PARAMETER(DriverObject);
    
    KdPrint(("CavernAudioDriver: AddDevice\n"));
    
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
    status = CreateCavernMiniportWaveRT(
        &unknownMiniport,
        GUID_NULL,
        NULL,
        NonPagedPoolNx
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: CreateCavernMiniportWaveRT failed 0x%08X\n", status));
        IoDetachDevice(lowerDeviceObject);
        IoDeleteDevice(functionalDeviceObject);
        if (resourceList) {
            resourceList->lpVtbl->Release(resourceList);
        }
        return status;
    }
    
    // Create PortCls port for WaveRT
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
        NULL,   // Device name
        NULL,   // DRM rights
        0,      // DRM right count
        NULL    // ISR
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
    
    // Clean up
    portWaveRT->lpVtbl->Release(portWaveRT);
    unknownMiniport->lpVtbl->Release(unknownMiniport);
    if (resourceList) {
        resourceList->lpVtbl->Release(resourceList);
    }
    
    return STATUS_SUCCESS;
}
