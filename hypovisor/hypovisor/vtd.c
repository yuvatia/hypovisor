#include "acpi.h"
#include <Aux_klib.h> // Include ordering is important
#include "vtd.h"
#include "intel-iommu.h"
#include "memory.h"

#define DMAR_SIGNATURE 'RAMD'

unsigned long long get_vtdbar() {
	unsigned long tableSize = 0;
	NTSTATUS status = 0;

	__debugbreak();
	status = AuxKlibGetSystemFirmwareTable('ACPI', DMAR_SIGNATURE, NULL, 0, &tableSize);

	AcpiTableDmar* table = (AcpiTableDmar*)ExAllocatePoolWithTag(NonPagedPool, tableSize, DMAR_SIGNATURE);

	status = AuxKlibGetSystemFirmwareTable('ACPI', DMAR_SIGNATURE, table, tableSize, &tableSize);
	// see QEMUs build_dmar_q35 in hw/i386/acpi-build.c
	AcpiDmarHardwareUnit* drhd = (AcpiDmarHardwareUnit*)((char*)table + sizeof(AcpiTableDmar));

	unsigned long long vtdbar = drhd->address;
	DbgPrint("[*] Found VTDBAR at %llx.\n", vtdbar);

	ExFreePoolWithTag(table, DMAR_SIGNATURE);

	__debugbreak();
	g_vtdbar = (char*)map_physical_memory(vtdbar, 0x1000);
	return vtdbar;
}

void enable_translation() {
	// Reset one-shot bytes
	*(UINT32*)(g_vtdbar + DMAR_GSTS_REG) &= 0x96FFFFFF;
	// Set command
	*(UINT32*)(g_vtdbar + DMAR_GCMD_REG) = (*(UINT32*)(g_vtdbar + DMAR_GSTS_REG)) | DMA_GCMD_TE;
	// Wait until hardware update status, notifying us the command had been processed successfully.
	while (((*(UINT32*)(g_vtdbar + DMAR_GSTS_REG)) & DMA_GSTS_TES) != DMA_GSTS_TES) {
		;
	}

	DbgPrint("[*] Enabled DMAR.\n");
}

void set_root_table(void* ept_pml4_va) {
	UINT64 pml4_pa = virtual_to_physical(ept_pml4_va);

	// Use Legacy-Mode translation.
	UINT64 root_table = 0;
	root_table = (pml4_pa >> 12) << 12;
	*(UINT64*)(g_vtdbar + DMAR_RTADDR_REG) = root_table;

	// Reset one-shot bytes
	*(UINT32*)(g_vtdbar + DMAR_GSTS_REG) &= 0x96FFFFFF;
	// Set command
	*(UINT32*)(g_vtdbar + DMAR_GCMD_REG) = (*(UINT32*)(g_vtdbar + DMAR_GSTS_REG)) | DMA_GCMD_SRTP;
	// Wait until hardware update status, notifying us the command had been processed successfully.
	while (((*(UINT32*)(g_vtdbar + DMAR_GSTS_REG)) & DMA_GSTS_RTPS) != DMA_GSTS_RTPS ) {
		;
	}

	DbgPrint("[*] Set RTADDR_REG.\n");
}

void invalidate_cache() {
	DbgPrint("[*] Unimplemented Ideally, this would be done using invalidation queues, and if these are unavailable, then using invalidation registers.\n");
}
