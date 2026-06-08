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
    char buf[32];
    int rc;

    TtyPrintf(0, "cp5_tty_rw: enter line\n");
    rc = TtyRead(0, buf, sizeof(buf));
    if (rc != 6 || !same(buf, "hello\n", 6)) {
        TtyPrintf(0, "cp5_tty_rw: read failed rc=%d\n", rc);
        Exit(1);
    }

    TtyWrite(0, "cp5_tty_rw: echo ", 17);
    TtyWrite(0, buf, rc);
    TtyPrintf(0, "cp5_tty_rw: ok\n");
    Exit(0);
}
