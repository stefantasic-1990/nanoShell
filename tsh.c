#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>

#define TSH_LINEBUFFERSIZE 100

struct termios terminal_settings;

char *tsh_getLine(char* prompt, int promptlen) {
    int buffersize = TSH_LINEBUFFERSIZE;
    int bufferpos = 0;
    int bufferlen = 0;
    char* buffer = malloc(sizeof(char) * buffersize);
    char cursorpos[7];
    char eseq[3];    
    char c;

    // get size of terminal window
    struct winsize ws;
    ioctl(1, TIOCGWINSZ, &ws);

    do {
        // refresh line and reset cursor
        snprintf(cursorpos, 7, "\x1b[%iG", promptlen + 1 + bufferpos);
        write(STDOUT_FILENO, "\x1b[0G", sizeof("\x1b[0G"));
        write(STDOUT_FILENO, prompt, promptlen);
        write(STDOUT_FILENO, buffer, buffersize);
        write(STDOUT_FILENO, "\x1b[0K", sizeof("\x1b[0K"));
        write(STDOUT_FILENO, cursorpos, sizeof(cursorpos));

        // read-in next character
        read(STDIN_FILENO, &c, 1);

        // handle character
        switch(c) {
            case 13: // enter
                write(STDOUT_FILENO, "\x1b[1E", sizeof("\x1b[1E"));
                goto returnLine;
            case 8: // ctrl+h
            case 127: // backspace
                if (bufferpos > 0) {
                    memmove(buffer+(bufferpos-1), buffer+bufferpos , bufferlen - bufferpos);
                    bufferpos--;
                    bufferlen--;
                    buffer[bufferlen] = '\0';
                }
                break;
            case 3: // ctrl+c
                return NULL;
            case 4: // ctrl+d
                break;
            case 20: // ctrl+t
                break; 
            case 16: // ctrl+p
                break; 
            case 14: // ctrl+n
                break; 
            case 27: // escape character
                // read-in the next two characters
                if (read(STDOUT_FILENO, eseq, 1) == -1) { break; }
                if (read(STDOUT_FILENO, eseq+1, 1) == -1) { break; }
                if (eseq[0] == '[') {
                    switch(eseq[1]) {
                        // right arrow key
                        case 'C':
                            if (bufferpos < bufferlen) { bufferpos++; }
                            break;
                        // left arrow key
                        case 'D':
                            if (bufferpos > 0) { bufferpos--; }
                            break;
                        // up arrow key
                        case 'A':
                            // get previous record in history
                            break;
                        // down arrow key
                        case 'B':
                            // get next record in history
                            break;
                    }
                }
                break;
            default: // store character in buffer
                if ((promptlen + bufferlen) < ws.ws_col) { 
                    memmove(buffer+bufferpos+1, buffer+bufferpos, bufferlen - bufferpos);
                    buffer[bufferpos] = c;
                    bufferpos++;
                    bufferlen++;
                }
                break;
        }
        // allocate more space for buffer if required
        if (bufferlen >= buffersize) {
            buffersize += TSH_LINEBUFFERSIZE;
            buffer = realloc(buffer, buffersize);
            if (!buffer) {return NULL;}
        }
    } while (1);

    returnLine:
        return buffer;
}

int enableRawTerminal() {
    struct termios modified_settings;

    // check TTY device
    if (!isatty(STDIN_FILENO)) {return -1;} 

    // change terminal settings
    modified_settings = terminal_settings;
    modified_settings.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    modified_settings.c_oflag &= ~(OPOST);
    modified_settings.c_cflag |= (CS8);
    modified_settings.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    modified_settings.c_cc[VMIN] = 1; 
    modified_settings.c_cc[VTIME] = 0;

    // set new terminal settings
    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&modified_settings) == -1) {return -1;};

    return 0;
}

void disableRawTerminal() {
    // restore initial settings
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&terminal_settings);
}

int main(int argc, char **argv) {
    char host[_POSIX_HOST_NAME_MAX];
    char cwd[PATH_MAX];
    char prompt[50];
    int promptlen;
    int status;
    char* line;
    char** args;

    // Enable raw terminal mode
    if (tcgetattr(STDIN_FILENO, &terminal_settings) == -1 ||
        enableRawTerminal() == -1 ||
        atexit(disableRawTerminal) != 0) {return -1;}
    // get prompt string and its length
    if (gethostname(host, sizeof(host)) == -1 || getcwd(cwd, sizeof(cwd)) == NULL) {return -1;} {
        promptlen = snprintf(prompt, 50, "%s@%s %s: ", getlogin(), host, strrchr(cwd, '/'));
    }
    // main program loop
    do {
        if ((line = tsh_getLine(prompt, promptlen)) == NULL) {return -1;}
        //if ((args = tsh_tokenizeLine(line)) == NULL) { return -1; }
        //if (tsh_executeCommand(args) == NULL) { return -1; }
    } while (1);

    disableRawTerminal();
}