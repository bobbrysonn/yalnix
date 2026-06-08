#include <yuser.h>

int
main(int argc, char **argv)
{
    TracePrintf(0, "cp4_target: pid %d argc %d\n", GetPid(), argc);
    if (argc >= 3) {
        TracePrintf(0, "cp4_target: argv %s %s %s\n", argv[0], argv[1], argv[2]);
    }
    Exit(7);
}
