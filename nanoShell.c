#include "craftLine.h"
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <termios.h>
#include <limits.h>

/**
 * Function:  toggleOutputProcessing
 * ----------------------
 * Toggle terminal driver output processing on/off.
 * 
 * @return Returns 0 if successful and 1 if not.
 * 
 * This function toggles the OPOST terminal device driver setting on and off.
 */
int toggleOutputPostprocessing() {
    struct termios terminal_settings;

    if (isatty(STDIN_FILENO)) {
        tcgetattr(STDIN_FILENO, &terminal_settings);
        terminal_settings.c_oflag ^= (OPOST);
        tcsetattr(STDIN_FILENO,TCSAFLUSH,&terminal_settings);
    } else {
        return 1;
    }

    return 0;
}

/**
 * Function:  tshExecuteCmd
 * ----------------------
 * Execute a command given command arguments.
 * 
 * @param cmdArgs The command arguments array.
 * @return Returns 0 if successful and 1 if not.
 * 
 * This function executes an individual command based on the given command arguments array.
 * The first argument in the array is always assumed to be the command name and the rest its parameter arguments.
 */
int tshExecuteCmd(char** cmd, int in, int out) {
    int status;
    int pid;

    if (strcmp(cmd[0], "cd") == 0 && out == 1) {
        if (cmd[1] == NULL) {return 1;}
        if (chdir(cmd[1]) != 0) {return -1;}
    } 
    if (strcmp(cmd[0], "exit") == 0 && out == 1) {
        exit(EXIT_SUCCESS);
    }

    pid = fork();
    if (pid == 0) {
        if (in != 0) {
            dup2(in, 0);
            close (in);
        }
        if (out != 1) {
            dup2(out, 1);
            close(out);
        }
        execvp(cmd[0], cmd);
        exit(EXIT_FAILURE);
    } else if (pid < 0 ) {
        return -1;
    } else {
        do {waitpid(pid, &status, WUNTRACED);}
        while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 0;
}


/**
 * Function:  tshParseCmdLine
 * ----------------------
 * Parse through the array of command line tokens and execute individual commands.
 * 
 * @param cmdArgs The array of command line tokens.
 * @return Returns 0 if successful and 1 if not.
 * 
 * This function moves through the input array of command line tokens from left to right,
 * executing the individual commands and implements batching, piping, redirection logic.
 */
int tshParseCmdLine(char** cmdArgs) {
    int pipeFd[2];
    int nextInputFd = 0;
    int cmdArgStart = 0;
    int cmdArgEnd = 0;
    int cmdArgLength = 0;
    int cmdArgIndex = 0;
    char** cmd;
    char* fn;
    FILE* fp;

    if (strcmp(cmdArgs[0], "\0") == 0) {return 0;}
    toggleOutputPostprocessing();

    while (1) {
        if (strcmp(cmdArgs[cmdArgIndex], "\0") == 0) {
            cmdArgLength = cmdArgEnd - cmdArgStart;
            cmd = calloc(cmdArgLength, sizeof(char*));
            memcpy(cmd, cmdArgs + cmdArgStart, cmdArgLength*sizeof(char*));
            tshExecuteCmd(cmd, nextInputFd, 1);
            goto end;
        } else if (strcmp(cmdArgs[cmdArgIndex], "&&") == 0 && strcmp(cmdArgs[cmdArgIndex + 1], "\0") != 0) {
            cmdArgLength = cmdArgEnd - cmdArgStart;
            cmd = calloc(cmdArgLength, sizeof(char*));
            memcpy(cmd, cmdArgs + cmdArgStart, cmdArgLength*sizeof(char*));
            tshExecuteCmd(cmd, nextInputFd, 1);
            nextInputFd = 0;
            cmdArgLength = 0;
            cmdArgEnd++;
            cmdArgStart = cmdArgEnd;
        } else if (strcmp(cmdArgs[cmdArgIndex], "|") == 0 && strcmp(cmdArgs[cmdArgIndex + 1], "\0") != 0) {
            cmdArgLength = cmdArgEnd - cmdArgStart;
            cmd = calloc(cmdArgLength, sizeof(char*));
            memcpy(cmd, cmdArgs + cmdArgStart, cmdArgLength*sizeof(char*));
            pipe(pipeFd);
            tshExecuteCmd(cmd, nextInputFd, pipeFd[1]);
            close(pipeFd[1]);
            nextInputFd = pipeFd[0];
            cmdArgLength = 0;
            cmdArgEnd++;
            cmdArgStart = cmdArgEnd;
        } else if (strcmp(cmdArgs[cmdArgIndex], ">") == 0 && strcmp(cmdArgs[cmdArgIndex+1], "\0") != 0) {
            cmdArgLength = cmdArgEnd - cmdArgStart;
            cmd = calloc(cmdArgLength, sizeof(char*));
            memcpy(cmd, cmdArgs + cmdArgStart, cmdArgLength*sizeof(char*));
            fn = cmdArgs[cmdArgEnd + 1];
            fp = fopen(fn, "a+");
            tshExecuteCmd(cmd, nextInputFd, fileno(fp));
            fclose(fp);
            memmove(cmdArgs + 2, cmdArgs + cmdArgStart, cmdArgLength*sizeof(char*));
            cmdArgStart += 2;
            cmdArgEnd++;
        } else if (strcmp(cmdArgs[cmdArgIndex], "<") == 0) {
            continue;
        } else {
            cmdArgEnd++;
            cmdArgLength++;
        }
        cmdArgIndex++;
    }

    end:
        toggleOutputPostprocessing();
        return 0;
}

/**
 * Function:  tshTokenizeCmdLine
 * ----------------------
 * Split the input command line into an array of command line tokens.
 * 
 * @param cmdLine The command line.
 * @return An array of command line tokens.
 * 
 * This function moves through the input command line string from left to right,
 * storing the individual tokens of the command line into an array and returns it.
 */
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

char* getHostname(void) {
    long hostnameMax = sysconf(_SC_HOST_NAME_MAX); // get max hostname length at runtime if able
    if (hostnameMax == -1) hostnameMax = 255; // fallback to POSIX minimal guarantee
    size_t hostnameMaxLen = (size_t)hostnameMax+1;

    char* hostname = malloc(hostnameMaxLen);
    if (!hostname) return NULL;

    // if gethostname failed or returned an empty string use default "unknown"
    if (gethostname(hostname, hostnameMaxLen) != 0 || hostname[0] == '\0') {
        strcpy(hostname, "unknown");
    };
    hostname[hostnameMax-1] = '\0'; // ensure null termination

    return hostname;
}

char* getCwd(void) {

}

char* getUser(void) {

}

char* buildPrompt(void) {
            // try to get the hostname, otherwise use default "unknown"
        if (gethostname(hostname, sizeof(hostname)) == -1) {
            strncpy(hostname, "unknown", sizeof(hostname));
            hostname[sizeof(hostname)-1] = '\0';
        }

        // try to get the current working directory, otherwise use default "unknown"
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            strncpy(cwd, "unknown", sizeof(cwd));
            cwd[sizeof(cwd)-1] = '\0';
        }
        username = getlogin();
        if (gethostname(host, sizeof(host)) == -1 || getcwd(cwd, sizeof(cwd)) != NULL) {
            snprintf(prompt, sizeof(prompt), "%s@%s %s: ", username, host, cwd);
        } else {
            return -1;
        }
}

int main(int argc, char **argv) {
    char hostname[HOST_NAME_MAX+1];
    char cwd[PATH_MAX];
    char* username;
    char prompt[150];
    char* cmdLine;
    char** cmdArgs;

    do {
        prompt = buildPrompt();
        cmdLine = craftLine(prompt);
        cmdArgs = tshTokenizeCmdLine(cmdLine);
        tshParseCmdLine(cmdArgs);
        
        free(cmdLine);
        free(cmdArgs);
    } while (1);
}