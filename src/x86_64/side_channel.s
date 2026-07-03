.intel_syntax noprefix
.section .text

.global cpuinfo_str
.type cpuinfo_str, @function
cpuinfo_str: // (char* dst[.13])
    push rbx

    xor rax, rax
    cpuid
    mov dword ptr[rdi], ebx
    mov dword ptr[rdi + 4], edx
    mov dword ptr[rdi + 8], ecx
    mov byte ptr[rdi + 12], 0

    pop rbx
    ret

.global measure_prefetch
.type measure_prefetch, @function
measure_prefetch: // uint64_t measure_prefetch(void* addr);
    // Serializing timer bracket: mfence;rdtscp ... lfence ... lfence ... rdtscp;mfence
    mfence
    rdtscp
    mov r8, rdx
    shl r8, 32
    or  r8, rax

    xor rax, rax
    lfence
    prefetchnta [rdi]
    prefetcht2  [rdi]
    xor rax, rax
    lfence

    rdtscp
    shl rdx, 32
    or  rax, rdx
    sub rax, r8

    mfence
    ret
