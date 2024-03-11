extern char *builtin_str[];
extern int (*builtin_func[]) (char **);

int xsh_num_builtins();
int xsh_cd(char **args);
int xsh_help(char **args);
int xsh_exit(char **args);