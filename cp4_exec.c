#include <yuser.h>

int
main(void)
{
    char *args[] = { "cp4_target", "alpha", "beta", 0 };

    TracePrintf(0, "cp4_exec: before exec pid %d\n", GetPid());
    if (Exec("cp4_target", args) == ERROR) {
        TracePrintf(0, "cp4_exec: exec failed\n");
        Exit(1);
    }

    TracePrintf(0, "cp4_exec: should not return\n");
    Exit(2);
}
