#include <yuser.h>

int
main(void)
{
    int first;
    int second;
    int pid;
    int status;

    TracePrintf(0, "cp4_fork: parent pid %d\n", GetPid());

    first = Fork();
    if (first == 0) {
        TracePrintf(0, "cp4_fork: child A pid %d\n", GetPid());
        Delay(2);
        Exit(11);
    }
    if (first < 0) {
        TracePrintf(0, "cp4_fork: first fork failed\n");
        Exit(1);
    }

    second = Fork();
    if (second == 0) {
        TracePrintf(0, "cp4_fork: child B pid %d\n", GetPid());
        Delay(1);
        Exit(22);
    }
    if (second < 0) {
        TracePrintf(0, "cp4_fork: second fork failed\n");
        Exit(2);
    }

    pid = Wait(&status);
    TracePrintf(0, "cp4_fork: wait pid %d status %d\n", pid, status);
    pid = Wait(&status);
    TracePrintf(0, "cp4_fork: wait pid %d status %d\n", pid, status);
    pid = Wait(&status);
    TracePrintf(0, "cp4_fork: final wait rc %d\n", pid);

    Exit(0);
}
