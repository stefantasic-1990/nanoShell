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
    char eseq[3];
    struct winsize ws;
    int buffersize = TSH_LINEBUFFERSIZE;
    int bufferpos = 0;
    int bufferlen = 0;
    char cursorpos[7];
    char* buffer = malloc(sizeof(char) * buffersize);

    ioctl(1, TIOCGWINSZ, &ws);

    do {
        // Refresh line and reset cursor
        write(STDOUT_FILENO, "\x1b[0G", sizeof("\x1b[0G"));
        write(STDOUT_FILENO, prompt, promptlen);
        write(STDOUT_FILENO, buffer, buffersize);
        write(STDOUT_FILENO, "\x1b[0K", sizeof("\x1b[0K"));

        // Position cursor
        snprintf(cursorpos, 7, "\x1b[%iG", promptlen + 1 + bufferpos);
        write(STDOUT_FILENO, cursorpos, sizeof(cursorpos));

        // Read in next character
        read(STDIN_FILENO, &c, 1);

        switch(c) {
            case 13:
                goto returnLine;
            case 127:
                if (bufferpos > 0) {
                    memmove(buffer+(bufferpos-1), buffer+bufferpos , bufferlen - bufferpos);
                    bufferpos--;
                    bufferlen--;
                    buffer[bufferlen] = '\0';
                    break;
                }
            case 27:
                if (read(STDOUT_FILENO, eseq, 1) == -1) { break; }
                if (read(STDOUT_FILENO, eseq+1, 1) == -1) { break; }
                if (eseq[0] == '[') {
                    switch(eseq[1]) {
                        // Right arrow key
                        case 'C':
                            if (bufferpos < bufferlen) { 
                                bufferpos++;
                            }
                            break;
                        // Left arrow key
                        case 'D':
                            if (bufferpos > 0) { 
                                bufferpos--;
                            }
                            break;
                    }
                }
                break;

            default:
                if ((promptlen + bufferlen) < ws.ws_col) { 
                    memmove(buffer+bufferpos+1, buffer+bufferpos, bufferlen - bufferpos);
                    buffer[bufferpos] = c;
                    bufferpos++;
                    bufferlen++;
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
    char** args;
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
        if ((line = tsh_getLine(prompt, promptlen)) == NULL) { return -1; }
        //if ((args = tsh_tokenizeLine(line)) == NULL) { return -1; }
        //if (tsh_executeCommand(args) == NULL) { return -1; }

    } while (1);
}