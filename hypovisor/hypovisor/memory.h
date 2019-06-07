#pragma once

#define MAXIMUM_ADDRESS	0xffffffffffffffff
#define REGION_SIZE PAGE_SIZE

UINT64 virtual_to_physical(void* va);
UINT64 physical_to_virtual(UINT64 pa);
UINT64 map_physical_memory(UINT64 pa, UINT64 size);
BOOLEAN allocate_region(IN PVirtualMachineState vmState, REGION_TYPE type);

#pragma alloc_text(PAGE, virtual_to_physical)
#pragma alloc_text(PAGE, physical_to_virtual)
#pragma alloc_text(PAGE, allocate_region)
