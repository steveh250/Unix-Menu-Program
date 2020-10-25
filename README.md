This menu.c was written back in the 90's for an IBM AIX system running some PICK applications.

I got it up an running on a Ubuntu 16.04 VM - use the following to get a compiled and running version.

Build on Ubuntu 16.04
=====================
1. Install curses libraries: 'sudo apt-get install libncurses5-dev libncursesw5-dev'
2. Compile the menu.c: 'gcc -v -o menu menu.c -lncurses'
3. Create a menu file - sample is attached.
4. Start it up: './menu menufile'

menufile - format
=================
'''
Line 1 - Main menu heading at top of screen
Line 2 - Menu heading
Option 1 - heading
Option 1 - command to run: either commands to run or call a shell script if longer than 250 chars
Option 2 - Menu option description that will appear on the menu
Option 2 - command to run
Option.....
'''

NOTE:
 - The menu app will decide whether to draw the menu in one or two columns depending on how many entries are in the menufile.
 - If you use it or modify it, I'd appreciate a mention in the source code.

Planned Updates
===============
As time permits - here's the plan:
 - (DONE) Clear up the compilation warnings
 - (DONE) Add some error checking to deal with the TTY segmentation violation errors
 - Enhancements
   - Add up/down arrow support to highlight menu option
   - (DONE) Remove the old 'OFF' ways of exiting the screen and just use Q/q
   - Maybe move to windows for the various areas of the screen or submenus (although it's so easy to call menu from within menu).
   - General code improvements
     - (DONE) Changed kerbpass to passwd
     - (DONE) Fixed a bug where after an execution the menu would redraw and leave the last menu option number selected displayed at the bottom of the screen - there was an redundant printw() - echo() had already been set.
