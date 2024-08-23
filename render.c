#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/ioctl.h>

struct termios termios_o; //terminal configuration object

struct winsize w; //window size

struct cursor { //cursor object
    int x;
    int y;
};

struct row { //single row of text
    int size;
    char *chars;
};

struct cursor C = {0,0}; //define the global cursor

#define CTRL_KEY(k) ((k) & 0x1f) //used to check if ctrl + some key was pressed

void exitRawMode(void){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios_o);
}

void enableRawMode(void){
    tcgetattr(STDIN_FILENO, &termios_o);
    struct termios termios_r = termios_o;
    atexit(exitRawMode);
    termios_r.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                    | INLCR | IGNCR | ICRNL | IXON);
    termios_r.c_oflag &= ~OPOST;
    termios_r.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    termios_r.c_cflag &= ~(CSIZE | PARENB);
    termios_r.c_cflag |= CS8;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios_r);
}

char processKeypress(){
    char c = '\0';
    read(STDIN_FILENO, &c, 1);
    //printf("%c;%u\r\n", c, c);
    if(c == CTRL_KEY('c')){
        write(STDOUT_FILENO, "\x1b[2J", 4); //clear entire screen
        write(STDOUT_FILENO, "\x1b[f", 3);  //move cursor to top left of screen
        exit(0);
    }
    if(c == '\x1b'){
        char buf[3];
        buf[0] = c;
        read(STDIN_FILENO, buf + 1, 1); //read next byte of input into buf
        read(STDIN_FILENO, buf + 2, 1); //read one more byte of input into buf
        if(buf[1] == '['){
            switch (buf[2])
            {
            case 'A':
                return 'w';
                break;
            case 'B':
                return 's';
                break;
            case 'C':
                return 'd';
                break;
            case 'D':
                return 'a';
                break;
            default:
                return '\x1b';
                break;
            }
        } else {
            return '\x1b';
        }
    }
    return c;
}

void cursor_move_cmd(void){ //move cursor to location specified by global cursor
    int buf_size = snprintf(NULL, 0, "\x1b[%d;%dH", C.y, C.x) + 1;
    char *buf = malloc(buf_size);
    snprintf(buf, buf_size, "\x1b[%d;%dH", C.y, C.x);
    write(STDOUT_FILENO, buf, buf_size - 1);
    free(buf);
}

void moveCursor(char input_char){ //process keypresses to move cursor
    switch (input_char)
    {
    case 'w':
        if(C.y < 0){
            C.y = 0;
        } else {
            C.y--;
        }
        cursor_move_cmd();
        //write(STDOUT_FILENO, "\x1b[A", 3);
        break;
    case 'a':
        if(C.x < 0){
            C.x = 0;
        } else {
            C.x--;
        }
        cursor_move_cmd();
        //write(STDOUT_FILENO, "\x1b[D", 3);
        break;
    case 's':
        if(C.y > w.ws_row){
            C.y = w.ws_row;
        } else {
            C.y++;
        }
        cursor_move_cmd();
        //write(STDOUT_FILENO, "\x1b[B", 3);
        break;
    case 'd':
        if(C.x > w.ws_col){
            C.x = w.ws_col;
        } else {
            C.x++;
        }
        cursor_move_cmd();
        //write(STDOUT_FILENO, "\x1b[C", 3);
        break;
    default:
        break;
    }
}


void getWinSize(void){
    ioctl(0, TIOCGWINSZ, &w);
}


int main(void){
    write(STDOUT_FILENO, "\x1b[2J", 4); //clear the screen
    enableRawMode();
    struct row test_row = {5, "hello"};
    //displayRow(&test_row);
    write(STDOUT_FILENO, "\x1b[f", 3);  //move cursor to top left of screen
    getWinSize();
    char c;
    while(1){
        c = processKeypress();
        moveCursor(c);
    }
}