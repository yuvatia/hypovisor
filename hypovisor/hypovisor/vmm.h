#pragma once

BOOLEAN main_vmexit_handler(PGUEST_REGS guest_regs);
extern ULONG64 g_guest_rip;
extern ULONG64 g_guest_rsp;