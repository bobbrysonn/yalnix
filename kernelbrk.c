#include "kernel.h"
#include "memory.h"

static int current_kernel_brk_page = 0;
static int highest_pre_vm_brk_page = 0;

int
SetKernelBrk(void *addr)
{
    int requested_page = UP_TO_PAGE(addr) >> PAGESHIFT;
    int page;

    if (current_kernel_brk_page == 0) {
        current_kernel_brk_page = _orig_kernel_brk_page;
        highest_pre_vm_brk_page = _orig_kernel_brk_page;
    }

    if (requested_page < _orig_kernel_brk_page) {
        requested_page = _orig_kernel_brk_page;
    }

    if (!vm_enabled) {
        if (requested_page > highest_pre_vm_brk_page) {
            highest_pre_vm_brk_page = requested_page;
        }
        current_kernel_brk_page = requested_page;
        return SUCCESS;
    }

    for (page = current_kernel_brk_page; page < requested_page; page++) {
        TracePrintf(2, "SetKernelBrk: would map kernel heap page %d\n", page);
    }
    for (page = requested_page; page < current_kernel_brk_page; page++) {
        TracePrintf(2, "SetKernelBrk: would unmap kernel heap page %d\n", page);
    }

    current_kernel_brk_page = requested_page;
    return SUCCESS;
}
