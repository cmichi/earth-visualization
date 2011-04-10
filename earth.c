/* 
 * Copyright (c) 2011, Michael Müller <http://micha.elmueller.net>
 *
 * Blueprint code taken from
 * xscreensaver, Copyright(c) 2001-2008 Jamie Zawinski <jwz@jwz.org>
 *
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 */

#define DEFAULTS ""
#define refresh_earth 0
#define release_earth 0

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))

#include "xlockmore.h"
#include "math.h"
#include "ctype.h"

#ifdef HAVE_GLUT
#  include <GL/glut.h>
#else
# ifdef HAVE_COCOA
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#  define HAVE_GETTIMEOFDAY
# else
#  include <GL/gl.h>
#  include <GL/glu.h>
# endif
#endif

#ifdef USE_GL /* stretches over whole file */

#define RGB 1.0 / 255.0
#define M_TWOPI 6.2831853071

/* earth texture, for building the globe */
#include "images/earth.xbm"


typedef struct {
	GLXContext *glx_context;
	Bool button_down_p;	
} earth_configuration;


typedef struct {
	Display *dpy;
	Window window;
	XWindowAttributes xgwa;
	GC draw_gc, erase_gc, text_gc;
	XFontStruct *fonts[6];
	int border;
	int delay;

	char *filename;
	FILE *in;
	
#ifdef HAVE_XSHM_EXTENSION
	Bool shm_p;
	XShmSegmentInfo shm_info;
#endif
} state;


static earth_configuration *bps = NULL;
static XrmOptionDescRec opts[] = {};
static argtype vars[] = {};

ENTRYPOINT ModeSpecOpt earth_opts = {
	countof(opts), 
	opts, 
	countof(vars), 
	vars, 
	NULL
};

/* angle in which the earth is currently rotated */
double angle = 0.0;

static Bool is_more_col(int x, int y, float point_width_on_map, 
		float point_height_on_map);
static int is_black(int x, int y, int img_width, int img_height);


/* window managment etc, called when window is resized */
ENTRYPOINT void
reshape_earth (ModeInfo *mi, int width, int height)
{
	GLfloat h = (GLfloat) height / (GLfloat) width;
	glClearColor(21 * RGB, 28 * RGB, 42 * RGB, 0.0);
	
	glViewport (0, 0, (GLint) width, (GLint) height);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective (25, 1/h, 1.0, 100.0);
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0.0, 0.0, 25.0,
		  0.0, 0.0, 0.0,
		  0.0, 1.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
}


/* ENTRYPOINT Bool */
static void earth_handle_event (ModeInfo *mi, XEvent *event)
{
	/* currently there is nothing handled here */
}


ENTRYPOINT void 
init_earth (ModeInfo *mi)
{
	int wire = MI_IS_WIREFRAME(mi);
	earth_configuration *bp;
	
	if (!bps) {
		bps = (earth_configuration *) calloc (MI_NUM_SCREENS(mi), 
				sizeof (earth_configuration));
		if (!bps) {
			fprintf(stderr, "%s: out of memory\n", progname);
			exit(1);
		}
	}
	

	bp = &bps[MI_SCREEN(mi)];
	bp->glx_context = init_GL(mi);
	
	reshape_earth (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

	 if (!wire)
	 {
		 glEnable(GL_BLEND);		 
		 glEnable(GL_DEPTH_TEST);
		 glShadeModel (GL_SMOOTH);
		 glEnable(GL_BLEND);
		 glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		 glEnable(GL_NORMALIZE);
		 glEnable( GL_POINT_SMOOTH );		 
		 
		 glEnable(GL_LIGHTING);
		 
		 GLfloat light1_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
		 GLfloat light1_ambient[] = { 1.0,1.0, 1.0, 1.0 };
		 GLfloat light1_specular[] = { 1.0, 1.0, 0.0, 1.0 };
		 GLfloat light1_position[] = { 0.0, 0.0, 0.0, 1.0 };
		 GLfloat spot_direction[] = { 0.0, 0.0, 1.0 };
		 
		 glLightfv(GL_LIGHT1, GL_AMBIENT, light1_ambient);
		 glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
		 glLightfv(GL_LIGHT1, GL_SPECULAR, light1_specular);
		 glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
		 glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0.01);
		 glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.01);
		 glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.01);
		 
		 glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 80.0);
		 glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, spot_direction);
		 glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 2.0);
		 
		 glEnable(GL_LIGHT1);		 
		 
		 GLfloat light2_diffuse[] = { 1.0, 0.0, 0.0, 0.4 };
		 GLfloat light2_ambient[] = { 1 * RGB, 0 * RGB, 0 * RGB,  1.0 };
		 GLfloat light2_specular[] = { 0.0, 1.0, 0.0, 1.0 };
		 GLfloat light2_position[] = { 0.0, 0.0, 0.0, 1.0 };
		 GLfloat spot_direction2[] = { 0.0, 0.0, -1.0 };
		 
		 glLightfv(GL_LIGHT2, GL_AMBIENT, light2_ambient);
		 glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_diffuse);
		 glLightfv(GL_LIGHT2, GL_SPECULAR, light2_specular);
		 glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
		 glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, 0.08);
		 glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.08);
		 glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.08);
		 
		 glLightf(GL_LIGHT2, GL_SPOT_CUTOFF, 180.0);
		 glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, spot_direction2);
		 glLightf(GL_LIGHT2, GL_SPOT_EXPONENT, 128.0);
		 
		 glEnable(GL_LIGHT2);
	 }
}


static void draw_particle(double uX, double uY, double uZ, double uX_, 
		double uY_, double uZ_) 
{	
	glPointSize(5.0);
	glBegin(GL_POINTS);
	glNormal3d(uX, uY, uZ);
	glVertex3f(uX_, uY_, uZ_);
	glEnd();

}


/* set the material for the particles that represent the globe */
static void particleMaterial() 
{
	GLfloat no_mat[] =  {0 * RGB, 0 * RGB, 0 * RGB,  1.0};
	GLfloat high_shininess[] =  {1.0};
	
	GLfloat no_mat_back[] =  {21 * RGB, 28 * RGB, 42 * RGB,  1.0};
	GLfloat high_shininess_back[] =  {10.0};	
	
	glMaterialfv(GL_BACK, GL_AMBIENT, no_mat);
	glMaterialfv(GL_BACK, GL_DIFFUSE, no_mat);
	glMaterialfv(GL_BACK, GL_SPECULAR, no_mat);
	glMaterialfv(GL_BACK, GL_EMISSION, no_mat);
	glMaterialfv(GL_BACK, GL_SHININESS, high_shininess);
	
	glMaterialfv(GL_FRONT, GL_AMBIENT, no_mat_back);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, no_mat_back);
	glMaterialfv(GL_FRONT, GL_SPECULAR, no_mat_back);
	glMaterialfv(GL_FRONT, GL_EMISSION, no_mat_back);
	glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess_back);
	
}


ENTRYPOINT void
draw_earth (ModeInfo *mi)
{	
	double c = 190.0;
	double scale = 4.0;
	double scale2 = 4.7;
	double point_width_on_map = (EARTH_WIDTH / c);
	double point_height_on_map = (EARTH_HEIGHT / c);
	double maxUY = 0;
	double minUY = 0;
	double x, y, uX, uY, uZ, uX_, uY_, uZ_;
	
	earth_configuration *bp = &bps[MI_SCREEN(mi)];
	Display *dpy = MI_DISPLAY(mi);
	Window window = MI_WINDOW(mi);	
	
	
	glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *(bp->glx_context));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | 
					GL_ACCUM_BUFFER_BIT);
	
	
	glPushMatrix();
	glRotated(90, 0.0, 0.0, 0.0);
	
	glPushMatrix();		
	angle = (int) (angle + 1) % 360;	
	glRotated(angle, 0.0, 0.0, 1.0);
	
	
	particleMaterial();				

	for (x = 0.0; x < M_PI; x += M_PI /c) {		
		for (y = 0.0; y < M_TWOPI; y += M_TWOPI / c) {
			uX = scale * sin(x) * cos(y);
			uY = scale * sin(x) * sin(y);
			uZ = scale * cos(x);
			
			uX_ = scale2 * sin(x) * cos(y);
			uY_ = scale2 * sin(x) * sin(y);
			uZ_ = scale2 * cos(x);
			
			/* yAufKarte = 0..500; */
			double yAufKarte = (EARTH_HEIGHT / 8.0) * (uZ + 4.0);
						
			/* 0..1000 */
			double xAufKarte = (y * EARTH_WIDTH) / M_TWOPI;
			xAufKarte = EARTH_WIDTH - xAufKarte;
			
			
			/* 0..25.6 */
			if (xAufKarte > maxUY) maxUY = xAufKarte;
			if (xAufKarte < minUY) minUY = xAufKarte;
			
			
			if (is_more_col(xAufKarte, yAufKarte,  
					point_width_on_map,  point_height_on_map) == True) 
					draw_particle(uX, uY, uZ, uX_, uY_, uZ_);
			
			
		}
	}

	glPopMatrix();	
	glPopMatrix();	
	
	
	if (mi->fps_p) 
		do_fps (mi);
	glFinish();
	glXSwapBuffers(dpy, window);
}



/*
 * returns the pixel color for a XBM (X BitMap) image.
 *
 * XBM is a plain text monochrome image format that takes the form of 
 * C source code. images can directly be compiled into an application.
 *
 */
static int is_black(int x, int y, int img_width, int img_height) 
{
	char byte_containing_px;
	
	/* byte, containing pixel bit */
	int	byte_containing_px_position = ((y * img_width) + x) / 8;
	
	
	if (img_width % 8 > 0)
		byte_containing_px_position += (y / 8) + 1;
	
	/* pointer in ranges? */
	if (byte_containing_px_position >=  ((img_width * img_height) / 8))
		return 0;
	
	byte_containing_px = *(earth_bits + byte_containing_px_position);
	
	/* searched byte has to be on rightmost position */
	byte_containing_px >>= x;
	
	return (byte_containing_px & 0x1) ? 1 : 0;
}


/* 
 * checks whether there is a bigger black then white part
 * in the square "width * height"
 */
static Bool is_more_col(int x, int y, float point_width_on_map, 
				float point_height_on_map) 
{
	int x1, y1;
	int pixels_in_color = 0;
	int treshold = (point_width_on_map * point_height_on_map) / 2;
	
	for (x1 = 0; x1 < point_width_on_map; x1++) {
		for (y1 = 0; y1 < point_height_on_map; y1++) {
			
			if (is_black(x + x1, y + y1, EARTH_WIDTH, EARTH_HEIGHT) == 1)
				pixelsInColor++;
			
		}
	}
	
	if (pixels_in_color >= treshold)
		return True;
	else
		return False;
}


XSCREENSAVER_MODULE_2 ("Earth", earth, earth)

#endif /* USE_GL */
