#ifndef _KPWN_KERNEL_
#define _KPWN_KERNEL_

#include <stddef.h>

extern void* kbase;

void set_kbase(void* addr);

size_t kfunc_abs(void* fptr, int argc, ...);
size_t kfunc_off(void* fptr, int argc, ...);

#endif