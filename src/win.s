.intel_syntax noprefix

.section .text

.global win
.type win, @function

win:
    mov rdi, 0x68732f6e69622f
    push rdi
    mov rdi, rsp
    
    push 0
    push rdi
    mov rsi, rsp
    mov rdx, rsp
    mov rax, 59
    syscall
