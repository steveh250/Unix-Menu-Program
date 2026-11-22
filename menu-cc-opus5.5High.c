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
/* This file is a refactor of menu.c (version 1.14). See menu.c for the    */
/* full revision history. Behaviour and screen layout are unchanged, with  */
/* these fixes:                                                            */
/*  - Buffer overflows fixed (text fields, keyboard input buffer).         */
/*  - Menu choices out of range are ignored instead of reading past the    */
/*    end of the menu array.                                               */
/*  - Trailing CR/LF is stripped correctly, including on a last line that  */
/*    has no newline, and DOS (CRLF) menufiles.                            */
/*  - A menufile missing its heading or description lines is reported.     */
/*  - An unset LOGNAME is handled explicitly (no NULL passed to curses).   */
/*  - The colour palette no longer redefines newt's own                    */
/*    newtDefaultColorPalette symbol.                                      */
/*  - Global state is replaced by a menu structure passed to functions.    */
/*                                                                         */
/* Build: gcc -o menu menu-cc-opus5.5High.c -lncurses -lnewt               */
/* Usage: menu <-c | -n> menufile                                          */
/***************************************************************************/

#define _POSIX_C_SOURCE 200809L

#include <curses.h>
#include <newt.h>

#include <ctype.h>
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
#define ITEM_TEXT_OFFSET      4   /* Gap from item number to its text    */

#define INPUT_MAX_CHARS       3   /* Characters accepted at the prompt   */

static const char PROMPT_TEXT[]        = "Enter Option : ";
static const char EXIT_HINT_TEXT[]     = "(Q/q to quit)";
static const char PASSWORD_HINT_TEXT[] = "CP to change password";
static const char NO_TTY_TEXT[]        = "NOTTY";
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

enum ui_mode {
  UI_CURSES,
  UI_NEWT
};

/* Shared with the SIGINT handler, which needs to know what to tear down. */
static enum ui_mode g_ui_mode = UI_CURSES;
static volatile sig_atomic_t g_ui_active = 0;

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
static enum ui_mode parse_arguments(int argc, char *argv[], const char **menu_path);
static void load_menu_file(const char *path, struct menu *menu, const char *prog);
static int read_field(FILE *fp, char **buf, size_t *cap, char *dest,
                      size_t max_len, const char *label, const char *prog);
static void shutdown_and_exit(int status);
static void handle_interrupt(int sig);

static void run_curses_menu(const struct menu *menu);
static void curses_draw_screen(const struct menu *menu, const char *login_name,
                               const char *login_term);
static void curses_draw_item(int row, int col, int number, const char *text);
static void curses_draw_single_column(const struct menu *menu);
static void curses_draw_two_columns(const struct menu *menu);
static void curses_draw_prompt_line(void);
static void curses_handle_input(const struct menu *menu);
static void curses_run_shell(const char *command);
static int  centered_column(const char *text);

static void run_newt_menu(const struct menu *menu);
static void newt_suspend_callback(void *data);
static void newt_help_callback(newtComponent co, void *tag);

/***************************************************************************/
/* main                                                                    */
/***************************************************************************/
int main(int argc, char *argv[]) {
  static struct menu menu;
  const char *menu_path = NULL;

  g_ui_mode = parse_arguments(argc, argv, &menu_path);
  load_menu_file(menu_path, &menu, argv[0]);

  signal(SIGINT, handle_interrupt);

  if (g_ui_mode == UI_CURSES) {
    run_curses_menu(&menu);
  } else {
    run_newt_menu(&menu);
  }

  /* Both menus loop until the user quits, which exits the program. */
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

static enum ui_mode parse_arguments(int argc, char *argv[], const char **menu_path) {
  enum ui_mode mode = UI_CURSES;

  if (argc < 3) {
    usage(argv[0]);
  }

  if (strcmp(argv[1], "-c") == 0) {
    mode = UI_CURSES;
  } else if (strcmp(argv[1], "-n") == 0) {
    mode = UI_NEWT;
  } else {
    usage(argv[0]);
  }

  *menu_path = argv[2];
  return mode;
}

/***************************************************************************/
/* Menufile loading                                                        */
/*                                                                         */
/* Format: line 1 = heading, line 2 = description, then pairs of lines     */
/* (item description, item command). At most MAX_MENU_ITEMS items are      */
/* read; any further lines are ignored. An item description with no       */
/* following command line is ignored.                                      */
/***************************************************************************/

/* Reads one line into dest (without its line ending). Returns 1 on success, */
/* 0 at end of file. A line longer than max_len is reported and exits.       */
static int read_field(FILE *fp, char **buf, size_t *cap, char *dest,
                      size_t max_len, const char *label, const char *prog) {
  ssize_t nread = getline(buf, cap, fp);
  size_t len;

  if (nread == -1) {
    return 0;
  }

  len = (size_t)nread;
  while (len > 0 && ((*buf)[len - 1] == '\n' || (*buf)[len - 1] == '\r')) {
    len--;
  }
  (*buf)[len] = '\0';

  if (len > max_len) {
    fprintf(stderr, "\n%s is too long (it must be at most %zu characters, it is %zu characters long.)\n",
            label, max_len, len);
    fprintf(stderr, " - %s: %s\n", label, *buf);
    free(*buf);
    fclose(fp);
    usage(prog);
  }

  memcpy(dest, *buf, len + 1);
  return 1;
}

static void load_menu_file(const char *path, struct menu *menu, const char *prog) {
  FILE *fp;
  char *buf = NULL;
  size_t cap = 0;
  int count = 0;

  memset(menu, 0, sizeof(*menu));

  fp = fopen(path, "r");
  if (fp == NULL) {
    fprintf(stderr, "Cannot open your menu file '%s': %s\n", path, strerror(errno));
    usage(prog);
  }

  if (!read_field(fp, &buf, &cap, menu->heading, MAX_HEADING_LEN, "Menu Heading", prog) ||
      !read_field(fp, &buf, &cap, menu->description, MAX_DESCRIPTION_LEN, "Menu Description", prog)) {
    fprintf(stderr, "\nMenu file '%s' must start with a heading line and a description line.\n", path);
    free(buf);
    fclose(fp);
    usage(prog);
  }

  while (count < MAX_MENU_ITEMS) {
    struct menu_item *item = &menu->items[count];

    if (!read_field(fp, &buf, &cap, item->description, MAX_ITEM_DESC_LEN, "Description", prog) ||
        !read_field(fp, &buf, &cap, item->command, MAX_COMMAND_LEN, "Command", prog)) {
      break;
    }
    count++;
  }

  menu->item_count = count;
  free(buf);
  fclose(fp);
}

/***************************************************************************/
/* Shutdown                                                                */
/***************************************************************************/
static void shutdown_and_exit(int status) {
  if (g_ui_active) {
    if (g_ui_mode == UI_CURSES) {
      endwin();
    } else {
      newtPopWindow();
      newtFinished();
    }
    g_ui_active = 0;
  }
  exit(status);
}

static void handle_interrupt(int sig) {
  (void)sig;
  shutdown_and_exit(EXIT_SUCCESS);
}

/***************************************************************************/
/* Curses menu                                                             */
/***************************************************************************/
static void run_curses_menu(const struct menu *menu) {
  const char *login_name = getenv("LOGNAME");
  const char *login_term = getenv("TTY");

  if (login_name == NULL) {
    login_name = "";
  }
  if (login_term == NULL) {
    login_term = NO_TTY_TEXT;
  }

  initscr();
  g_ui_active = 1;

  for (;;) {
    curses_draw_screen(menu, login_name, login_term);

    if (menu->item_count <= SINGLE_COLUMN_MAX) {
      curses_draw_single_column(menu);
    } else {
      curses_draw_two_columns(menu);
    }

    move(ROW_PROMPT, (int)strlen(PROMPT_TEXT));
    refresh();

    curses_handle_input(menu);
  }
}

/* Column at which text is centred on the screen (matches menu.c layout). */
static int centered_column(const char *text) {
  int col = (SCREEN_COLS + 1 - (int)strlen(text)) / 2;
  return col < 0 ? 0 : col;
}

/* Heading bar, description, password hint bar and prompt line. */
static void curses_draw_screen(const struct menu *menu, const char *login_name,
                               const char *login_term) {
  int term_col = SCREEN_COLS - (int)strlen(login_term);

  attron(A_UNDERLINE);

  /* Heading bar: login name on the left, heading centred, TTY on the right. */
  mvprintw(ROW_HEADING, 0, "%*s", SCREEN_COLS, "");
  mvaddstr(ROW_HEADING, centered_column(menu->heading), menu->heading);
  mvaddstr(ROW_HEADING, 0, login_name);
  mvaddstr(ROW_HEADING, term_col < 0 ? 0 : term_col, login_term);

  mvaddstr(ROW_DESCRIPTION, centered_column(menu->description), menu->description);

  /* Password hint bar */
  mvprintw(ROW_PASSWORD_HINT, 0, "%*s", SCREEN_COLS, "");
  mvaddstr(ROW_PASSWORD_HINT, centered_column(PASSWORD_HINT_TEXT), PASSWORD_HINT_TEXT);

  attroff(A_UNDERLINE);

  curses_draw_prompt_line();
}

static void curses_draw_item(int row, int col, int number, const char *text) {
  mvprintw(row, col, "%d.", number);
  mvaddstr(row, col + ITEM_TEXT_OFFSET, text);
}

/* Up to SINGLE_COLUMN_MAX items, double spaced, in one column. */
static void curses_draw_single_column(const struct menu *menu) {
  int i;

  for (i = 0; i < menu->item_count && i < SINGLE_COLUMN_MAX; i++) {
    curses_draw_item(ROW_BODY_START + i * SINGLE_COLUMN_GAP, SINGLE_COLUMN_X,
                     i + 1, menu->items[i].description);
  }
}

/* Items split across two single-spaced columns; the left takes any extra. */
static void curses_draw_two_columns(const struct menu *menu) {
  int left_count = menu->item_count - menu->item_count / 2;
  int i;

  for (i = 0; i < menu->item_count; i++) {
    int in_left = i < left_count;
    int row = ROW_BODY_START + (in_left ? i : i - left_count);
    int col = in_left ? DUAL_LEFT_X : DUAL_RIGHT_X;

    curses_draw_item(row, col, i + 1, menu->items[i].description);
  }
}

/* Prompt line; does not refresh, the caller should. */
static void curses_draw_prompt_line(void) {
  move(ROW_PROMPT, 0);
  clrtoeol();
  mvaddstr(ROW_PROMPT, 0, PROMPT_TEXT);
  mvaddstr(ROW_PROMPT, SCREEN_COLS - (int)strlen(EXIT_HINT_TEXT), EXIT_HINT_TEXT);
}

/* Leaves curses, runs a shell command, then restores the screen. */
static void curses_run_shell(const char *command) {
  endwin();
  (void)system(command);
  doupdate();
}

/* Reads the user's choice at the prompt and acts on it. */
static void curses_handle_input(const struct menu *menu) {
  char input[INPUT_MAX_CHARS + 1] = "";
  char *end;
  long choice;

  raw();
  echo();
  nl();
  scanw("%3s", input);

  if (strcasecmp(input, "q") == 0) {
    shutdown_and_exit(EXIT_SUCCESS);
  } else if (strcasecmp(input, "cp") == 0) {
    curses_run_shell(CHANGE_PASSWORD_CMD);
  } else if (isdigit((unsigned char)input[0])) {
    choice = strtol(input, &end, 10);
    if (*end == '\0' && choice >= 1 && choice <= menu->item_count) {
      curses_run_shell(menu->items[choice - 1].command);
    }
  }

  curses_draw_prompt_line();
}

/***************************************************************************/
/* Newt menu                                                               */
/* Acknowledgment: based on https://pagure.io/newt/blob/master/f/test.c    */
/***************************************************************************/
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

static void run_newt_menu(const struct menu *menu) {
  newtComponent form, listbox;
  struct newtExitStruct es;
  int quit_key;
  int i;

  newtInit();
  g_ui_active = 1;
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
  quit_key = menu->item_count + 1;
  newtListboxAppendEntry(listbox, "Quit", (void *)(intptr_t)quit_key);

  newtFormAddComponents(form, listbox, NULL);
  newtRefresh();
  newtFormSetTimer(form, 200);

  for (;;) {
    int selected;

    do {
      newtFormRun(form, &es);
      newtRefresh();
    } while (es.reason != NEWT_EXIT_COMPONENT);

    selected = (int)(intptr_t)newtListboxGetCurrent(listbox);

    if (selected == quit_key) {
      newtFormDestroy(form);
      shutdown_and_exit(EXIT_SUCCESS);
    }

    if (selected >= 1 && selected <= menu->item_count) {
      /* Suspend newt so the command's output displays cleanly. */
      newtSuspend();
      (void)system(menu->items[selected - 1].command);
      newtResume();
    }
  }
}
