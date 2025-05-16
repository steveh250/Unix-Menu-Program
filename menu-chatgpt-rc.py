# 5/16/2025: This is a Python version created directly by ChatGPT - after a fair bit of back and fore to recreate the newt style interface.
# NOTE: TREAT THIS AS A RELEASE CANDIDATE AS IT HAS NOT BEEN FULLY TESTED.

import curses
import subprocess
import sys
import os

try:
    from dialog import Dialog
    DIALOG_AVAILABLE = True
except ImportError:
    DIALOG_AVAILABLE = False

MAX_NO_ITEMS = 30

class MenuItem:
    def __init__(self, description, command):
        self.description = description
        self.command = command

class Menu:
    def __init__(self, heading, description, items):
        self.heading = heading
        self.description = description
        self.items = items[:MAX_NO_ITEMS]

    def run_curses(self):
        curses.wrapper(self.menu_loop)

    def draw_menu(self, stdscr):
        curses.curs_set(0)
        stdscr.clear()
        h, w = stdscr.getmaxyx()

        stdscr.addstr(0, (w - len(self.heading)) // 2, self.heading, curses.A_BOLD | curses.A_UNDERLINE)
        stdscr.addstr(2, (w - len(self.description)) // 2, self.description, curses.A_UNDERLINE)

        for i, item in enumerate(self.items):
            x = 4 if i < 15 else w // 2
            y = 5 + (i % 15)
            stdscr.addstr(y, x, f"{i + 1}. {item.description}")

        stdscr.addstr(h - 2, 0, "Enter Option (number), 'cp' to change password, or 'q' to quit:")
        stdscr.refresh()

    def menu_loop(self, stdscr):
        while True:
            self.draw_menu(stdscr)
            curses.echo()
            choice = stdscr.getstr().decode().strip()

            if choice.lower() == 'q':
                break
            elif choice.lower() == 'cp':
                curses.endwin()
                subprocess.run(['clear'])
                subprocess.run(['passwd'])
            elif choice.isdigit():
                index = int(choice) - 1
                if 0 <= index < len(self.items):
                    curses.endwin()
                    subprocess.run(self.items[index].command, shell=True)
                    input("Press Enter to return to the menu...")

    def run_newt(self):
        if not DIALOG_AVAILABLE:
            print("python3-dialog module not available. Please install 'dialog' and 'pythondialog'.")
            sys.exit(1)

        d = Dialog(dialog="dialog")
        d.set_background_title(self.heading)

        while True:
            choices = [(str(i + 1), item.description) for i, item in enumerate(self.items)]
            choices.append(('q', 'Quit'))

            code, tag = d.menu(self.description, choices=choices)

            if code in (d.CANCEL, d.ESC) or tag == 'q':
                break
            elif tag == 'cp':
                subprocess.run(['clear'])
                subprocess.run(['passwd'])
            elif tag.isdigit():
                index = int(tag) - 1
                if 0 <= index < len(self.items):
                    subprocess.run(self.items[index].command, shell=True)
                    input("Press Enter to return to the menu...")


def load_menu_file(filename):
    with open(filename, 'r') as f:
        heading = f.readline().strip()
        description = f.readline().strip()
        items = []
        while True:
            desc = f.readline()
            cmd = f.readline()
            if not desc or not cmd:
                break
            items.append(MenuItem(desc.strip(), cmd.strip()))
        return Menu(heading, description, items)


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 menu.py <-c | -n> <menufile>")
        print("  -c : curses-based menu")
        print("  -n : newt-style menu (via python3-dialog)")
        sys.exit(1)

    mode = sys.argv[1]
    menufile = sys.argv[2]
    menu = load_menu_file(menufile)

    if mode == '-c':
        menu.run_curses()
    elif mode == '-n':
        menu.run_newt()
    else:
        print("Invalid mode. Use -c for curses or -n for newt.")
        sys.exit(1)

if __name__ == '__main__':
    main()
