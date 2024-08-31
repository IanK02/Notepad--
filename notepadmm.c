#include "notepadmm.h" //header file of function prototypes
/***
 * IMPORTANT NOTES
 * The system of the array of rows is 0 indexed, however the cursor is 1 indexed,
 * sort of like a graph from math, the left most bottom box is at 1,1, imagine a 2d grid
 * where valid points aren't on the intersections of the unit lines but 
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
#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)
#define MIN_ROW_CAPACITY 64

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
  size_t capacity;
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

/*** Global Variables ***/
struct editor E; //The global editor struct
struct cmd_buf cbuf; //The global command buffer

/*** Function Prototypes ***/
//note for these prototypes I didn't want to include them in the header file because
//they take one of the structs I've defined as parameters so I just didn't want to bother
//with defining the struct in the header file
void shiftLineCharsR(int index, row *row);
void shiftLineCharsL(int index, row *row);
void initializeRowMemory(row *r, size_t capacity);

/*** Command Buffer ***/
void add_cmd(char *cmd, int last_cmd){
  if(cmd != NULL){
    int cmd_len = snprintf(NULL, 0, "%s", cmd);
    if(last_cmd) cmd_len++;
    cbuf.len += cmd_len;
    cbuf.cmds = realloc(cbuf.cmds, cbuf.len); //reallocate cmds to make space for the new command
    if(cbuf.cmds == NULL){ //check if realloc was successful
      printf("Memory allocation failed\n");
      return;
    }
    if(last_cmd){
      memcpy(cbuf.cmds + cbuf.len - cmd_len, cmd, cmd_len-1);
    }else{
      memcpy(cbuf.cmds + cbuf.len - cmd_len, cmd, cmd_len);
    }
    //snprintf(cbuf.cmds + cbuf.len - cmd_len, cmd_len, "%s", cmd);
    //write(cbuf.cmds + cbuf.len - cmd_len, cmd, cmd_len);
  }
}

void writeCmds(void){
  write(STDOUT_FILENO, cbuf.cmds, cbuf.len);
  cbuf.len = 0; //set len back to 0
  //cbuf.cmds = NULL; //reduce cmds back to NULL
  free(cbuf.cmds);
  cbuf.cmds = NULL; //set cmds back to NULL
}

/*** Editor Initialization ***/
void getWinSize(void){
    ioctl(0, TIOCGWINSZ, &E.w);
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

void exitRawMode(void){
  //Called on program exit to disable raw mode and return terminal to normal
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.termios_o);
}

void initEditor(void){
  //initialize the global editor object's values as well as clear screen and set cursor at starting position
  E.rows = NULL; //initialize rows to null as no text is present yet
  E.numrows = 0;
  appendRow(); //we create the first row, it has no chars, E.numrows doesn't need to be initialized anymore
  E.Cx = 1; //initialize cursor position to (1,1) which is the top left of the screen
  E.Cy = 1;

  //E.numrows = 0; //initialize numrows to 0 as no rows of text are present yet
  //we don't have to initialize termios_o as enableRawMode takes care of setting its attributes
  getWinSize(); //this call to get winsize takes cares of initializing winsize w to have the correct values

  write(STDOUT_FILENO, "\x1b[2J", 4); //clear the screen
  write(STDOUT_FILENO, "\x1b[H", 3); //actually move the cursor to the top left of the screen

  cbuf.cmds = NULL; //initialize the command buffers commands to "" and length to 0
  cbuf.len = 0;
}

/*** Row Manipulation Methods ***/
void initializeRowMemory(row *r, size_t capacity) {
    //initialize chars to have MIN_ROW_CAPACITY bytes
    r->chars = malloc(capacity);
    r->capacity = MIN_ROW_CAPACITY;
    if (r->chars == NULL) {
        // Handle memory allocation failure
        exit(1);
    }
    //set chars equal to all 0(the null terminator)
    memset(r->chars, 0, capacity);
    //ensure the first character of chars is null terminator and set length to 0
    r->chars[0] = '\0';
    r->length = 0;
}

void appendRow(void) {
    //increate numrows and allocate memory for the new row
    E.numrows++;
    E.rows = realloc(E.rows, sizeof(row) * E.numrows);
    if (E.rows == NULL) {
        // Handle memory allocation failure
        exit(1);
    }
    //initialize the memory of the newly created row
    initializeRowMemory(&E.rows[E.numrows - 1], MIN_ROW_CAPACITY);
}

void deleteExistingRow(void){
  E.numrows--; //decrement number of rows

  E.rows = realloc(E.rows, sizeof(row) * E.numrows); //reallocate the memory of rows to accomodate the new array size
  if (E.rows == NULL) { //check if reallocation was successful
      printf("Memory allocation failed\n");
      exit(1);
  }
}

void shiftRowsDown(int index){ //shift all rows up to index down 1
  for(int i = E.numrows - 1; i > index; i--){
    E.rows[i] = E.rows[i-1];
  }
}

void shiftRowsUp(int index){
  for(int i = index; i < E.numrows - 1; i++){
    E.rows[i] = E.rows[i+1];
  }
}

void addRow(void){ //make a new row of text that is actually visible
  if(E.Cx-1 == E.rows[E.Cy-1].length && E.Cy == E.numrows){ //check if cursor is at the end of the row it's on and if current row is 
      appendRow();                                       //the bottom row
      incrementCursor(0,1,0,0); //move cursor down
  }else if (E.Cx-1 == E.rows[E.Cy-1].length){ //cursor at end of row but not on bottom row
      appendRow();
      shiftRowsDown(E.Cy-1);

      E.rows[E.Cy].chars = NULL; //initialize the new row we just made room for to empty
      E.rows[E.Cy].length = 0;

      incrementCursor(0,1,0,0); //move cursor down
  }else if(E.Cx-1 != E.rows[E.Cy-1].length && E.Cx-1 != 0){ 
          //cursor not at end of row or beginning of row 
    appendRow();
    int copy_length = E.rows[E.Cy-1].length - (E.Cx-1); //the length of how much of the string to move to the next row down
    char *copy_buf;
    copy_buf = (char *)malloc(copy_length); //allocate enough bytes for a buffer to store the part of the row to move
    memcpy(copy_buf, E.rows[E.Cy-1].chars + E.rows[E.Cy-1].length - copy_length, copy_length); //write the current row to copy_buf
    shiftRowsDown(E.Cy-1); //shift all rows down

    E.rows[E.Cy].chars = copy_buf; //copy copy_buf to the row above current row
    E.rows[E.Cy].length = copy_length;

    E.rows[E.Cy-1].chars[E.Cx-1] = '\0'; //cut off the current row at cursor position
    E.rows[E.Cy-1].length = E.rows[E.Cy-1].length - copy_length; //decrement the current row's length
    E.rows[E.Cy-1].chars = realloc(E.rows[E.Cy-1].chars, E.rows[E.Cy-1].length); //reallocate current row's memory

    incrementCursor(0,1,0,0); //move cursor down
    
  }else if (E.Cx-1 == 0){ //cursor at beginning of row, can be any row
    appendRow();
    shiftRowsDown(E.Cy-1);
    E.rows[E.Cy-1].chars = NULL; //reset the old row to nothing, this is where this differs from line 288
    E.rows[E.Cy-1].length = 0;

    incrementCursor(0,1,0,0); //move cursor down
  }
  E.Cx = 1; //snap the cursor to the far left of the current row
}

void removeRow(int backSpace){
  //if(E.Cx - 1 == 0 && E.Cy - 1 >= 0){ //check that cursor is at far left of the screen and not on the highest row
  //  shiftRowsUp(E.Cy-1); //shift all rows up one 
  //  deleteExistingRow(); 
  //}
  if(backSpace){
    //copy everything in front of the cursor
    int copy_length = E.rows[E.Cy-1].length - (E.Cx-1); //the length of how much of the string to move to the next row down
    char *copy_buf;
    copy_buf = (char *)malloc(copy_length); //allocate enough bytes for a buffer to store the part of the row to move
    memcpy(copy_buf, E.rows[E.Cy-1].chars + E.rows[E.Cy-1].length - copy_length, copy_length); //write the current row to copy_buf

    //move the cursor up and snap the cursor to the far left of the new row
    incrementCursor(1,0,0,0); //increment cursor up if moveCursor is 1
    E.Cx = E.rows[E.Cy-1].length + 1; //snap cursor to the far left of the new row

    //append the contents of copy_buf to the new row
    E.rows[E.Cy-1].length += copy_length; //increase the length of the new row accordingly
    E.rows[E.Cy-1].chars = realloc(E.rows[E.Cy-1].chars, E.rows[E.Cy-1].length); //reallocate the new row's memory
    memcpy(E.rows[E.Cy-1].chars + E.Cx - 1, copy_buf, copy_length); //write copy_buf to the end of the new row

    shiftRowsUp(E.Cy); //shift all rows up one up to the row below the current row
    deleteExistingRow(); //delete the bottom row
  } else {
    shiftRowsUp(E.Cy-1); //shift all rows up one  up to the current row
    deleteExistingRow(); //delete the bottom row
  }
}

/*** Cursor Manipulation Methods ***/
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
}

/*** Charcater Manipulation Methods ***/
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

void addPrintableChar(char c) {
    if (E.Cx < E.w.ws_col) {
        //temporary pointer to the row we're editing
        row *currentRow = &E.rows[E.Cy-1];
        
        //double capacity if needed
        if (currentRow->length + 2 > currentRow->capacity) {//+2 for new char and null terminator
            size_t new_capacity = GROW_CAPACITY(currentRow->capacity);
            //make sure we don't drop below MIN_ROW_CAPACITY
            if (new_capacity < MIN_ROW_CAPACITY) new_capacity = MIN_ROW_CAPACITY;
            
            //allocate memory to a new row
            char *new_chars = realloc(currentRow->chars, new_capacity);
            if (new_chars == NULL) {
                // Handle memory allocation failure
                return;
            }
            
            //initialize newly allocated memory
            memset(new_chars + currentRow->capacity, 0, new_capacity - currentRow->capacity);
            
            //update global editor object's row to new_chars and new_capacity
            currentRow->chars = new_chars;
            currentRow->capacity = new_capacity;
        }
        
        //shift characters right to make room for the new one
        memmove(&currentRow->chars[E.Cx], &currentRow->chars[E.Cx-1], currentRow->length - E.Cx + 1);
        
        //insert the new character
        currentRow->chars[E.Cx-1] = c;
        //increment row length
        currentRow->length++;

        //increment cursor to account for the new character     
        E.Cx++;
    }
}
void tabPressed(){
  //add a space four times or less if we don't have the space
  if(E.w.ws_col - E.Cx > 3){
    for(int i = 0; i < 4; i++){
      addPrintableChar(' ');
    }
  } else {
    for(int i = 0; i < E.w.ws_col - E.Cx; i++){
      addPrintableChar(' ');
    }
  }
}

void backspacePrintableChar(void) {
    if (E.Cx > 1) {
        //temporary pointer to current row we're editing
        row *currentRow = &E.rows[E.Cy-1];

        //shift left all the characters up to the cursor in the row
        memmove(&currentRow->chars[E.Cx-1], &currentRow->chars[E.Cx], currentRow->length - E.Cx + 1);
        currentRow->length--;
        
        //+1 to account for null terminator
        size_t new_capacity = currentRow->length + 1;
        //make sure we don't drop below MIN_ROW_CAPACITY
        if (new_capacity < MIN_ROW_CAPACITY) new_capacity = MIN_ROW_CAPACITY;
        
        /*** These Lines were the cause of lots of problems, do not reallocate to make a smaller row, it's not worth the trouble***/
        //reallocate row size
        //char *new_chars = realloc(currentRow->chars, new_capacity);
        //if (new_chars == NULL) {
        //    //handle memory allocation failure
        //    return;
        //}

        //assign global editor's row chars to new_chars
        //currentRow->chars = new_chars;
        //---------------------------------------------------------------------------------------
        
        //delete the last character
        currentRow->chars[currentRow->length] = '\0';
        //decrement character to account for the new shorter row
        E.Cx--;
    }
}

void deletePrintableChar(void){
    if(E.Cx > 0){
        //temporary pointer to current row we're editing
        row *currentRow = &E.rows[E.Cy-1];

        //shift left all the characters up to the cursor in the row
        memmove(&currentRow->chars[E.Cx-1], &currentRow->chars[E.Cx], currentRow->length - E.Cx + 1);
        currentRow->length--;
        
        //+1 to account for null terminator
        size_t new_capacity = currentRow->length + 1;
        //make sure we don't drop below MIN_ROW_CAPACITY
        if (new_capacity < MIN_ROW_CAPACITY) new_capacity = MIN_ROW_CAPACITY;
        
        //reallocate row size
        char *new_chars = realloc(currentRow->chars, new_capacity);
        if (new_chars == NULL) {
            //handle memory allocation failure
            return;
        }

        //assign global editor's row chars to new_chars
        currentRow->chars = new_chars;
        //delete the last character
        currentRow->chars[currentRow->length] = '\0';
        //DO NOT decrement character to account for the new shorter row
        //this is how delete is different from backspace
    }
}

/*** User Input Processing ***/
void sortEscapes(char c){
    char *buff = malloc(4); //three character buffer to store all three characters of the arrow key commands
    buff[0] = c;
    read(STDIN_FILENO, buff + 1, 1); //read next byte of input into buf
    read(STDIN_FILENO, buff + 2, 1); //read next byte of input into buf
    if(buff[2] == '3'){ //delete key was pressed
        read(STDIN_FILENO, buff + 3, 1); //read in the last tilde of the delete sequence ("\x1b[3~")
        if(E.rows[E.Cy-1].length != 0){ //check if the row isn't empty
            int current_char = (int)E.rows[E.Cy-1].chars[E.Cx - 1];
            if(current_char >= 32 && current_char < 127){ //check if the current character the cursor is on is a printable character
                deletePrintableChar();
            }
        } else if(E.Cy != E.numrows){ //check that the cursor isn't on the bottom row
            removeRow(0);
        }
    } else { //arrow key was pressed 
        //read(STDIN_FILENO, buf + 2, 1); //read one more byte of input into buf
        moveCursor(c, buff); //moveCursor will only increment C's position, we can't just increment Cy or Cx because we have to 
        //read in the rest of the command buffer, so that's why we use a separate moveCursor function
    }
    free(buff); //free memory allocated to buf
    buff = NULL; //set buf back to NULL
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
  //write(STDOUT_FILENO, &c, 1);
  if(ascii_code >= 32 && ascii_code < 127){ //the character inputted is a printable character, including space!
    addPrintableChar(c);
  } else if (ascii_code == 13){ //user pressed enter
    addRow();
  } else if (ascii_code == 127){ //user pressed backspace
    if(E.Cx-1 == 0 && E.Cy > 1){ //check if cursor is at far left of screen and not on the highest row
      removeRow(1);
    }else{
      backspacePrintableChar();
    }
  } else if (ascii_code == 27){ //escape character was pressed, weird functions coming up
    //moveCursor(c); //this function will move the cursor and will need a special 3 character buffer to read the cursor move command
    sortEscapes(c);
  } else if (ascii_code == 9){ //tab was pressed
    tabPressed();
  } else { //one of the unmapped keys was pressed so just do nothing
    return;
  }
}

/*** Visible Output ***/
void cursor_move_cmd(void){ //move cursor to location specified by global cursor
    write(STDOUT_FILENO, "\x1b[?25l", 6); //make cursor invisible
    //add_cmd("\x1b[?25l"); //make cursor inivisble
    int buf_size = snprintf(NULL, 0, "\x1b[%d;%dH", E.Cy, E.Cx) + 1;
    char *buf = malloc(buf_size);
    if(buf == NULL){
      printf("%s", "Memory allocation failed\n");
    }
    snprintf(buf, buf_size, "\x1b[%d;%dH", E.Cy, E.Cx);
    write(STDOUT_FILENO, buf, buf_size); //move cursor to location specified by Cx and Cy
    write(STDOUT_FILENO, "\x1b[?25h", 6); //make cursor visible
    //add_cmd(buf);
    //add_cmd("\x1b[?25h"); //make cursor visible
    //add_cmd(buf);

    if (buf != NULL) { //null check before freeing
      free(buf);
      buf = NULL;
    }

    buf = NULL; //set buf back to NULL
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
    //write(STDOUT_FILENO, E.rows[i].chars, E.rows[i].length); //write each row within the dynamic array of rows to the screen
    //write(STDOUT_FILENO, "\r\n", 2); //new row and carriage return between rows
    if(E.rows[i].chars != NULL) add_cmd(E.rows[i].chars, 0);
    add_cmd("\r\n", 0);
  }
  //add_cmd("\r\n", 0);
  //printf("%s", cbuf.cmds);
  writeCmds();

  //printing for debugging purposes
  //for(int i = 0; i < E.numrows; i++){
  //  printf("%s\n\r", E.rows[i]);
  //}
  
  cursor_move_cmd(); //move cursor to current cursor position(visible change)
}

/*** Main Loop ***/
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