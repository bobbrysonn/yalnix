#include <fcntl.h>
#include <unistd.h>
#include "kernel.h"
#include "memory.h"
#include "process.h"
#include <load_info.h>

static int MapRange(pte_t *pt, int start, int count, int prot);

int
LoadProgram(char *name, char *args[], PCB *proc)
{
    int fd;
    struct load_info li;
    int i;
    char *cp;
    char **cpp;
    char *cp2;
    char *stack_ptr;
    char *argbuf;
    int argcount;
    int argsize;
    int text_pg1;
    int data_pg1;
    int data_npg;
    int stack_npg;
    int stack_base_pg;
    long segment_size;

    if ((fd = open(name, O_RDONLY)) < 0) {
        TracePrintf(0, "LoadProgram: can't open '%s'\n", name);
        return ERROR;
    }

    if (LoadInfo(fd, &li) != LI_NO_ERROR) {
        TracePrintf(0, "LoadProgram: '%s' is not a Yalnix executable\n", name);
        close(fd);
        return ERROR;
    }

    if (li.entry < VMEM_1_BASE) {
        TracePrintf(0, "LoadProgram: '%s' is not linked for Region 1\n", name);
        close(fd);
        return ERROR;
    }

    text_pg1 = (li.t_vaddr - VMEM_1_BASE) >> PAGESHIFT;
    data_pg1 = (li.id_vaddr - VMEM_1_BASE) >> PAGESHIFT;
    data_npg = li.id_npg + li.ud_npg;

    argsize = 0;
    for (i = 0; args[i] != 0; i++) {
        argsize += strlen(args[i]) + 1;
    }
    argcount = i;

    cp = ((char *)VMEM_1_LIMIT) - argsize;
    cpp = (char **)(((int)cp -
                     ((argcount + 3 + POST_ARGV_NULL_SPACE) * sizeof(void *))) & ~7);
    cp2 = (char *)cpp - INITIAL_STACK_FRAME_SIZE;
    stack_ptr = cp2;
    stack_npg = (VMEM_1_LIMIT - DOWN_TO_PAGE(cp2)) >> PAGESHIFT;
    stack_base_pg = MAX_PT_LEN - stack_npg;

    if (text_pg1 < 0 || data_pg1 < text_pg1 ||
        text_pg1 + li.t_npg > MAX_PT_LEN ||
        data_pg1 + data_npg > stack_base_pg - 1) {
        close(fd);
        return ERROR;
    }

    argbuf = (char *)malloc(argsize);
    if (argbuf == 0) {
        close(fd);
        return ERROR;
    }

    cp2 = argbuf;
    for (i = 0; args[i] != 0; i++) {
        strcpy(cp2, args[i]);
        cp2 += strlen(cp2) + 1;
    }

    MemoryFreePageTableFrames(proc->region1_pt);

    if (MapRange(proc->region1_pt, text_pg1, li.t_npg,
                 PROT_READ | PROT_WRITE) == ERROR ||
        MapRange(proc->region1_pt, data_pg1, data_npg,
                 PROT_READ | PROT_WRITE) == ERROR ||
        MapRange(proc->region1_pt, stack_base_pg, stack_npg,
                 PROT_READ | PROT_WRITE) == ERROR) {
        free(argbuf);
        close(fd);
        return KILL;
    }
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

    lseek(fd, li.t_faddr, SEEK_SET);
    segment_size = li.t_npg << PAGESHIFT;
    if (read(fd, (void *)li.t_vaddr, segment_size) != segment_size) {
        free(argbuf);
        close(fd);
        return KILL;
    }

    lseek(fd, li.id_faddr, SEEK_SET);
    segment_size = li.id_npg << PAGESHIFT;
    if (read(fd, (void *)li.id_vaddr, segment_size) != segment_size) {
        free(argbuf);
        close(fd);
        return KILL;
    }
    close(fd);

    for (i = text_pg1; i < text_pg1 + li.t_npg; i++) {
        proc->region1_pt[i].prot = PROT_READ | PROT_EXEC;
    }
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

    memset((void *)li.id_end, 0, li.ud_end - li.id_end);

    memset(cpp, 0, VMEM_1_LIMIT - (int)cpp);
    *cpp++ = (char *)argcount;
    cp2 = argbuf;
    for (i = 0; i < argcount; i++) {
        *cpp++ = cp;
        strcpy(cp, cp2);
        cp += strlen(cp) + 1;
        cp2 += strlen(cp2) + 1;
    }
    free(argbuf);
    *cpp++ = 0;
    *cpp++ = 0;

    proc->user_context.pc = (void *)li.entry;
    proc->user_context.sp = stack_ptr;
    proc->user_heap_start = (void *)UP_TO_PAGE(li.ud_end);
    proc->user_brk = proc->user_heap_start;
    proc->user_stack_low_page = stack_base_pg;

    return SUCCESS;
}

static int
MapRange(pte_t *pt, int start, int count, int prot)
{
    int i;
    int pfn;

    for (i = 0; i < count; i++) {
        pfn = FrameAlloc();
        if (pfn == ERROR) {
            return ERROR;
        }
        if (MemoryMapPage(pt, start + i, pfn, prot) == ERROR) {
            FrameFree(pfn);
            return ERROR;
        }
    }

    return SUCCESS;
}
