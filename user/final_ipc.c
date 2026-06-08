#include <yuser.h>

static int
same(char *buf, char *want, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        if (buf[i] != want[i]) {
            return 0;
        }
    }
    return 1;
}

int
main(void)
{
    int pipe_id;
    int lock_id;
    int cvar_id;
    int rc;
    char buf[8];

    if (PipeInit(&pipe_id) == ERROR) {
        Exit(1);
    }
    if (PipeWrite(pipe_id, "abcdef", 6) != 6) {
        Exit(2);
    }
    rc = PipeRead(pipe_id, buf, 3);
    if (rc != 3 || !same(buf, "abc", 3)) {
        Exit(3);
    }
    rc = PipeRead(pipe_id, buf, 4);
    if (rc != 3 || !same(buf, "def", 3)) {
        Exit(4);
    }
    if (Reclaim(pipe_id) == ERROR) {
        Exit(5);
    }

    if (LockInit(&lock_id) == ERROR) {
        Exit(6);
    }
    rc = Fork();
    if (rc == 0) {
        if (Acquire(lock_id) == ERROR) {
            Exit(7);
        }
        Exit(0);
    }
    if (rc < 0) {
        Exit(8);
    }
    Delay(2);
    if (Acquire(lock_id) == ERROR) {
        Exit(9);
    }
    if (Release(lock_id) == ERROR || Reclaim(lock_id) == ERROR) {
        Exit(10);
    }

    if (CvarInit(&cvar_id) == ERROR ||
        CvarBroadcast(cvar_id) == ERROR ||
        Reclaim(cvar_id) == ERROR) {
        Exit(11);
    }

    TtyPrintf(0, "final_ipc: ok\n");
    Exit(0);
}
