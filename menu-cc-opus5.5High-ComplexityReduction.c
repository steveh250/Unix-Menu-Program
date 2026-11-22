/***************************************************************************/
/* Unix Menu Program                                                       */
/*                                                                         */
/* Reads a text "menufile" and displays it as a menu, using either a plain */
/* curses screen (-c) or a newt list box (-n). Selecting an entry runs its */
/* shell command and then returns to the menu.                             */
/*-------------------------------------------------------------------------*/
/* Original author : Steve Harris (steve.harris@shaw.ca)                   */
/* Licence         : GPL v3                                                */
/*-------------------------------------------------------------------------*/
/* This file is a version of menu-cc-opus5.5High.c reworked to lower its   */
/* cyclomatic and cognitive complexity. It keeps that file's bug fixes and */
/* the screen layout of menu.c. The main changes:                          */
/*  - Loops end on their own condition (quit choice, file end) instead of  */
/*    an infinite loop with an exit buried inside, removing nesting.       */
/*  - The UI is chosen once, as a function pointer, so main and the        */
/*    shutdown code no longer branch on curses vs newt.                    */
/*  - One column-drawing routine serves both the one- and two-column       */
/*    curses layouts.                                                      */
/*  - Line endings are stripped with strcspn instead of a loop.            */
/*  - Checks that could never fail were removed: menufile length limits    */
/*    keep centred text on screen, and the newt list box only holds valid  */
/*    item keys.                                                           */
/*  - The prompt line is drawn once per screen, not twice.                 */
/*                                                                         */
/* Build: gcc -o menu menu-cc-opus5.5High-ComplexityReduction.c            */
/*            -lncurses -lnewt                                             */
/* Usage: menu <-c | -n> menufile                                          */
/***************************************************************************/

#define _POSIX_C_SOURCE 200809L

#include <curses.h>
#include <newt.h>

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

/***************************************************************************/
/* Menufile limits                                                         */
/***************************************************************************/
#define MAX_MENU_ITEMS        30
#define MAX_HEADING_LEN       80
#define MAX_DESCRIPTION_LEN   80
#define MAX_ITEM_DESC_LEN     30
#define MAX_COMMAND_LEN       80

/***************************************************************************/
/* Curses screen layout (an 80x24 terminal)                                */
/***************************************************************************/
#define SCREEN_COLS           80
#define ROW_HEADING            0
#define ROW_DESCRIPTION        2
#define ROW_BODY_START         5
#define ROW_PASSWORD_HINT     21
#define ROW_PROMPT            23

#define SINGLE_COLUMN_MAX     8   /* More items than this: two columns   */
#define SINGLE_COLUMN_X       30
#define SINGLE_COLUMN_GAP     2   /* Rows between items (double spaced)  */
#define DUAL_LEFT_X           7
#define DUAL_RIGHT_X          (DUAL_LEFT_X + 44)
#define DUAL_COLUMN_GAP       1
#define ITEM_TEXT_OFFSET      4   /* Gap from item number to its text    */

#define INPUT_MAX_CHARS       3   /* Characters accepted at the prompt   */

static const char PROMPT_TEXT[]         = "Enter Option : ";
static const char EXIT_HINT_TEXT[]      = "(Q/q to quit)";
static const char PASSWORD_HINT_TEXT[]  = "CP to change password";
static const char NO_TTY_TEXT[]         = "NOTTY";
static const char CHANGE_PASSWORD_CMD[] = "clear;passwd $LOGNAME;clear";

/***************************************************************************/
/* Types                                                                   */
/***************************************************************************/
struct menu_item {
  char description[MAX_ITEM_DESC_LEN + 1];
  char command[MAX_COMMAND_LEN + 1];
};

struct menu {
  char heading[MAX_HEADING_LEN + 1];
  char description[MAX_DESCRIPTION_LEN + 1];
  struct menu_item items[MAX_MENU_ITEMS];
  int item_count;
};

/* State for reading the menufile one line at a time. */
struct line_reader {
  FILE *fp;
  char *buf;
  size_t cap;
  const char *prog;
};

/* Runs a menu until the user quits. One per UI (curses, newt). */
typedef void (*menu_runner)(const struct menu *menu);

/* Restores the terminal on exit: endwin or newtFinished once a UI has */
/* started, NULL before that. Read by the SIGINT handler.              */
static int (*volatile g_ui_teardown)(void) = NULL;

/***************************************************************************/
/* Newt colour palette                                                     */
/***************************************************************************/
static const struct newtColors menu_color_palette = {
  "white",     "blue",       /* root fg, bg                */
  "black",     "lightgray",  /* border fg, bg              */
  "black",     "lightgray",  /* window fg, bg              */
  "white",     "black",      /* shadow fg, bg              */
  "red",       "lightgray",  /* title fg, bg               */
  "lightgray", "red",        /* button fg, bg              */
  "red",       "lightgray",  /* active button fg, bg       */
  "lightgray", "blue",       /* checkbox fg, bg            */
  "lightgray", "red",        /* active checkbox fg, bg     */
  "lightgray", "blue",       /* entry box fg, bg           */
  "blue",      "lightgray",  /* label fg, bg               */
  "black",     "lightgray",  /* listbox fg, bg             */
  "lightgray", "blue",       /* active listbox fg, bg      */
  "black",     "lightgray",  /* textbox fg, bg             */
  "lightgray", "red",        /* active textbox fg, bg      */
  "white",     "blue",       /* help line                  */
  "lightgray", "blue",       /* root text                  */
  "blue",                    /* scale full                 */
  "red",                     /* scale empty                */
  "blue",      "lightgray",  /* disabled entry fg, bg      */
  "black",     "lightgray",  /* compact button fg, bg      */
  "lightgray", "red",        /* active & sel listbox       */
  "black",     "brown"       /* selected listbox           */
};

/***************************************************************************/
/* Function prototypes                                                     */
/***************************************************************************/
static void usage(const char *prog);
static menu_runner parse_arguments(int argc, char *argv[], const char **menu_path);
static int  read_field(struct line_reader *in, char *dest, size_t max_len, const char *label);
static int  read_menu_items(struct line_reader *in, struct menu_item items[]);
static void load_menu_file(const char *path, struct menu *menu, const char *prog);
static void quit_menu(int sig);

static void run_curses_menu(const struct menu *menu);
static const char *curses_prompt(const struct menu *menu, char input[]);
static void curses_handle_choice(const struct menu *menu, const char *input);
static void curses_draw_screen(const struct menu *menu);
static void curses_draw_column(const struct menu *menu, int first, int last,
                               int col, int row_gap);
static void curses_run_shell(const char *command);
static int  centered_column(const char *text);
static const char *env_or_default(const char *name, const char *fallback);

static void run_newt_menu(const struct menu *menu);
static int  newt_wait_for_selection(newtComponent form, newtComponent listbox);
static void newt_suspend_callback(void *data);
static void newt_help_callback(newtComponent co, void *tag);

/***************************************************************************/
/* main                                                                    */
/***************************************************************************/
int main(int argc, char *argv[]) {
  static struct menu menu;
  const char *menu_path = NULL;
  menu_runner run_menu = parse_arguments(argc, argv, &menu_path);

  load_menu_file(menu_path, &menu, argv[0]);
  signal(SIGINT, quit_menu);
  run_menu(&menu);
  return EXIT_SUCCESS;
}

/***************************************************************************/
/* Command line handling                                                   */
/***************************************************************************/
static void usage(const char *prog) {
  fprintf(stderr, "\nUsage: %s <-c | -n> menufile\n", prog);
  fprintf(stderr, " -c : curses base menu\n");
  fprintf(stderr, " -n : newt based menu\n");
  fprintf(stderr, " menufile : Input for menu, options and commands\n");
  fprintf(stderr, "\n See https://github.com/steveh250/Unix-Menu-Program for formatting.\n\n");
  exit(EXIT_FAILURE);
}

/* Returns the menu runner for the -c / -n flag; exits with usage otherwise. */
static menu_runner parse_arguments(int argc, char *argv[], const char **menu_path) {
  if (argc < 3) {
    usage(argv[0]);
  }
  *menu_path = argv[2];

  if (strcmp(argv[1], "-c") == 0) {
    return run_curses_menu;
  }
  if (strcmp(argv[1], "-n") == 0) {
    return run_newt_menu;
  }

  usage(argv[0]);
  return NULL; /* not reached */
}

/***************************************************************************/
/* Menufile loading                                                        */
/*                                                                         */
/* Format: line 1 = heading, line 2 = description, then pairs of lines     */
/* (item description, item command). At most MAX_MENU_ITEMS items are      */
/* read; any further lines are ignored. An item description with no       */
/* following command line is ignored.                                      */
/***************************************************************************/

/* Reads one line into dest, without its line ending. Returns 1 on success, */
/* 0 at end of file. A line longer than max_len is reported and exits.      */
static int read_field(struct line_reader *in, char *dest, size_t max_len, const char *label) {
  size_t len;

  if (getline(&in->buf, &in->cap, in->fp) == -1) {
    return 0;
  }

  in->buf[strcspn(in->buf, "\r\n")] = '\0';
  len = strlen(in->buf);

  if (len > max_len) {
    fprintf(stderr, "\n%s is too long (it must be at most %zu characters, it is %zu characters long.)\n",
            label, max_len, len);
    fprintf(stderr, " - %s: %s\n", label, in->buf);
    usage(in->prog);
  }

  memcpy(dest, in->buf, len + 1);
  return 1;
}

/* Reads description/command pairs until the file ends or the menu is full. */
static int read_menu_items(struct line_reader *in, struct menu_item items[]) {
  int count = 0;

  while (count < MAX_MENU_ITEMS &&
         read_field(in, items[count].description, MAX_ITEM_DESC_LEN, "Description") &&
         read_field(in, items[count].command, MAX_COMMAND_LEN, "Command")) {
    count++;
  }
  return count;
}

static void load_menu_file(const char *path, struct menu *menu, const char *prog) {
  struct line_reader in = { NULL, NULL, 0, prog };

  memset(menu, 0, sizeof(*menu));

  in.fp = fopen(path, "r");
  if (in.fp == NULL) {
    fprintf(stderr, "Cannot open your menu file '%s': %s\n", path, strerror(errno));
    usage(prog);
  }

  if (!read_field(&in, menu->heading, MAX_HEADING_LEN, "Menu Heading") ||
      !read_field(&in, menu->description, MAX_DESCRIPTION_LEN, "Menu Description")) {
    fprintf(stderr, "\nMenu file '%s' must start with a heading line and a description line.\n", path);
    usage(prog);
  }

  menu->item_count = read_menu_items(&in, menu->items);

  free(in.buf);
  fclose(in.fp);
}

/***************************************************************************/
/* Shutdown: used both as the SIGINT handler and for a normal quit.        */
/***************************************************************************/
static void quit_menu(int sig) {
  (void)sig;
  if (g_ui_teardown != NULL) {
    g_ui_teardown();
  }
  exit(EXIT_SUCCESS);
}

/***************************************************************************/
/* Curses menu                                                             */
/***************************************************************************/
static void run_curses_menu(const struct menu *menu) {
  char input[INPUT_MAX_CHARS + 1];

  initscr();
  g_ui_teardown = endwin;

  while (strcasecmp(curses_prompt(menu, input), "q") != 0) {
    curses_handle_choice(menu, input);
  }
  quit_menu(0);
}

/* Draws the menu and reads the user's choice into input, which it returns. */
static const char *curses_prompt(const struct menu *menu, char input[]) {
  curses_draw_screen(menu);
  raw();
  echo();
  nl();
  input[0] = '\0'; /* scanw leaves input unchanged on an empty line */
  scanw("%3s", input);
  return input;
}

/* Runs the command for "cp" or a menu number; anything else is ignored. */
static void curses_handle_choice(const struct menu *menu, const char *input) {
  int choice = atoi(input);

  if (strcasecmp(input, "cp") == 0) {
    curses_run_shell(CHANGE_PASSWORD_CMD);
  } else if (choice >= 1 && choice <= menu->item_count) {
    curses_run_shell(menu->items[choice - 1].command);
  }
}

/* Draws the whole screen and leaves the cursor at the prompt. */
static void curses_draw_screen(const struct menu *menu) {
  const char *login_name = env_or_default("LOGNAME", "");
  const char *login_term = env_or_default("TTY", NO_TTY_TEXT);
  int left_count = menu->item_count - menu->item_count / 2;

  attron(A_UNDERLINE);

  /* Heading bar: login name on the left, heading centred, TTY on the right. */
  mvprintw(ROW_HEADING, 0, "%*s", SCREEN_COLS, "");
  mvaddstr(ROW_HEADING, centered_column(menu->heading), menu->heading);
  mvaddstr(ROW_HEADING, 0, login_name);
  mvaddstr(ROW_HEADING, SCREEN_COLS - (int)strlen(login_term), login_term);

  mvaddstr(ROW_DESCRIPTION, centered_column(menu->description), menu->description);

  /* Password hint bar */
  mvprintw(ROW_PASSWORD_HINT, 0, "%*s", SCREEN_COLS, "");
  mvaddstr(ROW_PASSWORD_HINT, centered_column(PASSWORD_HINT_TEXT), PASSWORD_HINT_TEXT);

  attroff(A_UNDERLINE);

  /* Prompt line */
  move(ROW_PROMPT, 0);
  clrtoeol();
  mvaddstr(ROW_PROMPT, 0, PROMPT_TEXT);
  mvaddstr(ROW_PROMPT, SCREEN_COLS - (int)strlen(EXIT_HINT_TEXT), EXIT_HINT_TEXT);

  /* Menu items: one double-spaced column, or two single-spaced columns */
  /* with any odd item on the left.                                     */
  if (menu->item_count <= SINGLE_COLUMN_MAX) {
    curses_draw_column(menu, 0, menu->item_count, SINGLE_COLUMN_X, SINGLE_COLUMN_GAP);
  } else {
    curses_draw_column(menu, 0, left_count, DUAL_LEFT_X, DUAL_COLUMN_GAP);
    curses_draw_column(menu, left_count, menu->item_count, DUAL_RIGHT_X, DUAL_COLUMN_GAP);
  }

  move(ROW_PROMPT, (int)strlen(PROMPT_TEXT));
  refresh();
}

/* Draws items first..last-1 as a numbered column starting at the body row. */
static void curses_draw_column(const struct menu *menu, int first, int last,
                               int col, int row_gap) {
  int i;

  for (i = first; i < last; i++) {
    int row = ROW_BODY_START + (i - first) * row_gap;

    mvprintw(row, col, "%d.", i + 1);
    mvaddstr(row, col + ITEM_TEXT_OFFSET, menu->items[i].description);
  }
}

/* Leaves curses, runs a shell command, then restores the screen. */
static void curses_run_shell(const char *command) {
  endwin();
  (void)system(command);
  doupdate();
}

/* Column at which text is centred (matches menu.c). Text is at most */
/* SCREEN_COLS long, so the result is never negative.                */
static int centered_column(const char *text) {
  return (SCREEN_COLS + 1 - (int)strlen(text)) / 2;
}

static const char *env_or_default(const char *name, const char *fallback) {
  const char *value = getenv(name);
  return value != NULL ? value : fallback;
}

/***************************************************************************/
/* Newt menu                                                               */
/* Acknowledgment: based on https://pagure.io/newt/blob/master/f/test.c    */
/***************************************************************************/
static void run_newt_menu(const struct menu *menu) {
  newtComponent form, listbox;
  int quit_key = menu->item_count + 1;
  int selected;
  int i;

  newtInit();
  g_ui_teardown = newtFinished;
  newtCls();
  newtSetColors(menu_color_palette);
  newtRefresh();

  newtSetSuspendCallback(newt_suspend_callback, NULL);
  newtSetHelpCallback(newt_help_callback);

  newtDrawRootText(0, 0, menu->heading);
  newtPushHelpLine("Scroll up or down and select the menu item to execute.");
  newtOpenWindow(10, 3, 65, 23, menu->description);

  form = newtForm(NULL, "Scroll to the menu item, or type first letter of menu item, "
                        "and press enter to execute.", 0);

  listbox = newtListbox(15, 1, 20, NEWT_FLAG_RETURNEXIT | NEWT_FLAG_BORDER |
                                   NEWT_FLAG_SCROLL | NEWT_FLAG_SHOWCURSOR);

  /* List box keys are 1-based item numbers; "Quit" follows the last item. */
  for (i = 0; i < menu->item_count; i++) {
    newtListboxAppendEntry(listbox, menu->items[i].description, (void *)(intptr_t)(i + 1));
  }
  newtListboxAppendEntry(listbox, "Quit", (void *)(intptr_t)quit_key);

  newtFormAddComponents(form, listbox, NULL);
  newtRefresh();
  newtFormSetTimer(form, 200);

  /* Every key in the list box is an item number or quit_key, so any */
  /* other selection is a valid item.                                */
  for (selected = newt_wait_for_selection(form, listbox);
       selected != quit_key;
       selected = newt_wait_for_selection(form, listbox)) {
    /* Suspend newt so the command's output displays cleanly. */
    newtSuspend();
    (void)system(menu->items[selected - 1].command);
    newtResume();
  }

  newtFormDestroy(form);
  quit_menu(0);
}

/* Runs the form until an entry is chosen; returns that entry's key. */
static int newt_wait_for_selection(newtComponent form, newtComponent listbox) {
  struct newtExitStruct es;

  do {
    newtFormRun(form, &es);
    newtRefresh();
  } while (es.reason != NEWT_EXIT_COMPONENT);

  return (int)(intptr_t)newtListboxGetCurrent(listbox);
}

static void newt_suspend_callback(void *data) {
  (void)data;
  newtSuspend();
  raise(SIGTSTP);
  newtResume();
}

static void newt_help_callback(newtComponent co, void *tag) {
  (void)co;
  newtWinMessage("Help", "Ok", (char *)tag);
}
