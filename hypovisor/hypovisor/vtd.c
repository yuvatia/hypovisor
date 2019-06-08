#include "acpi.h"
#include <Aux_klib.h> // Include ordering is important
#include "vtd.h"

#define DMAR_SIGNATURE 'RAMD'

// TODO: get back here and finish DMAR

unsigned long long get_vtdbar() {
	unsigned long tableSize = 0;
	NTSTATUS status = 0;

	status = AuxKlibGetSystemFirmwareTable('ACPI', DMAR_SIGNATURE, NULL, 0, &tableSize);

	AcpiTableDmar* table = (AcpiTableDmar*)ExAllocatePoolWithTag(NonPagedPool, tableSize, DMAR_SIGNATURE);

	status = AuxKlibGetSystemFirmwareTable('ACPI', DMAR_SIGNATURE, table, tableSize, &tableSize);
	// see QEMUs build_dmar_q35 in hw/i386/acpi-build.c
	AcpiDmarHardwareUnit* drhd = (AcpiDmarHardwareUnit*)((char*)table + sizeof(AcpiTableDmar));

	unsigned long long vtdbar = drhd->address;

	ExFreePoolWithTag(table, DMAR_SIGNATURE);

	return vtdbar;
}

int is_legacy_translation() {
	return 0;
}

int is_queue_invalidation_supported() {
	return 0;
}