#include <wdm.h>
#include <intrin.h>

#include "common.h"


VOID DrvUnload(PDRIVER_OBJECT  DriverObject);
NTSTATUS DriverEntry(PDRIVER_OBJECT  pDriverObject, PUNICODE_STRING  pRegistryPath);
NTSTATUS DrvUnsupported(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS DrvCreate(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS DrvClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DrvUnload)
#pragma alloc_text(PAGE, DrvCreate)

NTSTATUS DriverEntry(PDRIVER_OBJECT  pDriverObject, PUNICODE_STRING  pRegistryPath)
{
	UNREFERENCED_PARAMETER(pRegistryPath);

	NTSTATUS NtStatus = STATUS_SUCCESS;
	PDEVICE_OBJECT pDeviceObject = NULL;
	UNICODE_STRING usDriverName, usDosDeviceName;

	DbgPrint("[*] DriverEntry Called.\n");

	RtlInitUnicodeString(&usDriverName, L"\\Device\\MyHypervisorDevice");
	RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\MyHypervisorDevice");

	NtStatus = IoCreateDevice(pDriverObject, 0, &usDriverName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDeviceObject);

	if (NtStatus == STATUS_SUCCESS)
	{
		for (UINT64 uiIndex = 0; uiIndex < IRP_MJ_MAXIMUM_FUNCTION; uiIndex++)
			pDriverObject->MajorFunction[uiIndex] = DrvUnsupported;

		DbgPrint("[*] Setting Devices major functions.\n");
		pDriverObject->MajorFunction[IRP_MJ_CLOSE] = DrvClose;
		pDriverObject->MajorFunction[IRP_MJ_CREATE] = DrvCreate;
		pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DrvUnsupported;
		pDriverObject->MajorFunction[IRP_MJ_READ] = DrvUnsupported;
		pDriverObject->MajorFunction[IRP_MJ_WRITE] = DrvUnsupported;
		
		pDriverObject->DriverUnload = DrvUnload;
		pDeviceObject->Flags |= IO_TYPE_DEVICE;
		pDeviceObject->Flags &= (~DO_DEVICE_INITIALIZING);
		IoCreateSymbolicLink(&usDosDeviceName, &usDriverName);
	}
	return NtStatus;
}

NTSTATUS DrvUnsupported(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	DbgPrint("[*] This function is not supported :( !\n");

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DrvClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	DbgPrint("[*] DrvClose called!\n");

	DbgPrint("[*] Attempting to shutdown VT-x on all cores...\n");
	terminate_vmx();
	DbgPrint("[*] Finished terminating hypervisor.\n");

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS DrvCreate(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	DbgPrint("[*] DrvCreate called!\n");

	DbgPrint("[*] Attempting to virtualize cores...\n");
	virtualize_cores();
	DbgPrint("[*] All cores have been successfully virtualized!\n");

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

VOID DrvUnload(PDRIVER_OBJECT  DriverObject)
{
	UNICODE_STRING usDosDeviceName;
	DbgPrint("[*] DrvUnload Called\n");
	RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\MyHypervisorDevice");
	IoDeleteSymbolicLink(&usDosDeviceName);
	IoDeleteDevice(DriverObject->DeviceObject);
}