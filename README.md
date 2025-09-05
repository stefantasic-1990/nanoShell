# nanoShell

## To-do

- [ ] Non-interactive mode: only render prompts and enable `craftLine` when `isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)`. Fall back to simple command execution for non-interactive/programmatic input.

## Features

- [x] Single-line editing mode
- [x] Command parsing with argument quoting and quote escaping
- [x] Command batching, piping, redirection
- [x] Command history
