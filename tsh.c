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

char** tshTokenizeCmdLine(char* cmdLine) {
    int cmdLinePosition = 0;
    int cmdArgsIndex = 0;
    int cmdArgsSize = 10;
    int argPosition = 0;
    int argSize = 20;
    int quoteModeFlag = 0;
    char** cmdArgs = calloc(cmdArgsSize, sizeof(char*));

    while (1) {
        cmdArgs[cmdArgsIndex] = calloc(argSize, sizeof(char));
        while (1) {
            switch (cmdLine[cmdLinePosition]) {
            case '\0':
                if (argPosition != 0) {
                    cmdArgs[cmdArgsIndex][argPosition] = '\0';
                    cmdArgs[cmdArgsIndex+1] = "\0";
                } else {cmdArgs[cmdArgsIndex] = "\0";}
                return cmdArgs;
            case '\"':
                quoteModeFlag = 1 - quoteModeFlag;
                cmdLinePosition++;
                break;
            case '\\':
                cmdLinePosition++;
                switch(cmdLine[cmdLinePosition]) {
                case 'n':
                    cmdArgs[cmdArgsIndex][argPosition] = '\r';
                    argPosition++;
                    cmdArgs[cmdArgsIndex][argPosition] = '\n';
                    argPosition++;
                    cmdLinePosition++;
                    break;
                case '\\':
                    cmdArgs[cmdArgsIndex][argPosition] = '\\';
                    argPosition++;
                    cmdLinePosition++;
                    break;
                case '\"':
                    cmdArgs[cmdArgsIndex][argPosition] = '\"';
                    argPosition++;
                    cmdLinePosition++;
                    break;
                case '\'':
                    cmdArgs[cmdArgsIndex][argPosition] = '\'';
                    argPosition++;
                    cmdLinePosition++;
                    break;
                case 'r':
                    cmdArgs[cmdArgsIndex][argPosition] = '\r';
                    argPosition++;
                    cmdLinePosition++;
                    break;
                }
                break;
            case ' ':
                if (quoteModeFlag == 0) {
                    if (argPosition != 0) {
                        cmdArgs[cmdArgsIndex][argPosition] = '\0';
                        cmdLinePosition++;
                        goto nextToken;
                        }
                    cmdLinePosition++;
                    break;
                }
            default:
                cmdArgs[cmdArgsIndex][argPosition] = cmdLine[cmdLinePosition];
                cmdLinePosition++;
                argPosition++;
            }

            if (argPosition >= argSize) {
            argSize += argSize;
            cmdArgs[cmdArgsIndex] = realloc(cmdArgs[cmdArgsIndex], argSize * sizeof(char));
            if (!cmdArgs[cmdArgsIndex]) {return NULL;}
            }  
        }

        nextToken:
            cmdArgsIndex++;
            argPosition = 0;

            if (cmdArgsIndex >= cmdArgsSize) {
            cmdArgsSize += cmdArgsSize;
            cmdArgs = realloc(cmdArgs, cmdArgsSize * sizeof(char*));
            if (!cmdArgs) {return NULL;}
            } 
    }
}

int main(int argc, char **argv) {
    char host[_POSIX_HOST_NAME_MAX];
    char cwd[PATH_MAX];
    char prompt[50];
    char* cmdLine;
    char** cmdArgs;

    do {
        if (gethostname(host, sizeof(host)) == -1 || getcwd(cwd, sizeof(cwd)) != NULL) {
            snprintf(prompt, 50, "%s@%s %s: ", getlogin(), host, strrchr(cwd, '/'));
        } else {
            return -1;
        }
        
        cmdLine = craftLine(prompt);
        cmdArgs = tshTokenizeCmdLine(cmdLine);
        tshParseLine(cmdArgs);
        free(cmdLine);
        free(cmdArgs);
    } while (1);
}