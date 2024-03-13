extern char *builtin_str[];
extern int (*builtin_func[]) (char **);

extern int xsh_num_builtins();
extern int xsh_cd(char **args);
extern int xsh_help(char **args);
extern int xsh_exit(char **args);