.intel_syntax noprefix
.global db_int_handler
.global ipi_int_handler
.extern debug_handle_db
.extern ipi_handle_interrupt

.macro SAVE_GP_REGISTERS
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rbx
    push rdx
    push rcx
    push rax
.endm

.macro RESTORE_GP_REGISTERS
    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
.endm

.macro SWAPGS_IF_USER
    test byte ptr [rsp + 8], 3
    jz 1f
    swapgs
1:
.endm

.macro RESTORE_GS_IF_USER
    test byte ptr [rsp + 8], 3
    jz 1f
    cli
    swapgs
1:
.endm

.text

db_int_handler:
    SWAPGS_IF_USER
    SAVE_GP_REGISTERS
    cld
    mov rdi, rsp
    call debug_handle_db
    RESTORE_GP_REGISTERS
    RESTORE_GS_IF_USER
    iretq

ipi_int_handler:
    SWAPGS_IF_USER
    SAVE_GP_REGISTERS
    cld
    mov rdi, rsp
    call ipi_handle_interrupt
    RESTORE_GP_REGISTERS
    RESTORE_GS_IF_USER
    iretq
