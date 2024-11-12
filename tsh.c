#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>

#define CMD_HISTORY_SIZE 10

struct termios initial_terminal_settings; // original terminal settings
int pid = -1; // global pid to differentiate parent when doing atexit()

int enableRawTerminal() {
    struct termios modified_terminal_settings;

    // save initial terminal settings
    if (tcgetattr(STDIN_FILENO, &initial_terminal_settings) == -1) {return 1;} 

    // check TTY device
    if (!isatty(STDIN_FILENO)) {return -1;} 

    // change terminal settings
    if (tcgetattr(STDIN_FILENO, &modified_terminal_settings) == -1) {return 1;} 
    modified_terminal_settings.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    modified_terminal_settings.c_oflag &= ~(OPOST);
    modified_terminal_settings.c_cflag |= (CS8);
    modified_terminal_settings.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    modified_terminal_settings.c_cc[VMIN] = 1; 
    modified_terminal_settings.c_cc[VTIME] = 0;

    // set new terminal settings
    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&modified_terminal_settings) == -1) {return -1;};

    return 0;
}

void disableRawTerminal() {
    // restore initial settings
    if (pid != 0) {tcsetattr(STDIN_FILENO,TCSAFLUSH,&initial_terminal_settings);}
}

int toggleOutputPostprocessing() {
    struct termios modified_terminal_settings;

    // change terminal settings
    if (tcgetattr(STDIN_FILENO, &modified_terminal_settings) == -1) {return 1;} 
    modified_terminal_settings.c_oflag ^= (OPOST);

    // set terminal settings
    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&modified_terminal_settings) == -1) {return -1;};

    return 0;
}

int tsh_executeCmd(char** cmd, int in, int out) {
    int status;

    // check if command is builtin
    if (strcmp(cmd[0], "cd") == 0 && out == 1) {
        if (cmd[1] == NULL) {return 1;}
        if (chdir(cmd[1]) != 0) {return -1;}
    } 
    if (strcmp(cmd[0], "exit") == 0 && out == 1) {
        exit(EXIT_SUCCESS);
    }

    // create child process
    pid = fork();
    if (pid == 0) {
        // reidirect input/output
        if (in != 0) {
            dup2(in, 0);
            close (in);
        }
        if (out != 1) {
            dup2(out, 1);
            close(out);
        }
        // child process execute command
        execvp(cmd[0], cmd);
        // child process exits if execvp can't find the executable in the path
        exit(EXIT_FAILURE);
    } else if (pid < 0 ) {
        // fork error
        return -1;
    } else {
        // parent process wait for child
        do {waitpid(pid, &status, WUNTRACED);}
        while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 0;
}


int tsh_parseCommand(char** args) {
    int pipefd[2]; // pipe file descriptor array
    int nextin = 0; // file descriptor to be used for next input stream
    int cmd_start = 0; // command string start index
    int cmd_end = 0; // command string end index
    int cmd_len = 0; // command length in arg
    int i = 0; // loop index
    char** cmd; // current command arg array
    char* fn; // file name
    FILE* fp; // file pointer

    // check if token array is empty
    if (strcmp(args[0], "\0") == 0) {return 0;}

    // turn off output postprocessing
    toggleOutputPostprocessing();

    // parse args array from left to right and figure out the next command
    while (1) {
        // if  null string run command and stop parsing
        if (strcmp(args[i], "\0") == 0) {
            cmd_len = cmd_end - cmd_start;
            cmd = calloc(cmd_len, sizeof(char*));
            memcpy(cmd, args + cmd_start, cmd_len*sizeof(char*));
            tsh_executeCmd(cmd, nextin, 1);
            goto end;
        // if && run command and do no piping or redirection
        } else if (strcmp(args[i], "&&") == 0 && strcmp(args[i + 1], "\0") != 0) {
            cmd_len = cmd_end - cmd_start;
            cmd = calloc(cmd_len, sizeof(char*));
            memcpy(cmd, args + cmd_start, cmd_len*sizeof(char*));
            tsh_executeCmd(cmd, nextin, 1);
            nextin = 0;
            cmd_len = 0;
            cmd_end++;
            cmd_start = cmd_end;
        // if | run command and pipe to next
        } else if (strcmp(args[i], "|") == 0 && strcmp(args[i + 1], "\0") != 0) {
            cmd_len = cmd_end - cmd_start;
            cmd = calloc(cmd_len, sizeof(char*));
            memcpy(cmd, args + cmd_start, cmd_len*sizeof(char*));
            pipe(pipefd);
            tsh_executeCmd(cmd, nextin, pipefd[1]);
            close(pipefd[1]);
            nextin = pipefd[0];
            cmd_len = 0;
            cmd_end++;
            cmd_start = cmd_end;
        // if > redirect output stream of current command to given file, run command
        } else if (strcmp(args[i], ">") == 0 && strcmp(args[i+1], "\0") != 0) {
            cmd_len = cmd_end - cmd_start;
            cmd = calloc(cmd_len, sizeof(char*));
            memcpy(cmd, args + cmd_start, cmd_len*sizeof(char*));
            fn = args[cmd_end + 1];
            fp = fopen(fn, "a+");
            tsh_executeCmd(cmd, nextin, fileno(fp));
            fclose(fp);
            memmove(args + 2, args + cmd_start, cmd_len*sizeof(char*));
            cmd_start += 2;
            cmd_end++;
        // if < redirect input stream of current command to given file, run command
        } else if (strcmp(args[i], "<") == 0) {
            continue;
        // add arg to current command
        } else {
            cmd_end++;
            cmd_len++;
        }
        // increment loop index
        i++;
    }

    end:
        // turn on output postprocessing
        toggleOutputPostprocessing();
        return 0;
}

char** tsh_parseLine(char* line) {
    int line_p = 0; // line buff position
    int args_s = 10; // args buff size
    int args_p = 0; // args buff position
    int arg_s = 20; // arg buff size
    int arg_p = 0; // arg buff position
    int qmode = 0; // quoted mode flag 
    char** args = calloc(args_s, sizeof(char*));

    while (1) {
        // allocate space for next token
        args[args_p] =  calloc(arg_s, sizeof(char));

        // parse next token
        while (1) {
            // if end of line is reached
            switch (line[line_p]) {
            case '\0':
                // if we are building a token null-terminate it
                if (arg_p != 0) {
                    args[args_p][arg_p] = '\0';
                    args[args_p+1] = "\0";
                } else {args[args_p] = "\0";}
                return args;
            case '\"':
                // change quoted mode
                qmode = 1 - qmode;
                line_p++;
                break;
            // // if escape character
            case '\\':
                // get next character and determine escape sequence
                // we ignore the escape character by itself
                line_p++;
                switch(line[line_p]) {
                case 'n':
                    args[args_p][arg_p] = '\r';
                    arg_p++;
                    args[args_p][arg_p] = '\n';
                    arg_p++;
                    line_p++;
                    break;
                case '\\':
                    args[args_p][arg_p] = '\\';
                    arg_p++;
                    line_p++;
                    break;
                case '\"':
                    args[args_p][arg_p] = '\"';
                    arg_p++;
                    line_p++;
                    break;
                case '\'':
                    args[args_p][arg_p] = '\'';
                    arg_p++;
                    line_p++;
                    break;
                case 'r':
                    args[args_p][arg_p] = '\r';
                    arg_p++;
                    line_p++;
                    break;
                }
                break;
            // if space character
            case ' ':
                // only treat as a token separator if not in quoted mode
                // otherwise we skip down to default and treat as regular character
                if (qmode == 0) {
                    // if we are building a token null-terminate it, set next token to NULL
                    if (arg_p != 0) {
                        args[args_p][arg_p] = '\0';
                        line_p++;
                        goto nextToken;
                        }
                    // if we are not building a token, ignore the white space
                    line_p++;
                    break;
                }
                // if we are building a token null-terminate it, set next token to NULL
            // if regular character
            default:
                // add character to token string
                args[args_p][arg_p] = line[line_p];
                line_p++;
                arg_p++;
            }
            // reallocate more token characters if required
            if (arg_p >= arg_s) {
            arg_s += arg_s;
            args[args_p] = realloc(args[args_p], arg_s * sizeof(char));
            if (!args[args_p]) {return NULL;}
            }  
        }

        nextToken:
            // set variables for next token string
            args_p++;
            arg_p = 0;

            // reallocate more token pointers if required
            if (args_p >= args_s) {
            args_s += args_s;
            args = realloc(args, args_s * sizeof(char*));
            if (!args) {return NULL;}
            } 
    }
}

char* tshGetCommandLine(char* prompt, int prompt_l, char* cmdhis[CMD_HISTORY_SIZE]) {
    int window_width; // terminal window column width    
    int cmdhis_p = 0; // command history position

    char* buff; // input line buffer
    int buff_total_size = 100; // input line buffer total size
    int buff_char_len = 0; // input line buffer character length
    int buff_curs_pos = 0; // input line buffer cursor position
    int buff_disp_off = 0; // input line buffer display offset
    int buff_disp_len = 0; // input line buffer display length

    char cursor_p[10]; // cursor position escape sequence
    char eseq[3]; // input escape sequence
    char c; // input character

    // initialize line buffer
    buff = calloc(buff_total_size, sizeof(char)); // line buffer
    buff[0] = '\0';

    // get line display length
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == -1) {window_width = 80;} else {window_width = ws.ws_col - 1;}
    buff_disp_len = window_width - prompt_l;

    do {
        // refresh line
        snprintf(cursor_p, sizeof(cursor_p), "\x1b[%iG", prompt_l + 1 + buff_curs_pos - buff_disp_off);
        write(STDOUT_FILENO, "\x1b[0G", strlen("\x1b[0G"));
        write(STDOUT_FILENO, prompt, prompt_l);
        write(STDOUT_FILENO, (buff + buff_disp_off), (buff_total_size < buff_disp_len) ? buff_total_size : buff_disp_len);
        write(STDOUT_FILENO, "\x1b[0K", strlen("\x1b[0K"));
        write(STDOUT_FILENO, cursor_p, strlen(cursor_p));
        // read-in next character
        read(STDIN_FILENO, &c, 1);

        // handle key
        switch(c) {
            case 13: // enter
                goto returnLine;
            case 8: // ctrl+h
            case 127: // backspace
                if (buff_curs_pos > 0) {
                    memmove(buff+(buff_curs_pos-1), buff+buff_curs_pos , buff_char_len - buff_curs_pos);
                    buff_curs_pos--;
                    buff_char_len--;
                    buff[buff_char_len] = '\0';
                }
                break;
            case 3: // ctrl+c
                exit(EXIT_SUCCESS);
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
            case 21: // ctrl+u - clear input
                free(buff);
                buff = calloc(buff_total_size, sizeof(char));
                buff_curs_pos = 0;
                buff_disp_off = 0;
                buff_char_len = 0;
                break;
            case 27: // escape character
                // read-in the next two characters
                if (read(STDOUT_FILENO, eseq, 1) == -1) {break;}
                if (read(STDOUT_FILENO, eseq+1, 1) == -1) {break;}
                if (eseq[0] == '[') {
                    switch(eseq[1]) {
                        case 'C': // right arrow key
                            if (buff_curs_pos < buff_char_len) {buff_curs_pos++;}
                            if ((buff_curs_pos - buff_disp_off) > buff_disp_len) {buff_disp_off++;}
                            break;
                        case 'D': // left arrow key
                            if (buff_curs_pos > 0) {buff_curs_pos--;}
                            if ((buff_curs_pos - buff_disp_off) < 0) {buff_disp_off--;}
                            break;
                        case 'A': // up arrow key
                            // get previous record in history
                            if ((cmdhis[cmdhis_p + 1] != NULL) && cmdhis_p < (CMD_HISTORY_SIZE - 1)) {
                                if (cmdhis_p == 0) {cmdhis[0] = strdup(buff);}
                                cmdhis_p++;
                                buff_char_len = strlen(cmdhis[cmdhis_p]);
                                buff_total_size = strlen(cmdhis[cmdhis_p]) + 1;
                                buff_curs_pos = buff_char_len;
                                buff_disp_off = (buff_char_len < buff_disp_len) ? 0 : buff_char_len - buff_disp_len;
                                free(buff);
                                buff = calloc(buff_total_size, sizeof(char));
                                strcpy(buff, cmdhis[cmdhis_p]);
                            }
                            break;
                        case 'B': // down arrow key
                            // get next record in history
                            if (cmdhis_p > 0) {
                                cmdhis_p--;
                                buff_char_len = strlen(cmdhis[cmdhis_p]);
                                buff_total_size = strlen(cmdhis[cmdhis_p]) + 1;
                                buff_curs_pos = buff_char_len;
                                buff_disp_off = (buff_char_len < buff_disp_len) ? 0 : buff_char_len - buff_disp_len;
                                free(buff);
                                buff = calloc(buff_total_size, sizeof(char));
                                strcpy(buff, cmdhis[cmdhis_p]);
                            }    
                            break;
                    }
                }
                break;
            default: // store character in buff
                memmove(buff+buff_curs_pos+1, buff+buff_curs_pos, buff_char_len - buff_curs_pos);
                buff[buff_curs_pos] = c;
                buff_curs_pos++;
                buff_char_len++;
                buff[buff_char_len] = '\0';
                if ((buff_curs_pos - buff_disp_off) > buff_disp_len) {buff_disp_off++;}
                break;
        }
        // allocate more space for buff if required
        if (buff_char_len + 1 >= buff_total_size) {
            buff_total_size += buff_total_size;
            buff = realloc(buff, buff_total_size);
            if (!buff) {return NULL;}
        }
    } while (1);

    returnLine:
        // free first history element
        free(cmdhis[0]);
        cmdhis[0] = NULL;
        // if command not empty, copy into history
        if (buff[0] != '\0') {
            // free last element memory
            free(cmdhis[CMD_HISTORY_SIZE - 1]);
            // move elements, ignore first which is reserved for current line edit
            memmove(cmdhis + 2, cmdhis + 1, CMD_HISTORY_SIZE*sizeof(char*)*2 - sizeof(char*)*2);
            cmdhis[1] = strdup(buff);
        }
        write(STDOUT_FILENO, "\r\n", sizeof("\r\n"));
        return buff;
}


int main(int argc, char **argv) {
    char host[_POSIX_HOST_NAME_MAX]; // machine hostname
    char cwd[PATH_MAX]; // current working directory
    char prompt[50]; // prompt string
    char** args; // command line tokens
    char* line; // command line
    int prompt_l; // prompt character length
    FILE* fp; // file pointer

    int i = 1; // loop index
    char* cmd; // command
    size_t cmd_len; // command length
    char cmdhisfn[] = "./cmdhis.txt"; // command history file name
    char* cmdhis[CMD_HISTORY_SIZE] = {NULL}; // command history

    // enable raw terminal mode
    if (enableRawTerminal() == -1) {return -1;}
    // ensure that terminal settings are restored at shell exit
    if (atexit(disableRawTerminal) != 0) {return -1;}

    // restore command history
    fp = fopen(cmdhisfn, "a+");
    rewind(fp);
    while (i < CMD_HISTORY_SIZE) {
        getline(&cmd, &cmd_len, fp);
        // if command available and file is not new
        if (strcmp(cmd, "\0") != 0) {
            // overwrite newline character and load
            cmd[strlen(cmd)-1] = '\0';
            cmdhis[i] = strdup(cmd);
        }
        i++;
    }

    // main program loop
    do {
        // assemble prompt string and get its length
        if (gethostname(host, sizeof(host)) == -1 || getcwd(cwd, sizeof(cwd)) == NULL) {return -1;}
        prompt_l = snprintf(prompt, 50, "%s@%s %s: ", getlogin(), host, strrchr(cwd, '/'));
        // get command line, parse it, and execute
        line = tshGetCommandLine(prompt, prompt_l, cmdhis);
        args = tsh_parseLine(line);
        tsh_parseCommand(args);
        // // free memory
        free(line);
        free(args);
        // save command history
        ftruncate(fileno(fp), 0);
        for (int i = 1; i < CMD_HISTORY_SIZE; i++) {
            // if command available in history store into file
            if (cmdhis[i] != NULL) {
                fprintf(fp, "%s\n", cmdhis[i]);
                fflush(fp);
            }
        }
    } while (1);
}