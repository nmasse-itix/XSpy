/*
 * XSpy - Spies a window for key events.
 * Copyright (C) 2006  Nicolas MASSE <nicolas27.masse@laposte.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* Defines the behavior of the program : windows list or window spy */
typedef enum {MODE_LIST = 0, MODE_SPY = 1} Mode;


/**
 * Prints the details about the window.
 *
 * @param dsp	the display.
 * @param w	the window.
 * @param depth the depth in the window stack.
 */
void PrintWindow(Display * dsp, Window w, int depth) {
	int i;
	char * name;
	
	/* indent */
	for (i = 0; i < depth; i++) {
		printf("  ");
	}
	
	printf("%#.8lx - ", w);
	
	/* Display the window title */
	int res = XFetchName(dsp, w, &name);
	if (res) {
		printf("\"%s\"", name);
	} else {
		printf("(has no name)");
	}
	XFree(name);

	printf("\n");
}

/**
 * Lists recursively all the windows of the display.
 *
 * @param dsp	the display.
 * @param w	the parent window.
 * @param depth	the depth in the window stack.
 */
void ListWindows(Display * dsp, Window w, int depth) {
	Window rootWindow;
	Window * windowsList;
	Window parentWindow;
	unsigned int nWindows;
	
	int res = XQueryTree(
			dsp, 				/* Display */
			w, 				/* Window */
			&rootWindow,			/* Root window */
			&parentWindow,			/* Parent window */
			&windowsList,			/* Windows list */
			&nWindows			/* Number of child windows */
			);

	if (res) {
		int i;

		for (i = 0; i < nWindows; i++) {
			PrintWindow(dsp, windowsList[i], depth);
			ListWindows(dsp, windowsList[i], depth + 1);
		}

		XFree(windowsList);
	}
}

/**
 * Lists all the windows of the connected display to the standard output.
 *
 * @param	dsp	the display
 */
void ListAllWindows(Display * dsp) {
	/* Print the root window */
	Window w = DefaultRootWindow(dsp);
	PrintWindow(dsp, w, 0);	
	
	/* List all subwindows recursively */
	ListWindows(dsp, w, 1);
}

/**
 * Listens to the window w.
 *
 * @param	dsp	the display
 * @param	w	the window
 */
void RegisterListener(Display * dsp, Window w) {
	/* Select useful events */
	XSelectInput(dsp, w, KeyPressMask|KeyReleaseMask);
}

/**
 * Recursively registers the listeners using w as root window.
 *
 * @param	dsp	the display
 * @param	w	the root window
 */
void RegisterListeners(Display * dsp, Window w) {
	Window rootWindow;
	Window * windowsList;
	Window parentWindow;
	unsigned int nWindows;
	
	int res = XQueryTree(
			dsp, 				/* Display */
			w, 				/* Window */
			&rootWindow,			/* Root window */
			&parentWindow,			/* Parent window */
			&windowsList,			/* Windows list */
			&nWindows			/* Number of child windows */
			);

	if (res) {
		int i;

		for (i = 0; i < nWindows; i++) {
			RegisterListener(dsp, windowsList[i]);
			RegisterListeners(dsp, windowsList[i]);
		}

		XFree(windowsList);
	}
	
}

/**
 * Registers all the listeners.
 *
 * @param	dsp	the display.
 */
void RegisterAllListeners(Display * dsp) {
	/* Register the root window */
	Window w = DefaultRootWindow(dsp);
	RegisterListener(dsp, w);	
	
	/* Register all subwindows recursively */
	RegisterListeners(dsp, w);
	
}

/**
 * Endless loop which waits for key events and prints them on the standard 
 * output.
 *
 * @param	dsp	the display.
 */
void WaitForKeys(Display * dsp) {
	XEvent evt;
	XKeyEvent * kevt = (XKeyEvent *) &evt;
	KeySym key;
	char key_str[256];
	size_t size;
	
	while (1) {
		/* Wait for an event */
		XNextEvent(dsp, &evt);

		switch (evt.type) {
		case KeyPress:
		case KeyRelease:
			/* Retrieve information about the event */
			size = XLookupString(kevt, key_str, 256, &key, 0);
			key_str[size] = '\0';
		
			printf("%#.8lx: ", kevt->window);
			
			if (evt.type == KeyPress) {
				printf("KeyPress: ");
			} else {
				printf("KeyRelease: ");
			}
		
			/* Prints the key name */
			printf("%s\n", XKeysymToString(key));

			break;
		default:
			printf("UnhandledEvent\n");
			break;
		}
	}
}

/**
 * Displays an help message and exits.
 */
void DisplayUsage(void) {
	fprintf(stderr, "Usage: xspy {list | spy [window_id]| help}\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Valid modes :\n");
	fprintf(stderr, "\tlist : displays all windows of the display.\n");
	fprintf(stderr, "\tspy : listen the windows for key events.\n");
	fprintf(stderr, "\thelp : this help message.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Parameters :\n");
	fprintf(stderr, "\twindow_id : the id of the window to listen.\n");
	fprintf(stderr, "\t            If not specified, listen to all windows\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "How to change the display ?\n");
	fprintf(stderr, "\t$ DISPLAY=host:display program\n");
	fprintf(stderr, "\tExample :\n");
	fprintf(stderr, "\t$ DISPLAY=tx10:0 xspy\n");
	exit(1);
}

/**
 * Parses the options of the program.
 *
 * @param argc	the number of arguments
 * @param argv	an array of arguments.
 * @param win	a pointer to the window id to listen. 
 * @return	the mode.
 */
Mode ParseOptions(int argc, char ** argv, Window * win) {
	Mode mode = MODE_LIST;
	
	if (argc > 1) {
		if (strcmp(argv[1], "list") == 0) {
			mode = MODE_LIST;
		} else if (strcmp(argv[1], "spy") == 0) {
			mode = MODE_SPY;
			if (argc != 3) {
				*win = 0;
			} else {
				char * endPtr = NULL;
				*win = strtol(argv[2], &endPtr, 16);
				if (*endPtr != '\0') {
					fprintf(stderr, "Unknown number: '%s'\n", argv[2]);
					DisplayUsage();
				}
			}
		} else if (strcmp(argv[1], "help") == 0) {
			DisplayUsage();
		} else {
			fprintf(stderr, "Unknown mode '%s'\n", argv[1]);
			DisplayUsage();
		}
	} else {
		DisplayUsage();
	}

	return mode;
}

/**
 * Entry point of the program.
 *
 * @return 0
 */
int main(int argc, char ** argv) {
	/* The window to listen */
	Window win;

	/* Reads options and sets global variables */
	Mode mode = ParseOptions(argc, argv, &win);
	
	Display * dsp = XOpenDisplay(NULL);
	if (dsp == NULL) {
		fprintf(stderr, "Unable to open display\n");
		exit(1);
	}

	switch (mode) {
	case MODE_LIST:
		ListAllWindows(dsp);
		break;
	case MODE_SPY:
		if (win == 0) {
			RegisterAllListeners(dsp);
		} else {
			RegisterListener(dsp, win);
		}
		
		WaitForKeys(dsp);
		break;
	}
	
	XCloseDisplay(dsp);
	
	return 0;
}

