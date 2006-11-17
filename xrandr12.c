/*
 * Copyright © 2006 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>	/* we share subpixel information */
#include <string.h>
#include <stdlib.h>

static char *program_name;

static char *direction[5] = {
    "normal", 
    "left", 
    "inverted", 
    "right",
    "\n"};

/* subpixel order */
static char *order[6] = {
    "unknown",
    "horizontal rgb",
    "horizontal bgr",
    "vertical rgb",
    "vertical bgr",
    "no subpixels"};

static char *connection[3] = {
    "connected",
    "disconnected",
    "unknown connection"};

static void
usage(void)
{
    fprintf(stderr, "usage: %s [options]\n", program_name);
    fprintf(stderr, "  where options are:\n");
    fprintf(stderr, "  -display <display> or -d <display>\n");
    fprintf(stderr, "  -help\n");
    fprintf(stderr, "  -o <normal,inverted,left,right,0,1,2,3>\n");
    fprintf(stderr, "            or --orientation <normal,inverted,left,right,0,1,2,3>\n");
    fprintf(stderr, "  -q        or --query\n");
    fprintf(stderr, "  -s <size>/<width>x<height> or --size <size>/<width>x<height>\n");
    fprintf(stderr, "  -r <rate> or --rate <rate>\n");
    fprintf(stderr, "  -v        or --version\n");
    fprintf(stderr, "  -x        (reflect in x)\n");
    fprintf(stderr, "  -y        (reflect in y)\n");
    fprintf(stderr, "  --screen <screen>\n");
    fprintf(stderr, "  --verbose\n");

    exit(1);
    /*NOTREACHED*/
}

int
main (int argc, char **argv)
{
    Display       *dpy;
    XRRScreenResources	*sr;
    XRRScreenSize *sizes;
    XRRScreenConfiguration *sc;
    int		nsize;
    int		nrate;
    short		*rates;
    Window	root;
    Status	status = RRSetConfigFailed;
    int		rot = -1;
    int		verbose = 0, query = 0;
    Rotation	rotation = RR_Rotate_0, current_rotation, rotations;
    XEvent	event;
    XRRScreenChangeNotifyEvent *sce;    
    char          *display_name = NULL;
    int 		i, j;
    SizeID	current_size;
    short		current_rate;
    int		rate = -1;
    int		size = -1;
    int		dirind = 0;
    int		setit = 0;
    int		screen = -1;
    int		version = 0;
    int		event_base, error_base;
    int		reflection = 0;
    int		width = 0, height = 0;
    int		mmwidth = 0, mmheight = 0;
    int		ret = 0;
    int		have_size = 0, have_mm_size = 0;
    int		major_version, minor_version;
    RRMode	mode = (RRMode) -1;
    RRCrtc	crtc = 0;
    RROutput	output = (RROutput) -1;
    int		x = 0;
    int		y = 0;
    int		minWidth, minHeight;
    int		maxWidth, maxHeight;

    program_name = argv[0];
    if (argc == 1) query = 1;
    for (i = 1; i < argc; i++) {
	if (!strcmp ("-display", argv[i]) || !strcmp ("-d", argv[i])) {
	    if (++i>=argc) usage ();
	    display_name = argv[i];
	    continue;
	}
	if (!strcmp("-help", argv[i])) {
	    usage();
	    continue;
	}
	if (!strcmp ("--verbose", argv[i])) {
	    verbose = 1;
	    continue;
	}

	if (!strcmp ("-s", argv[i]) || !strcmp ("--size", argv[i])) {
	    if (++i>=argc) usage ();
	    if (sscanf (argv[i], "%dx%d", &width, &height) == 2)
		have_size = 1;
	    else {
		usage();
	    }
	    continue;
	}
	if (!strcmp ("-mm", argv[i]) || !strcmp ("--mm", argv[i])) {
	    if (++i>=argc) usage ();
	    if (sscanf (argv[i], "%dx%d", &mmwidth, &mmheight) == 2)
		have_mm_size = 1;
	    else {
		usage();
	    }
	    continue;
	}
	if (!strcmp ("-m", argv[i]) || !strcmp ("--mode", argv[i])) {
	    if (++i>=argc) usage ();
	    mode = strtoul (argv[i], NULL, 0);
	    setit = 1;
	    continue;
	}
	if (!strcmp ("-c", argv[i]) || !strcmp ("--crtc", argv[i])) {
	    if (++i>=argc) usage ();
	    crtc = strtoul (argv[i], NULL, 0);
	    continue;
	}
	if (!strcmp ("-o", argv[i]) || !strcmp ("--output", argv[i])) {
	    if (++i>=argc) usage ();
	    output = strtoul (argv[i], NULL, 0);
	    continue;
	}

	if (!strcmp ("-v", argv[i]) || !strcmp ("--version", argv[i])) {
	    version = 1;
	    continue;
	}

	if (!strcmp ("-x", argv[i])) {
	    if (++i>=argc) usage ();
	    x = strtoul (argv[i], NULL, 0);
	    continue;
	}
	if (!strcmp ("-y", argv[i])) {
	    if (++i>=argc) usage ();
	    y = strtoul (argv[i], NULL, 0);
	    continue;
	}
	if (!strcmp ("--screen", argv[i])) {
	    if (++i>=argc) usage ();
	    screen = atoi (argv[i]);
	    if (screen < 0) usage();
	    continue;
	}
	if (!strcmp ("-q", argv[i]) || !strcmp ("--query", argv[i])) {
	    query = 1;
	    continue;
	}
	if (!strcmp ("-r", argv[i]) || !strcmp ("--rotation", argv[i])) {
	    char *endptr;
	    if (++i>=argc) usage ();
	    dirind = strtol(argv[i], &endptr, 0);
	    if (*endptr != '\0') {
		for (dirind = 0; dirind < 4; dirind++) {
		    if (strcmp (direction[dirind], argv[i]) == 0) break;
		}
		if ((dirind < 0) || (dirind > 3))  usage();
	    }
	    rot = dirind;
	    setit = 1;
	    continue;
	}
	usage();
    }
    if (verbose) query = 1;

    dpy = XOpenDisplay (display_name);

    if (dpy == NULL) {
	fprintf (stderr, "Can't open display %s\n", XDisplayName(display_name));
	exit (1);
    }
    
    XRRQueryVersion (dpy, &major_version, &minor_version);
    if (!(major_version > 1 || minor_version >= 2))
    {
	fprintf (stderr, "Randr version too old (need 1.2 or better)\n");
	exit (1);
    }
	
    if (screen < 0)
	screen = DefaultScreen (dpy);
    if (screen >= ScreenCount (dpy)) {
	fprintf (stderr, "Invalid screen number %d (display has %d)\n",
		 screen, ScreenCount (dpy));
	exit (1);
    }

    root = RootWindow (dpy, screen);

    if (XRRGetScreenSizeRange (dpy, root, &minWidth, &minHeight,
			       &maxWidth, &maxHeight))
    {
	printf ("min screen size: %d x %d\n", minWidth, minHeight);
	printf ("max screen size: %d x %d\n", maxWidth, maxHeight);
    }
    sr = XRRGetScreenResources (dpy, root);

    printf ("timestamp: %ld\n", sr->timestamp);
    printf ("configTimestamp: %ld\n", sr->configTimestamp);
    for (i = 0; i < sr->ncrtc; i++) {
	XRRCrtcInfo	*xci;
	int		j;
	
	printf ("\tcrtc: 0x%x\n", sr->crtcs[i]);
	xci = XRRGetCrtcInfo (dpy, sr, sr->crtcs[i]);
	printf ("\t\t%dx%d +%u+%u\n",
		xci->width, xci->height, xci->x, xci->y);
	printf ("\t\tmode: 0x%x\n", xci->mode);
	printf ("\t\toutputs:");
	for (j = 0; j < xci->noutput; j++)
	    printf (" 0x%x", xci->outputs[j]);
	printf ("\n");
	printf ("\t\tpossible:");
	for (j = 0; j < xci->npossible; j++)
	    printf (" 0x%x", xci->possible[j]);
	printf ("\n");
    }
    for (i = 0; i < sr->noutput; i++) {
	XRROutputInfo	*xoi;
	Atom		*props;
	int		j, nprop;

	printf ("\toutput: 0x%x\n", sr->outputs[i]);
	xoi = XRRGetOutputInfo (dpy, sr, sr->outputs[i]);
	printf ("\t\tname: %s\n", xoi->name);
	printf ("\t\ttimestamp: %d\n", xoi->timestamp);
	printf ("\t\tcrtc: 0x%x\n", xoi->crtc);
	printf ("\t\tconnection: %s\n", connection[xoi->connection]);
	printf ("\t\tsubpixel_order: %s\n", order[xoi->subpixel_order]);
	printf ("\t\tphysical_size: %5d x %5d\n", xoi->mm_width,xoi->mm_height);
	printf ("\t\tmodes:");
	for (j = 0; j < xoi->nmode; j++)
	    printf(" 0x%x%s", xoi->modes[j], j < xoi->npreferred ? "*" : "");
	printf ("\n");
	printf ("\t\tclones:");
	for (j = 0; j < xoi->nclone; j++)
	    printf(" 0x%x", xoi->clones[j]);
	printf ("\n");

	props = XRRListOutputProperties (dpy, sr->outputs[i], &nprop);
	printf ("\t\tproperties:\n");
	for (j = 0; j < nprop; j++) {
	    unsigned char *prop;
	    int actual_format;
	    unsigned long nitems, bytes_after;
	    Atom actual_type;

	    XRRGetOutputProperty (dpy, sr->outputs[i], props[j],
				  0, 100, False, AnyPropertyType,
				  &actual_type, &actual_format,
				  &nitems, &bytes_after, &prop);

	    if (actual_type == XA_INTEGER && actual_format == 8) {
		int k;

		printf("\t\t\t%s:\n", XGetAtomName (dpy, props[j]));
		for (k = 0; k < nitems; k++) {
		    if (k % 16 == 0)
			printf ("\t\t\t");
		    printf("%02x", (unsigned char)prop[k]);
		    if (k % 16 == 15)
			printf("\n");
		}
	    } else if (actual_format == 8) {
		printf ("\t\t\t%s: %s%s\n", XGetAtomName (dpy, props[j]),
			prop, bytes_after ? "..." : "");
	    } else {
		printf ("\t\t\t%s: ????\n", XGetAtomName (dpy, props[j]));
	    }
	}

	XRRFreeOutputInfo (xoi);
    }
    for (i = 0; i < sr->nmode; i++) {
	double	rate;
	printf ("\tmode: 0x%04x", sr->modes[i].id);
	printf (" %15.15s", sr->modes[i].name);
	printf (" %5d x %5d", sr->modes[i].width, sr->modes[i].height);
	if (sr->modes[i].hTotal && sr->modes[i].vTotal)
	    rate = ((double) sr->modes[i].dotClock / 
		    ((double) sr->modes[i].hTotal * (double) sr->modes[i].vTotal));
	else
	    rate = 0;
	printf (" %6.1fHz %6.1fMhz (with blanking: %d x %d)", rate,
		(float)sr->modes[i].dotClock / 1000000,
		sr->modes[i].hTotal, sr->modes[i].vTotal);
	printf ("\n");
    }
    if (sr == NULL) 
    {
	fprintf (stderr, "Cannot get screen resources\n");
	exit (1);
    }

    if (have_size)
    {
	if (!have_mm_size)
	{
	    mmwidth = DisplayWidthMM(dpy, screen);
	    mmheight = DisplayHeightMM(dpy, screen);
	}
	XRRSetScreenSize (dpy, root, width, height, mmwidth, mmheight);
    }
    if (setit)
    {
	int noutput;
	
	if (!crtc)
	    crtc = sr->crtcs[0];

	switch (output) {
	case None:
	    noutput = 0;
	    break;
	case (RROutput) -1:
	    output = sr->outputs[0];
	default:
	    noutput = 1;
	    break;
	}

	if (mode != (RRMode) -1)
	{
	    Status status;
	    
	    status = XRRSetCrtcConfig (dpy, sr, crtc, CurrentTime, x, y,
				       mode, rotation, &output, noutput);
	    printf ("status: %d\n", status);
	}
    }

    XRRFreeScreenResources (sr);

    XSync (dpy, False);

    
    return(ret);
}
