#ifndef NOTEPADMM_H
#define NOTEPADMM_H
#include <stdio.h>

void add_cmd(char *, int);
void writeCmds(void);
void getWinSize(void);
void initEditor(char *);
void exitRawMode(void);
void enableRawMode(void);
void appendRow(void);
void shiftRowsDown(int);
void cursor_move_cmd(void);
void incrementCursor(int, int, int, int);
void moveCursor(char, char *);
void addPrintableChar(char);
void backspacePrintableChar(void);
void deletePrintableChar(void);
void sortEscapes(char);
void addRow(void);
void sortKeypress(char);
char processKeypress(void);
void clearScreen(void);
void writeScreen(void);
void removeRow(int);
void free_all_rows(void);
void readFile(char *);
void saveFile(void);
void writeFile(char *);
void statusWrite(char *);
long getFileSize(FILE *);
void scrollRight(void);
void scrollLeft(void);
void searchHighlight(char **);
void searchPrompt(void);
void highlightSyntax(char **, int);
char** readTextArray(char *);
int inlineCommentHighlight(char **);
void multilineCommentHighlight(char **);
int* markMultilineRows(void);
void commentEntireRow(char **);
int checkKeywordHighlight(char *, char *, int);

#endif 