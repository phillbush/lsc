#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

static Display *dpy;
static Window root;
static Window active;
static int screen;
static Atom utf8string;
static Atom netwmname;
static Atom netwmvisiblename;
static Atom netwmdesktop;
static Atom netactivewindow;
static Atom netclientlist;
static Atom netclientliststacking;
static Atom netwmwindowtype;
static Atom netwmwindowtypedesktop;
static Atom netwmwindowtypedock;
static Atom netwmwindowtypetoolbar;
static Atom netwmwindowtypemenu;
static Atom netwmwindowtypeutility;
static Atom netwmwindowtypesplash;
static Atom netwmwindowtypedialog;
static Atom netwmwindowtypenormal;
static Atom netwmstate;
static Atom netwmstatemodal;
static Atom netwmstatesticky;
static Atom netwmstatemaximizedvert;
static Atom netwmstatemaximizedhorz;
static Atom netwmstateshaded;
static Atom netwmstateskiptaskbar;
static Atom netwmstateskippager;
static Atom netwmstatehidden;
static Atom netwmstatefullscreen;
static Atom netwmstateabove;
static Atom netwmstatebelow;
static Atom netwmstatedemandsattention;
static Atom netwmstatefocused;

static int aflag = 0;   /* whether to list only the active client */
static int sflag = 0;   /* whether to sort windows by stacking order */
static int lflag = 0;   /* whether to list in long format */

/* show usage */
static void
usage(void)
{
	(void)fprintf(stderr, "usage: lsc [-als]\n");
	exit(1);
}

/* get cardinal property from given window */
static unsigned long
getcardprop(Window win, Atom prop)
{
	unsigned long ret;
	unsigned char *u;
	unsigned long dl;   /* dummy variable */
	int di;             /* dummy variable */
	Atom da;            /* dummy variable */

	if (XGetWindowProperty(dpy, win, prop, 0L, 1L, False, XA_CARDINAL,
		               &da, &di, &dl, &dl, &u) != Success) {
		return 0;
	}
	ret = *(unsigned long *)u;
	XFree(u);
	return ret;
}

/* get atom property from given window */
static unsigned long
getatomprop(Window win, Atom prop, Atom **atoms)
{
	unsigned char *list;
	unsigned long len;
	unsigned long dl;   /* dummy variable */
	int di;             /* dummy variable */
	Atom da;            /* dummy variable */

	if (XGetWindowProperty(dpy, win, prop, 0L, 1024, False, XA_ATOM,
		               &da, &di, &len, &dl, &list) != Success) {
		*atoms = NULL;
		return 0;
	}
	*atoms = (Atom *)list;
	return len;
}

/* get window property from root window */
static unsigned long
getwinprop(Atom prop, Window **wins)
{
	unsigned char *list;
	unsigned long len;
	unsigned long dl;   /* dummy variable */
	int di;             /* dummy variable */
	Atom da;            /* dummy variable */

	if (XGetWindowProperty(dpy, root, prop, 0L, 1024, False, XA_WINDOW,
		               &da, &di, &len, &dl, &list) != Success) {
		*wins = NULL;
		return 0;
	}
	*wins = (Window *)list;
	return len;
}

/* get text property atom from window win into array text */
static int
gettextprop(Window win, Atom atom, char *text, unsigned int size)
{
	/* this routine was get from dwm */
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, win, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

/* get active window */
static Window
getactivewin(void)
{
	Window *list;
	Window ret = None;

	if (getwinprop(netactivewindow, &list))
		ret = *list;
	XFree(list);
	return ret;
}

/* get array of windows */
static unsigned long
getwins(Window **wins)
{
	if (sflag)
		return getwinprop(netclientliststacking, wins);
	else
		return getwinprop(netclientlist, wins);
}

/* list window type, states, properties, etc */
static void
longlist(Window win)
{
	Atom *atoms;
	int desk;
	unsigned long i, natoms, n;
	char type = '-';
	char state[11] = "----------";
	char name[BUFSIZ] = "\0";
	XWMHints *wmhints = NULL;
	Window *list = NULL;
	Window transfor = 0x0;
	XID group = 0x0;

	if (getatomprop(win, netwmwindowtype, &atoms)) {
		if (*atoms == None || *atoms == netwmwindowtypenormal)
			type = '-';
		else if (*atoms == netwmwindowtypedesktop)
			type = 'k';
		else if (*atoms == netwmwindowtypedock)
			type = 'b';
		else if (*atoms == netwmwindowtypetoolbar)
			type = 't';
		else if (*atoms == netwmwindowtypemenu)
			type = 'm';
		else if (*atoms == netwmwindowtypeutility)
			type = 'u';
		else if (*atoms == netwmwindowtypesplash)
			type = 's';
		else if (*atoms == netwmwindowtypedialog)
			type = 'd';
		else
			type = 'x';
		XFree(atoms);
	}

	if ((wmhints = XGetWMHints(dpy, win)) != NULL) {
		if (wmhints->flags & XUrgencyHint) {
			state[8] = 'u';
		}
		if (wmhints->flags & WindowGroupHint) {
			group = wmhints->window_group;
		}
	}
	XFree(wmhints);

	if (getwinprop(XA_WM_TRANSIENT_FOR, &list) > 0) {
		if (*list != None) {
			transfor = *list;
			state[0] = 't';
		}
	}
	XFree(list);

	if (win == active)
		state[9] = 'a';
	if ((natoms = getatomprop(win, netwmstate, &atoms)) > 0) {
		for (i = 0; i < natoms; i++) {
			if (atoms[i] == netwmstatemodal) {
				if (state[0] == 't') {
					state[0] = 'T';
				} else {
					state[0] = 'm';
				}
			} else if (atoms[i] == netwmstatesticky) {
				state[1] = 'y';
			} else if (atoms[i] == netwmstatemaximizedvert) {
				if (state[2] == 'h') {
					state[2] = 'M';
				} else {
					state[2] = 'v';
				}
			} else if (atoms[i] == netwmstatemaximizedhorz) {
				if (state[2] == 'v') {
					state[2] = 'M';
				} else {
					state[2] = 'h';
				}
			} else if (atoms[i] == netwmstateshaded) {
				state[3] = 's';
			} else if (atoms[i] == netwmstateskiptaskbar) {
				if (state[4] == 'p') {
					state[4] = 'S';
				} else {
					state[4] = 't';
				}
			} else if (atoms[i] == netwmstateskippager) {
				if (state[4] == 't') {
					state[4] = 'S';
				} else {
					state[4] = 'p';
				}
			} else if (atoms[i] == netwmstatehidden) {
				state[5] = 'h';
			} else if (atoms[i] == netwmstatefullscreen) {
				state[6] = 'f';
			} else if (atoms[i] == netwmstateabove) {
				state[7] = 'a';
			} else if (atoms[i] == netwmstatebelow) {
				state[7] = 'b';
			} else if (atoms[i] == netwmstatedemandsattention) {
				if (state[8] == 'u') {
					state[8] = 'U';
				} else {
					state[8] = 'a';
				}
			} else if (atoms[i] == netwmstatefocused) {
				if (state[9] == 'a') {
					state[9] = 'A';
				} else {
					state[9] = 'f';
				}
			}
		}
		XFree(atoms);
	}

	n = getcardprop(win, netwmdesktop);
	desk = (n ==  0xFFFFFFFF) ? -1 : n;

	if (!gettextprop(win, netwmvisiblename, name, sizeof name))
		if (!gettextprop(win, netwmname, name, sizeof name))
			gettextprop(win, XA_WM_NAME, name, sizeof name);

	printf("%c%s %3d 0x%08lx 0x%08lx 0x%08lx %s\n", type, state, desk, group, transfor, win, name);
}

/* lsc: list clients */
int
main(int argc, char *argv[])
{
	Window *wins;
	XEvent ev;
	int nwins, ch, i;

	while ((ch = getopt(argc, argv, "als")) != -1) {
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 0)
		usage();

	/* open connection to the server */
	if ((dpy = XOpenDisplay(NULL)) == NULL)
		errx(1, "could not open display");
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	/* intern atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	netwmname = XInternAtom(dpy, "_NET_WM_NAME", False);
	netwmvisiblename = XInternAtom(dpy, "_NET_WM_VISIBLE_NAME", False);
	netwmdesktop = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
	netactivewindow = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netclientlist = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	netclientliststacking = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
	netwmwindowtype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netwmwindowtypedesktop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
	netwmwindowtypedock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	netwmwindowtypetoolbar = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
	netwmwindowtypemenu = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
	netwmwindowtypeutility = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
	netwmwindowtypesplash = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
	netwmwindowtypedialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netwmwindowtypenormal = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	netwmstate = XInternAtom(dpy, "_NET_WM_STATE", False);
	netwmstatemodal = XInternAtom(dpy, "_NET_WM_STATE_MODAL", False);
	netwmstatesticky = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
	netwmstatemaximizedvert = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	netwmstatemaximizedhorz = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	netwmstateshaded = XInternAtom(dpy, "_NET_WM_STATE_SHADED", False);
	netwmstateskiptaskbar = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
	netwmstateskippager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
	netwmstatehidden = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);
	netwmstatefullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netwmstateabove = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
	netwmstatebelow = XInternAtom(dpy, "_NET_WM_STATE_BELOW", False);
	netwmstatedemandsattention = XInternAtom(dpy, "_NET_WM_STATE_DEMANDS_ATTENTION", False);
	netwmstatefocused = XInternAtom(dpy, "_NET_WM_STATE_FOCUSED", False);

	/* list windows */
	if (aflag) {
		XSelectInput(dpy, root, PropertyChangeMask);
		ev.type = PropertyNotify;
		ev.xproperty.atom = netactivewindow;
		do {
			if (ev.type == PropertyNotify && ev.xproperty.atom == netactivewindow) {
				if (lflag) {
					longlist(getactivewin());
				} else {
					printf("0x%08lx\n", getactivewin());
				}
			}
		} while (!XNextEvent(dpy, &ev));
	} else {
		active = getactivewin();
		nwins = getwins(&wins);
		for (i = 0; i < nwins; i++) {
			if (lflag) {
				longlist(wins[i]);
			} else {
				printf("0x%08lx\n", wins[i]);
			}
		}
		XFree(wins);
	}

	/* close connection to the server */
	XCloseDisplay(dpy);

	return 0;
}
