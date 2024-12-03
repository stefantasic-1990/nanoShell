#include "craftLine.h"
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>

int toggleOutputPostprocessing() {
    struct termios terminal_settings;

    // change terminal settings
    if (tcgetattr(STDIN_FILENO, &terminal_settings) == -1) {return 1;} 
    terminal_settings.c_oflag ^= (OPOST);

    // set terminal settings
    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&terminal_settings) == -1) {return -1;};

    return 0;
}

int tsh_executeCmd(char** cmd, int in, int out) {
    int status;
    int pid;

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


int tshParseLine(char** args) {
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

char** tshTokenizeLine(char* line) {
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