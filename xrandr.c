/* 
 * Copyright © 2001 Keith Packard, member of The XFree86 Project, Inc.
 * Copyright © 2002 Hewlett Packard Company, Inc.
 * Copyright © 2006 Intel Corporation
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
 *
 * Thanks to Jim Gettys who wrote most of the client side code,
 * and part of the server code for randr.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>	/* we share subpixel information */
#include <string.h>
#include <stdlib.h>

#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 2)
#define HAS_RANDR_1_2 1
#endif

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
#if HAS_RANDR_1_2
    fprintf(stderr, "  --output <output>\n");
    fprintf(stderr, "  --crtc <crtc>\n");
    fprintf(stderr, "  --mode <mode>\n");
    fprintf(stderr, "  --pos <x>x<y>\n");
    fprintf(stderr, "  --left-of <output>\n");
    fprintf(stderr, "  --right-of <output>\n");
    fprintf(stderr, "  --above <output>\n");
    fprintf(stderr, "  --below <output>\n");
    fprintf(stderr, "  --off\n");
    fprintf(stderr, "  --fb <width>x<height>\n");
    fprintf(stderr, "  --dpi <dpi>\n");
    fprintf(stderr, "  --clone\n");
    fprintf(stderr, "  --panorama\n");
#endif
    fprintf(stderr, "  --screen <screen>\n");
    fprintf(stderr, "  --verbose\n");

    exit(1);
    /*NOTREACHED*/
}

#if HAS_RANDR_1_2
typedef enum _xrandr_policy {
    xrandr_clone, xrandr_panorama
} xrandr_policy_t;

typedef enum _xrandr_relation {
    xrandr_left_of, xrandr_right_of, xrandr_above, xrandr_below
} xrandr_relation_t;

typedef struct _xrandr_output {
    struct _xrandr_output   *next;
    char		    *output;
    XRROutputInfo	    *output_info;
    RROutput		    randr_output;
    char		    *crtc;
    RRCrtc		    randr_crtc;
    XRRCrtcInfo		    *crtc_info;
    char		    *mode;
    RRMode		    randr_mode;
    XRRModeInfo		    *mode_info;
    xrandr_relation_t	    relation;
    char		    *relative_to;
    int			    x, y;
    Rotation		    rotation;
    int			    setit;
} xrandr_output_t;

static char *connection[3] = {
    "connected",
    "disconnected",
    "unknown connection"};

#define OUTPUT_NAME 1

#define CRTC_OFF    2
#define CRTC_UNSET  3

#define MODE_NAME   1
#define MODE_OFF    2
#define MODE_UNSET  3

#define POS_UNSET   -1

static int
mode_height (XRRModeInfo *mode_info, Rotation rotation)
{
    switch (rotation & 0xf) {
    case RR_Rotate_0:
    case RR_Rotate_180:
	return mode_info->height;
    case RR_Rotate_90:
    case RR_Rotate_270:
	return mode_info->width;
    default:
	return 0;
    }
}

static int
mode_width (XRRModeInfo *mode_info, Rotation rotation)
{
    switch (rotation & 0xf) {
    case RR_Rotate_0:
    case RR_Rotate_180:
	return mode_info->width;
    case RR_Rotate_90:
    case RR_Rotate_270:
	return mode_info->height;
    default:
	return 0;
    }
}

#endif
    
int
main (int argc, char **argv)
{
    Display       *dpy;
    XRRScreenSize *sizes;
    XRRScreenConfiguration *sc;
    int		nsize;
    int		nrate;
    short		*rates;
    Window	root;
    Status	status = RRSetConfigFailed;
    int		rot = -1;
    int		verbose = 0, query = 0;
    Rotation	rotation, current_rotation, rotations;
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
    int		have_pixel_size = 0;
    int		ret = 0;
#if HAS_RANDR_1_2
    xrandr_output_t *xrandr_outputs = NULL;
    xrandr_output_t *xrandr_output = NULL;
    xrandr_output_t **xrandr_tail = &xrandr_outputs;
    char	*crtc;
    xrandr_policy_t policy = xrandr_clone;
    int		setit_1_2 = 0;
    int		has_1_2 = 0;
    int		query_1_2 = 0;
    int		major, minor;
    int		fb_width = 0, fb_height = 0;
    double    	dpi = 0;
    int		automatic = 0;
#endif

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
		have_pixel_size = 1;
	    else {
		size = atoi (argv[i]);
		if (size < 0) usage();
	    }
	    setit = 1;
	    continue;
	}

	if (!strcmp ("-r", argv[i]) || !strcmp ("--rate", argv[i])) {
	    if (++i>=argc) usage ();
	    rate = atoi (argv[i]);
	    if (rate < 0) usage();
	    setit = 1;
	    continue;
	}

	if (!strcmp ("-v", argv[i]) || !strcmp ("--version", argv[i])) {
	    version = 1;
	    continue;
	}

	if (!strcmp ("-x", argv[i])) {
	    reflection |= RR_Reflect_X;
	    setit = 1;
	    continue;
	}
	if (!strcmp ("-y", argv[i])) {
	    reflection |= RR_Reflect_Y;
	    setit = 1;
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
	if (!strcmp ("-o", argv[i]) || !strcmp ("--orientation", argv[i])) {
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
#if HAS_RANDR_1_2
	if (!strcmp ("--output", argv[i])) {
	    if (++i >= argc) usage();
	    xrandr_output = malloc (sizeof (xrandr_output_t));
	    if (!xrandr_output)
		usage();
	    xrandr_output->next = NULL;

	    xrandr_output->output = argv[i];
	    if (sscanf (xrandr_output->output, "0x%x", &xrandr_output->randr_output) != 1)
		xrandr_output->randr_output = OUTPUT_NAME;
	    xrandr_output->output_info = NULL;
	    
	    xrandr_output->crtc = NULL;
	    xrandr_output->randr_crtc = CRTC_UNSET;
	    xrandr_output->crtc_info = NULL;
	    
	    xrandr_output->mode = NULL;
	    xrandr_output->randr_mode = MODE_UNSET;
	    xrandr_output->mode_info = NULL;
	    
	    xrandr_output->relation = xrandr_right_of;
	    xrandr_output->relative_to = NULL;
	    xrandr_output->x = POS_UNSET;
	    xrandr_output->y = POS_UNSET;
	    xrandr_output->rotation = 0;
	    xrandr_output->setit = 1;

	    *xrandr_tail = xrandr_output;
	    xrandr_tail = &xrandr_output->next;
	    setit_1_2 = 1;
	    
	    continue;
	}
	if (!strcmp ("--crtc", argv[i])) {
	    if (++i >= argc) usage();
	    if (!xrandr_output) usage();
	    xrandr_output->crtc = argv[i];
	    if (sscanf (xrandr_output->crtc, "%d", &xrandr_output->randr_crtc))
		;
	    else if (sscanf (xrandr_output->crtc, "0x%x", &xrandr_output->randr_crtc) == 1)
		;
	    else
		usage ();
	    continue;
	}
	if (!strcmp ("--mode", argv[i])) {
	    if (++i >= argc) usage();
	    if (!xrandr_output) usage();
	    xrandr_output->mode = argv[i];
	    if (sscanf (xrandr_output->mode, "0x%x", &xrandr_output->randr_mode) != 1)
		xrandr_output->randr_mode = MODE_NAME;
	    continue;
	}
	if (!strcmp ("--pos", argv[i])) {
	    if (++i>=argc) usage ();
	    if (!xrandr_output) usage();
	    if (sscanf (argv[i], "%dx%d",
			&xrandr_output->x, &xrandr_output->y) != 2)
		usage ();
	    continue;
	}
	if (!strcmp ("--rotation", argv[i])) {
	    if (++i>=argc) usage ();
	    if (!xrandr_output) usage();
	    for (dirind = 0; dirind < 4; dirind++) {
		if (strcmp (direction[dirind], argv[i]) == 0) break;
	    }
	    if (dirind == 4)
		usage ();
	    xrandr_output->rotation = 1 << dirind;
	    continue;
	}
	if (!strcmp ("--left-of", argv[i])) {
	    if (++i>=argc) usage ();
	    if (!xrandr_output) usage();
	    xrandr_output->relation = xrandr_left_of;
	    xrandr_output->relative_to = argv[i];
	    continue;
	}
	if (!strcmp ("--right-of", argv[i])) {
	    if (++i>=argc) usage ();
	    if (!xrandr_output) usage();
	    xrandr_output->relation = xrandr_right_of;
	    xrandr_output->relative_to = argv[i];
	    continue;
	}
	if (!strcmp ("--above", argv[i])) {
	    if (++i>=argc) usage ();
	    if (!xrandr_output) usage();
	    xrandr_output->relation = xrandr_above;
	    xrandr_output->relative_to = argv[i];
	    continue;
	}
	if (!strcmp ("--below", argv[i])) {
	    if (++i>=argc) usage ();
	    if (!xrandr_output) usage();
	    xrandr_output->relation = xrandr_below;
	    xrandr_output->relative_to = argv[i];
	    continue;
	}
	if (!strcmp ("--off", argv[i])) {
	    if (!xrandr_output) usage();
	    xrandr_output->mode = "off";
	    xrandr_output->randr_mode = MODE_OFF;
	    continue;
	}
	if (!strcmp ("--fb", argv[i])) {
	    if (++i>=argc) usage ();
	    if (sscanf (argv[i], "%dx%d",
			&fb_width, &fb_height) != 2)
		usage ();
	    setit_1_2 = 1;

	    continue;
	}
	if (!strcmp ("--dpi", argv[i])) {
	    if (++i>=argc) usage ();
	    if (sscanf (argv[i], "%g", &dpi) != 1)
		usage ();
	    setit_1_2 = 1;
	    continue;
	}
	if (!strcmp ("--clone", argv[i])) {
	    policy = xrandr_clone;
	    setit_1_2 = 1;
	    continue;
	}
	if (!strcmp ("--panorama", argv[i])) {
	    policy = xrandr_panorama;
	    setit_1_2 = 1;
	    continue;
	}
	if (!strcmp ("--auto", argv[i])) {
	    automatic = 1;
	    setit_1_2 = 1;
	    continue;
	}
	if (!strcmp ("--q12", argv[i]))
	{
	    query_1_2 = 1;
	    continue;
	}
#endif
	usage();
    }
    if (verbose) query = 1;

    dpy = XOpenDisplay (display_name);

    if (dpy == NULL) {
	fprintf (stderr, "Can't open display %s\n", XDisplayName(display_name));
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

#if HAS_RANDR_1_2
    if (!XRRQueryVersion (dpy, &major, &minor))
    {
	fprintf (stderr, "RandR extension missing\n");
	exit (1);
    }
    if (major > 1 || (major == 1 && minor >= 2))
	has_1_2 = True;
	
    if (setit_1_2)
    {
	XRRScreenResources  *res;
	XRROutputInfo	    **output_infos;
	XRRCrtcInfo	    **crtc_infos;
	XRROutputInfo	    *output_info;
	XRRCrtcInfo	    *crtc_info;
	XRRModeInfo	    *mode_info;
	RROutput	    *outputs;
	int		    noutput;
	Bool		    *output_changing;
	Bool		    *crtc_changing;
	int		    c, o, m;
	int		    om, sm;
	int		    w = 0, h = 0;
	int		    minWidth, maxWidth, minHeight, maxHeight;

	if (!has_1_2)
	{
	    fprintf (stderr, "Server RandR version before 1.2\n");
	    exit (1);
	}
	res = XRRGetScreenResources (dpy, root);
	if (!res) usage ();
	output_infos = malloc (res->noutput * sizeof (XRROutputInfo *));
	output_changing = malloc (res->noutput * sizeof (Bool));
	outputs = malloc (res->noutput * sizeof (RROutput));
	for (o = 0; o < res->noutput; o++)
	{
	    output_infos[o] = XRRGetOutputInfo (dpy, res, res->outputs[o]);
	    output_changing[o] = False;
	}
	crtc_infos = malloc (res->ncrtc * sizeof (XRRCrtcInfo *));
	crtc_changing = malloc (res->ncrtc * sizeof (Bool));
	for (c = 0; c < res->ncrtc; c++)
	{
	    crtc_infos[c] = XRRGetCrtcInfo (dpy, res, res->crtcs[c]);
	    crtc_changing[c] = False;
	}
	
	/*
	 * Validate per-output information, fill in missing data
	 */
	for (xrandr_output = xrandr_outputs; xrandr_output; xrandr_output = xrandr_output->next)
	{
	    /* map argument to output structure */
	    for (o = 0; o < res->noutput; o++)
	    {
		if (xrandr_output->randr_output == OUTPUT_NAME) 
		{
		    if (!strcmp (output_infos[o]->name, xrandr_output->output))
			break;
		}
		else
		{
		    if (xrandr_output->randr_output == res->outputs[o])
			break;
		}
	    }
	    if (o == res->noutput)
	    {
		fprintf (stderr, "\"%s\": output unknown\n", xrandr_output->output);
		exit (1);
	    }
	    output_changing[o] = True;
	    
	    output_info = output_infos[o];
	    xrandr_output->output_info = output_info;
	    xrandr_output->randr_output = res->outputs[o];
	    
	    /*
	     * If using --off, set the desired mode and crtc to None
	     */
	    if (xrandr_output->randr_mode == MODE_OFF)
	    {
		xrandr_output->randr_mode = None;
		xrandr_output->randr_crtc = None;
		xrandr_output->x = 0;
		xrandr_output->y = 0;
		xrandr_output->rotation = RR_Rotate_0;
	    }
	    else
	    {
		/* map argument to crtc structure (if any) */
		for (c = 0; c < res->ncrtc; c++)
		{
		    if (xrandr_output->crtc == NULL)
		    {
			if (res->crtcs[c] == output_info->crtc)
			    break;
		    }
		    else if (xrandr_output->randr_crtc == res->crtcs[c])
			break;
		}
		if (c == res->ncrtc && xrandr_output->crtc)
		{
		    fprintf (stderr, "\"%s\": crtc unknown\n", xrandr_output->crtc);
		    exit (1);
		}
		if (c < res->ncrtc)
		{
		    crtc_info = crtc_infos[c];
		    xrandr_output->randr_crtc = res->crtcs[c];
		    crtc_changing[c] = True;
		}
		else
		{
		    crtc_info = NULL;
		    xrandr_output->randr_crtc = None;
		}
		xrandr_output->crtc_info = crtc_info;		

		/* map argument to mode structure (if any) */
		for (m = 0; m < res->nmode; m++)
		{
		    if (xrandr_output->mode == NULL)
		    {
			if (crtc_info && crtc_info->mode == res->modes[m].id)
			    break;
		    }
		    else if (xrandr_output->randr_mode == MODE_NAME)
		    {
			if (!strcmp (xrandr_output->mode, res->modes[m].name))
			    break;
		    }
		    else
		    {
			if (xrandr_output->randr_mode == res->modes[m].id)
			    break;
		    }
		}
		if (m == res->nmode && xrandr_output->mode)
		{
		    fprintf (stderr, "\"%s\": mode unknown\n", xrandr_output->mode);
		    exit (1);
		}
		if (m < res->nmode)
		    mode_info = &res->modes[m];
		else
		    mode_info = NULL;
		xrandr_output->mode_info = mode_info;
		xrandr_output->randr_mode = mode_info->id;

		if (xrandr_output->rotation == 0)
		{
		    if (crtc_info)
			xrandr_output->rotation = crtc_info->rotation;
		    else
			xrandr_output->rotation = RR_Rotate_0;
		}
		    
		if (xrandr_output->x == -1 || xrandr_output->y == -1)
		{
		    if (crtc_info)
		    {
			xrandr_output->x = crtc_info->x;
			xrandr_output->y = crtc_info->y;
		    }
		    else
		    {
			xrandr_output->x = 0;
			xrandr_output->y = 0;
		    }
		}
		
		if (mode_info)
		{
		    if (xrandr_output->x + mode_width (mode_info, xrandr_output->rotation) > w)
			w = xrandr_output->x + mode_width (mode_info, xrandr_output->rotation);
		    if (xrandr_output->y + mode_height (mode_info, xrandr_output->rotation) > h)
			h = xrandr_output->y + mode_height (mode_info, xrandr_output->rotation);
		}
	    }
	}
	
	/*
	 * Take existing output sizes into account to compute new screen size
	 */
	for (o = 0; o < res->noutput; o++)
	{
	    if (output_changing[o]) continue;
	    output_info = output_infos[o];
	    if (!output_info->crtc)
		continue;
	    for (c = 0; c < res->ncrtc; c++)
		if (res->crtcs[c] == output_info->crtc)
		    break;
	    if (c == res->ncrtc)
	    {
		fprintf (stderr, "\"%s\": connected to unknown crtc 0x%x\n",
			 output_info->name, output_info->crtc);
		exit (1);
	    }
	    crtc_info = crtc_infos[c];
	    for (m = 0; m < res->nmode; m++)
		if (res->modes[m].id == crtc_info->mode)
		    break;
	    if (m == res->nmode)
	    {
		fprintf (stderr, "crtc 0x%x: using unknown mode 0x%x\n",
			 res->crtcs[c], crtc_info->mode);
		exit (1);
	    }
	    mode_info = &res->modes[m];
	    if (crtc_info->x + mode_width (mode_info, crtc_info->rotation) > w)
		w = crtc_info->x + mode_width (mode_info, crtc_info->rotation);
	    if (crtc_info->y + mode_height (mode_info, crtc_info->rotation) > h)
		h = crtc_info->y + mode_height (mode_info, crtc_info->rotation);
	}

	/*
	 * Pick crtcs for any changing outputs that don't have one
	 */
	for (xrandr_output = xrandr_outputs; xrandr_output; xrandr_output = xrandr_output->next)
	{
	    if (xrandr_output->randr_mode && !xrandr_output->randr_crtc)
	    {
		int oc, co;

		output_info = xrandr_output->output_info;
		c = 0;
		for (oc = 0; oc < output_info->ncrtc; oc++)
		{
		    for (c = 0; c < res->ncrtc; c++)
			if (res->crtcs[c] == output_info->crtcs[oc])
			    break;
		    crtc_info = crtc_infos[c];
		    /*
		     * Make sure all of the outputs currently connected
		     * to this crtc are getting turned off
		     */
		    for (co = 0; co < crtc_info->noutput; co++)
		    {
			xrandr_output_t	    *cxo;

			for (cxo = xrandr_outputs; cxo; cxo = cxo->next)
			{
			    if (crtc_info->outputs[co] != cxo->randr_output)
				continue;
			    if (cxo->mode != None)
				break;
			}
			if (cxo)
			    break;
		    }
		    if (co == crtc_info->noutput)
			break;
		}
		if (oc < output_info->ncrtc)
		{
		    xrandr_output->crtc_info = crtc_info;
		    xrandr_output->randr_crtc = res->crtcs[c];
		    crtc_changing[c] = True;
		}
		else
		{
		    fprintf (stderr, "\"%s\": output has no available crtc\n",
			     output_info->name);
		    exit (1);
		}
	    }
	}
	
	
	XRRGetScreenSizeRange (dpy, RootWindow (dpy, screen),
			       &minWidth, &minHeight,
			       &maxWidth, &maxHeight);

	if (w < minWidth) w = minWidth;
	if (h < minHeight) h = minHeight;
	if (w > maxWidth || h > maxHeight)
	{
	    fprintf (stderr, "%dx%d: desired screen size too large\n", w, h);
	    exit (1);
	}
	
	/*
	 * Turn off any crtcs which are larger than the target size
	 */
	for (c = 0; c < res->ncrtc; c++)
	{
	    crtc_info = crtc_infos[c];
	    if (!crtc_info->mode) continue;
	    for (m = 0; m < res->nmode; m++)
		if (res->modes[m].id == crtc_info->mode)
		    break;
	    if (m == res->nmode)
	    {
		fprintf (stderr, "crtc 0x%x: using unknown mode 0x%x\n",
			 res->crtcs[c], crtc_info->mode);
		exit (1);
	    }
	    mode_info = &res->modes[m];
	    if (crtc_info->x + mode_width (mode_info, crtc_info->rotation) > w ||
		crtc_info->y + mode_height (mode_info, crtc_info->rotation) > h)
	    {
		if (verbose)
		    printf ("Temporarily disable crtc 0x%x\n", res->crtcs[c]); 

		XRRSetCrtcConfig (dpy, res, res->crtcs[c], CurrentTime,
				  0, 0, None, RR_Rotate_0, NULL, 0);
	    }
	}

	/*
	 * Set the screen size
	 */
	if (w != DisplayWidth (dpy, screen) || h != DisplayHeight (dpy, screen) || dpi != 0.0)
	{
	    int	mmWidth, mmHeight;

	    if (dpi <= 0)
		dpi = (25.4 * DisplayHeight (dpy, screen)) / DisplayHeightMM(dpy, screen);

	    mmWidth = (25.4 * DisplayWidth (dpy, screen)) / dpi;
	    mmHeight = (25.4 * DisplayHeight (dpy, screen)) / dpi;

	    if (verbose)
		printf ("Set screen size to %dx%d (%dx%d mm)\n",
			w, h, mmWidth, mmHeight);
    	    XRRSetScreenSize (dpy, RootWindow (dpy, screen), w, h, mmWidth, mmHeight);
	}

	/*
	 * Set crtcs
	 */

	for (c = 0; c < res->ncrtc; c++)
	{
	    RRMode  mode = None;
	    int	    x = 0;
	    int	    y = 0;
	    Rotation	rotation;
	    
	    crtc_info = crtc_infos[c];
	    if (!crtc_changing[c])
		continue;
	    noutput = 0;

	    /*
	     * Add existing and unchanging outputs
	     */
	    for (o = 0; o < res->noutput; o++)
		if (!output_changing[o] && output_infos[o]->crtc == res->crtcs[c])
		{
		    outputs[noutput++] = res->outputs[o];
		    mode = crtc_info->mode;
		    x = crtc_info->x;
		    y = crtc_info->y;
		    rotation = crtc_info->rotation;
		}
	    /*
	     * Add changing outputs
	     */
	    for (xrandr_output = xrandr_outputs; xrandr_output; xrandr_output = xrandr_output->next)
	    {
		output_info = xrandr_output->output_info;
		if (xrandr_output->randr_crtc == res->crtcs[c])
		{
		    outputs[noutput++] = xrandr_output->randr_output;
		    mode = xrandr_output->randr_mode;
		    x = xrandr_output->x;
		    y = xrandr_output->y;
		    rotation = xrandr_output->rotation;
		}
	    }
	    if (verbose) {
		if (mode)
		{
		    printf ("Setting crtc %d:\n", c);
		    printf ("\tPosition: %dx%d\n", x, y);
		    for (m = 0; m < res->nmode; m++)
			if (res->modes[m].id == mode)
			{
			    printf ("\tMode: %s\n", res->modes[m].name);
			    break;
			}
		    printf ("\tOutputs:");
		    for (o = 0; o < res->noutput; o++)
		    {
			int	co;

			for (co = 0; co < noutput; co++)
			    if (res->outputs[o] == outputs[co])
			    {
				printf (" \"%s\"", output_infos[o]->name);
				break;
			    }
		    }
		    printf ("\n");
		}
		else
		    printf ("Disabling crtc %d\n", c);
	    }
	    XRRSetCrtcConfig (dpy, res, res->crtcs[c], CurrentTime,
			      x, y, mode, rotation, outputs, noutput);
	}
	XSync (dpy, False);
	exit (0);
    }
    if (query_1_2)
    {
	XRRScreenResources	*sr;
	int		minWidth, minHeight;
	int		maxWidth, maxHeight;
	if (!has_1_2)
	{
	    fprintf (stderr, "Server RandR version before 1.2\n");
	    exit (1);
	}
	if (XRRGetScreenSizeRange (dpy, root, &minWidth, &minHeight,
				   &maxWidth, &maxHeight))
	{
	    printf ("min screen size: %d x %d\n", minWidth, minHeight);
	    printf ("cur screen size: %d x %d\n", DisplayWidth (dpy, screen), DisplayHeight(dpy, screen));
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
				      0, 100, False, False, AnyPropertyType,
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
    }
#endif
    
    sc = XRRGetScreenInfo (dpy, root);

    if (sc == NULL) 
	exit (1);

    current_size = XRRConfigCurrentConfiguration (sc, &current_rotation);

    sizes = XRRConfigSizes(sc, &nsize);

    if (have_pixel_size) {
	for (size = 0; size < nsize; size++)
	{
	    if (sizes[size].width == width && sizes[size].height == height)
		break;
	}
	if (size >= nsize) {
	    fprintf (stderr,
		     "Size %dx%d not found in available modes\n", width, height);
	    exit (1);
	}
    }
    else if (size < 0)
	size = current_size;

    if (rot < 0)
    {
	for (rot = 0; rot < 4; rot++)
	    if (1 << rot == (current_rotation & 0xf))
		break;
    }

    current_rate = XRRConfigCurrentRate (sc);

    if (rate < 0)
    {
	if (size == current_size)
	    rate = current_rate;
	else
	    rate = 0;
    }
    else
    {
	rates = XRRConfigRates (sc, size, &nrate);
	for (i = 0; i < nrate; i++)
	    if (rate == rates[i])
		break;
	if (i == nrate) {
	    fprintf (stderr, "Rate %d not available for this size\n", rate);
	    exit (1);
	}
    }

    if (version) {
	int major_version, minor_version;
	XRRQueryVersion (dpy, &major_version, &minor_version);
	printf("Server reports RandR version %d.%d\n", 
	       major_version, minor_version);
    }

    if (query) {
	printf(" SZ:    Pixels          Physical       Refresh\n");
	for (i = 0; i < nsize; i++) {
	    printf ("%c%-2d %5d x %-5d  (%4dmm x%4dmm )",
		    i == current_size ? '*' : ' ',
		    i, sizes[i].width, sizes[i].height,
		    sizes[i].mwidth, sizes[i].mheight);
	    rates = XRRConfigRates (sc, i, &nrate);
	    if (nrate) printf ("  ");
	    for (j = 0; j < nrate; j++)
		printf ("%c%-4d",
			i == current_size && rates[j] == current_rate ? '*' : ' ',
			rates[j]);
	    printf ("\n");
	}
    }

    rotations = XRRConfigRotations(sc, &current_rotation);

    rotation = 1 << rot ;
    if (query) {
	for (i = 0; i < 4; i ++) {
	    if ((current_rotation >> i) & 1) 
		printf("Current rotation - %s\n", direction[i]);
	}

	printf("Current reflection - ");
	if (current_rotation & (RR_Reflect_X|RR_Reflect_Y))
	{
	    if (current_rotation & RR_Reflect_X) printf ("X Axis ");
	    if (current_rotation & RR_Reflect_Y) printf ("Y Axis");
	}
	else
	    printf ("none");
	printf ("\n");


	printf ("Rotations possible - ");
	for (i = 0; i < 4; i ++) {
	    if ((rotations >> i) & 1)  printf("%s ", direction[i]);
	}
	printf ("\n");

	printf ("Reflections possible - ");
	if (rotations & (RR_Reflect_X|RR_Reflect_Y))
	{
	    if (rotations & RR_Reflect_X) printf ("X Axis ");
	    if (rotations & RR_Reflect_Y) printf ("Y Axis");
	}
	else
	    printf ("none");
	printf ("\n");
    }

    if (verbose) { 
	printf("Setting size to %d, rotation to %s\n",  size, direction[rot]);

	printf ("Setting reflection on ");
	if (reflection)
	{
	    if (reflection & RR_Reflect_X) printf ("X Axis ");
	    if (reflection & RR_Reflect_Y) printf ("Y Axis");
	}
	else
	    printf ("neither axis");
	printf ("\n");

	if (reflection & RR_Reflect_X) printf("Setting reflection on X axis\n");

	if (reflection & RR_Reflect_Y) printf("Setting reflection on Y axis\n");
    }

    /* we should test configureNotify on the root window */
    XSelectInput (dpy, root, StructureNotifyMask);

    if (setit) XRRSelectInput (dpy, root,
			       RRScreenChangeNotifyMask);
    if (setit) status = XRRSetScreenConfigAndRate (dpy, sc,
						   DefaultRootWindow (dpy), 
						   (SizeID) size, (Rotation) (rotation | reflection), rate, CurrentTime);

    XRRQueryExtension(dpy, &event_base, &error_base);

    if (setit && status == RRSetConfigFailed) {
	printf ("Failed to change the screen configuration!\n");
	ret = 1;
    }

    if (verbose && setit) {
	if (status == RRSetConfigSuccess)
	{
	    while (1) {
		int spo;
		XNextEvent(dpy, (XEvent *) &event);

		printf ("Event received, type = %d\n", event.type);
		/* update Xlib's knowledge of the event */
		XRRUpdateConfiguration (&event);
		if (event.type == ConfigureNotify)
		    printf("Received ConfigureNotify Event!\n");

		switch (event.type - event_base) {
		case RRScreenChangeNotify:
		    sce = (XRRScreenChangeNotifyEvent *) &event;

		    printf("Got a screen change notify event!\n");
		    printf(" window = %d\n root = %d\n size_index = %d\n rotation %d\n", 
			   (int) sce->window, (int) sce->root, 
			   sce->size_index,  sce->rotation);
		    printf(" timestamp = %ld, config_timestamp = %ld\n",
			   sce->timestamp, sce->config_timestamp);
		    printf(" Rotation = %x\n", sce->rotation);
		    printf(" %d X %d pixels, %d X %d mm\n",
			   sce->width, sce->height, sce->mwidth, sce->mheight);
		    printf("Display width   %d, height   %d\n",
			   DisplayWidth(dpy, screen), DisplayHeight(dpy, screen));
		    printf("Display widthmm %d, heightmm %d\n", 
			   DisplayWidthMM(dpy, screen), DisplayHeightMM(dpy, screen));
		    spo = sce->subpixel_order;
		    if ((spo < 0) || (spo > 5))
			printf ("Unknown subpixel order, value = %d\n", spo);
		    else printf ("new Subpixel rendering model is %s\n", order[spo]);
		    break;
		default:
		    if (event.type != ConfigureNotify) 
			printf("unknown event received, type = %d!\n", event.type);
		}
	    }
	}
    }
    XRRFreeScreenConfigInfo(sc);
    return(ret);
}
