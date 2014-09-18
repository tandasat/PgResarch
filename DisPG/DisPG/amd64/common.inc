;
; This module implements common macros
;


; Saves all general purpose registers to the stack
PUSHAQ MACRO
    push    r15
    push    r14
    push    r13
    push    r12
    push    r11
    push    r10
    push    r9
    push    r8
    push    rdi
    push    rsi
    push    rbp
    push    rbp    ; rsp
    push    rbx
    push    rdx
    push    rcx
    push    rax
ENDM


; Loads all general purpose registers from the stack
POPAQ MACRO
    pop     rax
    pop     rcx
    pop     rdx
    pop     rbx
    pop     rbp     ; rsp
    pop     rbp
    pop     rsi
    pop     rdi
    pop     r8
    pop     r9
    pop     r10
    pop     r11
    pop     r12
    pop     r13
    pop     r14
    pop     r15
ENDM


; Saves all general purpose registers and the flag register to the stack
PUSH_ALL_GENERAL_REGISTERS MACRO
    pushfq
    PUSHAQ
ENDM


; Loads all general purpose registers and the flag register from the stack
POP_ALL_GENERAL_REGISTERS MACRO
    POPAQ
    popfq
ENDM


; Implements jump to an arbitrary location without modifying registers.
; 0ffffffffffffffffh is used as a mark to be replaced with a correct address.
JMP_TEMPLATE MACRO
    nop     ; This is space for implanting int 3 for debugging
    push    rax
    mov     rax, 0ffffffffffffffffh
    xchg    rax, [rsp]
    ret
ENDM

