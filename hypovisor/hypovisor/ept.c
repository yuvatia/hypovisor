// Note: this is ripped-off of Alex Ioenscu's Simplevisor, speficially shvvmx.c

#include "common.h"
#include "ept.h"
#include "memory.h"

VOID vmx_mtrr_initialize(void* state) {
	UINT32 i;
	MTRR_CAPABILITIES mtrrCapabilities;
	MTRR_VARIABLE_BASE mtrrBase;
	MTRR_VARIABLE_MASK mtrrMask;
	unsigned long bit;
	
	PVirtualMachineState vm_state = (PVirtualMachineState)state;

	//
	// Read the capabilities mask
	//
	mtrrCapabilities.AsUlonglong = __readmsr(MTRR_MSR_CAPABILITIES);

	//
	// Iterate over each variable MTRR
	//
	for (i = 0; i < mtrrCapabilities.VarCnt; i++)
	{
		//
		// Capture the value
		//
		mtrrBase.AsUlonglong = __readmsr(MTRR_MSR_VARIABLE_BASE + i * 2);
		mtrrMask.AsUlonglong = __readmsr(MTRR_MSR_VARIABLE_MASK + i * 2);

		//
		// Check if the MTRR is enabled
		//
		vm_state->MtrrData[i].Type = (UINT32)mtrrBase.Type;
		vm_state->MtrrData[i].Enabled = (UINT32)mtrrMask.Enabled;
		if (vm_state->MtrrData[i].Enabled != FALSE)
		{
			//
			// Set the base
			//
			vm_state->MtrrData[i].PhysicalAddressMin = mtrrBase.PhysBase *
				MTRR_PAGE_SIZE;

			//
			// Compute the length
			//
			_BitScanForward64(&bit, mtrrMask.PhysMask * MTRR_PAGE_SIZE);
			vm_state->MtrrData[i].PhysicalAddressMax = vm_state->MtrrData[i].
				PhysicalAddressMin +
				(1ULL << bit) - 1;
		}
	}
}

UINT32 vmx_mtrr_adjust_effective_memory_type(void* state, _In_ UINT64 LargePageAddress, _In_ UINT32 CandidateMemoryType) {
	UINT32 i;

	PVirtualMachineState vm_state = (PVirtualMachineState)state;

	//
	// Loop each MTRR range
	//
	for (i = 0; i < sizeof(vm_state->MtrrData) / sizeof(vm_state->MtrrData[0]); i++)
	{
		//
		// Check if it's active
		//
		if (vm_state->MtrrData[i].Enabled != FALSE)
		{
			//
			// Check if this large page falls within the boundary. If a single
			// physical page (4KB) touches it, we need to override the entire 2MB.
			//
			if (((LargePageAddress + (_2MB - 1)) >= vm_state->MtrrData[i].PhysicalAddressMin) &&
				(LargePageAddress <= vm_state->MtrrData[i].PhysicalAddressMax))
			{
				//
				// Override candidate type with MTRR type
				//
				CandidateMemoryType = vm_state->MtrrData[i].Type;
			}
		}
	}

	//
	// Return the correct type needed
	//
	return CandidateMemoryType;
}

VOID vmx_ept_initialize(void* state) {
	UINT32 i, j;
	VMX_PDPTE tempEpdpte;
	VMX_LARGE_PDE tempEpde;

	PVirtualMachineState vm_state = (PVirtualMachineState)state;

	//
	// Fill out the EPML4E which covers the first 512GB of RAM
	//
	vm_state->Epml4[0].Read = 1;
	vm_state->Epml4[0].Write = 1;
	vm_state->Epml4[0].Execute = 1;
	vm_state->Epml4[0].PageFrameNumber = virtual_to_physical(&vm_state->Epdpt) / PAGE_SIZE;

	//
	// Fill out a RWX PDPTE
	//
	tempEpdpte.AsUlonglong = 0;
	tempEpdpte.Read = tempEpdpte.Write = tempEpdpte.Execute = 1;

	//
	// Construct EPT identity map for every 1GB of RAM
	//
	__stosq((UINT64*)vm_state->Epdpt, tempEpdpte.AsUlonglong, PDPTE_ENTRY_COUNT);
	for (i = 0; i < PDPTE_ENTRY_COUNT; i++)
	{
		//
		// Set the page frame number of the PDE table
		//
		vm_state->Epdpt[i].PageFrameNumber = virtual_to_physical(&vm_state->Epde[i][0]) / PAGE_SIZE;
	}

	//
	// Fill out a RWX Large PDE
	//
	tempEpde.AsUlonglong = 0;
	tempEpde.Read = tempEpde.Write = tempEpde.Execute = 1;
	tempEpde.Large = 1;

	//
	// Loop every 1GB of RAM (described by the PDPTE)
	//
	__stosq((UINT64*)vm_state->Epde, tempEpde.AsUlonglong, PDPTE_ENTRY_COUNT * PDE_ENTRY_COUNT);
	for (i = 0; i < PDPTE_ENTRY_COUNT; i++)
	{
		//
		// Construct EPT identity map for every 2MB of RAM
		//
		for (j = 0; j < PDE_ENTRY_COUNT; j++)
		{
			vm_state->Epde[i][j].PageFrameNumber = (i * 512) + j;
			vm_state->Epde[i][j].Type = vmx_mtrr_adjust_effective_memory_type(vm_state,
				vm_state->Epde[i][j].PageFrameNumber * _2MB,
				MTRR_TYPE_WB);
		}
	}
}