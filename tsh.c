#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>

#define TSH_LINEBUFFERSIZE 100

char *tsh_getLine(char* prompt, int promptlen) {
    char c;
    struct winsize ws;
    int buffersize = TSH_LINEBUFFERSIZE;
    int bufferpos = 0;
    int bufferlen = 0;
    char *buffer = malloc(sizeof(char) * buffersize);

    ioctl(1, TIOCGWINSZ, &ws);

    do {
        // Refresh line and reset cursor
        write(STDOUT_FILENO, "\x1b[0G", sizeof("\x1b[0G"));
        write(STDOUT_FILENO, prompt, promptlen);
        write(STDOUT_FILENO, buffer, buffersize);

        // Read in next character
        read(STDIN_FILENO, &c, 1);

        switch(c) {
            case 13:
                goto returnLine;
            default:
                if ((promptlen + bufferlen) < ws.ws_col) { 
                    buffer[bufferpos] = c;
                    bufferpos++;
                    bufferlen++;
                    write(STDOUT_FILENO, &c, sizeof(c));
                }
                break;
        }
        if (bufferlen >= buffersize) {
            buffersize += TSH_LINEBUFFERSIZE;
            buffer = realloc(buffer, buffersize);
            if (!buffer) { return NULL; }
        }
    } while (1);

    returnLine:
        write(STDOUT_FILENO, "\x1b[1E", sizeof("\x1b[1E"));
        return buffer;
}

int main (int argc, char **argv) {
    char host[_POSIX_HOST_NAME_MAX];
    char cwd[PATH_MAX];
    char* login;
    char prompt[50];
    int promptlen;

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
    promptlen = snprintf(prompt, 50, "%s@%s %s: ", getlogin(), host, strrchr(cwd, '/'));

    // Main program loop
    do {
        if ((line = tsh_getLine(prompt, promptlen)) == NULL) { 
            return -1; 
            } else {
            //write(STDOUT_FILENO, line, sizeof(line));
        }
    } while (status);
}