#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "builtin.h"

#define XSH_LINE_BUFFERSIZE 1024
#define XSH_CMD_HISTORY_BUFFERSIZE 10
#define XSH_TOKEN_BUFFERSIZE 64
#define XSH_TOKEN_DELIMITERS " \t\r\n\a"

int xsh_launch(char **args)
{
    pid_t pid, wpid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Child process enters here
        if (execvp(args[0], args) == -1) {
            perror("xsh");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error forking the process
        perror("xsh");
    } else {
        // Parent process enters here
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int xsh_execute(char **args)
{
    int i;

    if (args[0] == NULL) {
        // An empty command was entered.
        return 1;
    }

    // Iterate over builtins and execute if one was called
    for (i = 0; i < xsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    // Execute called program
    return xsh_launch(args);
}

char **xsh_tokenize_line(char *line)
{
    int buffersize = XSH_TOKEN_BUFFERSIZE;
    int position = 0;
    char **tokens = malloc(buffersize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "xsh: memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    // Initialize tokenizer and get first token
    token = strtok(line, XSH_TOKEN_DELIMITERS);

    while (token != NULL) {
        tokens[position] = token;
        position++;

    // If we have exceeded the buffer, reallocate with more memory
    if (position >= buffersize) {
        buffersize += XSH_TOKEN_BUFFERSIZE;
        tokens = realloc(tokens, buffersize * sizeof(char*));
        if (!tokens) {
            fprintf(stderr, "xsh: memory allocation error\n");
            exit(EXIT_FAILURE);
        }
    }
        // Get next token
        token = strtok(NULL, XSH_TOKEN_DELIMITERS);
    }

    tokens[position] = NULL;
    return tokens;
}

char *xsh_read_line(void)
{
    int buffersize = XSH_LINE_BUFFERSIZE;
    int position = 0;
    int input_char;
    char *buffer = malloc(sizeof(char) * buffersize);

    // check if memory allocation failed
    if (!buffer) {
        fprintf(stderr, "xsh: memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Read a character
        input_char = getchar();

        // If up button is pressed store last command in history into buffer
        //buffer = cmdhis[cmdhis_position];

        // If down button is pressed store next command in history into buffer

        // If we hit EOF, replace it with a null character, store into history and return
        if (input_char == EOF || input_char == '\n') {
            buffer[position] = '\0';
            // cmdhis[cmdhis_position] = buffer;
            // cmdhis_position++;
            return buffer;
        // Else add input character to string
        } else {
            buffer[position] = input_char;
        }
        position++;

        // If we have exceeded the buffer, reallocate with more memory
        if (position >= buffersize) {
            buffersize += XSH_LINE_BUFFERSIZE;
            buffer = realloc(buffer, buffersize);
            if (!buffer) {
                fprintf(stderr, "xsh: memory allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int main (int argc, char **argv)
{
    char *line;
    char **args;
    int status;

    // Initialize command history variables
    int cmdhis_buffersize = XSH_CMD_HISTORY_BUFFERSIZE;
    int cmdhis_position = 0;
    char **cmdhis = malloc(cmdhis_buffersize * sizeof(char*));

    // Load config files

    // Run command loop
    do {
      printf(":: ");
      line = xsh_read_line();
      args = xsh_tokenize_line(line);
      status = xsh_execute(args);

      free(line);
      free(args);
    } while (status);

    // perform cleanup step
}