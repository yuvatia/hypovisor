#include <ntddk.h>
#include <wdm.h>

#include "common.h"
#include "msr.h"
#include "vmx.h"
#include "memory.h"


UINT64 virtual_to_physical(void* va)
{
	return MmGetPhysicalAddress(va).QuadPart;
}

UINT64 physical_to_virtual(UINT64 pa)
{
	PHYSICAL_ADDRESS phys_addr;
	phys_addr.QuadPart = pa;

	return(UINT64)MmGetVirtualForPhysical(phys_addr);
}

UINT64 map_physical_memory(UINT64 pa, UINT64 size)
{
	PHYSICAL_ADDRESS phys_addr;
	phys_addr.QuadPart = pa;
	return (UINT64)MmMapIoSpace(phys_addr, size, MmNonCached);
}

BOOLEAN allocate_region(IN PVirtualMachineState vm_state, REGION_TYPE type)
{	
	// at IRQL > DISPATCH_LEVEL memory allocation routines don't work
	if (KeGetCurrentIrql() > DISPATCH_LEVEL)
		KeRaiseIrqlToDpcLevel();

	int is_vmxon = FALSE;
	char* type_name = NULL;
	if (type == VMCS) {
		type_name = "VMCS";
	} else if (type == VMXON) {
		type_name = "VMXON";
		is_vmxon = TRUE;
	} else {
		DbgPrint("[*] Error : Invalid region type.\n");
		return FALSE;
	}

	PHYSICAL_ADDRESS max_pa = { 0 };
	max_pa.QuadPart = MAXULONG64;


	int region_size = 2 * REGION_SIZE;
	unsigned char* buff = MmAllocateContiguousMemory(region_size + PAGE_SIZE, max_pa);  // Allocating a 4-KByte Contigous Memory region


	if (buff == NULL) {
		DbgPrint("[*] Error : Couldn't Allocate Buffer for %s Region.\n", type_name);
		return FALSE;// ntStatus = STATUS_INSUFFICIENT_RESOURCES;
	}
	UINT64 buff_pa = virtual_to_physical(buff);

	// zero-out memory 
	RtlSecureZeroMemory(buff, region_size + PAGE_SIZE);
	UINT64 buff_pa_aligned = (UINT64)((ULONG_PTR)(buff_pa + PAGE_SIZE - 1) &~(PAGE_SIZE - 1));
	UINT64 buff_va_aligned = (UINT64)((ULONG_PTR)(buff + PAGE_SIZE - 1) &~(PAGE_SIZE - 1));

	DbgPrint("[*] Virtual allocated buffer for %s at %llx\n", type_name, buff);
	DbgPrint("[*] Virtual aligned allocated buffer for %s at %llx\n", type_name, buff_va_aligned);
	DbgPrint("[*] Aligned physical buffer allocated for %s at %llx\n", type_name, buff_pa_aligned);

	// get IA32_VMX_BASIC_MSR RevisionId

	IA32_VMX_BASIC_MSR basic = { 0 };


	basic.All = __readmsr(MSR_IA32_VMX_BASIC);

	DbgPrint("[*] MSR_IA32_VMX_BASIC (MSR 0x480) Revision Identifier %llx\n", basic.Fields.RevisionIdentifier);

	//Changing Revision Identifier, per specs.
	*(UINT64 *)buff_va_aligned = basic.Fields.RevisionIdentifier;

	if (is_vmxon) {
		int status = __vmx_on(&buff_pa_aligned);
		if (status)
		{
			DbgPrint("[*] %s failed with status %d\n", type_name, status);
			return FALSE;
		}

	}

	if (is_vmxon) {
		vm_state->VMXON_REGION = buff_pa_aligned;
	}
	else {
		vm_state->VMCS_REGION = buff_pa_aligned;
	}

	return TRUE;
}
