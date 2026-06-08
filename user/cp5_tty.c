#include <yuser.h>

int
main(void)
{
    char big[1500];
    int i;

    TtyPrintf(0, "cp5_tty: hello from pid %d\n", GetPid());

    for (i = 0; i < (int)sizeof(big); i++) {
        big[i] = 'A' + (i % 26);
    }
    big[sizeof(big) - 1] = '\n';

    TtyWrite(0, big, sizeof(big));
    TtyPrintf(0, "cp5_tty: long write complete\n");
    Exit(0);
}
