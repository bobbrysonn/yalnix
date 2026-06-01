#include <yuser.h>

static int
expect(char *got, int got_len, char *want, int want_len)
{
    int i;

    if (got_len != want_len) {
        return 0;
    }
    for (i = 0; i < want_len; i++) {
        if (got[i] != want[i]) {
            return 0;
        }
    }
    return 1;
}

int
main(void)
{
    char buf[16];
    int rc;

    rc = TtyRead(0, buf, 3);
    if (!expect(buf, rc, "abc", 3)) {
        TtyPrintf(0, "cp5_read: first read failed rc=%d\n", rc);
        Exit(1);
    }

    rc = TtyRead(0, buf, 8);
    if (!expect(buf, rc, "def\n", 4)) {
        TtyPrintf(0, "cp5_read: leftover read failed rc=%d\n", rc);
        Exit(2);
    }

    rc = TtyRead(0, buf, 8);
    if (!expect(buf, rc, "xyz\n", 4)) {
        TtyPrintf(0, "cp5_read: second line failed rc=%d\n", rc);
        Exit(3);
    }

    TtyPrintf(0, "cp5_read: ok\n");
    Exit(0);
}
