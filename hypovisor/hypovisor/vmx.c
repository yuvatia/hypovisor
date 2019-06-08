#include <ntddk.h>
#include <wdm.h>

#include "helpers.h"
#include "vmx.h"
#include "common.h"
#include "msr.h"
#include "memory.h"
#include "regs.h"

void initialize_vmx();
int is_vmx_supported();
void enable_vmx_in_cr4();
#pragma alloc_text(PAGE, initialize_vmx)
#pragma alloc_text(PAGE, is_vmx_supported)
#pragma alloc_text(PAGE, enable_vmx_in_cr4)

PVirtualMachineState g_vm_state;

BOOLEAN run_on_processor(ULONG num, PEPTP eptp, PFUNC routine)
{
	KIRQL old_irql;

	KeSetSystemAffinityThread((KAFFINITY)(1 << num));

	old_irql = KeRaiseIrqlToDpcLevel();

	routine(num, eptp); // Invoke callback routine

	KeLowerIrql(old_irql);

	KeRevertToUserAffinityThread();

	return TRUE;
}


int virtualize_cores() {
	// TODO: check for ept support
	initialize_vmx();

	ULONG processor_count = KeQueryActiveProcessorCount(0);
	for (ULONG i = 0; i < processor_count; ++i) {
		run_on_processor(i, 0, VMXSaveState); // VmxSaveState will also virtualize the core.
	}

	return STATUS_SUCCESS;
}


void terminate_vmx() {

	DbgPrint("\n[*] Terminating VMX...\n");

	int processor_count = KeQueryActiveProcessorCount(0);

	for (size_t i = 0; i < processor_count; i++)
	{
		DbgPrint("\t\t + Terminating VMX on processor %d\n", i);
		terminate_on_processor((ULONG)i);

		// Free the destination memory
		MmFreeContiguousMemory((void*)physical_to_virtual(g_vm_state[i].VMXON_REGION));
		MmFreeContiguousMemory((void*)physical_to_virtual(g_vm_state[i].VMCS_REGION));
		MmFreeContiguousMemory((void*)g_vm_state[i].VMM_Stack);
		MmFreeContiguousMemory((void*)g_vm_state[i].MSRBitMap);
	}

	DbgPrint("[*] VMX Operation turned off successfully. \n");

}

BOOLEAN terminate_on_processor(ULONG processor_num)
{
	KIRQL old_irql;

	KeSetSystemAffinityThread((KAFFINITY)(1 << processor_num));

	old_irql = KeRaiseIrqlToDpcLevel();

	// Our routine is VMXOFF, however we cannot execute vmxoff directly because we aren't in the root partition.
	INT32 cpu_info[4];
	__cpuidex(cpu_info, 0x41414141, 0x42424242);

	KeLowerIrql(old_irql);

	KeRevertToUserAffinityThread();

	return TRUE;
}

void initialize_vmx() {
	PAGED_CODE();
	
	if (!is_vmx_supported()) {
		DbgPrint("[*] VMX is not supported on this platform.");
		return;
	}

	ULONG processor_count = KeQueryActiveProcessorCount(0);
	g_vm_state = ExAllocatePoolWithTag(NonPagedPool, sizeof(VirtualMachineState)* processor_count, POOLTAG);


	DbgPrint("\n=====================================================\n");

	KAFFINITY affinity_mask;
	for (size_t i = 0; i < processor_count; i++)
	{
		affinity_mask = (ULONG64)1 << i;
		KeSetSystemAffinityThread(affinity_mask);
		DbgPrint("\t\tCurrent thread is executing in %d th logical processor.\n", i);

		enable_vmx_in_cr4();	// Enabling VMX Operation
		DbgPrint("[*] VMX Operation Enabled Successfully !\n");

		allocate_region(&g_vm_state[i], VMXON);
		allocate_region(&g_vm_state[i], VMCS);

		DbgPrint("[*] VMCS Region is allocated at  ===============> %llx\n", g_vm_state[i].VMCS_REGION);
		DbgPrint("[*] VMXON Region is allocated at ===============> %llx\n", g_vm_state[i].VMXON_REGION);

		DbgPrint("\n=====================================================\n");

	}

}

void enable_vmx_in_cr4()
{
	ULONGLONG cr4 = __readcr4();
	cr4 |= 0x02000; // Set 14th bit to enable VMX
	__writecr4(cr4);
}

int is_vmx_supported()
{
	CPUID data = { 0 };

	// VMX bit
	__cpuid((int*)&data, 1);
	if ((data.ecx & (1 << 5)) == 0)
		return FALSE;

	IA32_FEATURE_CONTROL_MSR Control = { 0 };
	Control.All = __readmsr(MSR_IA32_FEATURE_CONTROL);

	// BIOS lock check
	if (Control.Fields.Lock == 0)
	{
		Control.Fields.Lock = TRUE;
		Control.Fields.EnableVmxon = TRUE;
		__writemsr(MSR_IA32_FEATURE_CONTROL, Control.All);
	}
	else if (Control.Fields.EnableVmxon == FALSE)
	{
		DbgPrint("[*] VMX locked off in BIOS");
		return FALSE;
	}

	return TRUE;
}


void VirtualizeCurrentSystem(int processor_id, PEPTP eptp, PVOID guest_stack);
#pragma alloc_text(PAGE, VirtualizeCurrentSystem)

void VirtualizeCurrentSystem(int processor_id, PEPTP eptp, PVOID guest_stack) {
	UNREFERENCED_PARAMETER(eptp);

	DbgPrint("\n======================== Virtualizing Current System =============================\n");

	// Allocate stack for the VM Exit Handler.
	UINT64 vmm_stack_va = (UINT64)ExAllocatePoolWithTag(NonPagedPool, VMM_STACK_SIZE, POOLTAG);
	g_vm_state[processor_id].VMM_Stack = vmm_stack_va;

	if (g_vm_state[processor_id].VMM_Stack == (UINT64)NULL)
	{
		DbgPrint("[*] Error in allocating VMM Stack.\n");
		return;
	}
	RtlZeroMemory((void*)g_vm_state[processor_id].VMM_Stack, VMM_STACK_SIZE);

	// Allocate memory for MSRBitMap
	g_vm_state[processor_id].MSRBitMap = (UINT64)ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, POOLTAG);  // should be aligned
	if (g_vm_state[processor_id].MSRBitMap == (UINT64)NULL)
	{
		DbgPrint("[*] Error in allocating MSRBitMap.\n");
		return;
	}
	RtlZeroMemory((void*)g_vm_state[processor_id].MSRBitMap, PAGE_SIZE);
	g_vm_state[processor_id].MSRBitMapPhysical = virtual_to_physical((void*)g_vm_state[processor_id].MSRBitMap);

	DbgPrint("[*] MSR Bitmap address : %llx\n", g_vm_state[processor_id].MSRBitMap);

	SetMSRBitmap(0xc0000082, processor_id, TRUE, TRUE);


	// Clear the VMCS State
	if (__vmx_vmclear(&g_vm_state[processor_id].VMCS_REGION)) {
		DbgPrint("[*] Fail to clear VMCS!\n");
		return;
	}

	// Load VMCS (Set the Current VMCS)
	if (__vmx_vmptrld(&g_vm_state[processor_id].VMCS_REGION))
	{
		DbgPrint("[*] Fail to load VMCS !\n");
		return;
	}

	DbgPrint("[*] Setting up VMCS for current system.\n");
	setup_vmcs_for_current_guest(&g_vm_state[processor_id], guest_stack);


	// Change this hook (detect modification of MSRs using RDMSR & WRMSR)
	DbgPrint("[*] Setting up MSR bitmaps.\n");

	DbgPrint("[*] Executing VMLAUNCH.\n");
	__vmx_vmlaunch();

	// if VMLAUNCH succeed will never be here !
	ULONG64 ErrorCode = 0;
	__vmx_vmread(VM_INSTRUCTION_ERROR, &ErrorCode);
	__vmx_off();
	DbgPrint("[*] VMLAUNCH Error : 0x%llx\n", ErrorCode);
	DbgBreakPoint();

	DbgPrint("\n===================================================================\n");

	// Return without error
	__vmx_off();
	DbgPrint("[*] VMXOFF Executed Successfully. !\n");
}

BOOLEAN SetMSRBitmap(ULONG64 msr, int processor_id, BOOLEAN read, BOOLEAN write) {

	if (!read && !write)
	{
		// Invalid Command
		return FALSE;
	}


	if (msr <= 0x00001FFF)
	{
		if (read)
		{
			SetBit(g_vm_state[processor_id].MSRBitMap, msr, TRUE);
		}
		if (write)
		{
			SetBit(g_vm_state[processor_id].MSRBitMap + 2048, msr, TRUE);
		}
	}
	else if ((0xC0000000 <= msr) && (msr <= 0xC0001FFF))
	{

		if (read)
		{
			SetBit(g_vm_state[processor_id].MSRBitMap + 1024, msr - 0xC0000000, TRUE);
		}
		if (write)
		{
			SetBit(g_vm_state[processor_id].MSRBitMap + 3072, msr - 0xC0000000, TRUE);

		}
	}
	else
	{
		return FALSE;
	}
	return TRUE;
}


// Index starts from 0 , not 1
BOOLEAN SetTargetControls(UINT64 CR3, UINT64 Index) {
	if (Index >= 4)
	{
		// Not supported for more than 4 , at least for now :(
		return FALSE;
	}

	if (CR3 == 0)
	{
		//if (gCR3_Target_Count <= 0)
		if(0)
		{
			// Invalid command as gCR3_Target_Count cannot be less than zero
			return FALSE;
		}
		else
		{
			//gCR3_Target_Count -= 1;
			if (Index == 0)
			{
				__vmx_vmwrite(CR3_TARGET_VALUE0, 0);
			}
			if (Index == 1)
			{
				__vmx_vmwrite(CR3_TARGET_VALUE1, 0);
			}
			if (Index == 2)
			{
				__vmx_vmwrite(CR3_TARGET_VALUE2, 0);
			}
			if (Index == 3)
			{
				__vmx_vmwrite(CR3_TARGET_VALUE3, 0);
			}
		}
	}
	else
	{
		if (Index == 0)
		{
			__vmx_vmwrite(CR3_TARGET_VALUE0, CR3);
		}
		if (Index == 1)
		{
			__vmx_vmwrite(CR3_TARGET_VALUE1, CR3);
		}
		if (Index == 2)
		{
			__vmx_vmwrite(CR3_TARGET_VALUE2, CR3);
		}
		if (Index == 3)
		{
			__vmx_vmwrite(CR3_TARGET_VALUE3, CR3);
		}
		//gCR3_Target_Count += 1;
	}

	__vmx_vmwrite(CR3_TARGET_COUNT,0/* gCR3_Target_Count*/);
	return TRUE;
}


BOOLEAN setup_vmcs_for_current_guest(IN PVirtualMachineState vm_state, PVOID guest_stack) {
	BOOLEAN Status = FALSE;
	VMX_EPTP vmxEptp = { 0 };

	DbgPrint("[*] Initializing MTRR\n");
	vmx_mtrr_initialize(vm_state);
	DbgPrint("[*] Initializing EPT\n");
	vmx_ept_initialize(vm_state);

	// Load Extended Page Table Pointer
	//
	// Enable EPT features if supported
	//
	if (1) // TODO: check for EPT support and enter only if supported
	{
		//
		// Configure the EPTP
		//
		vmxEptp.AsUlonglong = 0;
		vmxEptp.PageWalkLength = 3;
		vmxEptp.Type = MTRR_TYPE_WB;
		vmxEptp.PageFrameNumber = virtual_to_physical(vm_state->Epml4) / PAGE_SIZE;

		//
		// Load EPT Root Pointer
		//
		__vmx_vmwrite(EPT_POINTER, vmxEptp.AsUlonglong);
	}

	ULONG64 gdt_base = 0;
	SEGMENT_SELECTOR segment_selector = { 0 };

	// 
	__vmx_vmwrite(HOST_ES_SELECTOR, GetEs() & 0xF8);
	__vmx_vmwrite(HOST_CS_SELECTOR, GetCs() & 0xF8);
	__vmx_vmwrite(HOST_SS_SELECTOR, GetSs() & 0xF8);
	__vmx_vmwrite(HOST_DS_SELECTOR, GetDs() & 0xF8);
	__vmx_vmwrite(HOST_FS_SELECTOR, GetFs() & 0xF8);
	__vmx_vmwrite(HOST_GS_SELECTOR, GetGs() & 0xF8);
	__vmx_vmwrite(HOST_TR_SELECTOR, GetTr() & 0xF8);

	// Setting the link pointer to the required value for 4KB VMCS.
	__vmx_vmwrite(VMCS_LINK_POINTER, ~0ULL);


	__vmx_vmwrite(GUEST_IA32_DEBUGCTL, __readmsr(MSR_IA32_DEBUGCTL) & 0xFFFFFFFF);
	__vmx_vmwrite(GUEST_IA32_DEBUGCTL_HIGH, __readmsr(MSR_IA32_DEBUGCTL) >> 32);


	/* Time-stamp counter offset */
	__vmx_vmwrite(TSC_OFFSET, 0);
	__vmx_vmwrite(TSC_OFFSET_HIGH, 0);

	__vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MASK, 0);
	__vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MATCH, 0);

	__vmx_vmwrite(VM_EXIT_MSR_STORE_COUNT, 0);
	__vmx_vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0);

	__vmx_vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0);
	__vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, 0);


	gdt_base = Get_GDT_Base();

	fill_guest_selector_data((PVOID)gdt_base, ES, GetEs());
	fill_guest_selector_data((PVOID)gdt_base, CS, GetCs());
	fill_guest_selector_data((PVOID)gdt_base, SS, GetSs());
	fill_guest_selector_data((PVOID)gdt_base, DS, GetDs());
	fill_guest_selector_data((PVOID)gdt_base, FS, GetFs());
	fill_guest_selector_data((PVOID)gdt_base, GS, GetGs());
	fill_guest_selector_data((PVOID)gdt_base, LDTR, GetLdtr());
	fill_guest_selector_data((PVOID)gdt_base, TR, GetTr());

	__vmx_vmwrite(GUEST_FS_BASE, __readmsr(MSR_FS_BASE));
	__vmx_vmwrite(GUEST_GS_BASE, __readmsr(MSR_GS_BASE));

	DbgPrint("[*] MSR_IA32_VMX_PROCBASED_CTLS : 0x%llx\n", AdjustControls(CPU_BASED_ACTIVATE_MSR_BITMAP | CPU_BASED_ACTIVATE_SECONDARY_CONTROLS, MSR_IA32_VMX_PROCBASED_CTLS));
	DbgPrint("[*] MSR_IA32_VMX_PROCBASED_CTLS2 : 0x%llx\n", AdjustControls(CPU_BASED_CTL2_ENABLE_EPT | CPU_BASED_CTL2_RDTSCP | CPU_BASED_CTL2_ENABLE_INVPCID | CPU_BASED_CTL2_ENABLE_XSAVE_XRSTORS, MSR_IA32_VMX_PROCBASED_CTLS2));

	__vmx_vmwrite(CPU_BASED_VM_EXEC_CONTROL, AdjustControls(CPU_BASED_ACTIVATE_MSR_BITMAP | CPU_BASED_ACTIVATE_SECONDARY_CONTROLS, MSR_IA32_VMX_PROCBASED_CTLS));
	__vmx_vmwrite(SECONDARY_VM_EXEC_CONTROL, AdjustControls(CPU_BASED_CTL2_ENABLE_EPT | CPU_BASED_CTL2_RDTSCP | CPU_BASED_CTL2_ENABLE_INVPCID | CPU_BASED_CTL2_ENABLE_XSAVE_XRSTORS, MSR_IA32_VMX_PROCBASED_CTLS2));


	__vmx_vmwrite(PIN_BASED_VM_EXEC_CONTROL, AdjustControls(0, MSR_IA32_VMX_PINBASED_CTLS));
	__vmx_vmwrite(VM_EXIT_CONTROLS, AdjustControls(VM_EXIT_IA32E_MODE | VM_EXIT_ACK_INTR_ON_EXIT, MSR_IA32_VMX_EXIT_CTLS));
	__vmx_vmwrite(VM_ENTRY_CONTROLS, AdjustControls(VM_ENTRY_IA32E_MODE, MSR_IA32_VMX_ENTRY_CTLS));

	__vmx_vmwrite(CR3_TARGET_COUNT, 0);
	__vmx_vmwrite(CR3_TARGET_VALUE0, 0);
	__vmx_vmwrite(CR3_TARGET_VALUE1, 0);
	__vmx_vmwrite(CR3_TARGET_VALUE2, 0);
	__vmx_vmwrite(CR3_TARGET_VALUE3, 0);

	__vmx_vmwrite(CR0_GUEST_HOST_MASK, 0);
	__vmx_vmwrite(CR4_GUEST_HOST_MASK, 0);
	__vmx_vmwrite(CR0_READ_SHADOW, 0);
	__vmx_vmwrite(CR4_READ_SHADOW, 0);


	__vmx_vmwrite(GUEST_CR0, __readcr0());
	__vmx_vmwrite(GUEST_CR3, __readcr3());
	__vmx_vmwrite(GUEST_CR4, __readcr4());

	__vmx_vmwrite(GUEST_DR7, 0x400);

	__vmx_vmwrite(HOST_CR0, __readcr0());
	__vmx_vmwrite(HOST_CR3, __readcr3());
	__vmx_vmwrite(HOST_CR4, __readcr4());


	__vmx_vmwrite(GUEST_GDTR_BASE, Get_GDT_Base());
	__vmx_vmwrite(GUEST_IDTR_BASE, Get_IDT_Base());
	__vmx_vmwrite(GUEST_GDTR_LIMIT, Get_GDT_Limit());
	__vmx_vmwrite(GUEST_IDTR_LIMIT, Get_IDT_Limit());

	__vmx_vmwrite(GUEST_RFLAGS, Get_RFLAGS());

	__vmx_vmwrite(GUEST_SYSENTER_CS, __readmsr(MSR_IA32_SYSENTER_CS));
	__vmx_vmwrite(GUEST_SYSENTER_EIP, __readmsr(MSR_IA32_SYSENTER_EIP));
	__vmx_vmwrite(GUEST_SYSENTER_ESP, __readmsr(MSR_IA32_SYSENTER_ESP));

	get_segment_descriptor(&segment_selector, GetTr(), (PUCHAR)Get_GDT_Base());
	__vmx_vmwrite(HOST_TR_BASE, segment_selector.BASE);

	__vmx_vmwrite(HOST_FS_BASE, __readmsr(MSR_FS_BASE));
	__vmx_vmwrite(HOST_GS_BASE, __readmsr(MSR_GS_BASE));

	__vmx_vmwrite(HOST_GDTR_BASE, Get_GDT_Base());
	__vmx_vmwrite(HOST_IDTR_BASE, Get_IDT_Base());

	__vmx_vmwrite(HOST_IA32_SYSENTER_CS, __readmsr(MSR_IA32_SYSENTER_CS));
	__vmx_vmwrite(HOST_IA32_SYSENTER_EIP, __readmsr(MSR_IA32_SYSENTER_EIP));
	__vmx_vmwrite(HOST_IA32_SYSENTER_ESP, __readmsr(MSR_IA32_SYSENTER_ESP));


	__vmx_vmwrite(GUEST_RSP, (ULONG64)guest_stack);     //setup guest sp
	__vmx_vmwrite(GUEST_RIP, (ULONG64)VMXRestoreState);     //setup guest ip

	__vmx_vmwrite(HOST_RSP, ((ULONG64)vm_state->VMM_Stack + VMM_STACK_SIZE - 1));
	__vmx_vmwrite(HOST_RIP, (ULONG64)VMExitHandler);

	Status = TRUE;
	return Status;
}
