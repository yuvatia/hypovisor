PUBLIC STI_Instruction
PUBLIC CLI_Instruction
PUBLIC GetCs
PUBLIC GetDs
PUBLIC GetEs
PUBLIC GetSs
PUBLIC GetFs
PUBLIC GetGs
PUBLIC GetLdtr
PUBLIC GetTr
PUBLIC Get_GDT_Base
PUBLIC Get_IDT_Base
PUBLIC Get_GDT_Limit
PUBLIC Get_IDT_Limit
PUBLIC Get_RFLAGS

.code _text

Get_GDT_Base PROC
	LOCAL	gdtr[10]:BYTE
	sgdt	gdtr
	mov		rax, QWORD PTR gdtr[2]
	ret
Get_GDT_Base ENDP
;------------------------------------------------------------------------
GetCs PROC
	mov		rax, cs
	ret
GetCs ENDP
;------------------------------------------------------------------------
GetDs PROC
	mov		rax, ds
	ret
GetDs ENDP
;------------------------------------------------------------------------
GetEs PROC
	mov		rax, es
	ret
GetEs ENDP
;------------------------------------------------------------------------
GetSs PROC
	mov		rax, ss
	ret
GetSs ENDP
;------------------------------------------------------------------------
GetFs PROC
	mov		rax, fs
	ret
GetFs ENDP
;------------------------------------------------------------------------
GetGs PROC
	mov		rax, gs
	ret
GetGs ENDP
;------------------------------------------------------------------------
GetLdtr PROC
	sldt	rax
	ret
GetLdtr ENDP
;------------------------------------------------------------------------
GetTr PROC
	str	rax
	ret
GetTr ENDP
;------------------------------------------------------------------------
Get_IDT_Base PROC
	LOCAL	idtr[10]:BYTE
	
	sidt	idtr
	mov		rax, QWORD PTR idtr[2]
	ret
Get_IDT_Base ENDP
;------------------------------------------------------------------------

Get_GDT_Limit PROC
	LOCAL	gdtr[10]:BYTE

	sgdt	gdtr
	mov		ax, WORD PTR gdtr[0]
	ret
Get_GDT_Limit ENDP

;------------------------------------------------------------------------
Get_IDT_Limit PROC
	LOCAL	idtr[10]:BYTE
	
	sidt	idtr
	mov		ax, WORD PTR idtr[0]
	ret
Get_IDT_Limit ENDP
;------------------------------------------------------------------------
Get_RFLAGS PROC
	pushfq
	pop		rax
	ret
Get_RFLAGS ENDP
;------------------------------------------------------------------------
STI_Instruction PROC PUBLIC
STI
ret
STI_Instruction ENDP 

;------------------------------------------------------------------------

CLI_Instruction PROC PUBLIC
CLI
ret
CLI_Instruction ENDP 
;------------------------------------------------------------------------

end