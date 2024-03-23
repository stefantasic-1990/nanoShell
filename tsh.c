#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>

struct termios terminal_settings;

char** tsh_parseLine(char* line) {
    int line_p = 0; // line buffer position
    int args_s = 10; // args buffer size
    int args_p = 0; // args buffer position
    int arg_s = 20; // arg buffer size
    int arg_p = 0; // arg buffer position
    char* arg = malloc(arg_s * sizeof(char*));
    char** args = malloc(args_s * sizeof(char*));

    int c;

    while (1) {
        if (line[line_p] == '\0') {
            if (arg_p != 0) {
                arg[arg_p] = '\0';
                args[args_p] = arg;
                args[args_p+1] = NULL;
            }
            goto end;
        } else if (line[line_p] == '\\') {
            switch(line[line_p+1]) {
            default:
                line_p++;
                break;
            }
        } else if (line[line_p] == ' ') {
            if (arg_p != 0) {
                arg[arg_p] = '\0';
                write(STDOUT_FILENO, args[0], 10);
                read(STDIN_FILENO, &c, 1);
                *args[args_p] = *arg;
                write(STDOUT_FILENO, args[args_p], 10);
                read(STDIN_FILENO, &c, 1);
                args_p++;
                arg_p = 0;
            }
            line_p++;
        } else {
            arg[arg_p] = line[line_p];
            line_p++;
            arg_p++;
        }
    }

    end:
        for (int i = 0; args[i]; i++) {
            //if (args[i] == NULL) { read(STDIN_FILENO, &c, 1); };
            write(STDOUT_FILENO, args[i], 10);
            read(STDIN_FILENO, &c, 1);
        }

        return args;
}

char* tsh_getLine(char* prompt, int prompt_l) {
    int buffer_s = 50; // line buffer total size
    int buffer_l = 0; // line buffer character length
    int buffer_p = 0; // line buffer cursor position
    int buffer_o = 0; // line buffer display offset
    char* buffer = malloc(sizeof(char) * buffer_s); // line buffer
    char cursor_p[10]; // cursor position escape sequence
    char c; // input character
    char eseq[3]; // input escape sequence
    int window_c; // terminal window column width

    // get column width of terminal window
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == -1) {window_c = 80;} else {window_c = ws.ws_col - 1;}

    do {
        // refresh line
        snprintf(cursor_p, sizeof(cursor_p), "\x1b[%iG", prompt_l + 1 + buffer_p - buffer_o);
        write(STDOUT_FILENO, "\x1b[0G", strlen("\x1b[0G"));
        write(STDOUT_FILENO, prompt, prompt_l);
        write(STDOUT_FILENO, (buffer + buffer_o), (window_c - prompt_l));
        write(STDOUT_FILENO, "\x1b[0K", strlen("\x1b[0K"));
        write(STDOUT_FILENO, cursor_p, strlen(cursor_p));
        // read-in next character
        read(STDIN_FILENO, &c, 1);

        // handle character
        switch(c) {
            case 13: // enter
                if (buffer_l != 0) {
                    write(STDOUT_FILENO, "\x1b[1E", sizeof("\x1b[1E"));
                    // buffer[buffer_l] = '\0';
                    goto returnLine;
                }
                write(STDOUT_FILENO, "\x1b[1Ess", sizeof("\x1b[1E"));
                break;
            case 8: // ctrl+h
            case 127: // backspace
                if (buffer_p > 0) {
                    memmove(buffer+(buffer_p-1), buffer+buffer_p , buffer_l - buffer_p);
                    buffer_p--;
                    buffer_l--;
                    buffer[buffer_l] = '\0';
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
            case 11: // ctrl+k
                break;
            case 1: // ctrl+a
                break;
            case 5: // ctrl+e
                break;
            case 12: // ctrl+l
                break;
            case 23: // ctrl+w
                break;
            case 21: // ctrl+u
                free(buffer);
                buffer = malloc(sizeof(char) * buffer_s);
                buffer_p = 0;
                buffer_o = 0;
                buffer_l = 0;
                break;
            case 27: // escape character
                // read-in the next two characters
                if (read(STDOUT_FILENO, eseq, 1) == -1) {break;}
                if (read(STDOUT_FILENO, eseq+1, 1) == -1) {break;}
                if (eseq[0] == '[') {
                    switch(eseq[1]) {
                        // right arrow key
                        case 'C':
                            if (buffer_p < buffer_l) {buffer_p++;}
                            if ((buffer_p - buffer_o) > (window_c - prompt_l)) {buffer_o++;}
                            break;
                        // left arrow key
                        case 'D':
                            if (buffer_p > 0) {buffer_p--;}
                            if ((buffer_p - buffer_o) < 0) {buffer_o--;}
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
                memmove(buffer+buffer_p+1, buffer+buffer_p, buffer_l - buffer_p);
                buffer[buffer_p] = c;
                buffer_p++;
                buffer_l++;
                buffer[buffer_l] = '\0';
                if ((buffer_p - buffer_o) > (window_c - prompt_l)) {buffer_o++;}
                break;
        }
        // allocate more space for buffer if required
        if (buffer_l >= buffer_s) {
            buffer_s += buffer_s;
            buffer = realloc(buffer, buffer_s);
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
    int prompt_l;
    int status;
    char* line;
    char** args;

    // Enable raw terminal mode
    if (tcgetattr(STDIN_FILENO, &terminal_settings) == -1) {return 1;} 
    if (enableRawTerminal() == -1) {return 1;}
    if (atexit(disableRawTerminal) != 0) {return 1;}
    // assemble prompt string and get its length
    if (gethostname(host, sizeof(host)) == -1 || getcwd(cwd, sizeof(cwd)) == NULL) {return -1;} {
        prompt_l = snprintf(prompt, 50, "%s@%s %s: ", getlogin(), host, strrchr(cwd, '/'));
    }
    // main program loop
    do {
        if ((line = tsh_getLine(prompt, prompt_l)) == NULL) {return -1;}
        if ((args = tsh_parseLine(line)) == NULL) { return -1; }
        //if (tsh_executeCommand(args) == NULL) { return -1; }
    } while (1);

    disableRawTerminal();
}