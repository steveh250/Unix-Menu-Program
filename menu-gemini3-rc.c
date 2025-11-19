/* * Refactored Menu Program
 * Original Author: Steve Harris
 * Refactored for Modularity and Safety
 */

#define _GNU_SOURCE // For getline
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <curses.h>
#include <newt.h>

/* --- Constants & Configuration --- */
#define MAX_MENU_ITEMS 30
#define MAX_DESC_LEN 31
#define MAX_CMD_LEN 81
#define SCREEN_WIDTH 80
#define MENU_HEADING_LEN 80

typedef enum {
    UI_MODE_CURSES,
    UI_MODE_NEWT
} UIMode;

/* --- Data Structures --- */
typedef struct {
    char description[MAX_DESC_LEN];
    char command[MAX_CMD_LEN];
} MenuItem;

typedef struct {
    MenuItem items[MAX_MENU_ITEMS];
    int count;
    char heading[MENU_HEADING_LEN];
    char description[MENU_HEADING_LEN];
    char *filename;
    UIMode mode;
    char *user_login;
    char *user_term;
} MenuContext;

/* --- Prototypes --- */
// Core Logic
void init_context(MenuContext *ctx, int argc, char *argv[]);
void load_menu_file(MenuContext *ctx);
void execute_item(const MenuItem *item);
void change_password(const char *user);
void cleanup_and_exit(MenuContext *ctx, int exit_code);

// UI: Curses
void run_curses_ui(MenuContext *ctx);

// UI: Newt
void run_newt_ui(MenuContext *ctx);

/* --- Main Entry Point --- */
int main(int argc, char *argv[]) {
    MenuContext ctx;
    
    // 1. Initialize and Parse Arguments
    init_context(&ctx, argc, argv);

    // 2. Load Data
    load_menu_file(&ctx);

    // 3. Dispatch UI
    if (ctx.mode == UI_MODE_CURSES) {
        run_curses_ui(&ctx);
    } else {
        run_newt_ui(&ctx);
    }

    cleanup_and_exit(&ctx, 0);
    return 0;
}

/* --- Core Logic Implementation --- */

void usage(const char *prog_name) {
    fprintf(stderr, "\nUsage: %s <-c | -n> menufile\n", prog_name);
    fprintf(stderr, " -c : curses based menu\n");
    fprintf(stderr, " -n : newt based menu\n");
    exit(EXIT_FAILURE);
}

void init_context(MenuContext *ctx, int argc, char *argv[]) {
    if (argc < 3) usage(argv[0]);

    // Set Mode
    if (strcmp(argv[1], "-c") == 0) ctx->mode = UI_MODE_CURSES;
    else if (strcmp(argv[1], "-n") == 0) ctx->mode = UI_MODE_NEWT;
    else usage(argv[0]);

    // Set Filename
    ctx->filename = argv[2];
    ctx->count = 0;

    // Environment Variables
    ctx->user_login = getenv("LOGNAME");
    if (!ctx->user_login) ctx->user_login = "UNKNOWN";
    
    ctx->user_term = getenv("TTY");
    if (!ctx->user_term) ctx->user_term = "NOTTY";
}

void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

void load_menu_file(MenuContext *ctx) {
    FILE *file = fopen(ctx->filename, "r");
    if (!file) {
        perror("Cannot open menu file");
        exit(EXIT_FAILURE);
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    // Read Header
    if ((read = getline(&line, &len, file)) != -1) {
        trim_newline(line);
        snprintf(ctx->heading, MENU_HEADING_LEN, "%s", line);
    }
    
    // Read Description
    if ((read = getline(&line, &len, file)) != -1) {
        trim_newline(line);
        snprintf(ctx->description, MENU_HEADING_LEN, "%s", line);
    }

    // Read Items
    while (ctx->count < MAX_MENU_ITEMS) {
        // Read Description line
        read = getline(&line, &len, file);
        if (read == -1) break;
        trim_newline(line);
        snprintf(ctx->items[ctx->count].description, MAX_DESC_LEN, "%s", line);

        // Read Command line
        read = getline(&line, &len, file);
        if (read == -1) break;
        trim_newline(line);
        snprintf(ctx->items[ctx->count].command, MAX_CMD_LEN, "%s", line);

        ctx->count++;
    }

    free(line);
    fclose(file);
}

void execute_item(const MenuItem *item) {
    // Suspend UI handled by caller usually, but good to clear screen
    system("clear");
    system(item->command);
    printf("\nPress Enter to return...");
    getchar();
}

void change_password(const char *user) {
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "passwd %s", user);
    system("clear");
    system(cmd);
    printf("\nPress Enter to return...");
    getchar();
}

void cleanup_and_exit(MenuContext *ctx, int exit_code) {
    if (ctx->mode == UI_MODE_CURSES) {
        endwin();
    } else {
        newtFinished();
    }
    exit(exit_code);
}

/* --- Curses UI Implementation --- */

int get_center_x(const char *text) {
    return (SCREEN_WIDTH - strlen(text)) / 2;
}

void curses_draw_header(MenuContext *ctx) {
    char line_sep[] = "                                                                                "; // 80 spaces
    
    move(0, 0);
    attron(A_UNDERLINE);
    addstr(line_sep);
    
    mvaddstr(0, get_center_x(ctx->heading), ctx->heading);
    
    // Env info
    mvaddstr(0, 0, ctx->user_login);
    mvaddstr(0, SCREEN_WIDTH - strlen(ctx->user_term), ctx->user_term);
    
    // Description
    attroff(A_UNDERLINE);
    mvaddstr(2, get_center_x(ctx->description), ctx->description);
}

void curses_draw_footer() {
    char prompt[] = "Enter Option : ";
    char exit_msg[] = "(Q/q to quit)";
    char pass_msg[] = "CP to change password";
    char line_sep[] = "                                                                                ";

    move(21, 0);
    attron(A_UNDERLINE);
    addstr(line_sep);
    mvaddstr(21, get_center_x(pass_msg), pass_msg);
    attroff(A_UNDERLINE);

    move(23, 0);
    clrtoeol();
    mvaddstr(23, 0, prompt);
    mvaddstr(23, SCREEN_WIDTH - strlen(exit_msg), exit_msg);
}

void curses_draw_body(MenuContext *ctx) {
    int start_y = 5;
    
    if (ctx->count <= 8) {
        // Single Column
        for (int i = 0; i < ctx->count; i++) {
            mvprintw(start_y + (i * 2), 30, "%d. %s", i + 1, ctx->items[i].description);
        }
    } else {
        // Dual Column
        int split = ctx->count / 2;
        for (int i = 0; i < split; i++) {
             mvprintw(start_y + i, 5, "%d. %s", i + 1, ctx->items[i].description);
        }
        for (int i = split; i < ctx->count; i++) {
             mvprintw(start_y + (i - split), 45, "%d. %s", i + 1, ctx->items[i].description);
        }
    }
}

void run_curses_ui(MenuContext *ctx) {
    initscr();
    cbreak(); 
    noecho(); // We will handle echo manually via scanw if needed, or usually just getch

    char input[10];
    int choice;

    while (1) {
        clear();
        curses_draw_header(ctx);
        curses_draw_body(ctx);
        curses_draw_footer(ctx);
        
        move(23, 15); // Cursor at prompt
        echo();
        refresh();
        
        getnstr(input, 3);
        noecho();

        if (strcasecmp(input, "q") == 0) {
            break;
        } else if (strcasecmp(input, "cp") == 0) {
            def_prog_mode(); // Save curses state
            endwin();
            change_password(ctx->user_login);
            reset_prog_mode(); // Restore curses state
        } else {
            choice = atoi(input);
            if (choice > 0 && choice <= ctx->count) {
                def_prog_mode();
                endwin();
                execute_item(&ctx->items[choice - 1]);
                reset_prog_mode();
            }
        }
    }
}

/* --- Newt UI Implementation --- */

void run_newt_ui(MenuContext *ctx) {
    newtComponent form, listbox, button_quit;
    struct newtExitStruct es;

    newtInit();
    newtCls();
    
    // Using standard color palette handled by library defaults usually
    // but explicit palette code can be re-added here if desired.

    newtDrawRootText(0, 0, ctx->heading);
    newtPushHelpLine("Scroll and select item to execute.");
    
    // Window centered
    newtOpenWindow(10, 3, 65, 20, ctx->description);

    form = newtForm(NULL, NULL, 0);
    
    listbox = newtListbox(15, 1, 10, NEWT_FLAG_RETURNEXIT | NEWT_FLAG_BORDER | NEWT_FLAG_SCROLL);
    
    // Populate List
    for (int i = 0; i < ctx->count; i++) {
        // We pass the index + 1 as the key
        newtListboxAppendEntry(listbox, ctx->items[i].description, (void *)(long)(i + 1));
    }
    
    // Add components
    newtFormAddComponents(form, listbox, NULL);

    while (1) {
        newtFormRun(form, &es);

        if (es.reason == NEWT_EXIT_COMPONENT && es.u.co == listbox) {
            int selection = (int)(long)newtListboxGetCurrent(listbox);
            
            if (selection > 0 && selection <= ctx->count) {
                newtSuspend();
                execute_item(&ctx->items[selection - 1]);
                newtResume();
                // Redraw after return
                newtDrawRootText(0, 0, ctx->heading); 
                newtPushHelpLine("Scroll and select item to execute.");
            }
        } else {
            // Exit on F12 or specific quit keys if configured, 
            // or we can add a "Quit" button. 
            // For now, assume F12/Esc exits form logic
            break; 
        }
    }

    newtFormDestroy(form);
    newtPopWindow();
}
