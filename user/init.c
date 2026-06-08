#include <yuser.h>

int
main(int argc, char **argv)
{
    int pid;
    int loops = 0;

    (void)argc;
    (void)argv;

    pid = GetPid();
    TracePrintf(0, "init: pid %d starting\n", pid);

    while (1) {
        TracePrintf(0, "init: loop %d pid %d\n", loops, pid);
        if ((loops % 5) == 4) {
            Delay(2);
        } else {
            Pause();
        }
        loops++;
    }
}
