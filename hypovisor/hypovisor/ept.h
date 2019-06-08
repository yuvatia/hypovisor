#pragma once
#include <wdm.h>

#pragma warning(disable:4201)
#pragma warning(disable:4214)

#define MAX_NUM_OF_PAGES    0x20000
#define EPTE_READ       0x1
#define EPTE_READEXEC   0x5
#define EPTE_WRITE      0x2
#define EPTE_EXECUTE    0x4
#define EPTE_ATTR_MASK  0xFFF
#define EPTE_MT_SHIFT   3
#define EPT_LEVELS      4


#define CACHE_TYPE_UC		0x00 /* Uncacheable */
#define CACHE_TYPE_WC		0x01 /* Write-Combining */
#define CACHE_TYPE_WT		0x04 /* Writethrough */
#define CACHE_TYPE_WP		0x05 /* Write-Protect */
#define CACHE_TYPE_WB		0x06 /* Writeback */
#define CACHE_TYPE_UC_MINUS	0x07 /* UC minus */
#define GMTRR_VCNT		MTRR_VCNT_MAX

#define MAX_NUM_OF_PAGES    0x20000
#define EPTE_READ       0x1
#define EPTE_READEXEC   0x5
#define EPTE_WRITE      0x2
#define EPTE_EXECUTE    0x4
#define EPTE_ATTR_MASK  0xFFF
#define EPTE_MT_SHIFT   3
#define EPT_LEVELS      4

typedef struct _VMX_EPTP
{
	union
	{
		struct
		{
			UINT64 Type : 3;
			UINT64 PageWalkLength : 3;
			UINT64 EnableAccessAndDirtyFlags : 1;
			UINT64 Reserved : 5;
			UINT64 PageFrameNumber : 36;
			UINT64 ReservedHigh : 16;
		};
		UINT64 AsUlonglong;
	};
} VMX_EPTP, EPTP, *PVMX_EPTP, *PEPTP;

typedef struct _VMX_EPML4E
{
	union
	{
		struct
		{
			UINT64 Read : 1;
			UINT64 Write : 1;
			UINT64 Execute : 1;
			UINT64 Reserved : 5;
			UINT64 Accessed : 1;
			UINT64 SoftwareUse : 1;
			UINT64 UserModeExecute : 1;
			UINT64 SoftwareUse2 : 1;
			UINT64 PageFrameNumber : 36;
			UINT64 ReservedHigh : 4;
			UINT64 SoftwareUseHigh : 12;
		};
		UINT64 AsUlonglong;
	};
} VMX_EPML4E, *PVMX_EPML4E;

typedef struct _VMX_HUGE_PDPTE
{
	union
	{
		struct
		{
			UINT64 Read : 1;
			UINT64 Write : 1;
			UINT64 Execute : 1;
			UINT64 Type : 3;
			UINT64 IgnorePat : 1;
			UINT64 Large : 1;
			UINT64 Accessed : 1;
			UINT64 Dirty : 1;
			UINT64 UserModeExecute : 1;
			UINT64 SoftwareUse : 1;
			UINT64 Reserved : 18;
			UINT64 PageFrameNumber : 18;
			UINT64 ReservedHigh : 4;
			UINT64 SoftwareUseHigh : 11;
			UINT64 SupressVme : 1;
		};
		UINT64 AsUlonglong;
	};
} VMX_HUGE_PDPTE, *PVMX_HUGE_PDPTE;

typedef struct _VMX_PDPTE
{
	union
	{
		struct
		{
			UINT64 Read : 1;
			UINT64 Write : 1;
			UINT64 Execute : 1;
			UINT64 Reserved : 5;
			UINT64 Accessed : 1;
			UINT64 SoftwareUse : 1;
			UINT64 UserModeExecute : 1;
			UINT64 SoftwareUse2 : 1;
			UINT64 PageFrameNumber : 36;
			UINT64 ReservedHigh : 4;
			UINT64 SoftwareUseHigh : 12;
		};
		UINT64 AsUlonglong;
	};
} VMX_PDPTE, *PVMX_PDPTE;

typedef struct _VMX_LARGE_PDE
{
	union
	{
		struct
		{
			UINT64 Read : 1;
			UINT64 Write : 1;
			UINT64 Execute : 1;
			UINT64 Type : 3;
			UINT64 IgnorePat : 1;
			UINT64 Large : 1;
			UINT64 Accessed : 1;
			UINT64 Dirty : 1;
			UINT64 UserModeExecute : 1;
			UINT64 SoftwareUse : 1;
			UINT64 Reserved : 9;
			UINT64 PageFrameNumber : 27;
			UINT64 ReservedHigh : 4;
			UINT64 SoftwareUseHigh : 11;
			UINT64 SupressVme : 1;
		};
		UINT64 AsUlonglong;
	};
} VMX_LARGE_PDE, *PVMX_LARGE_PDE;

static_assert(sizeof(VMX_EPTP) == sizeof(UINT64), "EPTP Size Mismatch");
static_assert(sizeof(VMX_EPML4E) == sizeof(UINT64), "EPML4E Size Mismatch");
static_assert(sizeof(VMX_PDPTE) == sizeof(UINT64), "EPDPTE Size Mismatch");


enum invept_t
{
	single_context = 0x00000001,
	all_contexts = 0x00000002,
};


typedef struct INVEPT_DESC
{
	EPTP ept_pointer;
	UINT64  reserved;
}INVEPT_DESC, *PINVEPT_DESC;

#define PML4E_ENTRY_COUNT   512
#define PDPTE_ENTRY_COUNT   512
#define PDE_ENTRY_COUNT     512

typedef struct _SHV_MTRR_RANGE
{
	UINT32 Enabled;
	UINT32 Type;
	UINT64 PhysicalAddressMin;
	UINT64 PhysicalAddressMax;
} SHV_MTRR_RANGE, *PSHV_MTRR_RANGE;

typedef struct _ept_context
{
	SHV_MTRR_RANGE MtrrData[16];
	DECLSPEC_ALIGN(PAGE_SIZE) VMX_EPML4E Epml4[PML4E_ENTRY_COUNT];
	DECLSPEC_ALIGN(PAGE_SIZE) VMX_PDPTE Epdpt[PDPTE_ENTRY_COUNT];
	DECLSPEC_ALIGN(PAGE_SIZE) VMX_LARGE_PDE Epde[PDPTE_ENTRY_COUNT][PDE_ENTRY_COUNT];
} ept_context, *pept_context;

pept_context g_ept_ctx;

// Note: state is actually of type pept_context, but I don't really feel like solving that #include hell just yet.
VOID vmx_ept_initialize(void* state);
UINT32 vmx_mtrr_adjust_effective_memory_type(void* state, _In_ UINT64 LargePageAddress, _In_ UINT32 CandidateMemoryType);
VOID vmx_mtrr_initialize(void* state);
