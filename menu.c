/***************************************************************************/
/* This menu program has been created to allow the user to create a text   */
/* file that is read in and displayed as a menu.                           */
/* It uses curses so it should be portable across Unix's and terminals.    */
/*-------------------------------------------------------------------------*/
/* Author  : Steve Harris (steve.harris@shaw.ca)                           */
/***************************************************************************/
/* Version 1.1 - Steve Harris (13/11/96)                                   */
/* 1. Changed getch to scanw and added atoi to cope with string and number */
/* input.                                                                  */
/* 2. Tried using switch/case for menu selection but failed.               */
/* (Suspect the problem is due the variety of option for exiting the menu) */
/* 3. Added cp - change password and OFF/off/Q/q options.                  */
/***************************************************************************/
/* Version 1.2 - Steve Harris (14/11/96)                                   */
/* 1. Added function to draw_bottom_line to clear the menu option after the*/
/*    refresh - uses clrtoeol().                                           */
/* 2. Changed the quit option to use die() instead of exit(0).             */
/***************************************************************************/
/* Version 1.3 - Steve Harris (14/11/96)                                   */
/* 1. Convert the menu body drawing to a function.                         */
/* 2. Call it from thew construct menu function.                           */
/* 3. Change code to draw menu in different places and styles (spacing)    */
/*    depending on the number of menu options.                             */
/***************************************************************************/
/* Version 1.4 - Steve Harris (14/11/96)                                   */
/* 1. Changed the menu style to do side by side columns to balance the menu*/
/*    when using more than 8 menu options.                                 */
/***************************************************************************/
/* Version 1.5 - Steve Harris (14/11/96)                                   */
/* 1. Added comments.                                                      */
/* 2. Moved heading termination to file read function because it carried on*/
/*    truncating itself when <cr> was pressed on its own.                  */
/***************************************************************************/
/* Version 1.6 - Steve Harris (14/11/96)                                   */
/* 1. Added the prompt to change the password.                             */
/* 2. Hard coded the kerberos password option.                             */
/***************************************************************************/
/* Version 1.7 - Steve Harris (15/11/96)                                   */
/* 1. Changed the if/else tests for menu choices to be tidier.             */
/***************************************************************************/
/* Version 1.8 - Steve Harris (28/2/97)                                    */
/* 1. Introduced isendwin,wrefresh,doupdate because of intermittent screen */
/*    redraw problems.                                                     */
/***************************************************************************/
/* Version 1.9 - Steve Harris (02/6/2020)                                  */
/* 1. Added a few includes to get this up and running on Ubuntu 16.4       */
/***************************************************************************/
/* Version 1.10 - Steve Harris (06/6/2020)                                 */
/* 1. Move the check to see if the filename is valid to start of program   */
/* to deal with the missing argument before initiating curses which        */
/* messes the screen layout up.                                            */
/* 2. Error handling in to capture empty TTY                               */
/***************************************************************************/
/* Version 1.11 - Steve Harris (07/6/2020)                                 */
/* 1. Tidy up compiler warnings - seem to be C90 formatting - int's,       */
/* function declaration and incorrect types                                */
/* 2. Remove the old 'OFF' exit commands - just Q/q now.                   */
/* 3. Replace the kerbpass with passwd.                                    */
/* 4. Fix a bug where redrawing the menu after execution of command left   */
/* the last menu option selected displayed on the screen - there was a     */
/* printw after the scanw that was not needed as echo() had been set.      */
/***************************************************************************/
/* Version 1.12 - Steve Harris (10/26/2020): newt branch                   */
/* Added in usage, option processing and CODE_PATH with newt code.         */
/***************************************************************************/
#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "newt.h"

/************************************/
/*Setup structures for the menu file*/
/*and other parameters.             */
/************************************/
#define MAX_NO_ITEMS 30

struct menu_item
{
	char description[30];
	char command[80];
};

struct menu_item menu[MAX_NO_ITEMS];

/* CURSES - Declare functions */
int read_menu_file();
int construct_menu();
int draw_menu_body();
int draw_dual_body();
int get_option();
int find_center();
int draw_bottom_line();
void die();

/* CURSES - Setup variables to use in code*/
int center_x;
int count, total_no_of_items; 
char *ch;		/*Used for EOF*/
WINDOW *subone;		/*Window pointer for menu*/
char from_term[3]; 	/*Characters read in from terminal*/
int choice;		/*Used to hold number of menu choice*/
int item_step;		/*Gap plus one between menu options*/

/*CURSES - Used in constructing menu - messy isn't it*/
char line[81] = "                                                                                ";
char prompt_line[] = "Enter Option : ";
char exit_line[]="(Q/q to quit)";
char password_prompt[]="CP to change password";
char NOTTY[]="NOTTY";

/* NEWT - Setup */
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


/*BOTH - Menu file related settings*/
FILE *menu_file; 
char menu_heading[80];
char menu_description[80];
char *file_name;

/* BOTH - Variables */
char *login_name, *login_term; /* Store settings for menu heading */
char CODE_PATH[7]; /*CURSES or NEWT */

/* Usage */
void usage(char command[]) {
    printf ("\nUsage: %s <-c | -n> menufile\n", command);
    printf (" -c : curses base menu\n");
    printf (" -n : newt based menu\n");
    printf (" menufile : Input for menu, options and commands\n");
    printf ("\n See https://github.com/steveh250/Unix-Menu-Program for formatting.\n\n");
    exit(-1);
}

/***********************************************/
/*This is main() - where the real work is done */
/***********************************************/
int main(int argc, char *argv[])
{
     
	/* Process the arguments and determine the code path */
     if (argc < 3) {
        usage(argv[0]);
    };

    if (strcmp(argv[1],"-c") == 0) {
        strcpy(CODE_PATH,"CURSES");
    }
    else if (strcmp(argv[1],"-n") == 0) {
        strcpy(CODE_PATH,"NEWT");
    } else {
        usage(argv[0]);
    }
	
	/*Record menu file name*/
	file_name=argv[2];

	/* Open file and read in data - send error if not there.*/	
	menu_file = fopen(file_name,"r");
	if (menu_file == NULL)
	{
		printf("Cannot open your menu file.\n");
		usage(argv[0]);
		endwin();
		exit(8);
	};

	/* Setup parts independent of code path */
	signal(SIGINT, die);

	/*Setup the variables depending on the  */
	/*environment settings.                 */
	login_name=getenv("LOGNAME");
	login_term=getenv("TTY");
	if (login_term == NULL){
		login_term=NOTTY;}
	
	/********************/
	/*Read the menu file*/
	/********************/
	/*Read in first two heading lines from file and then read*/
	/*the rest into our array of structures*/
	fgets(menu_heading, sizeof(menu_heading), menu_file);
	fgets(menu_description, sizeof(menu_description), menu_file);

	count=0;

	while (count < MAX_NO_ITEMS)
	{
		ch = fgets(menu[count].description, sizeof(menu[count].description), menu_file);
		(void)fgets(menu[count].command, sizeof(menu[count].command), menu_file);			
		menu[count].command[strlen(menu[count].command)-1] = '\0';

		/*Check for end of file*/
		if (ch  == NULL) break;

		count++;
	};

	/*Truncate the menu headings*/
	menu_heading[strlen(menu_heading)-1] = '\0';
	menu_description[strlen(menu_description)-1] = '\0';

	fclose(menu_file);
	total_no_of_items=count;

	/* ********** CODE PATH Decision *******/
	/* Follow the CURSES or NEWT code path */
	/***************************************/

	if (strcmp(CODE_PATH,"CURSES") == 0) {	
		
		/* CURSES Code Path */

		/*Setup curses*/
		initscr();

		/*Draw menu*/
		while(1)
		{
			/*Setup menu outline*/
			construct_menu(file_name);

			if (total_no_of_items <= 8)
			{
				/*Draw single column, double spaced*/
				draw_menu_body(5,30,2,1,8);
			}
			else
			{
				/*Draw two column,single spaced*/
				draw_dual_body(5,7);
			}

			/*Position cursor in prompt and draw*/
			move(23,15);
			refresh();

			/*Get the menu option*/
			get_option();

			/*Clear the from_term and choice*/
			choice=0;
			from_term[0]='\0';

		} 

	} else { 
		/* We're opn the newt code path */

		/* Acknowledgment: based on source from: https://pagure.io/newt/blob/master/f/test.c */

		/* Setup variables */
	    newtComponent lb, b1, f, t;
	    struct callbackInfo cbis[3];
	    struct newtExitStruct es;
	 
	 	/* Initialize the screen */
	    newtInit();
	    newtCls();
	 
	 	/* Setup call backs */
	    newtSetSuspendCallback(suspend, NULL);
	    newtSetHelpCallback(helpCallback);
	 
	    /* Setup the headings and footer*/
	    newtDrawRootText(0, 0, menu_heading);
	    newtPushHelpLine(NULL);
	    newtOpenWindow(10, 5, 65, 16, menu_description);

	    /* Setup the form */
	    f = newtForm(NULL, "Scroll to the menu item and press enter to execute.", 0);
	 
	 	/* Setup the exit button */
	    b1 = newtCompactButton(3, 1, "Exit");
	 
	 	/* Add the components to the form */
	    newtFormAddComponents(f, b1, NULL);

	    /* Setup the list box */
	    lb = newtListbox(45, 1, 6, NEWT_FLAG_RETURNEXIT | NEWT_FLAG_BORDER |
	                                NEWT_FLAG_SCROLL | NEWT_FLAG_SHOWCURSOR);
	    newtListboxAppendEntry(lb, "First", (void *) 1);
	    newtListboxAppendEntry(lb, "Second", (void *) 2);
	    newtListboxAppendEntry(lb, "Third", (void *) 3);
	    newtListboxAppendEntry(lb, "Fourth", (void *) 4);
	  
	  	/* Add listbox for form  and refresh screen */
	    newtFormAddComponents(f, lb, NULL);
	    newtRefresh();
	    newtFormSetTimer(f, 200);
	 
	 	/* Run the menu until an option is selected */
	    do {
	        newtFormRun(f, &es);
	        newtRefresh();
	    } while (es.reason != NEWT_EXIT_COMPONENT);
	 
	 
	 	/* Record the item that was selected from the listbox */
	    int numhighlighted = (int)(long) newtListboxGetCurrent(lb);

	    /* Tidy up the windows and execute the command from the array */
	    newtPopWindow();
	    newtPopWindow();
	    newtFinished();
	    newtFormDestroy(f);

	    /* Run the command */
	    //printf("\nSelected listbox item (%d):\n", numhighlighted);
	 	system(menu[numhighlighted-1].command);

	    return 0;
	}
}

/*****************************************/
/*Following function constructs the menu.*/
/*****************************************/
int construct_menu(char menu_file_name[])
{
	/*Creates the main menu window*/

	/*Create the top line*/
	move(0,0);
	attron(A_UNDERLINE);
	addstr(line);
	move(0,0);
	addstr(login_name);
	move(0,80-strlen(login_term));
	addstr(login_term);

	find_center(menu_heading);
	move(0,center_x);
	addstr(menu_heading);

	find_center(menu_description);
	move(2,center_x);
	addstr(menu_description);
	attroff(A_UNDERLINE);

	/*Put in prompt line.*/
	move(21,0);
	attron(A_UNDERLINE);
	addstr(line);
	find_center(password_prompt);
	move(21,center_x);
	addstr(password_prompt);
	attroff(A_UNDERLINE);

	/*Draw the bottom line*/
	draw_bottom_line();

}

/***********************************/
/*Function to draw a dual menu body*/
/***********************************/
int draw_dual_body(int start_y_dual,int start_x_dual)
{
	int right_hand, left_hand, temp_count, right_y;

	/*Work out left and right hand sides*/
	right_hand=total_no_of_items/2;
	left_hand=total_no_of_items-right_hand;

	/*Put in menu options*/
	right_y=start_y_dual;

	move(start_y_dual,start_x_dual);

	for(temp_count=1;temp_count<=left_hand;temp_count++)
	{
		/*Left hand side*/
		printw("%d.",temp_count);
		move(start_y_dual,start_x_dual+4);
		addstr(menu[temp_count-1].description);
		
		start_y_dual++;

		move(start_y_dual,start_x_dual);
	}


	move(right_y,start_x_dual+44);

	for(temp_count=left_hand+1;temp_count<=total_no_of_items;temp_count++)
	{
		/*Right hand side*/
		move(right_y,start_x_dual+44);
		printw("%d. ", temp_count);
		move(right_y,start_x_dual+48);
		addstr(menu[temp_count-1].description);
		
		right_y++;

		move(right_y, start_x_dual+48);	
	}
}


/***************************************************/
/*Function to draw the menu body in a single column*/
/***************************************************/
int draw_menu_body(int start_y, int start_x, int item_gap, int start_option, int end_option)
{
	/*Draw rest of menu*/
	/*Put in menu options*/
	
	move(start_y,start_x);

	while ((start_option <= end_option) && (start_option <= total_no_of_items))
	{
		printw("%d.",start_option);
		move(start_y,start_x+4);
		addstr(menu[start_option-1].description);
		start_y+=item_gap;
		move(start_y,start_x);
		start_option++;
	}

}

/************************************************************************/
/*Draw the bottom line - in a function because its used in more than one*/
/*place. It doesn't do a refresh the calling function should do it.     */
/************************************************************************/
int draw_bottom_line()
{
	move(23,0);
	clrtoeol();
	move(23,0);
	addstr(prompt_line);
	move(23,80-strlen(exit_line));
	addstr(exit_line);
}

/************************************************************************/
/*function to get response from the user and load the appropriate option*/
/************************************************************************/
int get_option()
{
	/*Setup and get response from user*/
	raw();
	echo();
	nl();
	scanw("%3s",from_term);
	choice=atoi(from_term);
	
	/*Process menu choice*/

	if ((strcmp(from_term,"q") == 0) ||
	    (strcmp(from_term, "Q") == 0))
	{
		die();
	}
	else
	{
		if ((strcmp(from_term,"cp") ==0) ||
	    	   (strcmp(from_term,"CP") ==0))
		{
			endwin();
			system("clear;passwd $LOGNAME;clear");
			doupdate();
		}
		else
		{
			if (choice != 0)
			{
				endwin();
				system(menu[choice-1].command);
				doupdate();
			}
		}
	}
	draw_bottom_line();
}


/*********************************/
/*Function to service interrupt  */
/*or to simply terminate.        */
/*********************************/
void die()
{
	endwin();
	exit(0);
}

/***************************************/
/*Find the center of whats passed to it*/
/***************************************/
int find_center(char center_of_what[])
{
	center_x = (80-(strlen(center_of_what)-1))/2;
}