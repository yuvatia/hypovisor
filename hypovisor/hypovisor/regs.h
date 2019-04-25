#pragma once
#include <wdm.h>
USHORT GetCs(VOID);
USHORT GetDs(VOID);
USHORT GetEs(VOID);
USHORT GetSs(VOID);
USHORT GetFs(VOID);
USHORT GetGs(VOID);
USHORT GetLdtr(VOID);
USHORT GetTr(VOID);
USHORT Get_IDT_Limit(VOID);
USHORT Get_GDT_Limit(VOID);
ULONG64 inline Get_GDT_Base(void);
ULONG64 inline Get_IDT_Base(void);
ULONG64 Get_RFLAGS(VOID);
VOID STI_Instruction(VOID);
VOID CLI_Instruction(VOID);
