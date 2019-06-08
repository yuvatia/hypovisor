#pragma once
#include "common.h"
#include "acpi.h"
#include "intel-iommu.h"

extern char* g_vtdbar;

DECLSPEC_ALIGN(PAGE_SIZE) extern root_entry g_root_entry[0x100];
DECLSPEC_ALIGN(PAGE_SIZE) extern context_entry g_context_entry[0x100];

void enable_translation();
void set_root_table(void* ept_pml4_va);
unsigned long long get_vtdbar();