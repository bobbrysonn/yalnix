#include "kernel.h"
#include "memory.h"

static int current_kernel_brk_page = 0;
static int highest_pre_vm_brk_page = 0;

int
KernelBrkPage(void)
{
    if (current_kernel_brk_page == 0) {
        current_kernel_brk_page = _orig_kernel_brk_page;
        highest_pre_vm_brk_page = _orig_kernel_brk_page;
    }

    if (!vm_enabled && highest_pre_vm_brk_page > current_kernel_brk_page) {
        return highest_pre_vm_brk_page;
    }

    return current_kernel_brk_page;
}

int
SetKernelBrk(void *addr)
{
    int requested_page = UP_TO_PAGE(addr) >> PAGESHIFT;
    int old_page;
    int page;
    int pfn;
    int stack_base_page = KERNEL_STACK_BASE >> PAGESHIFT;

    old_page = KernelBrkPage();

    if (requested_page < _orig_kernel_brk_page) {
        requested_page = _orig_kernel_brk_page;
    }
    if (requested_page > stack_base_page) {
        TracePrintf(0, "SetKernelBrk: requested break 0x%x reaches kernel stack\n",
                    requested_page << PAGESHIFT);
        return ERROR;
    }

    if (!vm_enabled) {
        if (requested_page > highest_pre_vm_brk_page) {
            highest_pre_vm_brk_page = requested_page;
        }
        current_kernel_brk_page = requested_page;
        return SUCCESS;
    }

    for (page = old_page; page < requested_page; page++) {
        pfn = FrameAlloc();
        if (pfn == ERROR) {
            TracePrintf(0, "SetKernelBrk: out of physical memory\n");
            while (--page >= old_page) {
                FrameFree(kernel_region0_pt[page].pfn);
                MemoryUnmapPage(kernel_region0_pt, page);
            }
            return ERROR;
        }

        if (MemoryMapPage(kernel_region0_pt, page, pfn, PROT_READ | PROT_WRITE) == ERROR) {
            FrameFree(pfn);
            while (--page >= old_page) {
                FrameFree(kernel_region0_pt[page].pfn);
                MemoryUnmapPage(kernel_region0_pt, page);
            }
            return ERROR;
        }
    }

    for (page = requested_page; page < old_page; page++) {
        if (kernel_region0_pt[page].valid) {
            FrameFree(kernel_region0_pt[page].pfn);
            MemoryUnmapPage(kernel_region0_pt, page);
        }
    }

    current_kernel_brk_page = requested_page;
    return SUCCESS;
}
