#include <stdio.h>
#include <unistd.h>

int xsh_num_builtins();
int xsh_cd(char **args);
int xsh_help(char **args);
int xsh_exit(char **args);

/*
    List of builtin commands and list of corresponding function pointers
*/
char *builtin_str[] = {
    "cd",
    "help",
    "exit"
};

int (*builtin_func[]) (char **) = {
    &xsh_cd,
    &xsh_help,
    &xsh_exit
};


// Function to return the total number of builtin functions
int xsh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

/*
    Builtin function implementations
*/
int xsh_cd(char **args)
{
    if (args[1] == NULL) {
        fprintf(stderr, "xsh: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("xsh");
        }
    }
    
    return 1;
}

int xsh_help(char **args)
{
    int i;
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");

    for (i = 0; i < xsh_num_builtins(); i++) {
        printf("  %s\n", builtin_str[i]);
    }

    printf("Use the man command for information on other programs.\n");
    
    return 1;
}

int xsh_exit(char **args)
{
    return 0;
}