This menu.c was written back in the 90's for an IBM AIX system running some PICK applications.

I got it up an running on a Ubuntu 16.04 VM - use the following to get a compiled and running version.

Build on Ubuntu 16.04
=====================
1. Install curses libraries: 'sudo apt-get install libncurses5-dev libncursesw5-dev'
2. Compile the menu.c: 'gcc -v -o menu menu.c -lncurses'
3. This was written in the days of tty's so before running set the TTY env variable to something: 'export TTY="tty1"'
  - Either that or comment out the respective lines in the code.
4. Create a menu file - sample is attached.
5. Start it up: './menu menufile'

NOTE:
 - The menu app will decide whether to draw the menu in one or two columns depending on how many entries are in the menufile.
 - It was written for a system that used kerberos (hence the built in kbpasswd option) and PICK (hence the OFF/off exit commands).
 - If you use it or modify it, I'd appreciate a mention in the source code.

Disclaimer: I wasn't a developer when I wrote this - there will be some warnings during compilation and the error handling isn't
brilliant - trying leaving the TTY unset, sit back and watch the segmentation violation.

Planned Updates
===============
As time permits - here's the plan:
 - (DONE) Clear up the compilation warnings
 - (DONE) Add some error checking to deal with the TTY segmentation violation errors
 - Enhancements
   - Add up/down arrow support to highlight menu option
   - Remove the old 'OFF' ways of exiting the screen and just use Q/q
   - Maybe move to windows for the various areas of the screen or submenus (although it's so easy to call menu from within menu).
   - General code improvements
