#pragma once
#include "common.h"
#include "acpi.h"
#include "intel-iommu.h"

char* g_vtdbar = 0;

DECLSPEC_ALIGN(PAGE_SIZE) root_entry g_root_entry[0x100];
DECLSPEC_ALIGN(PAGE_SIZE) context_entry g_context_entry[0x100];