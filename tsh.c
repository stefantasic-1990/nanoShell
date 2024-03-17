#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

char *tsh_getLine()
{
    int key;

    key = getchar();
}

int main (int argc, char **argv)
{
    int status;
    char host[_POSIX_HOST_NAME_MAX];
    char cwd[PATH_MAX];

    do {

        if (gethostname(host, sizeof(host)) != -1 && getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s@%s %s: ", getlogin(), host, strrchr(cwd, '/'));
        } else { return -1; }

        tsh_getLine();

    } while (status);

}