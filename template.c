#include <ykernel.h>
#include "process.h"

int
LoadProgram(char *name, char *args[], PCB *proc)
{
    (void)name;
    (void)args;
    (void)proc;

    TracePrintf(1, "LoadProgram stub\n");
    return ERROR;
}
