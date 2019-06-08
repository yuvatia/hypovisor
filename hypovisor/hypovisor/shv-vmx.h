/*++

Copyright (c) Alex Ionescu.  All rights reserved.

Header Name:

vmx.h

Abstract:

This header defines the MSRs and VMCS fields for Intel x64 VT-x support.

Author:

Alex Ionescu (@aionescu) 16-Mar-2016 - Initial version

Environment:

Kernel mode only.

--*/

#pragma once
#include "common.h"

#pragma warning(disable:4201)
#pragma warning(disable:4214)

#define _1GB                        (1 * 1024 * 1024 * 1024)
#define _2MB                        (2 * 1024 * 1024)

#define DPL_USER                3
#define DPL_SYSTEM              0
#define MSR_GS_BASE             0xC0000101
#define MSR_DEBUG_CTL           0x1D9
#define RPL_MASK                3
#define MTRR_TYPE_UC            0
#define MTRR_TYPE_USWC          1
#define MTRR_TYPE_WT            4
#define MTRR_TYPE_WP            5
#define MTRR_TYPE_WB            6
#define MTRR_TYPE_MAX           7
#define SELECTOR_TABLE_INDEX    0x04
#define EFLAGS_ALIGN_CHECK      0x40000
#define AMD64_TSS               9
#ifndef PAGE_SIZE
#define PAGE_SIZE               4096
#endif
#define MTRR_MSR_CAPABILITIES   0x0fe
#define MTRR_MSR_DEFAULT        0x2ff
#define MTRR_MSR_VARIABLE_BASE  0x200
#define MTRR_MSR_VARIABLE_MASK  (MTRR_MSR_VARIABLE_BASE+1)
#define MTRR_PAGE_SIZE          4096
#define MTRR_PAGE_MASK          (~(MTRR_PAGE_SIZE-1))

typedef struct _KDESCRIPTOR
{
	UINT16 Pad[3];
	UINT16 Limit;
	void* Base;
} KDESCRIPTOR, *PKDESCRIPTOR;

typedef union _KGDTENTRY64
{
	struct
	{
		UINT16 LimitLow;
		UINT16 BaseLow;
		union
		{
			struct
			{
				UINT8 BaseMiddle;
				UINT8 Flags1;
				UINT8 Flags2;
				UINT8 BaseHigh;
			} Bytes;
			struct
			{
				UINT32 BaseMiddle : 8;
				UINT32 Type : 5;
				UINT32 Dpl : 2;
				UINT32 Present : 1;
				UINT32 LimitHigh : 4;
				UINT32 System : 1;
				UINT32 LongMode : 1;
				UINT32 DefaultBig : 1;
				UINT32 Granularity : 1;
				UINT32 BaseHigh : 8;
			} Bits;
		};
		UINT32 BaseUpper;
		UINT32 MustBeZero;
	};
	struct
	{
		INT64 DataLow;
		INT64 DataHigh;
	};
} KGDTENTRY64, *PKGDTENTRY64;

#pragma pack(push,4)
typedef struct _KTSS64
{
	UINT32 Reserved0;
	UINT64 Rsp0;
	UINT64 Rsp1;
	UINT64 Rsp2;
	UINT64 Ist[8];
	UINT64 Reserved1;
	UINT16 Reserved2;
	UINT16 IoMapBase;
} KTSS64, *PKTSS64;
#pragma pack(pop)

typedef struct _MTRR_CAPABILITIES
{
	union
	{
		struct
		{
			UINT64 VarCnt : 8;
			UINT64 FixedSupported : 1;
			UINT64 Reserved : 1;
			UINT64 WcSupported : 1;
			UINT64 SmrrSupported : 1;
			UINT64 Reserved_2 : 52;
		};
		UINT64 AsUlonglong;
	};
} MTRR_CAPABILITIES, *PMTRR_CAPABILITIES;
C_ASSERT(sizeof(MTRR_CAPABILITIES) == sizeof(UINT64));

typedef struct _MTRR_VARIABLE_BASE
{
	union
	{
		struct
		{
			UINT64 Type : 8;
			UINT64 Reserved : 4;
			UINT64 PhysBase : 36;
			UINT64 Reserved2 : 16;
		};
		UINT64 AsUlonglong;
	};
} MTRR_VARIABLE_BASE, *PMTRR_VARIABLE_BASE;
C_ASSERT(sizeof(MTRR_VARIABLE_BASE) == sizeof(UINT64));

typedef struct _MTRR_VARIABLE_MASK
{
	union
	{
		struct
		{
			UINT64 Reserved : 11;
			UINT64 Enabled : 1;
			UINT64 PhysMask : 36;
			UINT64 Reserved2 : 16;
		};
		UINT64 AsUlonglong;
	};
} MTRR_VARIABLE_MASK, *PMTRR_VARIABLE_MASK;
C_ASSERT(sizeof(MTRR_VARIABLE_MASK) == sizeof(UINT64));

#define CPU_BASED_VIRTUAL_INTR_PENDING          0x00000004
#define CPU_BASED_USE_TSC_OFFSETING             0x00000008
#define CPU_BASED_HLT_EXITING                   0x00000080
#define CPU_BASED_INVLPG_EXITING                0x00000200
#define CPU_BASED_MWAIT_EXITING                 0x00000400
#define CPU_BASED_RDPMC_EXITING                 0x00000800
#define CPU_BASED_RDTSC_EXITING                 0x00001000
#define CPU_BASED_CR3_LOAD_EXITING              0x00008000
#define CPU_BASED_CR3_STORE_EXITING             0x00010000
#define CPU_BASED_CR8_LOAD_EXITING              0x00080000
#define CPU_BASED_CR8_STORE_EXITING             0x00100000
#define CPU_BASED_TPR_SHADOW                    0x00200000
#define CPU_BASED_VIRTUAL_NMI_PENDING           0x00400000
#define CPU_BASED_MOV_DR_EXITING                0x00800000
#define CPU_BASED_UNCOND_IO_EXITING             0x01000000
#define CPU_BASED_ACTIVATE_IO_BITMAP            0x02000000
#define CPU_BASED_MONITOR_TRAP_FLAG             0x08000000
#define CPU_BASED_ACTIVATE_MSR_BITMAP           0x10000000
#define CPU_BASED_MONITOR_EXITING               0x20000000
#define CPU_BASED_PAUSE_EXITING                 0x40000000
#define CPU_BASED_ACTIVATE_SECONDARY_CONTROLS   0x80000000

#define PIN_BASED_EXT_INTR_MASK                 0x00000001
#define PIN_BASED_NMI_EXITING                   0x00000008
#define PIN_BASED_VIRTUAL_NMIS                  0x00000020
#define PIN_BASED_PREEMPT_TIMER                 0x00000040
#define PIN_BASED_POSTED_INTERRUPT              0x00000080

#define VM_EXIT_SAVE_DEBUG_CNTRLS               0x00000004
#define VM_EXIT_IA32E_MODE                      0x00000200
#define VM_EXIT_LOAD_PERF_GLOBAL_CTRL           0x00001000
#define VM_EXIT_ACK_INTR_ON_EXIT                0x00008000
#define VM_EXIT_SAVE_GUEST_PAT                  0x00040000
#define VM_EXIT_LOAD_HOST_PAT                   0x00080000
#define VM_EXIT_SAVE_GUEST_EFER                 0x00100000
#define VM_EXIT_LOAD_HOST_EFER                  0x00200000
#define VM_EXIT_SAVE_PREEMPT_TIMER              0x00400000
#define VM_EXIT_CLEAR_BNDCFGS                   0x00800000

#define VM_ENTRY_IA32E_MODE                     0x00000200
#define VM_ENTRY_SMM                            0x00000400
#define VM_ENTRY_DEACT_DUAL_MONITOR             0x00000800
#define VM_ENTRY_LOAD_PERF_GLOBAL_CTRL          0x00002000
#define VM_ENTRY_LOAD_GUEST_PAT                 0x00004000
#define VM_ENTRY_LOAD_GUEST_EFER                0x00008000
#define VM_ENTRY_LOAD_BNDCFGS                   0x00010000

#define SECONDARY_EXEC_VIRTUALIZE_APIC_ACCESSES 0x00000001
#define SECONDARY_EXEC_ENABLE_EPT               0x00000002
#define SECONDARY_EXEC_DESCRIPTOR_TABLE_EXITING 0x00000004
#define SECONDARY_EXEC_ENABLE_RDTSCP            0x00000008
#define SECONDARY_EXEC_VIRTUALIZE_X2APIC_MODE   0x00000010
#define SECONDARY_EXEC_ENABLE_VPID              0x00000020
#define SECONDARY_EXEC_WBINVD_EXITING           0x00000040
#define SECONDARY_EXEC_UNRESTRICTED_GUEST       0x00000080
#define SECONDARY_EXEC_APIC_REGISTER_VIRT       0x00000100
#define SECONDARY_EXEC_VIRTUAL_INTR_DELIVERY    0x00000200
#define SECONDARY_EXEC_PAUSE_LOOP_EXITING       0x00000400
#define SECONDARY_EXEC_ENABLE_INVPCID           0x00001000
#define SECONDARY_EXEC_ENABLE_VM_FUNCTIONS      0x00002000
#define SECONDARY_EXEC_ENABLE_VMCS_SHADOWING    0x00004000
#define SECONDARY_EXEC_ENABLE_PML               0x00020000
#define SECONDARY_EXEC_ENABLE_VIRT_EXCEPTIONS   0x00040000
#define SECONDARY_EXEC_XSAVES                   0x00100000
#define SECONDARY_EXEC_PCOMMIT                  0x00200000
#define SECONDARY_EXEC_TSC_SCALING              0x02000000

#define VMX_BASIC_REVISION_MASK                 0x7fffffff
#define VMX_BASIC_VMCS_SIZE_MASK                (0x1fffULL << 32)
#define VMX_BASIC_32BIT_ADDRESSES               (1ULL << 48)
#define VMX_BASIC_DUAL_MONITOR                  (1ULL << 49)
#define VMX_BASIC_MEMORY_TYPE_MASK              (0xfULL << 50)
#define VMX_BASIC_INS_OUT_INFO                  (1ULL << 54)
#define VMX_BASIC_DEFAULT1_ZERO                 (1ULL << 55)

#define VMX_EPT_EXECUTE_ONLY_BIT                (1ULL)
#define VMX_EPT_PAGE_WALK_4_BIT                 (1ULL << 6)
#define VMX_EPTP_UC_BIT                         (1ULL << 8)
#define VMX_EPTP_WB_BIT                         (1ULL << 14)
#define VMX_EPT_2MB_PAGE_BIT                    (1ULL << 16)
#define VMX_EPT_1GB_PAGE_BIT                    (1ULL << 17)
#define VMX_EPT_INVEPT_BIT                      (1ULL << 20)
#define VMX_EPT_AD_BIT                          (1ULL << 21)
#define VMX_EPT_EXTENT_CONTEXT_BIT              (1ULL << 25)
#define VMX_EPT_EXTENT_GLOBAL_BIT               (1ULL << 26)

#define HYPERV_CPUID_VENDOR_AND_MAX_FUNCTIONS   0x40000000
#define HYPERV_CPUID_INTERFACE                  0x40000001
#define HYPERV_CPUID_VERSION                    0x40000002
#define HYPERV_CPUID_FEATURES                   0x40000003
#define HYPERV_CPUID_ENLIGHTMENT_INFO           0x40000004
#define HYPERV_CPUID_IMPLEMENT_LIMITS           0x40000005

#define HYPERV_HYPERVISOR_PRESENT_BIT           0x80000000
#define HYPERV_CPUID_MIN                        0x40000005
#define HYPERV_CPUID_MAX                        0x4000ffff

#define GUEST_ACTIVITY_ACTIVE           0
#define GUEST_ACTIVITY_HLT              1
