#include <yuser.h>

int
main(void)
{
    char buf[24 * 1024];
    int i;

    for (i = 0; i < (int)sizeof(buf); i += 1024) {
        buf[i] = (char)i;
    }

    TracePrintf(0, "cp4_stack: touched %d bytes\n", (int)sizeof(buf));
    Exit(0);
}
