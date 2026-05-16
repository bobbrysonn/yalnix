#ifndef YALNIX_MEMORY_H
#define YALNIX_MEMORY_H

#include <ykernel.h>

extern pte_t *kernel_region0_pt;
extern unsigned int physical_frame_count;

int KernelBrkPage(void);
void MemoryInit(unsigned int pmem_size);
int FrameAlloc(void);
void FrameFree(int pfn);
int FrameIsAllocated(int pfn);
pte_t *MemoryAllocPageTable(void);
void MemoryBuildKernelRegion0(void);
void MemoryInstallPageTables(pte_t *region1_pt);
int MemoryMapPage(pte_t *pt, int index, int pfn, int prot);
void MemoryUnmapPage(pte_t *pt, int index);

#endif
