#include "common.h"
#include "msr.h"
#include "vmx.h"
#include "vmm.h"

ULONG64 g_guest_rip;
ULONG64 g_guest_rsp;

void handle_ept_violation(PGUEST_REGS guest_regs) {
	UNREFERENCED_PARAMETER(guest_regs);
}

ULONG64 get_next_instruction() {
	ULONG64 resume_rip = 0;
	ULONG64 guest_rip = 0;
	ULONGLONG instruction_len = 0;

	__vmx_vmread(GUEST_RIP, &guest_rip);
	__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &instruction_len);

	resume_rip = guest_rip + instruction_len;

	return resume_rip;
}

VOID resume_to_next_instruction()
{
	__vmx_vmwrite(GUEST_RIP, get_next_instruction());
}

BOOLEAN HandleCPUID(PGUEST_REGS state)
{
	INT32 cpu_info[4];


	// Check for the magic CPUID sequence, and check that it is coming from
	// Ring 0. Technically we could also check the RIP and see if this falls
	// in the expected function, but we may want to allow a separate "unload"
	// driver or code at some point.

	ULONG mode = 0;
	__vmx_vmread(GUEST_CS_SELECTOR, (ULONGLONG*)&mode);
	mode = mode & RPL_MASK;

	if ((state->rax == 0x41414141) && (state->rcx == 0x42424242) && mode == DPL_SYSTEM)
	{
		//DbgBreakPoint();
		return TRUE; // Indicates we have to turn off VMX
	}

	// Otherwise, issue the CPUID to the logical processor based on the indexes
	// on the VP's GPRs.
	__cpuidex(cpu_info, (INT32)state->rax, (INT32)state->rcx);

	// Check if this was CPUID 1h, which is the features request.
	if (state->rax == 1)
	{

		// Set the Hypervisor Present-bit in RCX, which Intel and AMD have both
		// reserved for this indication.
		cpu_info[2] |= HYPERV_HYPERVISOR_PRESENT_BIT;
	}

	else if (state->rax == HYPERV_CPUID_INTERFACE)
	{
		// Return our interface identifier
		cpu_info[0] = 'HVFS'; // [H]yper[v]isor [F]rom [S]cratch 
	}

	// Copy the values from the logical processor registers into the VP GPRs.
	state->rax = cpu_info[0];
	state->rbx = cpu_info[1];
	state->rcx = cpu_info[2];
	state->rdx = cpu_info[3];

	return FALSE; // Indicates we don't have to turn off VMX


}

void HandleControlRegisterAccess(PGUEST_REGS GuestRegs)
{
	ULONG movcrControlRegister = 0;
	ULONG movcrAccessType = 0;
	ULONG movcrOperandType = 0;
	ULONG movcrGeneralPurposeRegister = 0;

	ULONG64 ExitQualification = 0;
	ULONG64 GuestCR0 = 0;
	ULONG64 GuestCR3 = 0;
	ULONG64 GuestCR4 = 0;

	ULONG64 x = 0;

	__vmx_vmread(EXIT_QUALIFICATION, &ExitQualification);
	__vmx_vmread(GUEST_CR0, &GuestCR0);
	__vmx_vmread(GUEST_CR3, &GuestCR3);
	__vmx_vmread(GUEST_CR4, &GuestCR4);

	movcrControlRegister = (ULONG)(ExitQualification & 0x0000000F);
	movcrAccessType = (ULONG)((ExitQualification & 0x00000030) >> 4);
	movcrOperandType = (ULONG)((ExitQualification & 0x00000040) >> 6);
	movcrGeneralPurposeRegister = (ULONG)((ExitQualification & 0x00000F00) >> 8);

	/* Process the event (only for MOV CRx, REG instructions) */
	if (movcrOperandType == 0 && (movcrControlRegister == 0 || movcrControlRegister == 3 || movcrControlRegister == 4))
	{
		if (movcrAccessType == 0)
		{

			/* CRx <-- reg32 */

			if (movcrControlRegister == 0)
				x = GUEST_CR0;
			else if (movcrControlRegister == 3)
				x = GUEST_CR3;
			else
				x = GUEST_CR4;

			switch (movcrGeneralPurposeRegister)
			{

			case 0:  __vmx_vmwrite(x, GuestRegs->rax); break;
			case 1:  __vmx_vmwrite(x, GuestRegs->rcx); break;
			case 2:  __vmx_vmwrite(x, GuestRegs->rdx); break;
			case 3:  __vmx_vmwrite(x, GuestRegs->rbx); break;
			case 4:  __vmx_vmwrite(x, GuestRegs->rsp); break;
			case 5:  __vmx_vmwrite(x, GuestRegs->rbp); break;
			case 6:  __vmx_vmwrite(x, GuestRegs->rsi); break;
			case 7:  __vmx_vmwrite(x, GuestRegs->rdi); break;
			}
		}
		else if (movcrAccessType == 1)
		{
			/* reg32 <-- CRx */

			if (movcrControlRegister == 0)
				x = GuestCR0;
			else if (movcrControlRegister == 3)
				x = GuestCR3;
			else
				x = GuestCR4;

			switch (movcrGeneralPurposeRegister)
			{
			case 0:  GuestRegs->rax = x; break;
			case 1:  GuestRegs->rcx = x; break;
			case 2:  GuestRegs->rdx = x; break;
			case 3:  GuestRegs->rbx = x; break;
			case 4:  GuestRegs->rsp = x; break;
			case 5:  GuestRegs->rbp = x; break;
			case 6:  GuestRegs->rsi = x; break;
			case 7:  GuestRegs->rdi = x; break;
			default: break;
			}

		}

	}

}


void HandleMSRRead(PGUEST_REGS GuestRegs)
{
	MSR msr = { 0 };


	// RDMSR. The RDMSR instruction causes a VM exit if any of the following are true:
	// 
	// The "use MSR bitmaps" VM-execution control is 0.
	// The value of ECX is not in the ranges 00000000H - 00001FFFH and C0000000H - C0001FFFH
	// The value of ECX is in the range 00000000H - 00001FFFH and bit n in read bitmap for low MSRs is 1,
	//   where n is the value of ECX.
	// The value of ECX is in the range C0000000H - C0001FFFH and bit n in read bitmap for high MSRs is 1,
	//   where n is the value of ECX & 00001FFFH.

	if (((GuestRegs->rcx <= 0x00001FFF)) || ((0xC0000000 <= GuestRegs->rcx) && (GuestRegs->rcx <= 0xC0001FFF)))
	{
		msr.Content = __readmsr((ULONG)GuestRegs->rcx);
	}
	else
	{
		msr.Content = 0;
	}

	GuestRegs->rax = msr.Low;
	GuestRegs->rdx = msr.High;
}

void HandleMSRWrite(PGUEST_REGS GuestRegs)
{
	MSR msr = { 0 };

	// Check for sanity of MSR 
	if ((GuestRegs->rcx <= 0x00001FFF) || ((0xC0000000 <= GuestRegs->rcx) && (GuestRegs->rcx <= 0xC0001FFF)))
	{
		msr.Low = (ULONG)GuestRegs->rax;
		msr.High = (ULONG)GuestRegs->rdx;
		__writemsr((ULONG)GuestRegs->rcx, msr.Content);
	}
}


BOOLEAN main_vmexit_handler(PGUEST_REGS guest_regs)
{
	BOOLEAN status = FALSE;

	ULONGLONG exit_reason = 0;
	__vmx_vmread(VM_EXIT_REASON, &exit_reason);

	ULONGLONG exit_qual = 0;
	__vmx_vmread(EXIT_QUALIFICATION, &exit_qual);
	exit_reason &= 0xffff;

	switch (exit_reason)
	{
	case EXIT_REASON_TRIPLE_FAULT:
	{
		//		DbgBreakPoint();
		break;
	}

	// 25.1.2  Instructions That Cause VM Exits Unconditionally
	// The following instructions cause VM exits when they are executed in VMX non-root operation: CPUID, GETSEC,
	// INVD, and XSETBV. This is also true of instructions introduced with VMX, which include: INVEPT, INVVPID, 
	// VMCALL, VMCLEAR, VMLAUNCH, VMPTRLD, VMPTRST, VMRESUME, VMXOFF, and VMXON.

	case EXIT_REASON_VMCLEAR:
	case EXIT_REASON_VMPTRLD:
	case EXIT_REASON_VMPTRST:
	case EXIT_REASON_VMREAD:
	case EXIT_REASON_VMRESUME:
	case EXIT_REASON_VMWRITE:
	case EXIT_REASON_VMXOFF:
	case EXIT_REASON_VMXON:
	case EXIT_REASON_VMLAUNCH:
	{
		DbgPrint("\n [*] Target guest tries to execute VM Instruction ,"
			"it probably causes a fatal error or system halt as the system might"
			" think it has VMX feature enabled while it's not available due to our use of hypervisor.\n");

		DbgBreakPoint();
		ULONG rflags = 0;
		__vmx_vmread(GUEST_RFLAGS, (ULONGLONG*)&rflags);
		__vmx_vmwrite(GUEST_RFLAGS, rflags | 0x1); // cf=1 indicate vm instructions fail
		break;
	}

	case EXIT_REASON_CR_ACCESS:
	{
		HandleControlRegisterAccess(guest_regs);

		break;
	}
	case EXIT_REASON_MSR_READ:
	{
		ULONG ecx = guest_regs->rcx & 0xffffffff;
		DbgPrint("[*] RDMSR (based on bitmap) : 0x%llx\n", ecx);
		HandleMSRRead(guest_regs);

		break;
	}
	case EXIT_REASON_MSR_LOADING:
	{
		//DbgBreakPoint();
		break;
	}
	case EXIT_REASON_MSR_WRITE:
	{
		ULONG ecx = guest_regs->rcx & 0xffffffff;
		DbgPrint("[*] WRMSR (based on bitmap) : 0x%llx\n", ecx);
		HandleMSRWrite(guest_regs);

		break;
	}
	case EXIT_REASON_CPUID:
	{
		status = HandleCPUID(guest_regs); // Detect whether we have to turn off VMX or Not
		if (status)
		{
			// We have to save GUEST_RIP & GUEST_RSP somewhere to restore them directly
			g_guest_rip = get_next_instruction();
			__vmx_vmread(GUEST_RSP, &g_guest_rsp);
		}
		break;
	}
	case EXIT_REASON_EXCEPTION_NMI:
	{
		// HandleExceptionNMI();
		break;
	}
	case EXIT_REASON_IO_INSTRUCTION:
	{
		UINT64 guest_rip = 0;
		__vmx_vmread(GUEST_RIP, &guest_rip);

		DbgPrint("[*] RIP executed IO instruction : 0x%llx\n", guest_rip);
		DbgBreakPoint();

		break;
	}
	case EXIT_REASON_VMCALL:
	{
		DbgBreakPoint();
		break;
	}
	case EXIT_REASON_EPT_VIOLATION:
	{
		handle_ept_violation(guest_regs);
		DbgBreakPoint();
		break;
	}
	default:
	{

		//DbgBreakPoint();
		break;
	}
	}
	if (!status)
	{
		resume_to_next_instruction();
	}

	return status;
}