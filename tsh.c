#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>

#define TSH_LINEBUFFERSIZE 100

// int enableRawTerminal() {

// }

// int disableRawTerminal() {
//     tcsetattr(STDIN_FILENO,TCSAFLUSH,&canonical_settings);
// }

char *tsh_getLine(char* prompt, int prompt_s) {
    char c;
    int buffersize = TSH_LINEBUFFERSIZE;
    int bufferpos = 0;
    int bufferlen = 0;
    char *buffer = malloc(sizeof(char) * buffersize);

    do {
        // Refresh line and reset cursor
        write(STDOUT_FILENO, "\x1b[0G", sizeof("\033[0G"));
        write(STDOUT_FILENO, prompt, prompt_s);
        write(STDOUT_FILENO, buffer, buffersize);

        read(STDIN_FILENO, &c, 1);
        buffer[bufferpos] = c;
        bufferpos++;

        if (bufferlen >= buffersize) {
            buffersize += TSH_LINEBUFFERSIZE;
            buffer = realloc(buffer, buffersize);
            if (!buffer) { return NULL; }
        }

        write(STDOUT_FILENO, "\x1b[0K", sizeof("\033[0K"));
    } while (1);

    return buffer;
}

int main (int argc, char **argv) {
    char host[_POSIX_HOST_NAME_MAX];
    char cwd[PATH_MAX];
    char* login;
    char prompt[50];
    int prompt_s;

    int status;
    char* line;
    static struct termios canonical_settings;
    struct termios raw_settings;

    // Enable raw terminal mode
    if (tcgetattr(STDIN_FILENO, &canonical_settings) == -1) { return -1; }
    if (!isatty(STDIN_FILENO)) { return -1; } 
    //else {atexit(tcsetattr(STDIN_FILENO,TCSAFLUSH,&canonical_settings));}

    raw_settings = canonical_settings;
    raw_settings.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw_settings.c_oflag &= ~(OPOST);
    raw_settings.c_cflag |= (CS8);
    raw_settings.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw_settings.c_cc[VMIN] = 1; 
    raw_settings.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw_settings) == -1) { return -1; };

    // Get prompt string and prompt size
    if (gethostname(host, sizeof(host)) == -1 || getcwd(cwd, sizeof(cwd)) == NULL) { return -1; }
    prompt_s = snprintf(prompt, 50, "%s@%s %s: ", getlogin(), host, strrchr(cwd, '/'));

    // Main program loop
    do {
        if ((line = tsh_getLine(prompt, prompt_s)) == NULL) { 
            return -1; 
            } else {
            printf("Your input was: %s", line);
        }
    } while (status);
}

// linenoiseEditStart(&l,stdin_fd,stdout_fd,buf,buflen,prompt);
// char *res;
// while((res = linenoiseEditFeed(&l)) == linenoiseEditMore);
// linenoiseEditStop(&l);
// return res;