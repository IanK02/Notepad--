# Notepad--

Notepad-- is a simple text editor writtten in C without the curses library. It features simple keyword highlighting and a search function.  [Demo](https://youtu.be/eKTMT4QBZ7o)

## Install and Use
1. Download the code into a .zip
2. Unzip the .zip file
3. Using a Linux Terminal cd into the unzipped folder
4. Type `make` at the command line to compile the editor. Note that some systems do not come with make installed by default and you may need to type `sudo apt install make`.
5. Use `./notepadmm <filename>` to open the editor, or `./notepadmm` to create a new file
6. Use ctrl+S to save work, ctrl+B to search, and ctrl+C to quit

## Important Notes
Notepad-- only works on Linux in Linux terminals. That means that even if you are using something like WSL or Cygwin but try to run Notepad-- from within Windows Command Propmpt it will not work. This does not mean Notepad-- can't run on Linux subsystems, you just have to use a Linux terminal, I use [konsole](https://gnome-terminator.org/) and [terminator](https://gnome-terminator.org/) but any emulator should work.

## About the Project
Notepad-- is inspired heavily by antirez's [kilo](https://github.com/antirez/kilo) and the accompanying [tutorial](https://viewsourcecode.org/snaptoken/kilo/) walking you through the source code of kilo. Much of the high level structure of Notepad-- is modeled after kilo. Notepad-- is also my first project in C, having only written Hello, World! before this. I haven't gotten around to proper, extensive testing of the editor and as such there are doubtless many bugs I have not caught.
