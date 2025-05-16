/* Updated Menu Program in C - Refactored and Modernized */
/* 6/16/2025 - Had ChatGPT refactor the code - this was it's second attempt and works. */
/* NOTE: TREAT A RELEASE CANDIDTAE - HAS NOT BEEN FULLY TESTED. */

#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "newt.h"

#define MAX_NO_ITEMS 30
#define MENU_HEADING_LENGTH 80
#define MENU_DESC_LENGTH 80
#define DESC_LENGTH 30
#define CMD_LENGTH 80

typedef struct {
    char description[DESC_LENGTH + 1];
    char command[CMD_LENGTH + 1];
} MenuItem;

typedef struct {
    MenuItem items[MAX_NO_ITEMS];  // Holds all the menu items
    int totalItems;
    char heading[MENU_HEADING_LENGTH + 1];
    char description[MENU_DESC_LENGTH + 1];
    char *loginName;
    char *loginTerm;
    char codePath[8];   // Tracks the selected UI mode (CURSES or NEWT)
} MenuContext;

void die(MenuContext *ctx);
void usage(const char *command);
int readMenuFile(const char *fileName, MenuContext *ctx);
void constructMenu(MenuContext *ctx);
int drawMenuBody(MenuContext *ctx, int startY, int startX, int itemGap);
int drawDualBody(MenuContext *ctx, int startY, int startX);
void drawBottomLine(void);
void getOption(MenuContext *ctx);
int findCenter(const char *text);

int main(int argc, char *argv[]) {
    MenuContext ctx = { .totalItems = 0 };
    ctx.loginName = getenv("LOGNAME");
    ctx.loginTerm = getenv("TTY") ? getenv("TTY") : "NOTTY"; // Fallback if TTY is not set

    if (argc != 3) usage(argv[0]);

    if (strcmp(argv[1], "-c") == 0) {
        strcpy(ctx.codePath, "CURSES");
    } else if (strcmp(argv[1], "-n") == 0) {
        strcpy(ctx.codePath, "NEWT");
    } else {
        usage(argv[0]);
    }

    if (readMenuFile(argv[2], &ctx) != 0) exit(EXIT_FAILURE); // Now safely reads input with buffer checks and trimming
    signal(SIGINT, (void (*)(int)) die);

    if (strcmp(ctx.codePath, "CURSES") == 0) {
        initscr();
        while (1) {
            constructMenu(&ctx);
            if (ctx.totalItems <= 8) drawMenuBody(&ctx, 5, 30, 2);
            else drawDualBody(&ctx, 5, 7);
            move(23, 15);
            refresh();
            getOption(&ctx);
        }
    } else {
        newtInit();
        newtCls();
        newtSetColors(newtDefaultColorPalette);
        newtRefresh();
        newtDrawRootText(0, 0, ctx.heading);
        newtPushHelpLine("Scroll to the menu item and press enter.");
        newtOpenWindow(10, 3, 65, 23, ctx.description);
        newtComponent lb = newtListbox(15, 1, 20, NEWT_FLAG_RETURNEXIT | NEWT_FLAG_BORDER |
                                       NEWT_FLAG_SCROLL | NEWT_FLAG_SHOWCURSOR);

        for (int i = 0; i < ctx.totalItems; ++i)
            newtListboxAppendEntry(lb, ctx.items[i].description, (void *)(intptr_t)(i + 1));

        int quitMenuItem = ctx.totalItems + 1;
        newtListboxAppendEntry(lb, "Quit", (void *)(intptr_t)quitMenuItem);
        newtComponent f = newtForm(NULL, NULL, 0);
        newtFormAddComponent(f, lb);
        newtRefresh();

        struct newtExitStruct es;
        while (1) {
            newtFormRun(f, &es);
            int selection = (intptr_t)newtListboxGetCurrent(lb);
            if (selection == quitMenuItem) die(&ctx);
            newtSuspend();
            system(ctx.items[selection - 1].command);
            newtResume();
        }
    }
    return 0;
}

int readMenuFile(const char *fileName, MenuContext *ctx) {
    FILE *file = fopen(fileName, "r");
    if (!file) {
        perror("Error opening menu file");
        return -1;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    if ((read = getline(&line, &len, file)) > 0) {
        strncpy(ctx->heading, line, MENU_HEADING_LENGTH); // Safer string handling using strncpy
        ctx->heading[strcspn(ctx->heading, "\n")] = '\0';
    }
    if ((read = getline(&line, &len, file)) > 0) {
        strncpy(ctx->description, line, MENU_DESC_LENGTH);
        ctx->description[strcspn(ctx->description, "\n")] = '\0';
    }

    while ((read = getline(&line, &len, file)) != -1 && ctx->totalItems < MAX_NO_ITEMS) {
        strncpy(ctx->items[ctx->totalItems].description, line, DESC_LENGTH);
        ctx->items[ctx->totalItems].description[strcspn(ctx->items[ctx->totalItems].description, "\n")] = '\0';
        if ((read = getline(&line, &len, file)) == -1) break;
        strncpy(ctx->items[ctx->totalItems].command, line, CMD_LENGTH);
        ctx->items[ctx->totalItems].command[strcspn(ctx->items[ctx->totalItems].command, "\n")] = '\0';
        ctx->totalItems++;
    }
    free(line);
    fclose(file);
    return 0;
}

void constructMenu(MenuContext *ctx) {
    move(0, 0); attron(A_UNDERLINE); addstr("                                                                                ");
    move(0, findCenter(ctx->heading)); addstr(ctx->heading);
    move(0, 0); addstr(ctx->loginName);
    move(0, 80 - strlen(ctx->loginTerm)); addstr(ctx->loginTerm);
    move(2, findCenter(ctx->description)); addstr(ctx->description);
    attroff(A_UNDERLINE);
    drawBottomLine();
}

int drawMenuBody(MenuContext *ctx, int startY, int startX, int itemGap) {
    for (int i = 0; i < ctx->totalItems && i < 8; ++i) {
        move(startY + (i * itemGap), startX);
        printw("%d. %s", i + 1, ctx->items[i].description);
    }
    return 0;
}

int drawDualBody(MenuContext *ctx, int startY, int startX) {
    int mid = ctx->totalItems / 2 + ctx->totalItems % 2;
    for (int i = 0; i < mid; ++i) {
        move(startY + i, startX);
        printw("%d. %s", i + 1, ctx->items[i].description);
    }
    for (int i = mid; i < ctx->totalItems; ++i) {
        move(startY + i - mid, startX + 40);
        printw("%d. %s", i + 1, ctx->items[i].description);
    }
    return 0;
}

void drawBottomLine() {
    move(23, 0); clrtoeol();
    addstr("Enter Option: ");
    move(23, 80 - strlen("(Q/q to quit)"));
    addstr("(Q/q to quit)");
}

void getOption(MenuContext *ctx) {
    char input[4];
    echo(); raw();
    scanw("%3s", input);
    if (strcasecmp(input, "q") == 0) die(ctx);
    else if (strcasecmp(input, "cp") == 0) {
        endwin();
        system("clear; passwd $LOGNAME; clear");
        doupdate();
    } else {
        char *endptr;
    long opt = strtol(input, &endptr, 10); // Replaces atoi with strtol for better input validation
        if (*endptr == '\0' && opt > 0 && opt <= ctx->totalItems) {
            endwin();
            system(ctx->items[opt - 1].command);
            doupdate();
        }
    }
    drawBottomLine();
}

void die(MenuContext *ctx) {
    if (strcmp(ctx->codePath, "CURSES") == 0) endwin();
    else {
        newtPopWindow();
        newtFinished();
    }
    exit(0);
}

int findCenter(const char *text) {
    return (80 - strlen(text)) / 2;
}

void usage(const char *command) {
    fprintf(stderr, "Usage: %s <-c | -n> menufile\n", command);
    fprintf(stderr, "  -c : curses-based menu\n");
    fprintf(stderr, "  -n : newt-based menu\n");
    fprintf(stderr, "  menufile : path to menu text file\n");
    exit(EXIT_FAILURE);
}
