#ifndef YALNIX_KERNEL_H
#define YALNIX_KERNEL_H

#include <ykernel.h>

#define KERNEL_TRACE_BOOT 1
#define KERNEL_TRACE_DEBUG 3

extern int vm_enabled;

void KernelPanic(char *msg);

#endif
