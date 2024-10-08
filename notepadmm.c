/***
 * Notepad--
 * Written by Ian Kinsella
 */

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
#define MAX_LINE_LENGTH 1000
#define MAX_FILENAME 256

/*** Structures ***/
struct cmd_buf{
  /*** 
   * The Command Buffer(cbuf) will be a dynamically sized string that will be used to write
   * the entire screen in one big write command instead of having a separate write command for
   * each row
  */
  char *cmds;
  int len;
};

typedef struct row {
  /***
   * This represents a row of text, it will contain the information listed below
   * 1. char *chars - A string of the actual text of the row
   * 2. int length - Length of the row
   * 3. size_t capacity - memory capacity of the erow
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
   * 6. scroll(current vertical scroll)
   * 7. sidescroll(horizontal scroll)
   */
  row *rows; //dynamic arrow of rows of text
  int Cx; //cursor x position
  int Cy; //cursor y position
  int numrows; //number of rows
  struct termios termios_o; //terminal configuration struct(from termios.h)
  struct winsize w; //special struct used to store information about the terminal
  int scroll; //how far down the user is scrolled
  int sidescroll; //how far right the user is scrolled
};

/*** Global Variables ***/
struct editor E; //The global editor struct
struct cmd_buf cbuf; //The global command buffer
char *CURRENT_FILENAME; //The name of the current file open
int searchFlag; //Toggled if user is currently using the search feature
char searchQuery[256]; //The query the user searched for
char **keywords; //Array of strings of keywords to highlight
int numKeywords; //Number of keywords in the language

/*** Function Prototypes ***/
//note for these prototypes I didn't want to include them in the header file because
//they take one of the structs I've defined as parameters so I just didn't want to bother
//with defining the struct in the header file
void shiftLineCharsR(int index, row *row);
void shiftLineCharsL(int index, row *row);
void initializeRowMemory(row *r, size_t capacity);
row duplicate_row(row *original_row);
void setChars(row *row, char *chars, int strlen);

/*** Command Buffer ***/
void add_cmd(char *cmd, int last_cmd){
  /***
   * Used to add a command to the global command buffer(cbuf)
   */
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
  }
}

void writeCmds(void){
  /***
   * Writes all the commands in cbuf to STDOUT
   */
  write(STDOUT_FILENO, cbuf.cmds, cbuf.len);
  cbuf.len = 0; //set len back to 0
  free(cbuf.cmds);
  cbuf.cmds = NULL; //set cmds back to NULL
}

/*** Editor Initialization and Program Exit***/
void getWinSize(void){
  /***
   * Gets the window size
   */
    ioctl(0, TIOCGWINSZ, &E.w);
}

void enableRawMode(void){
  /***
   * Enables raw mode in the terminal as well as disabling raw mode on exit
   */
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
  /***
   * Disables raw mode
   */
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.termios_o);
}

void initEditor(char *filename){
  /***
   * Initialize the global editor object's fields, add the first empty row, get the window size,
   * and determine the language of the file open to adjust syntax highlighting
   */
  E.rows = NULL; //initialize rows to null as no text is present yet
  E.numrows = 0;
  appendRow(); //we create the first row, it has no chars, E.numrows doesn't need to be initialized anymore
  E.Cx = 1; //initialize cursor position to (1,1) which is the top left of the screen
  E.Cy = 1;
  E.scroll = 0; //scrolled to top of terminal to begin with
  E.sidescroll = 0; //scrolled to far left of terminal to begin with

  //we don't have to initialize termios_o as enableRawMode takes care of setting its attributes
  getWinSize(); //this call to get winsize takes cares of initializing winsize w to have the correct values
  E.w.ws_row--; //we decrement row by 1 to leave room for the status message bar

  write(STDOUT_FILENO, "\x1b[2J", 4); //clear the screen
  write(STDOUT_FILENO, "\x1b[H", 3); //actually move the cursor to the top left of the screen

  cbuf.cmds = NULL; //initialize the command buffers commands to "" and length to 0
  cbuf.len = 0; //1 for the null terminator

  CURRENT_FILENAME = NULL; //set CURRENT_FILENAME to null to handle the case the user doesn't open a file
  searchFlag = 0; //set serachFlag initiallly to 0 since we won't be searching on initialization
  if(filename[strlen(filename) - 1] == 'c'){
    keywords = readTextArray("ckeyword.txt");
  } else if (filename[strlen(filename) - 2] == 'v' && filename[strlen(filename) - 1] == 'a'){
    keywords = readTextArray("javakeyword.txt");
  } else if(filename[strlen(filename) - 2] == 'p' && filename[strlen(filename) - 1] == 'p'){
    keywords = readTextArray("cppkeyword.txt");
  } else{
    keywords = NULL;
    numKeywords = 0;
  }
}

void free_all_rows(void){
  /***
   * Free all rows of text in the global editor object E as well as the array of keywords
   */
  for(int i = 0; i < E.numrows; i++){
    free(E.rows[i].chars);
    E.rows[i].chars = NULL;
  }
  for(int i = 0; i < numKeywords; i++){
    free(keywords[i]);
    keywords[i] = NULL;
  }
  free(E.rows);
  free(keywords);
  E.rows = NULL;
}

/*** Row Manipulation Methods ***/
char* sideScrollCharSet(row *row){
  /***
   * Returns the string(adjusted for sidescroll and window size) that is to be printed to the screen
   */
  int offset = 0;
  if((row->length - E.sidescroll) >= E.w.ws_col){
    char *substr;
    substr = malloc(E.w.ws_col + offset + 1); //+1 for null terminator
    strncpy(substr, row->chars + E.sidescroll - offset, E.w.ws_col + offset); //copy chars over to substr
    substr[E.w.ws_col + offset] = '\0'; //ensure substr is null termirnated
    return substr;
  } else if(E.sidescroll <= row->length){
    char *substr;
    substr = malloc(row->length - E.sidescroll - offset + 1); //+1 for null terminator
    strncpy(substr, row->chars + E.sidescroll + offset, (row->length - E.sidescroll) - offset);
    substr[row->length - E.sidescroll - offset] = '\0'; //ensure substr is null termirnated
    return substr;
  } else if(E.sidescroll > row->length){
    return NULL;
  }
  return NULL;
}

void setChars(row *row, char *chars, int strlen){
  /***
   * Sets the characters of row to chars
   */
  if(row->capacity <= (size_t)strlen){//reallocate row's capacity if needed
    row->chars = realloc(row->chars, (size_t)(strlen+1)); //+1 for null terminator
    row->capacity = strlen + 1;
  }

  free(row->chars); //free row's chars before reassignment

  char *new_chars = malloc(row->capacity); //make a new_chars to copy chars to row
  memcpy(new_chars, chars, strlen + 1);//+1 for null terminator
  row->chars = new_chars;

  row->length = strlen;
  row->chars[strlen] = '\0'; //make sure chars is null terminated
} 

row duplicate_row(row *original) {
  /***
   * Creates a duplicate row of original
   */

  // Initialize a new row
  row new_row;

  new_row.chars = malloc(original->capacity * sizeof(char));
  if (new_row.chars == NULL) {
      // Handle memory allocation failure
      new_row.length = 0;
      new_row.capacity = 0;
      return new_row;
  }

  //copy original's chars to new_row's chars
  memcpy(new_row.chars, original->chars, original->length + 1);//+1 for null terminator

  //ensure new_row chars is null terminated
  new_row.chars[original->length] = '\0';

  // Copy other properties
  new_row.length = original->length;
  new_row.capacity = original->capacity;
  return new_row;
}

void initializeRowMemory(row *r, size_t capacity) {
  /***
   * Allocates memory for a newly created row and initializes its fields
   */
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
  /***
   * Adds a new unitialized row to the global editor object's array of rows
   */
  
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
  /***
   * Delete a row
   */
  if(E.rows[E.numrows-1].chars != NULL) free(E.rows[E.numrows-1].chars); //free the chars of the bottom row
  E.numrows--; //decrement number of rows
  if (E.rows == NULL) { //check if reallocation was successful
    printf("Memory allocation failed\n");
    exit(1);
  }
}

void shiftRowsDown(int index){
  /***
   * Shift all rows up to index down 1
   */
  free(E.rows[E.numrows-1].chars);
  for(int i = E.numrows - 1; i > index + 1; i--){
    E.rows[i] = E.rows[i-1];
  }

  row dup_row = duplicate_row(&E.rows[index]);

  E.rows[index+1] = dup_row;
}

void shiftRowsUp(int index){
  /***
   * Shift all rows down from index up 1
   */
  if(index < E.numrows-1) free(E.rows[index].chars);
  for(int i = index; i < E.numrows - 1; i++){
    E.rows[i] = E.rows[i+1];
  }

  int lowrow = E.numrows - 1;

  if((E.numrows - index) > 1){
    row dup_row = duplicate_row(&E.rows[lowrow]);
    E.rows[lowrow-1] = dup_row;
  }

}

void addRow(void){
  /***
   * Create a new row in the text editor in response to the user pressing enter, this method handles splitting a row and copying
   * its characters if the user presses enter in the middle of a row
   */
  int cy = E.Cy;
  if(E.Cx-1 == E.rows[cy-1].length && cy == E.numrows){ //check if cursor is at the end of the row it's on and if current row is 
      appendRow();                                      //the bottom row
      incrementCursor(0,1,0,0); //move cursor down
  }else if (E.Cx-1 == E.rows[cy-1].length){ //cursor at end of row but not on bottom row
      appendRow();
      shiftRowsDown(cy-1);

      memset(E.rows[cy].chars, 0, E.rows[cy].capacity);
      E.rows[cy].chars[0] = '\0';
      E.rows[cy].length = 0;

      incrementCursor(0,1,0,0); //move cursor down
  }else if(E.Cx-1 != E.rows[cy-1].length && E.Cx-1 != 0){ 
          //cursor not at end of row or beginning of row 
    appendRow();

    int copy_length = E.rows[cy-1].length - (E.Cx-1) + 1; //the length of how much of the string to move to the next row down

    shiftRowsDown(cy-1);

    memset(E.rows[cy].chars, 0, E.rows[cy].capacity);
    E.rows[cy].chars[0] = '\0';
    E.rows[cy].length = 0;

    memcpy(E.rows[cy].chars, E.rows[cy-1].chars + E.Cx - 1, copy_length);
    E.rows[cy].length = copy_length - 1;

    E.rows[cy-1].chars[E.Cx-1] = '\0'; //cut off the current row at cursor position
    E.rows[cy-1].length = E.Cx-1; //decrement the current row's length
    incrementCursor(0,1,0,0); //move cursor down
    
  }else if (E.Cx-1 == 0){ //cursor at beginning of row, can be any row
    appendRow();
    shiftRowsDown(cy-1);

    //reset current row to empty
    memset(E.rows[cy-1].chars, 0, E.rows[cy-1].capacity);
    E.rows[cy-1].chars[0] = '\0';
    E.rows[cy-1].length = 0;

    incrementCursor(0,1,0,0); //move cursor down
  }
  E.Cx = 1; //snap the cursor to the far left of the current row
  E.sidescroll = 0; //set sidescroll to 0
}

void removeRow(int backSpace){
  /***
   * Remove a row in response to the user pressing backspace or delete, this method handles copy and moving of characters
   */
  if(backSpace){
    if(E.rows[E.Cy-2].length != 0) E.Cx = E.rows[E.Cy-2].length + 1;
    incrementCursor(1,0,0,0); //increment cursor u
    int cy = E.Cy;

    if(E.rows[cy-1].length + E.rows[cy].length + 1 >= (int)E.rows[cy-1].capacity){
      E.rows[cy-1].chars = realloc(E.rows[cy-1].chars, E.rows[cy-1].capacity + E.rows[cy].length + 1);
      E.rows[cy-1].capacity += E.rows[cy].length + 1;
    }

    memcpy(E.rows[cy-1].chars + E.rows[cy-1].length, E.rows[cy].chars, E.rows[cy].length + 1);
    E.rows[cy-1].length += E.rows[cy].length;

    memset(E.rows[cy].chars, 0, E.rows[cy].capacity);
    E.rows[cy].chars[0] = '\0';
    E.rows[cy].length = 0;
    
    shiftRowsUp(E.Cy); //shift all rows up one up to the row below the current row
    deleteExistingRow(); //delete the bottom row

  } else {
    shiftRowsUp(E.Cy-1); //shift all rows up one up to the current row
    deleteExistingRow(); //delete the bottom row
  }
}

/*** Cursor Manipulation Methods ***/
void printCursorPos(void){
  /***
   * Print the current position of the cursor in the bottom right of the screen
   */
  int oldY = E.Cy;
  int oldX = E.Cx;
  int bufSize = snprintf(NULL, 0, "Ln %d, Col %d", E.Cy, E.Cx)+1;
  char *buf;
  buf = malloc(bufSize);
  snprintf(buf, bufSize, "Ln %d, Col %d", E.Cy, E.Cx);
  int offset = 22;
  if(bufSize > 22){
    offset = bufSize;
  }
  E.Cy = E.w.ws_row + E.scroll + 1;
  E.Cx = E.sidescroll + E.w.ws_col - offset;
  cursor_move_cmd();
  write(STDOUT_FILENO, "\x1b[0J", 4);
  write(STDOUT_FILENO, buf, bufSize);

  E.Cx = oldX;
  E.Cy = oldY;
  free(buf);
}

void incrementCursor(int up, int down, int left, int right){  
  /***
   * Increment the position of the global editor object's cursor
   */
    if((up^down^left^right) != 1 || up+down+left+right != 1){
      printf("Only one parameter can be 1, all others must be 0");
      return;
    }
    if(up && !down && !left && !right){ //up arrow
      if(E.Cy > 1){
        E.Cy--; //only deccrement if C.y is > 1, note that going up decrements C.y as the top left of the screen is 1,1
        //keep moving the cursor left until it hits a printable character or 
        //beginning of the row
        if(E.Cx > E.rows[E.Cy-1].length + 1){
          while(E.Cx > E.rows[E.Cy-1].length + 1){
            E.Cx--; //snap cursor to end of row
          }
        }
      }
    } else if(!up && down && !left && !right){ //down arrow
      if(E.Cy <= E.scroll + E.w.ws_row){ 
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
      if(E.Cx <= E.sidescroll + E.w.ws_col){
        E.Cx++; //only increment if C.x < columns limit, the column limit is positive and represents the farthest right column on screen
      }
    }
}   

void moveCursor(char *buf){
  /***
   * Map user arrow key presses to cursor movements
   */
    switch (buf[2])
    {
    case 'A': //up arrow
      incrementCursor(1,0,0,0); //increment the cursor's coordinates(stored in the global editor E)
      break;
    case 'B': //down arrow
      if(E.Cy <= E.numrows - 1) incrementCursor(0,1,0,0); //limit cursor to one above the lowest row
      break;
    case 'C': //right arrow
      if(E.Cx <= E.rows[E.Cy-1].length) incrementCursor(0,0,0,1); //limit cursor at only one space further right than the text
      break;
    case 'D': //left arrow
      incrementCursor(0,0,1,0);
      break;
    default:
      break;
    } 
}

/*** Scrolling Methods ***/
void scrollUp(void){
  if(E.scroll > 0) E.scroll--;
}

void scrollDown(void){
  E.scroll++;
}

void scrollRight(void){
  E.sidescroll++;
}

void scrollLeft(void){
  if(E.sidescroll > 0) E.sidescroll--;
}

void scrollCheck(void){
  /***
   * Check if the editor needs to scroll up or down in response to user inputs
   */
  if((E.Cy) - E.scroll > E.w.ws_row){
    scrollDown();
  } else if ((E.Cy-1) < E.scroll){
    scrollUp();
  }
}

void sidescrollCheck(void){
  /***
   * Check if the editor needs to scroll left or right in response to user inputs
   */
  if(E.Cx - E.sidescroll > E.w.ws_col){
    scrollRight();
  } else if (E.Cx-1 < E.sidescroll){
    E.sidescroll = E.Cx-1;
  }
}

/*** Charcater Manipulation Methods ***/
void insertStr(char **original, char* insert, size_t index){
  /***
   * Insert the string insert into original at index
   */
  if(index > strlen(*original)){
    printf("%s", "index out of bounds");
    exit(1);
  }

  char *result = malloc(strlen(*original) + strlen(insert) + 1);
  if(result == NULL){
    printf("Memory allocation failed");
    exit(1);
  }

  strncpy(result, *original, index);
  
  strcpy(result + index, insert);

  strcpy(result + index + strlen(insert), *original + index);

  free(*original);
  *original = result;
}

void shiftLineCharsR(int index, row *row){ 
  /***
   * Shift all characters in a row up to index right by 1
   */                                  
  for (int i = row->length - 1; i > index; i--) {
    row->chars[i] = row->chars[i - 1];
  }
}

void shiftLineCharsL(int index, row *row){
  /***
   * Shift all characters in a row to the right of index one to the left
   */
  for (int i = index; i < row->length - 1; i++) {
    row->chars[i] = row->chars[i + 1];
  }
}

void addPrintableChar(char c) {
  /***
   * Write a printable characters to the screen in response to user input
   */
  if (E.Cx - E.sidescroll <= E.w.ws_col) {
    //temporary pointer to the row we're editing
    row *currentRow = &E.rows[E.Cy-1];
    
    //double capacity if needed
    if (currentRow->length + 2 > (int)currentRow->capacity) {//+2 for new char and null terminator
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

    currentRow->chars[currentRow->length] = '\0'; //ensure row is always null terminated    
    E.Cx++; //increment cursor to account for the new character 
    if(E.Cx - E.sidescroll > E.w.ws_col){ //check if we need to scroll
      scrollRight();
    }
  }
}

void tabPressed(){
  /***
   * Write 4 spaces to the screen in response to the user pressing tab
   */
  for(int i = 0; i < 4; i++){
    addPrintableChar(' ');
  }
}

void backspacePrintableChar(void) {
  /***
   * Delete a printable character in response to the user pressing backspace
   */
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
          
    //delete the last character
    currentRow->chars[currentRow->length] = '\0';
    //decrement character to account for the new shorter row
    E.Cx--;
    if(E.Cx <= E.sidescroll){
      scrollLeft();
    } 
  }
}

void deletePrintableChar(void){
  /***
   * Delete a printable character in response to the user pressing delete
   */
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
    
    currentRow->chars[currentRow->length] = '\0';
    //DO NOT decrement character to account for the new shorter row
    //this is how delete is different from backspace
  }
}

/*** User Input Processing ***/
void searchPrompt(void){
  /***
   * Prompt the user for what words they want to search for
   */
  int oldX = E.Cx;
  int oldY = E.Cy;
  statusWrite("Search: ");

  exitRawMode();
  fgets(searchQuery, sizeof(searchQuery), stdin);
  searchQuery[strcspn(searchQuery, "\n")] = '\0';
  enableRawMode();
  E.Cx = oldX;
  E.Cy = oldY;
}

void sortEscapes(char c){
  /***
   * Determine whether or not the user pressed delete or an arrow key and call the appropriate methods
   */
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
    moveCursor(buff); //moveCursor will only increment C's position, we can't just increment Cy or Cx because we have to 
    //read in the rest of the command buffer, so that's why we use a separate moveCursor function
  }
  free(buff); //free memory allocated to buff
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
   * 6. Delete a row(backspace or enter)
   * 7. Save File(ctrl+s)
   * 8. Search for word(ctrl+b)
   * Each of these (1-8) will have their own function(s), which sortKeypress will call
   */
  int ascii_code = (int)c;
  if(ascii_code >= 32 && ascii_code < 127){ //the character inputted is a printable character
    addPrintableChar(c);
  } else if (ascii_code == 13){ //user pressed enter
    addRow();
  } else if (ascii_code == 127){ //user pressed backspace
    if((E.Cx-1 == 0) && (E.Cy > 1)){ //check if cursor is at far left of screen and not on the highest row
      removeRow(1);
    }else{
      backspacePrintableChar();
    }
  } else if (ascii_code == 27){ //escape character was pressed, weird functions coming up
    sortEscapes(c);
  } else if (ascii_code == 9){ //tab was pressed
    tabPressed();
  } else if (c == CTRL_KEY('s')){ //ctrl+s was pressed
    saveFile();
  }else if (c == CTRL_KEY('b')){ //ctrl+b was pressed
    if(searchFlag == 0) searchPrompt();
    //searchQuery[0] = 'v'; //for debug purposes only
    //searchQuery[1] = 'o';
    //searchQuery[2] = 'i';
    //searchQuery[3] = 'd';
    //searchQuery[4] = '\0';
    searchFlag = !searchFlag;
  } else { //one of the unmapped keys was pressed so just do nothing
    return;
  }
}

/*** Visible Output ***/
void cursor_move_cmd(void){ 
  /***
   * Write the commands to STDOUT to make the visual change of moving the cursor to the location specified by
   * the global editor object E
   */
  write(STDOUT_FILENO, "\x1b[?25l", 6); //make cursor invisible
  int buf_size = snprintf(NULL, 0, "\x1b[%d;%dH", E.Cy, E.Cx)+1;
  char *buf = malloc(buf_size);
  if(buf == NULL){
    printf("%s", "Memory allocation failed\n");
  }
  snprintf(buf, buf_size, "\x1b[%d;%dH", E.Cy-E.scroll, E.Cx - E.sidescroll);
  write(STDOUT_FILENO, buf, buf_size-1); //move cursor to location specified by Cx and Cy
  write(STDOUT_FILENO, "\x1b[?25h", 6); //make cursor visible

  if (buf != NULL) { //null check before freeing
    free(buf);
  }

  buf = NULL; //set buf back to NULL
}

char processKeypress(void){
  /***
   * Return the key pressed by the user and check if the user pressed ctrl+c to exit the editor
   */
  char c = '\0';
  read(STDIN_FILENO, &c, 1);
  if(c == CTRL_KEY('c')){ //used to check if key pressed was ctrl+c which is the key to close the editor
    write(STDOUT_FILENO, "\x1b[2J", 4); //clear entire screen
    write(STDOUT_FILENO, "\x1b[f", 3);  //move cursor to top left of screen
    free_all_rows();
    exit(0);
  }
  return c;
}

void clearScreen(void){
  /***
   * Write the commands to clear the screen except for the status message row
   */
  int buf_size = snprintf(NULL, 0, "\x1b[%d;%dH", E.w.ws_row, E.w.ws_col)+1;
  char *move_cmd = malloc(buf_size);
  snprintf(move_cmd, buf_size, "\x1b[%d;%dH", E.w.ws_row, E.w.ws_col);
  write(STDOUT_FILENO, move_cmd, buf_size-1);

  write(STDOUT_FILENO, "\x1b[1J", 4); //clear the entire screen(except status message row)
  write(STDOUT_FILENO, "\x1b[H", 3); //reset cursor to top left(visible change)
  free(move_cmd);
}

void writeScreen(void){
  /***
   * This will write each row within the global editor object's dynamic arrow of rows to the screen, account for comments,
   * syntax highlighting, and search highlighting
   */
  int *markedRows;
  markedRows = markMultilineRows(); //mark all the rows highlighted by a multiline comment
  if(E.numrows - E.scroll < E.w.ws_row){
    for(int i = E.scroll; i < E.numrows - 1; i++){
      char* written_chars;
      int commented; //the index at which a // occurs if it does
      row dup_row = duplicate_row(&E.rows[i]);
      if(searchFlag) {
        written_chars = sideScrollCharSet(&dup_row);
        commented = inlineCommentHighlight(&written_chars);
        searchHighlight(&written_chars, commented, markedRows[i]);
        if(markedRows[i] == 0) highlightSyntax(&written_chars, commented);
      } else {
        written_chars = sideScrollCharSet(&dup_row);
        commented = inlineCommentHighlight(&written_chars);
        if(markedRows[i] == 0) highlightSyntax(&written_chars, commented);
      }
      if(markedRows[i]) multilineCommentHighlight(&written_chars);
      add_cmd(written_chars, 0);
      add_cmd("\r\n", 0);
      free(dup_row.chars);
      if(written_chars != NULL) free(written_chars);
    }
    if(E.rows[E.numrows-1].chars != NULL){ 
      char* written_chars;
      int commented;
      row dup_row = duplicate_row(&E.rows[E.numrows-1]);
      if(searchFlag) {
        written_chars = sideScrollCharSet(&dup_row);
        commented = inlineCommentHighlight(&written_chars);
        searchHighlight(&written_chars, commented, markedRows[E.numrows-1]);
        if(markedRows[E.numrows-1] == 0) highlightSyntax(&written_chars, commented);
      } else {
        written_chars = sideScrollCharSet(&dup_row);
        commented = inlineCommentHighlight(&written_chars);
        if(markedRows[E.numrows-1] == 0) highlightSyntax(&written_chars, commented);
      }
      if(markedRows[E.numrows-1]) multilineCommentHighlight(&written_chars);
      add_cmd(written_chars, 0);
      free(dup_row.chars);
      if(written_chars != NULL) free(written_chars);
    }
  } else {
    for(int i = E.scroll; i < E.scroll + E.w.ws_row - 1; i++){
      char* written_chars;
      int commented;
      row dup_row = duplicate_row(&E.rows[i]);
      if(searchFlag) {
        written_chars = sideScrollCharSet(&dup_row);
        commented = inlineCommentHighlight(&written_chars);
        searchHighlight(&written_chars, commented, markedRows[i]);
        if(markedRows[i] == 0) highlightSyntax(&written_chars, commented);
      } else {
        written_chars = sideScrollCharSet(&dup_row);
        commented = inlineCommentHighlight(&written_chars);
        if(markedRows[i] == 0) highlightSyntax(&written_chars, commented);
      }
      if(markedRows[i]) multilineCommentHighlight(&written_chars);
      add_cmd(written_chars, 0);
      add_cmd("\r\n", 0);
      free(dup_row.chars);
      if(written_chars != NULL){
        free(written_chars);
      }
    }
    if(E.rows[E.scroll + E.w.ws_row - 1].chars != NULL) {
      char* written_chars;
      int commented;
      row dup_row = duplicate_row(&E.rows[E.scroll + E.w.ws_row - 1]);
      if(searchFlag) {
        written_chars = sideScrollCharSet(&dup_row);
        commented = inlineCommentHighlight(&written_chars);
        searchHighlight(&written_chars, commented, markedRows[E.scroll + E.w.ws_row - 1]);
        if(commented == 0 && markedRows[E.scroll + E.w.ws_row - 1] == 0) highlightSyntax(&written_chars, commented);
      } else {
        written_chars = sideScrollCharSet(&dup_row);
        commented = inlineCommentHighlight(&written_chars);
        if(markedRows[E.scroll + E.w.ws_row - 1] == 0) highlightSyntax(&written_chars, commented);
      }
      if(markedRows[E.scroll + E.w.ws_row - 1]) multilineCommentHighlight(&written_chars);
      add_cmd(written_chars, 0);
      free(dup_row.chars);
      if(written_chars != NULL) free(written_chars);
    }
  }
  writeCmds();
  printCursorPos();
  scrollCheck();
  sidescrollCheck();
  cursor_move_cmd(); //move cursor to current cursor position(visible change)
  free(markedRows);
}

/*** File IO ***/
void readFile(char *filename) {
  /***
   * Read a file into the global editor object E
   */
  if(strlen(filename) > 256){
    printf("%s\n", "File name too large");
    return;
  }
  CURRENT_FILENAME = filename; //CURRENT_FILENAME points to same block of memory as *filename which is argv[1]
  FILE *current_file;
  current_file = fopen(filename, "r");
  if (current_file == NULL) {
    perror("Error opening file");
    return;
  }

  char *line = NULL; //pointer to hold the line read
  size_t len = 0;   //size of the buffer
  ssize_t read;      //number of characters read
  if((read = getline(&line, &len, current_file)) != -1){//do a getline call once without appendRow since a row is appended during initEditor()
    setChars(E.rows, line, read-1);//-1 to exclude the \n because writeScreen will add it
  }

  while ((read = getline(&line, &len, current_file)) != -1) {
    appendRow(); //add a new row
    setChars(&E.rows[E.numrows-1], line, read-1); //-1 to exclude the \n because writeScreen will add it
  }
  
  if(E.numrows > 1) setChars(&E.rows[E.numrows-1], line, E.rows[E.numrows-1].length + 1); //add on the last character of the file

  free(line);
  fclose(current_file); 
}

void saveFile(void){
  /***
   * Save the contents of the global editor object to a file
   */
  statusWrite("Filename: ");
  
  char filename[MAX_FILENAME];

  exitRawMode(); //temporarily turn off RawMode
  if (fgets(filename, sizeof(filename), stdin) != NULL) {
    //remove the \n
    size_t length = strlen(filename);
    if (length > 0 && filename[length - 1] == '\n') {
      filename[length - 1] = '\0';
    }
    if(strlen(filename) > 256){
      statusWrite("Filename too large");
      enableRawMode();
      E.Cy = E.scroll+1; //snap cursor back to top of screen
      return;
    }
    if(CURRENT_FILENAME == NULL && strlen(filename) == 0){
      statusWrite("Filename cannot be empty");
      enableRawMode();
      E.Cy = E.scroll+1; //snap cursor back to top of screen
      return;
    }

    if(strlen(filename) == 0 && CURRENT_FILENAME != NULL){
      writeFile(CURRENT_FILENAME);
    } else if(strlen(filename) > 0){
      writeFile(filename);
    }
  }
  enableRawMode();
  E.Cy = E.scroll+1; //snap cursor back to top of screen
}

void writeFile(char *filename){
  /***
   * Write the contents of a file to the screen
   */
  FILE *fptr = fopen(filename, "w");

  if (fptr == NULL) {
    perror("Error opening file");
    return;
  }

  for(int i = 0; i < E.numrows - 1; i++){ //write all the chars within rows to the file, note that a \r is NOT written because for some 
    fprintf(fptr, "%s", E.rows[i].chars); //reason in .txt file land \r is not used, only \n, so we don't add them
    fprintf(fptr, "%s", "\n");
  }
  fprintf(fptr, "%s", E.rows[E.numrows-1].chars);
  
  long size = getFileSize(fptr);
  char *bytes_message = malloc(sizeof(long) + 18 + strlen(filename) + 1);
  sprintf(bytes_message, "%ld bytes written to %s", size, filename);
  statusWrite(bytes_message);

  fclose(fptr); 
  free(bytes_message);
}

long getFileSize(FILE *file){
  /***
   * Get a file's size in bytes
   */
  long size;
  fseek(file, 0, SEEK_END); //move pointer to end of file
  size = ftell(file); //get pointer position(this is the file size)
  fseek(file, 0, SEEK_SET); //move pointer back to beginning of file
  return size;
}

char** readTextArray(char* filename){
  /***
   * Read a .txt file into an array
   */
  //this method assumes the .txt file is in a format where each element of the array is on a separate line
  FILE *file;
  if((file = fopen(filename, "r")) == NULL){
    printf("%s", "Failed to open file");
    exit(1);
  }
  rewind(file);
  long start_pos = ftell(file); //get starting position of pointer
  int lines = 0;
  char c;

  while((c = fgetc(file)) != EOF){ //count the number of lines
    if(c == '\n'){
      lines++;
    }
  }

  if(lines > 0 ){ //count the last line which doesn't end with a \n
    lines++;
  }

  fseek(file, start_pos, SEEK_SET); //set file pointer back to beginning of file
  char **words = malloc(lines * sizeof(char *)); //initialize our array of char pointers

  if(strlen(filename) > 256){
    printf("%s\n", "File name too large");
    exit(1);
  }

  char *line = NULL; //pointer to hold the line read
  size_t len = 0;   //size of the buffer
  ssize_t read;      //number of characters read
  int i = 0;
  while((read = getline(&line, &len, file)) != -1){
    words[i] = malloc(read); 
    strncpy(words[i], line, read - 1);
    words[i][read - 1] = '\0'; //ensure words[i] is null terminated
    i++;
  }
  free(line);
  fclose(file);
  numKeywords = lines;
  return words;
}


/*** Status Bar Methods ***/
void statusWrite(char *message){
  /***
   * Write a message to the special status bar
   */
  E.Cy = E.w.ws_row + E.scroll + 1; //snap cursor to the lowest row reserved for status messsages
  E.Cx = E.sidescroll+1;
  cursor_move_cmd(); //move the cursor
  
  write(STDOUT_FILENO, "\x1b[2K", 4); //clear the special row
  write(STDOUT_FILENO, message, strlen(message)); //write the message to the special row

}

/*** Searching Methods ***/
void searchHighlight(char **chars, int commentIndex, int multiline){
  /***
   * Highlight the characters the user searched for
   */
  char *foundWord;
  int index = 0;
  int searchOffset = 0;
  if(*chars != NULL){
    if(searchQuery != NULL){
      foundWord = strstr(*chars + index + searchOffset, searchQuery);
      while(foundWord != NULL){
        searchOffset = strlen(searchQuery);
        index = foundWord - *chars;
        if(foundWord != NULL) {
          insertStr(chars, "\x1b[48;5;160m", index);
          if(index > commentIndex || multiline){
            insertStr(chars, "\x1b[0m\x1b[38;5;22m", index + 11 + strlen(searchQuery));
          } else {
            insertStr(chars, "\x1b[0m", index + 11 + strlen(searchQuery));
          }
          foundWord = strstr(*chars + index + searchOffset + 15, searchQuery);
        }
      }
    }
  }
}

/*** Syntax Highlighting***/
void highlightSyntax(char **chars, int inlineHighlight){
  /***
   * Highlight keywords in blue
   */
  int indicesLen = 0;
  //find the number of keywords in a line
  int keywordCount = 0;
  char* foundWord;
  if(*chars != NULL){
    for(int i = 0; i < numKeywords; i++){
      foundWord = strstr(*chars, keywords[i]);
      while(foundWord != NULL){
        keywordCount++;
        int index = foundWord - *chars;
        indicesLen++;
        if(checkKeywordHighlight(*chars, foundWord, strlen(keywords[i])) && index < inlineHighlight){
          insertStr(chars, "\x1b[38;5;26m", index);
          insertStr(chars, "\x1b[0m", index + 10 + strlen(keywords[i]));
          inlineHighlight += 14;
          foundWord = strstr(*chars + index + 14 + strlen(keywords[i]), keywords[i]);
        } else{
          foundWord = strstr(*chars + index + strlen(keywords[i]), keywords[i]);
        }
      }
    }
  }
}

int inlineCommentHighlight(char **chars){
  /***
   * Changes inline comments to green
   */
  char* foundWord;
  if(*chars != NULL){ 
    foundWord = strstr(*chars, "//");
    if(foundWord != NULL){
      int index = foundWord - *chars;
      insertStr(chars, "\x1b[38;5;22m", index);
      insertStr(chars, "\x1b[0m", strlen(*chars));
      return index;
    }
  }
  return 8000;
}

void multilineCommentHighlight(char **chars){
  /***
   * Handles making green rows that are included in multiline comments
   */
  char* foundWord;
  if(*chars != NULL){
    foundWord = strstr(*chars, "/*");
    if(foundWord != NULL){
      int index = foundWord - *chars;
      insertStr(chars, "\x1b[38;5;22m", index);
      insertStr(chars, "\x1b[0m", strlen(*chars));
    } else {
      insertStr(chars, "\x1b[38;5;22m", 0);
      insertStr(chars, "\x1b[0m", strlen(*chars));
    }
  }
}

int* markMultilineRows(void){
  /*
   *This method will create an int array where each entry is a 1 or a 0 denoting whether or not
   *the corresponding row in the global editor object E is included in a multiline comment
   */
  int *markedRows = malloc(E.numrows * sizeof(int)); 
  int mark_on = 0;
  char *foundWord;
  for(int i = 0; i < E.numrows; i++){
    if((foundWord = strstr(E.rows[i].chars, "/*")) != NULL){
      mark_on = 1;
    }
    if(mark_on){
      markedRows[i] = 1;
    } else {
      markedRows[i] = 0;
    }
    if((foundWord = strstr(E.rows[i].chars, "*/")) != NULL){
      mark_on = 0;
    }
  }
  return markedRows;
}

void commentEntireRow(char **chars){
  /***
   * Make an entire row green to denote it is commented out
   */
  insertStr(chars, "\x1b[38;5;22m", 0);
  insertStr(chars, "\x1b[0m", strlen(*chars));
}

int checkKeywordHighlight(char *fullLine, char *word, int wordlen){
  /***
   * Check if a found keyword should be highlighted, making sure it isn't embedded within another string
   */
  int passed = 1;
  if(word != fullLine){
    if(((int)*(word - 1) >= 65 && (int)*(word - 1) <= 90)|| 
        ((int)*(word - 1) >= 97 && (int)*(word - 1) <= 122)){
          passed = 0;
        }
  }
  if(*(word + wordlen) != '\0'){        
    if(((int)*(word + wordlen) >= 65 && (int)*(word + wordlen) <= 90) ||
        ((int)*(word + wordlen) >= 97 && (int)*(word + wordlen) <= 122)){
          passed = 0;
        }
    }
  return passed;
}

/*** Main Loop ***/
int main(int argc, char *argv[]){
  enableRawMode();
  if(argc == 2){
    initEditor(argv[1]);
  } else {
    initEditor("hello_world.c");
  }
  if(argc == 2){
    char *filename = argv[1];
    readFile(filename);
    clearScreen();
    writeScreen();
  }

  //readFile("hello_world.c");  //for debug purposes only
  //clearScreen();
  //writeScreen();

  while(1){ 
    char c = processKeypress();
    sortKeypress(c);
    clearScreen();
    scrollCheck();
    sidescrollCheck();
    writeScreen();
  }
}