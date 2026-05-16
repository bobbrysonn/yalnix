#include "kernel.h"
#include "memory.h"

pte_t *kernel_region0_pt = 0;
unsigned int physical_frame_count = 0;

static unsigned char frame_allocated[MAX_PMEM_SIZE / PAGESIZE];

void
MemoryInit(unsigned int pmem_size)
{
    unsigned int i;
    unsigned int kernel_stack_base_page;
    unsigned int kernel_stack_limit_page;

    physical_frame_count = pmem_size / PAGESIZE;
    if (physical_frame_count > (MAX_PMEM_SIZE / PAGESIZE)) {
        KernelPanic("physical memory exceeds frame table capacity");
    }

    for (i = 0; i < physical_frame_count; i++) {
        frame_allocated[i] = 0;
    }

    /* Reserve the kernel image and initial heap frames. */
    for (i = 0; i < (unsigned int)KernelBrkPage() && i < physical_frame_count; i++) {
        frame_allocated[i] = 1;
    }

    kernel_stack_base_page = KERNEL_STACK_BASE >> PAGESHIFT;
    kernel_stack_limit_page = KERNEL_STACK_LIMIT >> PAGESHIFT;
    for (i = kernel_stack_base_page; i < kernel_stack_limit_page && i < physical_frame_count; i++) {
        frame_allocated[i] = 1;
    }

    kernel_region0_pt = MemoryAllocPageTable();

    for (i = 0; i < (unsigned int)KernelBrkPage() && i < physical_frame_count; i++) {
        frame_allocated[i] = 1;
    }
}

int
FrameAlloc(void)
{
    unsigned int pfn;

    for (pfn = 0; pfn < physical_frame_count; pfn++) {
        if (!frame_allocated[pfn]) {
            frame_allocated[pfn] = 1;
            return (int)pfn;
        }
    }

    return ERROR;
}

void
FrameFree(int pfn)
{
    if (pfn < 0 || (unsigned int)pfn >= physical_frame_count) {
        TracePrintf(0, "FrameFree: invalid pfn %d\n", pfn);
        return;
    }

    frame_allocated[pfn] = 0;
}

int
FrameIsAllocated(int pfn)
{
    if (pfn < 0 || (unsigned int)pfn >= physical_frame_count) {
        return 0;
    }

    return frame_allocated[pfn] != 0;
}

pte_t *
MemoryAllocPageTable(void)
{
    pte_t *pt;

    pt = (pte_t *)calloc(MAX_PT_LEN, sizeof(pte_t));
    if (pt == 0) {
        KernelPanic("calloc page table failed");
    }

    return pt;
}

void
MemoryBuildKernelRegion0(void)
{
    int page;
    int stack_base_page;
    int stack_limit_page;

    if (kernel_region0_pt == 0) {
        KernelPanic("Region 0 page table is null");
    }

    /* VM starts with identity mappings for live kernel pages. */
    for (page = _first_kernel_text_page; page < _first_kernel_data_page; page++) {
        MemoryMapPage(kernel_region0_pt, page, page, PROT_READ | PROT_EXEC);
    }

    for (page = _first_kernel_data_page; page < KernelBrkPage(); page++) {
        MemoryMapPage(kernel_region0_pt, page, page, PROT_READ | PROT_WRITE);
    }

    stack_base_page = KERNEL_STACK_BASE >> PAGESHIFT;
    stack_limit_page = KERNEL_STACK_LIMIT >> PAGESHIFT;
    for (page = stack_base_page; page < stack_limit_page; page++) {
        MemoryMapPage(kernel_region0_pt, page, page, PROT_READ | PROT_WRITE);
    }
}

void
MemoryInstallPageTables(pte_t *region1_pt)
{
    WriteRegister(REG_PTBR0, (unsigned int)kernel_region0_pt);
    WriteRegister(REG_PTLR0, MAX_PT_LEN);
    WriteRegister(REG_PTBR1, (unsigned int)region1_pt);
    WriteRegister(REG_PTLR1, MAX_PT_LEN);
}

int
MemoryMapPage(pte_t *pt, int index, int pfn, int prot)
{
    if (pt == 0 || index < 0 || index >= MAX_PT_LEN || pfn < 0) {
        return ERROR;
    }

    pt[index].valid = 1;
    pt[index].prot = prot;
    pt[index].pfn = pfn;

    if (vm_enabled) {
        WriteRegister(REG_TLB_FLUSH, index << PAGESHIFT);
    }

    return SUCCESS;
}

void
MemoryUnmapPage(pte_t *pt, int index)
{
    if (pt == 0 || index < 0 || index >= MAX_PT_LEN) {
        return;
    }

    pt[index].valid = 0;
    pt[index].prot = PROT_NONE;
    pt[index].pfn = 0;

    if (vm_enabled) {
        WriteRegister(REG_TLB_FLUSH, index << PAGESHIFT);
    }
}
