#include "craftline.h"
#include <unistd.h>
#include <limits.h>

int main(int argc, char **argv) {
    char host[_POSIX_HOST_NAME_MAX];
    char cwd[PATH_MAX];
    char prompt[50];
    char** args;
    char* line;

    do {
        if (gethostname(host, sizeof(host)) == -1 || getcwd(cwd, sizeof(cwd)) != NULL) {
            snprintf(prompt, 50, "%s@%s %s: ", getlogin(), host, strrchr(cwd, '/'));
        } else {
            return -1;
        }
        
        line = craftLine(prompt);
        args = tshTokenizeLine(line);
        tshParseLine(args);

        free(line);
        free(args);
    } while (1);
}