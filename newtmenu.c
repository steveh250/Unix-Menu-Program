#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
 
#include "newt.h"
 
struct callbackInfo {
    newtComponent en;
    char * state;
};
 
void disableCallback(newtComponent co, void * data) {
    struct callbackInfo * cbi = data;
 
    if (*cbi->state == ' ') {
        newtEntrySetFlags(cbi->en, NEWT_FLAG_DISABLED, NEWT_FLAGS_RESET);
    } else {
        newtEntrySetFlags(cbi->en, NEWT_FLAG_DISABLED, NEWT_FLAGS_SET);
    }
 
    newtRefresh();
}
 
void suspend(void * d) {
    newtSuspend();
    raise(SIGTSTP);
    newtResume();
}
 
void helpCallback(newtComponent co, void * tag) {
    newtWinMessage("Help", "Ok", tag);
}
 
int main(void) {
    newtComponent lb, b1, f, t;
    struct callbackInfo cbis[3];
    char results[10];
    struct newtExitStruct es;
 
    newtInit();
    newtCls();
 
    newtSetSuspendCallback(suspend, NULL);
    newtSetHelpCallback(helpCallback);
 
    /* Chenge the following to use the headings */
    newtDrawRootText(0, 0, "Newt test program");
    newtPushHelpLine(NULL);
    newtDrawRootText(-50, 0, "More root text");
 
    newtOpenWindow(10, 5, 65, 16, "window 2");
 
    f = newtForm(NULL, "This is some help text", 0);
 
    b1 = newtCompactButton(3, 1, "Exit");
 
    newtFormAddComponents(f, b1, NULL);

    lb = newtListbox(45, 1, 6, NEWT_FLAG_RETURNEXIT | NEWT_FLAG_BORDER |
                                NEWT_FLAG_SCROLL | NEWT_FLAG_SHOWCURSOR);
    newtListboxAppendEntry(lb, "First", (void *) 1);
    newtListboxAppendEntry(lb, "Second", (void *) 2);
    newtListboxAppendEntry(lb, "Third", (void *) 3);
    newtListboxAppendEntry(lb, "Fourth", (void *) 4);
  
    t = newtTextbox(45, 10, 17, 5, NEWT_FLAG_WRAP);
 
    newtFormAddComponents(f, lb, t, NULL);
    newtRefresh();
    newtFormSetTimer(f, 200);
 
    do {
        newtFormRun(f, &es);
        newtRefresh();
    } while (es.reason != NEWT_EXIT_COMPONENT);
 
 
    int numhighlighted = (int)(long) newtListboxGetCurrent(lb);

    newtPopWindow();
    newtPopWindow();
    newtFinished();
 
    newtFormDestroy(f);

    /* Add in system() in here to call command from array based on position from newtLiostboxGetCurrent() */
    /* Menu will loop back around and be displayed because it's wrapped in menu.c */
    printf("\nSelected listbox item (%d):\n", numhighlighted);
 
    return 0;
}
