#include "craftLine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <termios.h>
#include <sys/ioctl.h>

struct termios initial_terminal_settings;
static char historyFilePath[] = "./craftlinehistory.txt";
static int historyBufferSize = 11;
static char** historyBuffer = NULL;

int enableRawTerminal() {
    struct termios modified_terminal_settings;
    if (isatty(STDIN_FILENO)) { 
        tcgetattr(STDIN_FILENO, &initial_terminal_settings);

        modified_terminal_settings = initial_terminal_settings;
        modified_terminal_settings.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        modified_terminal_settings.c_oflag &= ~(OPOST);
        modified_terminal_settings.c_cflag |= (CS8);
        modified_terminal_settings.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        modified_terminal_settings.c_cc[VMIN] = 1; 
        modified_terminal_settings.c_cc[VTIME] = 0;

        tcsetattr(STDIN_FILENO,TCSAFLUSH,&modified_terminal_settings);
    } else {
        return -1;
    } 
    return 0;
}

void disableRawTerminal() {
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&initial_terminal_settings);
}

void restoreHistory() {
    FILE* historyFile;
    historyFile = fopen(historyFilePath, "r+");
    if (historyFile != NULL) {
        for (int i = 1; i < historyBufferSize; i++) {
            char* line = NULL;
            size_t lineLength = 0;
            getline(&line, &lineLength, historyFile);
            if (strcmp(line, "\0") != 0) {
                line[strlen(line)-1] = '\0';
                historyBuffer[i] = strdup(line);
            }
        }
        fclose(historyFile);
    }
}

void saveHistory() {
    FILE* historyFile;
    historyFile = fopen(historyFilePath, "a+");
    ftruncate(fileno(historyFile), 0);
    for (int i = 1; i < historyBufferSize; i++) {
        if (historyBuffer[i] != NULL) {
            fprintf(historyFile, "%s\n", historyBuffer[i]);
            fflush(historyFile);
        }
    }
    fclose(historyFile);
}

void addToHistory(char* lineBuffer) {
    free(historyBuffer[historyBufferSize - 1]);
    memmove(historyBuffer + 2, historyBuffer + 1, historyBufferSize*sizeof(char*)*2 - sizeof(char*)*2);
    historyBuffer[1] = strdup(lineBuffer);
}

char* craftLine(char* prompt) {
    int promptLength = strlen(prompt);
    int lineHistoryPosition = 0;

    int lineLength = 0;
    int lineCursorPosition = 0;
    int lineDisplayOffset = 0;
    int lineDisplayLength = 0;

    char* lineBuffer;
    int lineBufferMaxSize = 100;
    lineBuffer = calloc(lineBufferMaxSize, sizeof(char));
    lineBuffer[0] = '\0';

    struct winsize ws;
    int terminalWindowWidth;
    if (ioctl(1, TIOCGWINSZ, &ws) == -1) {terminalWindowWidth = 80;} else {terminalWindowWidth = ws.ws_col - 1;}
    lineDisplayLength = terminalWindowWidth - promptLength;

    if (historyBuffer == NULL) {
        historyBuffer = calloc(historyBufferSize, sizeof(char*));
    }

    restoreHistory();
    if (enableRawTerminal() == -1) {
        return NULL;
    }

    do {
        write(STDOUT_FILENO, "\x1b[0G", strlen("\x1b[0G"));
        write(STDOUT_FILENO, prompt, promptLength);
        write(STDOUT_FILENO, (lineBuffer + lineDisplayOffset), (lineBufferMaxSize < lineDisplayLength) ? lineBufferMaxSize : lineDisplayLength);
        write(STDOUT_FILENO, "\x1b[0K", strlen("\x1b[0K"));

        char cursorEscCode[10];
        snprintf(cursorEscCode, sizeof(cursorEscCode), "\x1b[%iG", promptLength + 1 + lineCursorPosition - lineDisplayOffset);
        write(STDOUT_FILENO, cursorEscCode, strlen(cursorEscCode));

        char c;
        read(STDIN_FILENO, &c, 1);

        switch(c) {
            case 13: // enter
                goto returnLine;
            case 8: // ctrl+h
            case 127: // backspace
                if (lineCursorPosition > 0) {
                    memmove(lineBuffer+(lineCursorPosition-1), lineBuffer+lineCursorPosition , lineLength - lineCursorPosition);
                    lineCursorPosition--;
                    lineLength--;
                    lineBuffer[lineLength] = '\0';
                }
                break;
            case 3: // ctrl+c
                disableRawTerminal();
                write(STDOUT_FILENO, "\x0a", sizeof("\x0a"));
                exit(EXIT_SUCCESS);
            case 4: // ctrl+d
                break;
            case 20: // ctrl+t
                break; 
            case 16: // ctrl+x
                break; 
            case 14: // ctrl+n
                break;
            case 11: // ctrl+k
                break;
            case 1: // ctrl+a
                break;
            case 5: // ctrl+e
                break;
            case 12: // ctrl+l
                break;
            case 23: // ctrl+w
                break;
            case 21: // ctrl+u / clear input
                free(lineBuffer);
                lineBuffer = calloc(lineBufferMaxSize, sizeof(char));
                lineCursorPosition = 0;
                lineDisplayOffset = 0;
                lineLength = 0;
                break;
            case 27: { // escape character
                char escapeSequence[3];
                // read-in the next two characters
                if (read(STDIN_FILENO, escapeSequence, 1) == -1) {break;}
                if (read(STDIN_FILENO, escapeSequence+1, 1) == -1) {break;}
                if (escapeSequence[0] == '[') {
                    switch(escapeSequence[1]) {
                        case 'C': // right arrow key
                            if (lineCursorPosition < lineLength) {lineCursorPosition++;}
                            if ((lineCursorPosition - lineDisplayOffset) > lineDisplayLength) {lineDisplayOffset++;}
                            break;
                        case 'D': // left arrow key
                            if (lineCursorPosition > 0) {lineCursorPosition--;}
                            if ((lineCursorPosition - lineDisplayOffset) < 0) {lineDisplayOffset--;}
                            break;
                        case 'A': // up arrow key
                            // get previous record in history
                            if ((historyBuffer[lineHistoryPosition + 1] != NULL) && lineHistoryPosition < (historyBufferSize - 1)) {
                                if (lineHistoryPosition == 0) {historyBuffer[0] = strdup(lineBuffer);}
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
                        case 'B': // down arrow key
                            // get next record in history
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
            default: // store character in line buffer
                memmove(lineBuffer+lineCursorPosition+1, lineBuffer+lineCursorPosition, lineLength-lineCursorPosition);
                lineBuffer[lineCursorPosition] = c;
                lineCursorPosition++;
                lineLength++;
                lineBuffer[lineLength] = '\0';
                if ((lineCursorPosition - lineDisplayOffset) > lineDisplayLength) {lineDisplayOffset++;}
                break;
        }
        // allocate more space for buff if required
        if (lineLength + 1 >= lineBufferMaxSize) {
            lineBufferMaxSize += lineBufferMaxSize;
            lineBuffer = realloc(lineBuffer, lineBufferMaxSize);
            if (!lineBuffer) {return NULL;}
        }
    } while (true);

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