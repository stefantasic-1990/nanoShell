#include "craftLine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <termios.h>
#include <sys/ioctl.h>

// saved terminal settings for restoring raw-mode changes.
struct termios initial_terminal_settings;

// simple on-disk command history maintained as a txt file.
static char historyFilePath[] = "./craftLineHistory.txt";

/* fixed-size command buffer:
 * - historyBuffer[0] is a scratch slot (for current line).
 * - historyBuffer[1] holds the most recent persisted command.
 * - historyBuffer[historyBufferSize-1] holds the oldest.
 * - new commands shift the buffer down, discarding the oldest.
 */
static int historyBufferSize = 11;
static char** historyBuffer = NULL;

/**
 * enableRawTerminal
 * -----------------
 * put the terminal into raw mode.
 *
 * in raw mode:
 *   - input is delivered byte-by-byte without line buffering; the kernel does
 *     not echo characters, generate signals for control keys, or perform I/O
 *     translations.
 *
 * returns:
 *   0 on success; -1 if STDIN is not a TTY or attributes cannot be changed.
 */
int enableRawTerminal() {
    struct termios modified_terminal_settings;
    if (isatty(STDIN_FILENO)) {
        // save current settings for later restore.
        tcgetattr(STDIN_FILENO, &initial_terminal_settings);

        modified_terminal_settings = initial_terminal_settings;

        // configure terminal input flags; turn off special handling of input.
        // no break-to-signal, CR→NL mapping, parity check, stripping, or XON/XOFF flow control.
        modified_terminal_settings.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

        // configure output flags; no post-processing, send output bytes as-is, no NL→CRNL conversion, etc.
        modified_terminal_settings.c_oflag &= ~(OPOST);

        // configure control flags; use 8-bit characters.
        modified_terminal_settings.c_cflag |= CS8;

        // configure local flags; turn on raw input mode.
        // no echo, no canonical line buffering/editing, no extended functions, no signal generation.
        modified_terminal_settings.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

        // configure read buffer settings: return after 1 byte, no timeout
        modified_terminal_settings.c_cc[VMIN]  = 1;
        modified_terminal_settings.c_cc[VTIME] = 0;

        // apply modified settings and flush pending input.
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &modified_terminal_settings);
    } else {
        return -1;
    }
    return 0;
}

/**
 * disableRawTerminal
 * ------------------
 * restore the terminal connected to STDIN to its original settings.
 */
void disableRawTerminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &initial_terminal_settings);
}

/**
 * restoreHistory
 * --------------
 * load command history from historyFilePath into historyBuffer[1..N-1].
 * historyBuffer[0] is reserved as a scratch slot for the current line.
 *
 * Notes:
 *   - command lines are strdup'ed; caller must manage the memory used for stored lines.
 *   - will silently ignore a missing file assuming no saved history.
 */
void restoreHistory() {
    FILE* historyFile;
    historyFile = fopen(historyFilePath, "r");
    if (historyFile != NULL) {
        for (int i = 1; i < historyBufferSize; i++) {
            char* line = NULL;
            size_t lineLength = 0;
            // getline() allocates if *line is NULL; POSIX.1-2008.
            getline(&line, &lineLength, historyFile);
            if (strcmp(line, "\0") != 0) {
                // trim trailing '\n'.
                line[strlen(line)-1] = '\0';
                historyBuffer[i] = strdup(line);
            }
            free(line);
        }
        fclose(historyFile);
    }
}

/**
 * saveHistory
 * -----------
 * write historyBuffer[1..N-1] to historyFilePath (one line for each index).
 * truncates the file before writing; rewrites the file using the current buffer state.
 */
void saveHistory() {
    FILE* historyFile;
    historyFile = fopen(historyFilePath, "a+");
    /* truncate to start fresh. */
    ftruncate(fileno(historyFile), 0);
    for (int i = 1; i < historyBufferSize; i++) {
        if (historyBuffer[i] != NULL) {
            fprintf(historyFile, "%s\n", historyBuffer[i]);
            fflush(historyFile);
        }
    }
    fclose(historyFile);
}

/**
 * addToHistory
 * ------------
 * Shift history down and insert the new line at historyBuffer[1].
 * historyBuffer[0] remains the current-line scratch slot.
 *
 * Note:
 *   The memmove size arithmetic is delicate; ensure it matches buffer layout.
 */
void addToHistory(char* lineBuffer) {
    // discard last item
    free(historyBuffer[historyBufferSize - 1]);
    // shift buffer down [1..N-2] → [2..N-1].
    memmove(historyBuffer + 2,
            historyBuffer + 1,
            (historyBufferSize - 2) * sizeof(char*));
    historyBuffer[1] = strdup(lineBuffer);
}

/**
 * craftLine
 * ---------
 * read an editable command line with minimal in-terminal editing and history.
 *
 * behavior:
 *   - puts TTY into raw mode when called; restores TTY to original settings on exit.
 *   - handles basic cursor movement, backspace, and ↑/↓ history.
 *   - renders prompt + current line buffer each loop; uses ANSI escapes for cursor control.
 *   - returns a heap-allocated C string (caller frees).
 *
 * @param prompt  prompt string to display (printed on each line refresh).
 * @return        newly allocated command line string; NULL on raw-mode failure.
 */
char* craftLine(char* prompt) {
    int promptLength = strlen(prompt);
    int lineHistoryPosition = 0;

    // logical line state.
    int lineLength = 0;
    int lineCursorPosition = 0;   // Cursor index in lineBuffer.
    int lineDisplayOffset = 0;    // horizontal scroll offset; line is printed on screen starting here.
    int lineDisplayLength = 0;    // visible chars available on screen after prompt is printed; depends on window size.

    // editable line buffer (grows as needed).
    char* lineBuffer;
    int lineBufferMaxSize = 100;
    lineBuffer = calloc(lineBufferMaxSize, sizeof(char));
    lineBuffer[0] = '\0';

    // determine terminal window width to compute visible line region.
    struct winsize ws;
    int terminalWindowWidth;
    if (ioctl(1, TIOCGWINSZ, &ws) == -1) {terminalWindowWidth = 80;} else {terminalWindowWidth = ws.ws_col - 1;}
    lineDisplayLength = terminalWindowWidth - promptLength;

    // allocate history buffer on first use.
    if (historyBuffer == NULL) {
        historyBuffer = calloc(historyBufferSize, sizeof(char*));
    }

    // load persisted history and switch to raw mode.
    restoreHistory();
    if (enableRawTerminal() == -1) {
        return NULL;
    }

    do {
        // repaint: move to col 0, print prompt + visible slice of line buffer, clear to end of window (removes leftover chars if needed).
        write(STDOUT_FILENO, "\x1b[0G", strlen("\x1b[0G"));
        write(STDOUT_FILENO, prompt, promptLength);
        write(STDOUT_FILENO, (lineBuffer + lineDisplayOffset), (lineBufferMaxSize < lineDisplayLength) ? lineBufferMaxSize : lineDisplayLength);
        write(STDOUT_FILENO, "\x1b[0K", strlen("\x1b[0K"));

        // place cursor after prompt at logical position.
        char cursorEscCode[10];
        snprintf(cursorEscCode, sizeof(cursorEscCode), "\x1b[%iG", promptLength + 1 + lineCursorPosition - lineDisplayOffset);
        write(STDOUT_FILENO, cursorEscCode, strlen(cursorEscCode));

        // read a single byte in raw mode.
        char c;
        read(STDIN_FILENO, &c, 1);

        // handle input
        switch(c) {
            case 13: // enter
                goto returnLine;

            case 8:  // ctrl+h
            case 127: // Backspace
                if (lineCursorPosition > 0) {
                    // delete chararacter before cursor.
                    memmove(lineBuffer + (lineCursorPosition - 1), lineBuffer + lineCursorPosition, lineLength - lineCursorPosition);
                    lineCursorPosition--;
                    lineLength--;
                    lineBuffer[lineLength] = '\0';
                }
                break;

            case 3: // ctrl+c: restore TTY to original settings then exit.
                disableRawTerminal();
                write(STDOUT_FILENO, "\x0a", sizeof("\x0a"));
                exit(EXIT_SUCCESS);

            case 4: // ctrl+d:
                break;

            case 20: // ctrl+t:
                break;

            case 16: // ctrl+x:
                break;

            case 14: // ctrl+n:
                break;

            case 11: // ctrl+k:
                break;

            case 1:  // ctrl+a:
                break;

            case 5:  // ctrl+e:
                break;

            case 12: // ctrl+l:
                break;

            case 23: // ctrl+w:
                break;

            case 21: // ctrl+u: clear entire input.
                free(lineBuffer);
                lineBuffer = calloc(lineBufferMaxSize, sizeof(char));
                lineCursorPosition = 0;
                lineDisplayOffset = 0;
                lineLength = 0;
                break;

            case 27: { // esc: parse simple CSI arrow keys.
                char escapeSequence[3];
                // read next two bytes; ignore errors.
                if (read(STDIN_FILENO, escapeSequence, 1) == -1) {break;}
                if (read(STDIN_FILENO, escapeSequence+1, 1) == -1) {break;}
                if (escapeSequence[0] == '[') {
                    switch(escapeSequence[1]) {
                        case 'C': // right arrow: move cursor to right
                            if (lineCursorPosition < lineLength) {lineCursorPosition++;}
                            if ((lineCursorPosition - lineDisplayOffset) > lineDisplayLength) {lineDisplayOffset++;}
                            break;
                        case 'D': // left arrow: move cursor to left
                            if (lineCursorPosition > 0) {lineCursorPosition--;}
                            if ((lineCursorPosition - lineDisplayOffset) < 0) {lineDisplayOffset--;}
                            break;
                        case 'A': // up arrow: move to previous history entry.
                            if ((historyBuffer[lineHistoryPosition + 1] != NULL) && lineHistoryPosition < (historyBufferSize - 1)) {
                                if (lineHistoryPosition == 0) {historyBuffer[0] = strdup(lineBuffer);} /* Stash current line. */
                                lineHistoryPosition++;
                                lineLength = strlen(historyBuffer[lineHistoryPosition]);
                                lineBufferMaxSize = strlen(historyBuffer[lineHistoryPosition]) + 1;
                                lineCursorPosition = lineLength;
                                lineDisplayOffset = (lineLength < lineDisplayLength) ? 0 : lineLength - lineDisplayLength;
                                free(lineBuffer);
                                lineBuffer = calloc(lineBufferMaxSize, sizeof(char));
                                strcpy(lineBuffer, historyBuffer[lineHistoryPosition]);
                            }
                            break;
                        case 'B': // down arrow: move to next history entry, or currently buffered line.
                            if (lineHistoryPosition > 0) {
                                lineHistoryPosition--;
                                lineLength = strlen(historyBuffer[lineHistoryPosition]);
                                lineBufferMaxSize = strlen(historyBuffer[lineHistoryPosition]) + 1;
                                lineCursorPosition = lineLength;
                                lineDisplayOffset = (lineLength < lineDisplayLength) ? 0 : lineLength - lineDisplayLength;
                                free(lineBuffer);
                                lineBuffer = calloc(lineBufferMaxSize, sizeof(char));
                                strcpy(lineBuffer, historyBuffer[lineHistoryPosition]);
                            }
                            break;
                    }
                }
                break;
            }

            default: // insert character at cursor.
                memmove(lineBuffer + lineCursorPosition + 1, lineBuffer + lineCursorPosition, lineLength - lineCursorPosition);
                lineBuffer[lineCursorPosition] = c;
                lineCursorPosition++;
                lineLength++;
                lineBuffer[lineLength] = '\0';
                if ((lineCursorPosition - lineDisplayOffset) > lineDisplayLength) {lineDisplayOffset++;}
                break;
        }

        // grow buffer if needed.
        if (lineLength + 1 >= lineBufferMaxSize) {
            lineBufferMaxSize += lineBufferMaxSize;
            lineBuffer = realloc(lineBuffer, lineBufferMaxSize);
            if (!lineBuffer) {return NULL;}
        }
    } while (true);

    // add buffered line to history, restore TTY to original settings, print newline, return buffered line. */
    returnLine:
        free(historyBuffer[0]);
        historyBuffer[0] = NULL;
        if (lineBuffer[0] != '\0') {
            addToHistory(lineBuffer);
        }
        saveHistory();
        disableRawTerminal();
        write(STDOUT_FILENO, "\x0a", strlen("\x0a"));
        return lineBuffer;
}