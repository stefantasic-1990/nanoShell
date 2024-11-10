#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>



char* tshGetCommandLine(char* prompt, int prompt_l) {
    
}

char** tshParseCommandLine(char* line) {
    
}

int tshExecuteCommandLine(char** cmd_tokens) {
    
}

int main(int argc, char **argv) {
    char host[_POSIX_HOST_NAME_MAX]; // the machine hostname
    char cwd[PATH_MAX]; // the current working directory
    char prompt[50]; // the prompt string
    char* cmd_line; // the command line
    char** cmd_tokens; // the command line tokens
    int prompt_l; // the prompt length

    // command loop
    do {
        // get hostname and current working directory
        if (gethostname(host, sizeof(host)) == -1 || getcwd(cwd, sizeof(cwd)) == NULL) {return -1;}
        // save the prompt and get its length
        prompt_l = snprintf(prompt, 50, "%s@%s %s: ", getlogin(), host, strrchr(cwd, '/'));
        // get command line
        cmd_line = tshGetCommandLine(prompt, prompt_l);
        // parse command line
        cmd_tokens = tshParseCommandLine(cmd_line);
        // execute command line
        tshExecuteCommandLine(cmd_tokens);
        // free dynamic memory
        free(cmd_line);
        free(cmd_tokens);
    } while (1);
}