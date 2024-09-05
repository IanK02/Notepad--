#ifndef NOTEPADMM_H
#define NOTEPADMM_H

void add_cmd(char *, int);
void writeCmds(void);
void getWinSize(void);
void initEditor(void);
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
void readFile(const char *);
void basicRead(const char *);
void saveFile(void);

#endif 