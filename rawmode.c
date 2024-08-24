/***
 * IMPORTANT NOTES
 * The system of the array of rows is 0 indexed, however the cursor is 1 indexed,
 * sort of like a graph from math, the left most bottom BOX is at 1,1, imagine a 2d grid
 * where valid points aren't on the intersections of the unit(integer) lines but 
 * rather the boxes created by them
 */

#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <string.h>

/*** Defines  ***/
#define CTRL_KEY(k) ((k) & 0x1f) //used to check if ctrl + some character was pressed

/*** Structures ***/
struct cmd_buf{
  char *cmds;
  int len;
};

typedef struct row {
  /***
   * This represents a row of text, it will contain the information listed below
   * 1. char *chars - A string of the actual text of the row
   * 2. int length - Length of the row
   */
  char *chars;
  int length;
} row;

struct editor {
  /***
   * This will contain all information about the editor, listed below
   * 1. Dynamic array of rows of text
   * 2. Cursor(its position)
   * 3. Number of rows
   * 4. termios(terminal configuration)
   * 5. winsize(size of the window)
   */
  row *rows; //dynamic arrow of rows of text
  int Cx; //cursor x position
  int Cy; //cursor y position
  int numrows;
  struct termios termios_o; //terminal configuration struct(from termios.h)
  struct winsize w;
};

struct editor E; //The global editor struct
struct cmd_buf cbuf; //The global command buffer

/*** Functions ***/
void createNewRow(void);

void add_cmd(char *cmd){
  cbuf.len += strlen(cmd) + 1;
  cbuf.cmds = realloc(cbuf.cmds, cbuf.len); //reallocate cmds to make space for thew new command
  if(cbuf.cmds == NULL){ //check if realloc was successful
    printf("Memory allocation failed\n");
    return;
  }
  snprintf(cbuf.cmds, strlen(cmd) + 1, "%s", cmd); //add the cmd to cmds
}

void writeCmds(void){
  write(STDOUT_FILENO, cbuf.cmds, cbuf.len);
  cbuf.len = 0; //set len back to 0
  free(cbuf.cmds); //free memory allocated to cmds
}

void getWinSize(void){
    ioctl(0, TIOCGWINSZ, &E.w);
}

void initEditor(void){
  //initialize the global editor object's values as well as clear screen and set cursor at starting position
  //E.rows = NULL; //initialize rows to null as no text is present yet
  createNewRow(); //we create the first row, it has no chars, E.numrows doesn't need to be initialized anymore
  E.Cx = 1; //initialize cursor position to (1,1) which is the top left of the screen
  E.Cy = 1;

  //E.numrows = 0; //initialize numrows to 0 as no rows of text are present yet
  //we don't have to initialize termios_o as enableRawMode takes care of setting its attributes
  getWinSize(); //this call to get winsize takes cares of initializing winsize w to have the correct values

  write(STDOUT_FILENO, "\x1b[2J", 4); //clear the screen
  write(STDOUT_FILENO, "\x1b[H", 3); //actually move the cursor to the top left of the screen

  cbuf.cmds = NULL; //initialize the command buffers commands to NULL and length to 0
  cbuf.len = 0;
}

void exitRawMode(void){
  //Called on program exit to disable raw mode and return terminal to normal
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.termios_o);
}

void enableRawMode(void){
  //This function will enable raw mode in the terminal, as well as disable raw mode 
  //on the program's exit
  tcgetattr(STDIN_FILENO, &E.termios_o);
  struct termios termios_r = E.termios_o;

  atexit(exitRawMode);
  
  termios_r.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                  | INLCR | IGNCR | ICRNL | IXON);
  termios_r.c_oflag &= ~OPOST;
  termios_r.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  termios_r.c_cflag &= ~(CSIZE | PARENB);
  termios_r.c_cflag |= CS8;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios_r);
}

void createNewRow(void){ //create a new row of text
  E.numrows++; //increment number of rows

  E.rows = realloc(E.rows, sizeof(row) * E.numrows); //reallocate the memory of rows to accomodate for the new row
  if (E.rows == NULL) {
      printf("Memory allocation failed\n");
      exit(1);
  }
  E.rows[E.numrows - 1].chars = NULL; //initialize the new row's chars to NULL and length to 0
  E.rows[E.numrows - 1].length = 0;
}

void shiftRowsDown(int index){ //shift all rows up to index down 1
  for(int i = E.numrows - 1; i > index; i--){
    E.rows[i] = E.rows[i-1];
  }
}

void cursor_move_cmd(void){ //move cursor to location specified by global cursor
    int buf_size = snprintf(NULL, 0, "\x1b[%d;%dH", E.Cy, E.Cx) + 1;
    char *buf = malloc(buf_size);
    snprintf(buf, buf_size, "\x1b[%d;%dH", E.Cy, E.Cx);
    write(STDOUT_FILENO, buf, buf_size - 1);
    //add_cmd(buf);
    free(buf);
}

void incrementCursor(int up, int down, int left, int right){  
    if((up^down^left^right) != 1 || up+down+left+right != 1){
        printf("Only one parameter can be 1, all others must be 0");
        return;
    }
    if(up && !down && !left && !right){ //up arrow
        if(E.Cy > 1){
            E.Cy--; //only deccrement if C.y is > 1, note that going up decrements C.y as the top left of the screen is 1,1
            //keep moving the cursor left until it hits a printable character or 
            //beginning of the row
            while(E.Cx > E.rows[E.Cy-1].length + 1){
              E.Cx--; //snap cursor to end of row
            }
        }
    } else if(!up && down && !left && !right){ //down arrow
        if(E.Cy <= E.w.ws_row){
            E.Cy++; //only increment C.y if y is < rows limit, the row limit is positive and represents the lowest row of the screen
            //keep moving the cursor left until it hits a printable character or 
            //beginning of the row
            while(E.Cx > E.rows[E.Cy-1].length + 1){
              E.Cx--; //snap cursor to end of row
            }
        }
    } else if(!up && !down && left && !right){ //left arrow
        if(E.Cx > 1){
            E.Cx--; //only increment if C.x is > 1, we have to use 1 because the cursor actually controls the character behind it
        }
    } else if(!up && !down && !left && right){ //right arrow
        if(E.Cx < E.w.ws_col){
            E.Cx++; //only increment if C.x < columns limit, the column limit is positive and represents the farthest right column on screen
        }
    }
}   

void moveCursor(char c, char *buf){
    //char *buf = malloc(3); //three character buffer to store all three characters of the arrow key commands
    //buf[0] = c;
    //read(STDIN_FILENO, buf + 1, 1); //read next byte of input into buf
    //read(STDIN_FILENO, buf + 2, 1); //read one more byte of input into buf
    //buf will now contain c at buf[0], the byte(from user input) after c at buf[1], and the one after that at buf[2]

    switch (buf[2])
    {
    case 'A': //up arrow
        incrementCursor(1,0,0,0); //increment the cursor's coordinates(stored in the global editor E)

        //cursor_move_cmd();
        break;
    case 'B': //down arrow
        if(E.Cy <= E.numrows - 1) incrementCursor(0,1,0,0); //limit cursor to one above the lowest row

        //cursor_move_cmd();
        break;
    case 'C': //right arrow
        if(E.Cx <= E.rows[E.Cy-1].length) incrementCursor(0,0,0,1); //limit cursor at only one space further right than the text

        //cursor_move_cmd();
        break;
    case 'D': //left arrow
        incrementCursor(0,0,1,0);

        //cursor_move_cmd();
        break;
    default:
        break;
    }
    free(buf); //free the memory allocated to buf
    //free(cmd); //free the memory allocated to cmd 
}

void shiftLineCharsR(int index, row *row){ //shift all characters in a row to the right by one, at index two characters will be identical,
                                           //they will be copies of the character at that index in the original string
    for (int i = row->length - 1; i > index; i--) {
        row->chars[i] = row->chars[i - 1];
    }
}

void shiftLineCharsL(int index, row *row){ //shift all characters in a row to the left by one, last two characters will be 
                                           //copies of the last character of the original string
    for (int i = index; i < row->length - 1; i++) {
        row->chars[i] = row->chars[i + 1];
    }
}

void addPrintableChar(char c){
  E.rows[E.Cy-1].length++;//increment the row's length
  E.rows[E.Cy-1].chars = realloc(E.rows[E.Cy-1].chars, E.rows[E.Cy-1].length); //reallocate the memory of the row to accomodate the new character
  
  shiftLineCharsR(E.Cx-1, E.rows + E.Cy - 1); //shift all the characters(up to the character BEHIND the cursor) one to the right
  if(E.Cx <= E.rows[E.Cy-1].length && E.Cy >= 1) E.rows[E.Cy-1].chars[E.Cx-1] = c; //set the char of the character BEHIND the cursor to c

  E.Cx++; //move the cursor one to the right to account for the new character
}

void backspacePrintableChar(void){
    if(E.Cx > 1){
        shiftLineCharsL(E.Cx-2, E.rows + E.Cy - 1); //shift all the characters(up to the character BEHIND the cursor) one to the left
        E.rows[E.Cy-1].chars[E.rows[E.Cy-1].length - 1] = '\0'; //delete last character in the row
        E.rows[E.Cy-1].length--; //decrement the row's length
        E.rows[E.Cy-1].chars = realloc(E.rows[E.Cy-1].chars, E.rows[E.Cy-1].length); //reallocate the memory of the row to be smaller
        E.Cx--; //move the cursor one space left
    }
}

void deletePrintableChar(void){
    if(E.Cx > 0){
        shiftLineCharsL(E.Cx-1, E.rows + E.Cy-1); //shift all the characters(up to the character on the cursor) one to the left
        E.rows[E.Cy-1].chars[E.rows[E.Cy-1].length - 1] = '\0'; //delete last character in the row
        E.rows[E.Cy-1].length--; //decrement the row's length
        E.rows[E.Cy-1].chars = realloc(E.rows[E.Cy-1].chars, E.rows[E.Cy-1].length); //reallocate the memory of the row to be smaller
    }
}

void sortEscapes(char c){
    char *buf = malloc(4); //three character buffer to store all three characters of the arrow key commands
    buf[0] = c;
    read(STDIN_FILENO, buf + 1, 1); //read next byte of input into buf
    read(STDIN_FILENO, buf + 2, 1); //read next byte of input into buf
    if(buf[2] == '3'){ //delete key was pressed
        read(STDIN_FILENO, buf + 3, 1); //read in the last tilde of the delete sequence ("\x1b[3~")
        if(E.rows[E.Cy-1].chars != NULL){ //check if the row is empty
            int current_char = (int)E.rows[E.Cy-1].chars[E.Cx - 1];
            if(current_char >= 32 && current_char < 127){ //check if the current character the cursor is on is a printable character
                deletePrintableChar();
            }
        }
        free(buf); //free memory allocated to buf
    } else { //arrow key was pressed 
        //read(STDIN_FILENO, buf + 2, 1); //read one more byte of input into buf
        moveCursor(c, buf); //moveCursor will only increment C's position
    }
}

void addRow(void){ //add a new row of text in response to the enter key being pressed
  if(E.Cx-1 == E.rows[E.Cy-1].length && E.Cy == E.numrows){ //check if cursor is at the end of the row it's on and if current row is 
      createNewRow();                                       //the bottom row
      incrementCursor(0,1,0,0); //move cursor down
  }else if (E.Cx-1 == E.rows[E.Cy-1].length){ //cursor at end of row but not on bottom row
      createNewRow();
      shiftRowsDown(E.Cy-1);

      E.rows[E.Cy].chars = NULL; //initialize the new row we just made room for to empty
      E.rows[E.Cy].length = 0;

      incrementCursor(0,1,0,0); //move cursor down
  }else if(E.Cx-1 != E.rows[E.Cy-1].length && E.Cx-1 != 0){ 
          //cursor not at end of row or beginning of row 
    createNewRow();
    int copy_length = E.rows[E.Cy-1].length - (E.Cx-1); //the length of how much of the string to move to the next row down
    char *copy_buf;
    copy_buf = (char *)malloc(copy_length); //allocate enough bytes for a buffer to store the part of the row to move
    memcpy(copy_buf, E.rows[E.Cy-1].chars + E.rows[E.Cy-1].length - copy_length, copy_length); //write the last 4 bytes of chars to copy_buf
    shiftRowsDown(E.Cy-1); //shift all rows down
    printf("%s", copy_buf);

    E.rows[E.Cy].chars = copy_buf; //copy copy_buf to the row above current row
    E.rows[E.Cy].length = copy_length;

    E.rows[E.Cy-1].chars[E.Cx-1] = '\0'; //cut off the current row at cursor position
    E.rows[E.Cy-1].length = E.rows[E.Cy-1].length - copy_length; //decrement the current row's length
    E.rows[E.Cy-1].chars = realloc(E.rows[E.Cy-1].chars, E.rows[E.Cy-1].length); //reallocate current row's memory

    incrementCursor(0,1,0,0); //move cursor down
    //free(copy_buf);
    
  }else if (E.Cx-1 == 0){ //cursor at beginning of row, can be any row
    createNewRow();
    shiftRowsDown(E.Cy-1);
    E.rows[E.Cy-1].chars = NULL; //reset the old row to nothing, this is where this differs from line 288
    E.rows[E.Cy-1].length = 0;

    incrementCursor(0,1,0,0); //move cursor down
  }
}

void sortKeypress(char c){
  /***
   * This function will determine based on the key pressed if the desired action should be
   * 1. Insert a printable character
   * 2. Move the cursor(this doesn't actually write/delete any characters or create/delete rows
   *    it only sends a command to the terminal)
   * 3. Delete a character(backspace)         
   * 4. Shift all characters in a row(space)
   * 5. Create a new line(enter)
   * Each of these (1-5) will have their own function(s), which sortKeypress will call
   */
  int ascii_code = (int)c;
  printf("%u", ascii_code);
  //write(STDOUT_FILENO, &c, 1);
  if(ascii_code >= 32 && ascii_code < 127){ //the character inputted is a printable character, including space!
    addPrintableChar(c);
  } else if (ascii_code == 13){ //user pressed enter
    addRow();
  } else if (ascii_code == 127){ //user pressed backspace
    backspacePrintableChar();
  } else if (ascii_code == 27){ //escape character was pressed, weird functions coming up
    //moveCursor(c); //this function will move the cursor and will need a special 3 character buffer to read the cursor move command
    sortEscapes(c);
  } else { //one of the unmapped keys was pressed so just do nothing
    return;
  }
}

char processKeypress(void){
  //This function will read a key pressed from the user and return it, no more
    char c = '\0';
    read(STDIN_FILENO, &c, 1);
    //printf("%c;%u\r\n", c, c);
    if(c == CTRL_KEY('c')){ //used to check if key pressed was CTRL-C which is the key to close the editor
        write(STDOUT_FILENO, "\x1b[2J", 4); //clear entire screen
        write(STDOUT_FILENO, "\x1b[f", 3);  //move cursor to top left of screen
        exit(0);
    }
  return c;
}

void clearScreen(void){
  write(STDOUT_FILENO, "\x1b[2J", 4); //clear the entire screen
  write(STDOUT_FILENO, "\x1b[H", 3); //rest cursor to top left(visible change)
}

void writeScreen(void){
  /***
   * This will write each row within the global editor object's dynamic arrow of rows to the screen
   */
  for(int i = 0; i < E.numrows; i++){
    write(STDOUT_FILENO, E.rows[i].chars, E.rows[i].length); //write each row within the dynamic array of rows to the screen
    write(STDOUT_FILENO, "\r\n", 2); //new row and carriage return between rows
  }
  cursor_move_cmd(); //move cursor to current cursor position(visible change)
}

int main(void){
  enableRawMode();
  initEditor();
  while(1){ //replace with 1 when developing
    char c = processKeypress();
    sortKeypress(c);
    clearScreen();
    writeScreen();
    //writeCmds();
  }
}