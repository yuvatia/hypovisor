// Note: this is ripped-off of Alex Ioenscu's Simplevisor, speficially shvvmx.c

#include "common.h"
#include "vmx.h"
#include "ept.h"
#include "memory.h"

PEPTP create_ept_mapping() {
	PAGED_CODE();

	// Allocate EPTP
	PEPTP EPTPointer = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, POOLTAG);

	if (!EPTPointer) {
		return NULL;
	}
	RtlZeroMemory(EPTPointer, PAGE_SIZE);

	//	Allocate EPT PML4
	PEPT_PML4E EPT_PML4 = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, POOLTAG);
	if (!EPT_PML4) {
		ExFreePoolWithTag(EPTPointer, POOLTAG);
		return NULL;
	}
	RtlZeroMemory(EPT_PML4, PAGE_SIZE);

	//	Allocate EPT Page-Directory-Pointer-Table
	PEPT_PDPTE EPT_PDPT = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, POOLTAG);
	if (!EPT_PDPT) {
		ExFreePoolWithTag(EPT_PML4, POOLTAG);
		ExFreePoolWithTag(EPTPointer, POOLTAG);
		return NULL;
	}
	RtlZeroMemory(EPT_PDPT, PAGE_SIZE);

	//	Allocate EPT Page-Directory
	PEPT_PDE EPT_PD = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, POOLTAG);

	if (!EPT_PD) {
		ExFreePoolWithTag(EPT_PDPT, POOLTAG);
		ExFreePoolWithTag(EPT_PML4, POOLTAG);
		ExFreePoolWithTag(EPTPointer, POOLTAG);
		return NULL;
	}
	RtlZeroMemory(EPT_PD, PAGE_SIZE);

	//	Allocate EPT Page-Table
	PEPT_PTE EPT_PT = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, POOLTAG);

	if (!EPT_PT) {
		ExFreePoolWithTag(EPT_PD, POOLTAG);
		ExFreePoolWithTag(EPT_PDPT, POOLTAG);
		ExFreePoolWithTag(EPT_PML4, POOLTAG);
		ExFreePoolWithTag(EPTPointer, POOLTAG);
		return NULL;
	}
	RtlZeroMemory(EPT_PT, PAGE_SIZE);

	// Setup PT by allocating two pages Continuously
	// We allocate two pages because we need 1 page for our RIP to start and 1 page for RSP 1 + 1 = 2

	const int PagesToAllocate = 100;

	UINT64 guest_memory = (UINT64)ExAllocatePoolWithTag(NonPagedPool, PagesToAllocate * PAGE_SIZE, POOLTAG);

	//VirtualGuestMemoryAddress = Guest_Memory;

	RtlZeroMemory((void*)guest_memory, PagesToAllocate * PAGE_SIZE);

	for (size_t i = 0; i < PagesToAllocate; i++)
	{
		EPT_PT[i].Fields.AccessedFlag = 0;
		EPT_PT[i].Fields.DirtyFlag = 0;
		EPT_PT[i].Fields.EPTMemoryType = 6;
		EPT_PT[i].Fields.Execute = 1;
		EPT_PT[i].Fields.ExecuteForUserMode = 0;
		EPT_PT[i].Fields.IgnorePAT = 0;
		EPT_PT[i].Fields.PhysicalAddress = (virtual_to_physical((void*)(guest_memory + (i * PAGE_SIZE))) / PAGE_SIZE);
		EPT_PT[i].Fields.Read = 1;
		EPT_PT[i].Fields.SuppressVE = 0;
		EPT_PT[i].Fields.Write = 1;

	}

	// Setting up PDE
	EPT_PD->Fields.Accessed = 0;
	EPT_PD->Fields.Execute = 1;
	EPT_PD->Fields.ExecuteForUserMode = 0;
	EPT_PD->Fields.Ignored1 = 0;
	EPT_PD->Fields.Ignored2 = 0;
	EPT_PD->Fields.Ignored3 = 0;
	EPT_PD->Fields.PhysicalAddress = (virtual_to_physical(EPT_PT) / PAGE_SIZE);
	EPT_PD->Fields.Read = 1;
	EPT_PD->Fields.Reserved1 = 0;
	EPT_PD->Fields.Reserved2 = 0;
	EPT_PD->Fields.Write = 1;

	// Setting up PDPTE
	EPT_PDPT->Fields.Accessed = 0;
	EPT_PDPT->Fields.Execute = 1;
	EPT_PDPT->Fields.ExecuteForUserMode = 0;
	EPT_PDPT->Fields.Ignored1 = 0;
	EPT_PDPT->Fields.Ignored2 = 0;
	EPT_PDPT->Fields.Ignored3 = 0;
	EPT_PDPT->Fields.PhysicalAddress = (virtual_to_physical(EPT_PD) / PAGE_SIZE);
	EPT_PDPT->Fields.Read = 1;
	EPT_PDPT->Fields.Reserved1 = 0;
	EPT_PDPT->Fields.Reserved2 = 0;
	EPT_PDPT->Fields.Write = 1;

	// Setting up PML4E
	EPT_PML4->Fields.Accessed = 0;
	EPT_PML4->Fields.Execute = 1;
	EPT_PML4->Fields.ExecuteForUserMode = 0;
	EPT_PML4->Fields.Ignored1 = 0;
	EPT_PML4->Fields.Ignored2 = 0;
	EPT_PML4->Fields.Ignored3 = 0;
	EPT_PML4->Fields.PhysicalAddress = (virtual_to_physical(EPT_PDPT) / PAGE_SIZE);
	EPT_PML4->Fields.Read = 1;
	EPT_PML4->Fields.Reserved1 = 0;
	EPT_PML4->Fields.Reserved2 = 0;
	EPT_PML4->Fields.Write = 1;

	// Setting up EPTP

	EPTPointer->Fields.DirtyAndAceessEnabled = 1;
	EPTPointer->Fields.MemoryType = 6; // 6 = Write-back (WB)
	EPTPointer->Fields.PageWalkLength = 3;  // 4 (tables walked) - 1 = 3 
	EPTPointer->Fields.PML4Address = (virtual_to_physical(EPT_PML4) / PAGE_SIZE);
	EPTPointer->Fields.Reserved1 = 0;
	EPTPointer->Fields.Reserved2 = 0;

	DbgPrint("[*] Extended Page Table Pointer allocated at %llx", EPTPointer);

	return EPTPointer;
}


VOID
ShvVmxMtrrInitialize(
	_In_ PSHV_VP_DATA VpData
)
{
	UINT32 i;
	MTRR_CAPABILITIES mtrrCapabilities;
	MTRR_VARIABLE_BASE mtrrBase;
	MTRR_VARIABLE_MASK mtrrMask;
	unsigned long bit;

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
		VpData->MtrrData[i].Type = (UINT32)mtrrBase.Type;
		VpData->MtrrData[i].Enabled = (UINT32)mtrrMask.Enabled;
		if (VpData->MtrrData[i].Enabled != FALSE)
		{
			//
			// Set the base
			//
			VpData->MtrrData[i].PhysicalAddressMin = mtrrBase.PhysBase *
				MTRR_PAGE_SIZE;

			//
			// Compute the length
			//
			_BitScanForward64(&bit, mtrrMask.PhysMask * MTRR_PAGE_SIZE);
			VpData->MtrrData[i].PhysicalAddressMax = VpData->MtrrData[i].
				PhysicalAddressMin +
				(1ULL << bit) - 1;
		}
	}
}

UINT32
ShvVmxMtrrAdjustEffectiveMemoryType(
	_In_ PSHV_VP_DATA VpData,
	_In_ UINT64 LargePageAddress,
	_In_ UINT32 CandidateMemoryType
)
{
	UINT32 i;

	//
	// Loop each MTRR range
	//
	for (i = 0; i < sizeof(VpData->MtrrData) / sizeof(VpData->MtrrData[0]); i++)
	{
		//
		// Check if it's active
		//
		if (VpData->MtrrData[i].Enabled != FALSE)
		{
			//
			// Check if this large page falls within the boundary. If a single
			// physical page (4KB) touches it, we need to override the entire 2MB.
			//
			if (((LargePageAddress + (_2MB - 1)) >= VpData->MtrrData[i].PhysicalAddressMin) &&
				(LargePageAddress <= VpData->MtrrData[i].PhysicalAddressMax))
			{
				//
				// Override candidate type with MTRR type
				//
				CandidateMemoryType = VpData->MtrrData[i].Type;
			}
		}
	}

	//
	// Return the correct type needed
	//
	return CandidateMemoryType;
}

VOID
ShvVmxEptInitialize(
	_In_ PSHV_VP_DATA VpData
)
{
	UINT32 i, j;
	VMX_PDPTE tempEpdpte;
	VMX_LARGE_PDE tempEpde;

	//
	// Fill out the EPML4E which covers the first 512GB of RAM
	//
	VpData->Epml4[0].Read = 1;
	VpData->Epml4[0].Write = 1;
	VpData->Epml4[0].Execute = 1;
	VpData->Epml4[0].PageFrameNumber = ShvOsGetPhysicalAddress(&VpData->Epdpt) / PAGE_SIZE;

	//
	// Fill out a RWX PDPTE
	//
	tempEpdpte.AsUlonglong = 0;
	tempEpdpte.Read = tempEpdpte.Write = tempEpdpte.Execute = 1;

	//
	// Construct EPT identity map for every 1GB of RAM
	//
	__stosq((UINT64*)VpData->Epdpt, tempEpdpte.AsUlonglong, PDPTE_ENTRY_COUNT);
	for (i = 0; i < PDPTE_ENTRY_COUNT; i++)
	{
		//
		// Set the page frame number of the PDE table
		//
		VpData->Epdpt[i].PageFrameNumber = ShvOsGetPhysicalAddress(&VpData->Epde[i][0]) / PAGE_SIZE;
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
	__stosq((UINT64*)VpData->Epde, tempEpde.AsUlonglong, PDPTE_ENTRY_COUNT * PDE_ENTRY_COUNT);
	for (i = 0; i < PDPTE_ENTRY_COUNT; i++)
	{
		//
		// Construct EPT identity map for every 2MB of RAM
		//
		for (j = 0; j < PDE_ENTRY_COUNT; j++)
		{
			VpData->Epde[i][j].PageFrameNumber = (i * 512) + j;
			VpData->Epde[i][j].Type = ShvVmxMtrrAdjustEffectiveMemoryType(VpData,
				VpData->Epde[i][j].PageFrameNumber * _2MB,
				MTRR_TYPE_WB);
		}
	}
}