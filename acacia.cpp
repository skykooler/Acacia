    /*------------------------------------------------------------------------
 * A demonstration of OpenGL in a  ARGB window
 *    => support for composited window transparency
 *
 * (c) 2011 by Wolfgang 'datenwolf' Draxinger
 *     See me at comp.graphics.api.opengl and StackOverflow.com

 * License agreement: This source code is provided "as is". You
 * can use this source code however you want for your own personal
 * use. If you give this source code to anybody else then you must
 * leave this message in it.
 *
 * This program is based on the simplest possible
 * Linux OpenGL program by FTB (see info below)

  The simplest possible Linux OpenGL program? Maybe...

  (c) 2002 by FTB. See me in comp.graphics.api.opengl

  --
  <\___/>
  / O O \
  \_____/  FTB.

------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <png.h>
#include <assert.h>
#include <dirent.h>
#include <sys/time.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xutil.h>


#include <string>
#include <vector>
#include <algorithm>

#define USE_CHOOSE_FBCONFIG
#define PNG_SIG_BYTES 8

#define TYPE_FILE 0
#define TYPE_FOLDER 1

using namespace std;

static void fatalError(const char *why)
{
    fprintf(stderr, "%s", why);
    exit(0x666);
}

typedef struct  {
    unsigned long   flags;
    unsigned long   functions;
    unsigned long   decorations;
    long            inputMode;
    unsigned long   status;
} Hints;

Hints rhints;
Atom property;

float Opacity = 0.75;
float Falloff = 0.5;
float scaleFactor = 0.2;
const double pi = 3.14159265359;

float red = 0.5;
float green = 0.75;
float blue = 1.0;

int mouse_x, mouse_y;
int button;

static int Xscreen;
static Atom del_atom;
static Colormap cmap;
static Display *Xdisplay;
static XVisualInfo *visual;
static XRenderPictFormat *pict_format;
static GLXFBConfig *fbconfigs, fbconfig;
static int numfbconfigs;
static GLXContext render_context;
static Window Xroot, window_handle;
static GLXWindow glX_window_handle;
static int width, height;

static int VisData[] = {
GLX_RENDER_TYPE, GLX_RGBA_BIT,
GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
GLX_DOUBLEBUFFER, True,
GLX_RED_SIZE, 8,
GLX_GREEN_SIZE, 8,
GLX_BLUE_SIZE, 8,
GLX_ALPHA_SIZE, 8,
GLX_DEPTH_SIZE, 16,
None
};

struct timeval earlier;
struct timeval later;

long long delta_t;

long long timeval_diff(struct timeval *difference, struct timeval *end_time, struct timeval *start_time) {
  struct timeval temp_diff;

  if(difference==NULL) {
    difference=&temp_diff;
  }

  difference->tv_sec =end_time->tv_sec -start_time->tv_sec;
  difference->tv_usec=end_time->tv_usec-start_time->tv_usec;

  /* Using while instead of if below makes the code slightly more robust. */

  while(difference->tv_usec<0) {
    difference->tv_usec+=1000000;
    difference->tv_sec -=1;
  }

  return 1000000LL*difference->tv_sec+difference->tv_usec;

} /* timeval_diff() */

void clickRoot();

static int isExtensionSupported(const char *extList, const char *extension) {

  const char *start;
  const char *where, *terminator;

  /* Extension names should not have spaces. */
  where = strchr(extension, ' ');
  if ( where || *extension == '\0' )
    return 0;

  /* It takes a bit of care to be fool-proof about parsing the
     OpenGL extensions string. Don't be fooled by sub-strings,
     etc. */
  for ( start = extList; ; ) {
    where = strstr( start, extension );

    if ( !where )
      break;

    terminator = where + strlen( extension );

    if ( where == start || *(where - 1) == ' ' )
      if ( *terminator == ' ' || *terminator == '\0' )
    return 1;

    start = terminator;
  }
  return 0;
}

static Bool WaitForMapNotify(Display *d, XEvent *e, char *arg) {
    return d && e && arg && (e->type == MapNotify) && (e->xmap.window == *(Window*)arg);
}

static void describe_fbconfig(GLXFBConfig fbconfig) {
    int doublebuffer;
    int red_bits, green_bits, blue_bits, alpha_bits, depth_bits;

    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_DOUBLEBUFFER, &doublebuffer);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_RED_SIZE, &red_bits);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_GREEN_SIZE, &green_bits);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_BLUE_SIZE, &blue_bits);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_ALPHA_SIZE, &alpha_bits);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_DEPTH_SIZE, &depth_bits);

    fprintf(stderr, "FBConfig selected:\n"
    "Doublebuffer: %s\n"
    "Red Bits: %d, Green Bits: %d, Blue Bits: %d, Alpha Bits: %d, Depth Bits: %d\n",
    doublebuffer == True ? "Yes" : "No",
    red_bits, green_bits, blue_bits, alpha_bits, depth_bits);
}

static void createTheWindow() {
    XEvent event;
    int x,y, attr_mask;
    XSizeHints hints;
    XWMHints *startup_state;
    XTextProperty textprop;
    XSetWindowAttributes attr = {0,};
    static const char *title = "Acacia";

    Xdisplay = XOpenDisplay(NULL);
    if (!Xdisplay) {
    fatalError("Couldn't connect to X server\n");
    }
    Xscreen = DefaultScreen(Xdisplay);
    Xroot = RootWindow(Xdisplay, Xscreen);

    fbconfigs = glXChooseFBConfig(Xdisplay, Xscreen, VisData, &numfbconfigs);
    fbconfig = 0;
    int i;
    for(i = 0; i<numfbconfigs; i++) {
        visual = (XVisualInfo*) glXGetVisualFromFBConfig(Xdisplay, fbconfigs[i]);
        if(!visual)
            continue;

        pict_format = XRenderFindVisualFormat(Xdisplay, visual->visual);
        if(!pict_format)
            continue;

        fbconfig = fbconfigs[i];
        if(pict_format->direct.alphaMask > 0) {
            break;
        }
    }

    if(!fbconfig) {
    fatalError("No matching FB config found");
    }

    describe_fbconfig(fbconfig);

    /* Create a colormap - only needed on some X clients, eg. IRIX */
    cmap = XCreateColormap(Xdisplay, Xroot, visual->visual, AllocNone);

    attr.colormap = cmap;
    attr.background_pixmap = None;
    attr.border_pixmap = None;
    attr.border_pixel = 0;
    attr.event_mask =
    StructureNotifyMask |
    EnterWindowMask |
    LeaveWindowMask |
    ExposureMask |
    ButtonPressMask |
    ButtonReleaseMask |
    PointerMotionMask |
    Button1MotionMask |
    OwnerGrabButtonMask |
    KeyPressMask |
    KeyReleaseMask;

    attr_mask =
    CWBackPixmap|
    CWColormap|
    CWBorderPixel|
    CWEventMask;

    width = DisplayWidth(Xdisplay, DefaultScreen(Xdisplay));//2;
    height = DisplayHeight(Xdisplay, DefaultScreen(Xdisplay));//2;
    x=0, y=0;

    window_handle = XCreateWindow(  Xdisplay,
            Xroot,
            x, y, width, height,
            0,
            visual->depth,
            InputOutput,
            visual->visual,
            attr_mask, &attr);

    if( !window_handle ) {
    fatalError("Couldn't create the window\n");
    }

    #if USE_GLX_CREATE_WINDOW
        int glXattr[] = { None };
        glX_window_handle = glXCreateWindow(Xdisplay, fbconfig, window_handle, glXattr);
        if( !glX_window_handle ) {
        fatalError("Couldn't create the GLX window\n");
        }
    #else
        glX_window_handle = window_handle;
    #endif

    textprop.value = (unsigned char*)title;
    textprop.encoding = XA_STRING;
    textprop.format = 8;
    textprop.nitems = strlen(title);

    hints.x = x;
    hints.y = y;
    hints.width = width;
    hints.height = height;
    hints.flags = USPosition|USSize;

    startup_state = XAllocWMHints();
    startup_state->initial_state = NormalState;
    startup_state->flags = StateHint;

    XSetWMProperties(Xdisplay, window_handle,&textprop, &textprop,
        NULL, 0,
        &hints,
        startup_state,
        NULL);


    XFree(startup_state);

    XMapWindow(Xdisplay, window_handle);
    XIfEvent(Xdisplay, &event, WaitForMapNotify, (char*)&window_handle);

    /*****************************/
    rhints.flags = 2;        // Specify that we're changing the window decorations.
    rhints.decorations = 0;  // 0 (false) means that window decorations should go bye-bye.
    property = XInternAtom(Xdisplay,"_MOTIF_WM_HINTS",True);
    XChangeProperty(Xdisplay,window_handle,property,property,32,PropModeReplace,(unsigned char *)&rhints,5);

    // XMoveResizeWindow(Xdisplay,window_handle,0,0,width,height);

    XMapRaised(Xdisplay,window_handle);

    XGrabPointer(Xdisplay,window_handle,True,0,GrabModeAsync,GrabModeAsync,window_handle,0L,CurrentTime);

    // XGrabKeyboard(Xdisplay,window_handle,False,GrabModeAsync,GrabModeAsync,CurrentTime);
    /*****************************/


    if ((del_atom = XInternAtom(Xdisplay, "WM_DELETE_WINDOW", 0)) != None) {
    XSetWMProtocols(Xdisplay, window_handle, &del_atom, 1);
    }
}

static int ctxErrorHandler( Display *dpy, XErrorEvent *ev ) {
    fputs("Error at context creation", stderr);
    return 0;
}

static void createTheRenderContext() {
    int dummy;
    if (!glXQueryExtension(Xdisplay, &dummy, &dummy)) {
    fatalError("OpenGL not supported by X server\n");
    }

#if USE_GLX_CREATE_CONTEXT_ATTRIB
    #define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
    #define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
    render_context = NULL;
    if( isExtensionSupported( glXQueryExtensionsString(Xdisplay, DefaultScreen(Xdisplay)), "GLX_ARB_create_context" ) ) {
    typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );
    if( glXCreateContextAttribsARB ) {
        int context_attribs[] =
        {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 0,
        //GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
        };

        int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&ctxErrorHandler);

        render_context = glXCreateContextAttribsARB( Xdisplay, fbconfig, 0, True, context_attribs );

        XSync( Xdisplay, False );
        XSetErrorHandler( oldHandler );

        fputs("glXCreateContextAttribsARB failed", stderr);
    } else {
        fputs("glXCreateContextAttribsARB could not be retrieved", stderr);
    }
    } else {
        fputs("glXCreateContextAttribsARB not supported", stderr);
    }

    if(!render_context)
    {
#else
    {
#endif
    render_context = glXCreateNewContext(Xdisplay, fbconfig, GLX_RGBA_TYPE, 0, True);
    if (!render_context) {
        fatalError("Failed to create a GL context\n");
    }
    }

    if (!glXMakeContextCurrent(Xdisplay, glX_window_handle, glX_window_handle, render_context)) {
    fatalError("glXMakeCurrent failed for window\n");
    }
}

static int updateTheMessageQueue() {
    XEvent event;
    XConfigureEvent *xc;

    while (XPending(Xdisplay))
    {
    XNextEvent(Xdisplay, &event);
    switch (event.type)
    {
    case ClientMessage:
        if (event.xclient.data.l[0] == del_atom)
        {
        return 0;
        }
    break;

    case ButtonPress:
        switch(event.xbutton.button){
            case Button1:
                mouse_x=event.xbutton.x;
                mouse_y=height-event.xbutton.y;
                button=Button1;
                clickRoot();
                printf("Mouse position: %i %i Button: %i\n", mouse_x,mouse_y,button);
                break;
            case Button3:
                mouse_x=event.xbutton.x;
                mouse_y=height-event.xbutton.y;
                button=Button3;
                break;
            default:
                break;
        }
        break;

    case MotionNotify:
        mouse_x = event.xbutton.x;
        mouse_y = height-event.xbutton.y;
        break;

    case ConfigureNotify:
        xc = &(event.xconfigure);
        width = xc->width;
        height = xc->height;
        break;
    }
    }
    return 1;
}

/*  6----7
   /|   /|
  3----2 |
  | 5--|-4
  |/   |/
  0----1

*/

GLfloat cube_vertices[][8] =  {
    /*  X     Y     Z   Nx   Ny   Nz    S    T */
    {-1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 0.0, 0.0}, // 0
    { 1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 1.0, 0.0}, // 1
    { 1.0,  1.0,  1.0, 0.0, 0.0, 1.0, 1.0, 1.0}, // 2
    {-1.0,  1.0,  1.0, 0.0, 0.0, 1.0, 0.0, 1.0}, // 3

    { 1.0, -1.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0}, // 4
    {-1.0, -1.0, -1.0, 0.0, 0.0, -1.0, 1.0, 0.0}, // 5
    {-1.0,  1.0, -1.0, 0.0, 0.0, -1.0, 1.0, 1.0}, // 6
    { 1.0,  1.0, -1.0, 0.0, 0.0, -1.0, 0.0, 1.0}, // 7

    {-1.0, -1.0, -1.0, -1.0, 0.0, 0.0, 0.0, 0.0}, // 5
    {-1.0, -1.0,  1.0, -1.0, 0.0, 0.0, 1.0, 0.0}, // 0
    {-1.0,  1.0,  1.0, -1.0, 0.0, 0.0, 1.0, 1.0}, // 3
    {-1.0,  1.0, -1.0, -1.0, 0.0, 0.0, 0.0, 1.0}, // 6

    { 1.0, -1.0,  1.0,  1.0, 0.0, 0.0, 0.0, 0.0}, // 1
    { 1.0, -1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 0.0}, // 4
    { 1.0,  1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 1.0}, // 7
    { 1.0,  1.0,  1.0,  1.0, 0.0, 0.0, 0.0, 1.0}, // 2

    {-1.0, -1.0, -1.0,  0.0, -1.0, 0.0, 0.0, 0.0}, // 5
    { 1.0, -1.0, -1.0,  0.0, -1.0, 0.0, 1.0, 0.0}, // 4
    { 1.0, -1.0,  1.0,  0.0, -1.0, 0.0, 1.0, 1.0}, // 1
    {-1.0, -1.0,  1.0,  0.0, -1.0, 0.0, 0.0, 1.0}, // 0

    {-1.0, 1.0,  1.0,  0.0,  1.0, 0.0, 0.0, 0.0}, // 3
    { 1.0, 1.0,  1.0,  0.0,  1.0, 0.0, 1.0, 0.0}, // 2
    { 1.0, 1.0, -1.0,  0.0,  1.0, 0.0, 1.0, 1.0}, // 7
    {-1.0, 1.0, -1.0,  0.0,  1.0, 0.0, 0.0, 1.0}, // 6
};

static void draw_cube(void) {
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glVertexPointer(3, GL_FLOAT, sizeof(GLfloat) * 8, &cube_vertices[0][0]);
    glNormalPointer(GL_FLOAT, sizeof(GLfloat) * 8, &cube_vertices[0][3]);
    glTexCoordPointer(2, GL_FLOAT, sizeof(GLfloat) * 8, &cube_vertices[0][6]);

    glDrawArrays(GL_QUADS, 0, 24);
}

float const light0_dir[]={0,1,0,0};
float const light0_color[]={78./255., 80./255., 184./255.,1};

float const light1_dir[]={-1,1,1,0};
float const light1_color[]={255./255., 220./255., 97./255.,1};

float const light2_dir[]={0,-1,0,0};
float const light2_color[]={31./255., 75./255., 16./255.,1};

void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a=1) {
    glColor4f(r,g,b,a);
    glDisable(GL_TEXTURE_2D);
    const GLshort v[8] = { x, y, x+w, y, x+w, y+h, x, y+h };

    glVertexPointer( 2, GL_SHORT, 0, v );
    glEnableClientState( GL_VERTEX_ARRAY );
    
    glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
}

void draw_img(float x, float y, float w, float h, GLuint tex,float r=1,float g=1, float b=1, float a=1) {
        glColor4f(r,g,b,a);
        const GLshort t[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
        const GLshort v[8] = { x, y, x+w, y, x+w, y+h, x, y+h };

        glVertexPointer( 2, GL_SHORT, 0, v );
        glEnableClientState( GL_VERTEX_ARRAY );
    
        glTexCoordPointer( 2, GL_SHORT, 0, t );
        glEnableClientState( GL_TEXTURE_COORD_ARRAY );
        
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
        glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );//*/

    }

typedef struct {
        GLuint tex;
        int width;
        int height;
} Img;


char * load_png(char *name, int *width, int *height) {
    FILE *png_file = fopen(name, "rb");
    assert(png_file);

    uint8_t header[PNG_SIG_BYTES];

    fread(header, 1, PNG_SIG_BYTES, png_file);
    assert(!png_sig_cmp(header, 0, PNG_SIG_BYTES));

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png_ptr);

    png_infop info_ptr = png_create_info_struct(png_ptr);
    assert(info_ptr);

    png_infop end_info = png_create_info_struct(png_ptr);
    assert(end_info);

    assert(!setjmp(png_jmpbuf(png_ptr)));
    png_init_io(png_ptr, png_file);
    png_set_sig_bytes(png_ptr, PNG_SIG_BYTES);
    png_read_info(png_ptr, info_ptr);

    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);

    png_uint_32 bit_depth, color_type;
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
            
    if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if (bit_depth == 16)
            png_set_strip_16(png_ptr);
            
    if(color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    else if(color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        {
            png_set_gray_to_rgb(png_ptr);
        }

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    else
        png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

    png_read_update_info(png_ptr, info_ptr);

    png_uint_32 rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    png_uint_32 numbytes = rowbytes*(*height);
    png_byte* pixels = (png_byte*)malloc(numbytes);
    png_byte** row_ptrs = (png_byte**)malloc((*height) * sizeof(png_byte*));

    int i;
    for (i=0; i<(*height); i++)
      row_ptrs[i] = pixels + ((*height) - 1 - i)*rowbytes;

    png_read_image(png_ptr, row_ptrs);

    free(row_ptrs);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    fclose(png_file);

    return (char *)pixels;
}

Img load_img(const char *filename) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    
    int w, h;
    GLubyte *pixels = (GLubyte *)load_png((char*)filename, &w, &h);
    gluBuild2DMipmaps( GL_TEXTURE_2D, 4, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels );
    free(pixels);
    Img img;
    img.tex = tex;
    img.width = w;
    img.height = h;
    return img;
}

Img circle;
Img line;

class File {
    public:
        File();
        File(string);
        void draw(float opacity=1);
        string path;
        int filetype;
        float hoverScaleFactor;
        float hoverVelocity;
    /* data */
};
File::File() {
    filetype = TYPE_FILE;
    hoverScaleFactor = 0;
    hoverVelocity = 0;
}
File::File(string pth) {
    path = pth;
    filetype = TYPE_FILE;
    hoverScaleFactor = 0;
    hoverVelocity = 0;
}
void File::draw(float opacity) {
    draw_img(-128,-128,256,256,circle.tex,red,green,blue,opacity);
}

File FILE_NOT_FOUND;

class Folder : public File {
    public:
        // Get initializer from File. Probably.
        Folder(string);// : File(pth) {};
        vector<File *> children;
        // void read();
        void read(int);
        void draw(float opacity=1);
        void click(float opacity=1);
        void transform(float opacity=1);
        Folder * openChild;
        int openChildIndex;
};

void GetFilesInDirectory(std::vector<File *> &out, const string &directory, int levelsleft)
{
    
    DIR *dpdf;
    struct dirent *epdf;

    dpdf = opendir(directory.c_str());
    if (dpdf != NULL){
       while (epdf = readdir(dpdf)){
            if (epdf->d_name[0] == '.')
                continue;
            string thefile;
            thefile.append(directory);
            thefile.append(epdf->d_name);
            if (epdf->d_type==DT_REG) {
                printf("%s: %s\n", "File", thefile.c_str());
                out.push_back(new File(thefile));
            } else if (epdf->d_type==DT_DIR) {
                thefile.append("/");
                printf("%s: %s\n", "Folder",thefile.c_str());
                Folder *f = new Folder(thefile);
                if (levelsleft>0) {
                    (*f).read(levelsleft-1);
                }
                out.push_back(f);
            }
            // printf("Filename: %s\n",epdf->d_name);
       }
    }
}

Folder::Folder(string pth) {
    path = pth;
    filetype = TYPE_FOLDER;
    hoverScaleFactor = 0;
    hoverVelocity = 0;
    openChild = (Folder *)&FILE_NOT_FOUND;
    openChildIndex = -1;
}

void Folder::read(int levelsleft=1) {
    GetFilesInDirectory(children,path,levelsleft);
}
void Folder::draw(float opacity) {
    // printf("%s\n", path.c_str());
    glPushMatrix();
    int last_mouse_x = mouse_x;
    int last_mouse_y = mouse_y;
    if (not (openChild==(Folder *)&FILE_NOT_FOUND)) {
        float theta = ((float)openChildIndex)/(float)(children.size())*2*pi;
        float x = 256*opacity*sin(theta);
        float y = 256*opacity*cos(theta);
        // glScalef(1/scaleFactor,1/scaleFactor,1/scaleFactor);
        // glTranslatef(-x,-y,0);
        mouse_x = (mouse_x*scaleFactor)+x;
        mouse_y = (mouse_y*scaleFactor)+y;
    }
    draw_img(-128,-128,256,256,circle.tex,red,green,blue,opacity);
    int sub_last_mouse_x, sub_last_mouse_y;
    int s = children.size();
    for (int i=0; i<s; i++) {
        float theta = ((float)i)/(float)(children.size())*2*pi;
        float x = 256*opacity*sin(theta);
        float y = 256*opacity*cos(theta);
        // glLineWidth(2.5); 
        // glColor4f(red,green,blue,1.0);
        // glBegin(GL_LINES);
        //     glVertex3f(0.0,0.0,0.0);
        //     glVertex3f(140.0,y,0.0);
        // glEnd();
        // glFlush();
        glPushMatrix();
        glRotatef(180*theta/pi,0.0f,0.0f,1.0f);
        draw_img(-4,0,8,256*opacity,line.tex,red,green,blue,opacity*0.25);
        glPopMatrix();
        glPushMatrix();
        // glTranslatef(x-(128*scaleFactor),y-(128*scaleFactor),0);
        sub_last_mouse_x = mouse_x;
        sub_last_mouse_y = mouse_y;
        float localScaleFactor = scaleFactor;
        glTranslatef(x,y,0);
        // mouse_x = (mouse_x-x)/scaleFactor;
        // mouse_y = (mouse_y-y)/scaleFactor;
        mouse_x = (mouse_x-x)/localScaleFactor;
        mouse_y = (mouse_y-y)/localScaleFactor;
        float hoverVelocityTarget;
        if (mouse_x>-128 and mouse_x<128 and mouse_y>-128 and mouse_y<128 and (not (openChildIndex==i))) {
            // Set a target for the velocity of what it would be if we just used a straight exponential ease
            hoverVelocityTarget = ((1-(*children.at(i)).hoverScaleFactor)/100000);
            // if (opacity==1) {
            //     printf("delta_t: %i\n", (int)delta_t);
            // }
        } else {
            hoverVelocityTarget = ((0-(*children.at(i)).hoverScaleFactor)/100000);
        }
        // Exponentially ease velocity towards target
        (*children.at(i)).hoverVelocity = (*children.at(i)).hoverVelocity+((hoverVelocityTarget-(*children.at(i)).hoverVelocity)/100000)*delta_t;
        // Yes, I'm using Euler integration. So sue me. It's fast, and the decaying exponentials cancel out any energy gain.
        (*children.at(i)).hoverScaleFactor = (*children.at(i)).hoverScaleFactor+(*children.at(i)).hoverVelocity*delta_t;
        localScaleFactor = scaleFactor+(*children.at(i)).hoverScaleFactor*scaleFactor*0.5;
        glScalef(localScaleFactor,localScaleFactor,localScaleFactor);
        float child_opacity = 1;
        if (children.size()>24) {
            child_opacity = opacity*(24.0/(float)children.size());
        }
        switch ((*children.at(i)).filetype) {
            case TYPE_FILE:
                (*(children.at(i))).draw(child_opacity*Falloff);
                break;
            case TYPE_FOLDER:
                if (openChildIndex==i) {
                    (*(Folder*)(children.at(i))).draw(1);
                } else {
                    (*(Folder*)(children.at(i))).draw(child_opacity*Falloff);
                }
                break;
        }
        mouse_y = sub_last_mouse_y;
        mouse_x = sub_last_mouse_x;
        glPopMatrix();
    }
    mouse_y = last_mouse_y;
    mouse_x = last_mouse_x;

    glPopMatrix();
}
void Folder::click(float opacity) {
    printf("Click got to %s\n", path.c_str());
    int last_mouse_x = mouse_x;
    int last_mouse_y = mouse_y;
    if (not (openChild==(Folder *)&FILE_NOT_FOUND)) {
        float theta = ((float)openChildIndex)/(float)(children.size())*2*pi;
        float x = 256*opacity*sin(theta);
        float y = 256*opacity*cos(theta);
        mouse_x = (mouse_x*scaleFactor)+x;
        mouse_y = (mouse_y*scaleFactor)+y;
    }
    int s = children.size();
    int sub_last_mouse_x;
    int sub_last_mouse_y;
    for (int i=0; i<s; i++) {
        float theta = ((float)i)/(float)(children.size())*2*pi;
        float x = 256*opacity*sin(theta);
        float y = 256*opacity*cos(theta);
        sub_last_mouse_x = mouse_x;
        sub_last_mouse_y = mouse_y;
        float localScaleFactor = scaleFactor;
        // glTranslatef(x,y,0);
        mouse_x = (mouse_x-x)/scaleFactor;
        mouse_y = (mouse_y-y)/scaleFactor;
        printf("%s: mouse_x: %i, mouse_y: %i\n", path.c_str(), mouse_x, mouse_y);
        if (i==openChildIndex) {
            (*openChild).click(opacity);
        } else {
            if (mouse_x>-128 and mouse_x<128 and mouse_y>-128 and mouse_y<128) {
                switch ((*children.at(i)).filetype) {
                    case TYPE_FILE:
                        printf("Clicked %s\n", (*children.at(i)).path.c_str());
                        break;
                    case TYPE_FOLDER:
                        printf("Clicked folder %s\n", (*children.at(i)).path.c_str());
                        openChild = (Folder *)children.at(i);
                        openChildIndex = std::find(children.begin(), children.end(), openChild) - children.begin();
                    printf("%i\n", openChildIndex);
                    break;
                }
            }
        }
        mouse_x = sub_last_mouse_x;
        mouse_y = sub_last_mouse_y;
    }
    mouse_x = last_mouse_x;
    mouse_y = last_mouse_y;
}

void Folder::transform(float opacity){
    if (not (openChild==(Folder *)&FILE_NOT_FOUND)) {
        float theta = ((float)openChildIndex)/(float)(children.size())*2*pi;
        float x = 256*opacity*sin(theta);
        float y = 256*opacity*cos(theta);
        glScalef(1/scaleFactor,1/scaleFactor,1/scaleFactor);
        glTranslatef(-x,-y,0);
        (*openChild).transform(opacity);
    }
}

Folder root("/");

void clickRoot() {
    int last_mouse_x = mouse_x;
    int last_mouse_y = mouse_y;
    mouse_x = mouse_x-width/2;
    mouse_y = mouse_y-height/2;
    root.click();
    mouse_x = last_mouse_x;
    mouse_y = last_mouse_y;
}

static void redrawTheWindow()
{
    gettimeofday(&later,NULL);
    delta_t = timeval_diff(NULL,&later,&earlier);
    gettimeofday(&earlier,NULL);

    float const aspect = (float)width / (float)height;

    static float a=0;
    static float b=0;
    static float c=0;

    glDrawBuffer(GL_BACK);

    glViewport(0, 0, width, height);

    // Clear with alpha = 0.0, i.e. full transparency
    glClearColor(0.0, 0.0, 0.0, Opacity);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    // glOrtho(0.0, width, 0.0, height, -1.0, 1.0);
    gluOrtho2D(0, width, 0, height);
    

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* COLOR */
    // draw_img(width/2-128,height/2-128,256,256,circle.tex,red,green,blue,1.0);
    glTranslatef(width/2,height/2,0);
    int last_mouse_x = mouse_x;
    int last_mouse_y = mouse_y;
    mouse_x = mouse_x-width/2;
    mouse_y = mouse_y-height/2;
    root.transform();
    root.draw();
    mouse_x = last_mouse_x;
    mouse_y = last_mouse_y;

    // glCullFace(GL_FRONT);
    // draw_cube();
    // glCullFace(GL_BACK);
    // draw_cube();

    a = fmod(a+0.1, 360.);
    b = fmod(b+0.5, 360.);
    c = fmod(c+0.25, 360.);

    glXSwapBuffers(Xdisplay, glX_window_handle);
}

int main(int argc, char *argv[])
{
    gettimeofday(&earlier,NULL);
    mouse_x = -1;
    mouse_y = -1;
    createTheWindow();
    createTheRenderContext();
    circle = load_img("ring.png");
    line = load_img("line.png");
    root.read(1);

    while (updateTheMessageQueue()) {
    redrawTheWindow();
    }

    return 0;
}