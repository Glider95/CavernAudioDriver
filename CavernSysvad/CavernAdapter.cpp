/***************************************************************************
 * CavernAdapter.cpp
 * 
 * Adapter for Cavern Audio Driver
 ***************************************************************************/

#include <ntddk.h>
#include <wdf.h>
#include <portcls.h>
#include <ks.h>
#include <ksmedia.h>
#include "CavernMiniportWaveRT.h"

// Forward declaration
NTSTATUS AddDevice(_In_ PDRIVER_OBJECT DriverObject, _In_ PDEVICE_OBJECT PhysicalDeviceObject);

// Driver entry point
extern "C" NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    
    KdPrint(("CavernAudioDriver: DriverEntry\n"));
    
    status = PcInitializeAdapterDriver(
        DriverObject,
        RegistryPath,
        AddDevice
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
    
    UNREFERENCED_PARAMETER(DriverObject);
    
    KdPrint(("CavernAudioDriver: AddDevice\n"));
    
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
    
    PDEVICE_OBJECT lowerDeviceObject = IoAttachDeviceToDeviceStack(
        functionalDeviceObject,
        PhysicalDeviceObject
    );
    
    if (!lowerDeviceObject) {
        KdPrint(("CavernAudioDriver: IoAttachDeviceToDeviceStack failed\n"));
        IoDeleteDevice(functionalDeviceObject);
        return STATUS_UNSUCCESSFUL;
    }
    
    status = CreateCavernMiniportWaveRT(
        &unknownMiniport,
        GUID_NULL,
        NULL,
        POOL_FLAG_NON_PAGED
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: CreateCavernMiniportWaveRT failed 0x%08X\n", status));
        IoDetachDevice(lowerDeviceObject);
        IoDeleteDevice(functionalDeviceObject);
        return status;
    }
    
    PPORTWAVERT portWaveRT = NULL;
    status = PcNewPort(
        (PPORT*)&portWaveRT,
        IID_IPortWaveRT
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: PcNewPort failed 0x%08X\n", status));
        unknownMiniport->Release();
        IoDetachDevice(lowerDeviceObject);
        IoDeleteDevice(functionalDeviceObject);
        return status;
    }
    
    // Get the IMiniportWaveRT interface
    IMiniportWaveRT *miniportWaveRT = NULL;
    status = unknownMiniport->QueryInterface(IID_IMiniportWaveRT, (PVOID*)&miniportWaveRT);
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: QueryInterface failed 0x%08X\n", status));
        portWaveRT->Release();
        unknownMiniport->Release();
        IoDetachDevice(lowerDeviceObject);
        IoDeleteDevice(functionalDeviceObject);
        return status;
    }
    
    // Initialize the miniport
    status = miniportWaveRT->Init(portWaveRT);
    
    if (!NT_SUCCESS(status)) {
        KdPrint(("CavernAudioDriver: Miniport Init failed 0x%08X\n", status));
        miniportWaveRT->Release();
        portWaveRT->Release();
        unknownMiniport->Release();
        IoDetachDevice(lowerDeviceObject);
        IoDeleteDevice(functionalDeviceObject);
        return status;
    }
    
    functionalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    
    KdPrint(("CavernAudioDriver: AddDevice completed successfully\n"));
    
    miniportWaveRT->Release();
    portWaveRT->Release();
    unknownMiniport->Release();
    
    return STATUS_SUCCESS;
}
